/** 
 * ECM Standalone FM Transmitter
 *
 * A lot of code and circuit was guided by http://www.km5z.us/FM-Stereo-Broadcaster.php
 *
 */

#include <Wire.h>
#include <EEPROM.h>

#define topFM  107900000 // top of the FM range
#define botFM   87500000 // bottom of the FM range
#define incrFM    200000 // FM channel increment
#define encoderPinA 3
#define encoderPinB 2
#define encoderSwitch 4

int encoderPos = 0; // initial encoder position
int lastEncoderSwitchState = LOW; // initial button state
int encoderSwitchState; // current button state
long lastDebounceTime = 0; // temporary debounce time storage
long debounceDelay = 50; // debounce time

byte number[4]; // number to be displayed on the 7 segment display
byte ls247pins[] = {
  12, 10, 8, 6};
byte anodepins[] = {
  13, 9, 11, 7};
byte dp = 5; // decimal point

// for parsing incoming serial messages
int serialArray[2];
int serialCount = 0;

long initialFrequency = 99300000;
long frequency = initialFrequency; // the default initial frequency in Hz
long newFrequency = 0;
boolean gOnAir = false; // initially NOT on air

//////////
// MAIN //
//////////

void setup() {
  Serial.begin(9600);
  initRadio();
}

void loop() {
  readSwitch(); // check if the switch on the encoder has been pressed
  setDisplayFrequency(frequency); // prepare current frequency for 7 segment display
  for (int i = 0; i < 4; i++) { // display the frequency on the 7 segment display
    led7segWriteDigit(i, number[i]);
  }
  checkSerial(); // check for serial info from Max to set a frequency that way
}

///////////////////
// OUR FUNCTIONS //
///////////////////

void initRadio() {
  // attempt to read the last saved frequency from EEPROM
  newFrequency = loadFrequency();
  // test if outside the FM Range
  if (newFrequency < botFM || newFrequency > topFM) frequency = initialFrequency; // haven't saved before, use the default.
  else frequency = newFrequency; // we have a valid frequency!

  // set pin types
  pinMode(encoderPinA, INPUT_PULLUP);
  pinMode(encoderPinB, INPUT_PULLUP);
  pinMode(encoderSwitch, INPUT_PULLUP);
  for (int i = 0; i < 4; i++) {
    pinMode(ls247pins[i], OUTPUT);
    pinMode(anodepins[i], OUTPUT);
  }
  pinMode(dp, OUTPUT);  
  digitalWrite(dp, HIGH);
  attachInterrupt(0, doEncoder, CHANGE);  // encoder pin on interrupt 0 - pin 2

  Wire.begin(); // join i2c bus as master
  transmitter_standby(frequency);
  delay(2000);
  transmitter_setup(frequency);
}

void checkSerial() {
  if (Serial.available() > 0) {
    int inByte = Serial.read(); // read incoming message
    if (inByte == 254) transmitter_standby(frequency); // set transmitter into standby mode
    if (inByte == 255) serialCount = 0; // change the transmission frequency
    // set incoming byte into a temporary array and move through it
    // these values will be reassigned
    else {
      serialArray[serialCount] = inByte;
      serialCount++; 
      if (serialCount == 2) {
        transmitter_standby(frequency);
        // parse incoming numbers from Max and set that as the new frequency
        int left = serialArray[0] * 100;
        long tempFrequency = left + (long) serialArray[1];
        frequency = tempFrequency * 10000;
        set_freq(frequency);
        saveFrequency(frequency); // save the frequency to EEPROM
        // write the incoming messages for debugging in Max
        // Serial.write(serialArray[0]);
        // Serial.write(serialArray[1]);
        // reset the serial count to receive the next message
        serialCount = 0;
      }
    }
  }
}

void readSwitch() {
  int reading = digitalRead(encoderSwitch); // read encoder switch
  if (reading != lastEncoderSwitchState) {
    lastDebounceTime = millis(); // start debouncing
  }
  if ((millis() - lastDebounceTime) > debounceDelay) {
    encoderSwitchState = reading; // we're past the debounce period, so read the button press
  }
  // when the button is pressed
  if (encoderSwitchState == LOW) {
    // go into standby
    if (gOnAir) {
      transmitter_standby(frequency);
      encoderSwitchState = HIGH;
    }
    // then set the new frequency
    else {
      set_freq(frequency);
      saveFrequency(frequency); // save the Frequency to EEPROM Memory
      delay(1000);
      encoderSwitchState = HIGH;
    }
  }
  lastEncoderSwitchState = reading;
}

// display frequency on 7 segment display
void setDisplayFrequency(long input) {
  // int freq;
  int freq = (int) (input / 100000);
  number[0] = freq / 1000; // should be 0 or 1
  if (number[0] == 0) number[0] = 15; // display blank LCD character
  freq = freq % 1000;
  number[1] = freq / 100;
  freq = freq % 100;
  number[2] = freq / 10;
  freq = freq % 10;
  number[3] = freq;
}

void led7segWriteDigit(int digit, int value) {
  // turn off the previous led
  if (digit == 0) digitalWrite(anodepins[3], HIGH);
  else digitalWrite(anodepins[digit-1], HIGH);
  // set decimal point
  if (digit == 3) (digitalWrite(dp, LOW));
  else (digitalWrite(dp, HIGH));
  // write incoming integer value to 7 segment display
  for (int i = 0; i < 4; i++) {
    digitalWrite(ls247pins[i], (bitRead(value, i)));
  } 
  // turn on the appropriate digit
  digitalWrite(anodepins[digit], LOW);
  delay(5);
}

///////////////
// INTERRUPT //
///////////////

void doEncoder() {
  // If pinA and pinB are both high or both low, the encoder is spinning forward.
  // If they're different, it's going backward.
  if (digitalRead(encoderPinA) == digitalRead(encoderPinB)) {
    frequency -= incrFM; 
    frequency = constrain(frequency, botFM, topFM);  // Keeps us in range
  } 
  else {
    frequency += incrFM; 
    frequency = constrain(frequency, botFM, topFM);  // Keeps us in range
  }
}

/////////////////////////////
// RADIO CONTROL FUNCTIONS //
/////////////////////////////

void transmitter_setup(long initFrequency) {
  i2c_send(0x0E, B00000101); // Software reset
  i2c_send(0x01, B10110100); // Register 1: forced subcarrier, pilot tone on
  i2c_send(0x02, B00000011); // Register 2: Unlock detect off, 2mW Tx Power
  set_freq(initFrequency); // set the frequency
  // i2c_send(0x00, B10100001); // Register 0: 200mV audio input, 75us pre-emphasis on, crystal off, power on
  i2c_send(0x00, B00100001); // Register 0: 100mV audio input, 75us pre-emphasis on, crystal off, power on
  i2c_send(0x0E, B00000101); // Software reset
  i2c_send(0x06, B00011110); // Register 6: charge pumps at 320uA and 80 uA
}

void transmitter_standby(long aFrequency) {
  // i2c_send(0x00, B10100000); // Register 0: 200mV audio input, 75us pre-emphasis on, crystal off, power OFF
  i2c_send(0x00, B00100000); // Register 0: 100mV audio input, 75us pre-emphasis on, crystal off, power OFF
  delay(100);
  gOnAir = false;
}

void set_freq(long aFrequency) {
  int new_frequency;
  
  // new range checking; implement the (experimentally determined) VFO bands:
  if (aFrequency < 88500000) i2c_send(0x08, B00011011); // band 3
  else if (aFrequency < 97900000) i2c_send(0x08, B00011010); // band 2
  else if (aFrequency < 103000000) i2c_send(0x08, B00011001); // band 1 
  else i2c_send(0x08, B00011000); // band 0

  new_frequency = (aFrequency + 304000) / 8192;
  byte reg3 = new_frequency & 255; // extract low byte of frequency register
  byte reg4 = new_frequency >> 8; // extract high byte of frequency register
  i2c_send(0x03, reg3); // send low byte
  i2c_send(0x04, reg4); // send high byte
  i2c_send(0x0E, B00000101); // software reset  

  // i2c_send(0x00, B10100001); // Register 0: 200mV audio input, 75us pre-emphasis on, crystal off, power ON
  i2c_send(0x00, B00100001); // Register 0: 100mV audio input, 75us pre-emphasis on, crystal off, power ON

  gOnAir = true;
}

void i2c_send(byte reg, byte data) { 
  Wire.beginTransmission(B1100111); // transmit to device 1100111
  Wire.write(reg); // sends register address
  Wire.write(data); // sends register data
  Wire.endTransmission(); // stop transmitting
  delay(5); // allow register to set
}

//////////////////////
// EEPROM FUNCTIONS //
//////////////////////

void saveFrequency(long aFrequency) {
  long memFrequency = 0; // for use in read/write to EEPROM
  memFrequency = aFrequency / 10000;
  EEPROM.write(0, memFrequency / 256); // right-most byte
  EEPROM.write(1, memFrequency - (memFrequency / 256) * 256); // next to right-most byte
}

long loadFrequency() {
  long memFrequency = 0; // for use in read/write to EEPROM
  memFrequency = EEPROM.read(0) * 256 + EEPROM.read(1);
  memFrequency *= 10000;
  return memFrequency;
}
