#include <EEPROM.h>
#include <Wire.h>

#define topFM  107900000            // Top of the FM Dial Range in USA
#define botFM   87500000            // Bottom of the FM Dial Range in USA
#define incrFM    200000            // FM Channel Increment in USA

byte number[] = {
  15, 8, 8, 7};

int serialCount = 0; // for receiving serial from Max
int serialArray[2];

long initialFrequency = 99300000;  // the default initial frequency in Hz
long frequency;          
long newFrequency = 0;
boolean gOnAir = false; // initially not on the air

void setup() {
  Serial.begin(9600); //for debugging
  initRadio();
}

void initRadio() {
  newFrequency = loadFrequency(); // attempt to read the last saved frequency from EEPROM
  // test if outside the FM Range
  if (newFrequency < botFM || newFrequency > topFM) frequency = initialFrequency; // haven't saved before, use the default.
  else frequency = newFrequency; // we have a valid frequency!
  
  Wire.begin();                       // join i2c bus as master
  transmitter_standby(frequency);
  delay(2000);
  transmitter_setup(frequency);
}

void loop() {
  check_serial();
}


void transmitter_setup(long initFrequency) {
  i2c_send(0x0E, B00000101); //Software reset
  i2c_send(0x01, B10110100); //Register 1: forced subcarrier, pilot tone on
  i2c_send(0x02, B00000011); //Register 2: Unlock detect off, 2mW Tx Power
  set_freq(initFrequency);
  //i2c_send(0x00, B10100001); //Register 0: 200mV audio input, 75us pre-emphasis on, crystal off, power on
  i2c_send(0x00, B00100001); //Register 0: 100mV audio input, 75us pre-emphasis on, crystal off, power on
  i2c_send(0x0E, B00000101); //Software reset
  i2c_send(0x06, B00011110); //Register 6: charge pumps at 320uA and 80 uA
}

void transmitter_standby(long aFrequency) {
  //i2c_send(0x00, B10100000); //Register 0: 200mV audio input, 75us pre-emphasis on, crystal off, power OFF
  i2c_send(0x00, B00100000); //Register 0: 100mV audio input, 75us pre-emphasis on, crystal off, power OFF

  delay(100);
  gOnAir = false;
}

void set_freq(long aFrequency) {
  int new_frequency;
  // New Range Checking... Implement the (experimentally determined) VFO bands:
  if (aFrequency < 88500000) {                       // Band 3
    i2c_send(0x08, B00011011);
    // Serial.println("Band 3");
  }  
  else if (aFrequency < 97900000) {                 // Band 2
    i2c_send(0x08, B00011010);
    // Serial.println("Band 2");
  }
  else if (aFrequency < 103000000) {                  // Band 1 
    i2c_send(0x08, B00011001);
    // Serial.println("Band 1");
  }
  else {
    // Must be OVER 103.000.000,                    // Band 0
    i2c_send(0x08, B00011000);
    // Serial.println("Band 0");
  }

  new_frequency = (aFrequency + 304000) / 8192;
  byte reg3 = new_frequency & 255;                  //extract low byte of frequency register
  byte reg4 = new_frequency >> 8;                   //extract high byte of frequency register
  i2c_send(0x03, reg3);                             //send low byte
  i2c_send(0x04, reg4);                             //send high byte

  // Retain old 'band set' code for reference....  
  // if (new_frequency <= 93100000) { i2c_send(0x08, B00011011); }
  // if (new_frequency <= 96900000) { i2c_send(0x08, ); }
  // if (new_frequency <= 99100000) { i2c_send(0x08, B00011001); }
  // if (new_frequency >  99100000) { i2c_send(0x08, B00011000); }

  i2c_send(0x0E, B00000101);                        //software reset  

  // Serial.print("Frequency changed to ");
  // Serial.println(aFrequency, DEC);

  //i2c_send(0x00, B10100001); //Register 0: 200mV audio input, 75us pre-emphasis on, crystal off, power ON
  i2c_send(0x00, B00100001); //Register 0: 100mV audio input, 75us pre-emphasis on, crystal off, power ON

  gOnAir = true;
}

void i2c_send(byte reg, byte data) { 
  Wire.beginTransmission(B1100111);               // transmit to device 1100111
  Wire.write(reg);                                 // sends register address
  Wire.write(data);                                // sends register data
  Wire.endTransmission();                         // stop transmitting
  delay(5);                                       // allow register to set
}

// check serial from Max/MSP
void check_serial() {
  if (Serial.available() > 0) {
    int inByte = Serial.read();
    if (inByte == 255) serialCount = 0;
    // set incoming byte into a temporary array and move through it
    // these values will be reassigned
    else {
      serialArray[serialCount] = inByte;
      serialCount++; 
      if (serialCount == 2) {
        transmitter_standby(frequency);
        int left = serialArray[0] * 100;
        long tempFrequency = left + (long) serialArray[1];
        frequency = tempFrequency * 10000;
        set_freq(frequency);
        saveFrequency(frequency);
        Serial.write(serialArray[0]);
        Serial.write(serialArray[1]);
        // reset the serial count to receive the next message
        serialCount = 0;
      }
    }
    // set transmitter into standby mode
    if (inByte == 254) transmitter_standby(frequency);
  }
}

//////////////////////
// EEPROM FUNCTIONS //
//////////////////////

void saveFrequency(long aFrequency) {
  long memFrequency = 0; // for use in Read / Write to EEProm

  //Serial.print( "Saving: " );
  //Serial.println(aFrequency, DEC);

  memFrequency = aFrequency / 10000;
  EEPROM.write(0, memFrequency / 256); // right-most byte
  EEPROM.write(1, memFrequency - (memFrequency / 256) * 256); // next to right-most byte
}

long loadFrequency() {
  long memFrequency = 0; // for use in Read / Write to EEProm

  memFrequency = EEPROM.read(0) * 256 + EEPROM.read(1);
  memFrequency *= 10000;

  //Serial.print("Retrieving: " );
  //Serial.println(memFrequency, DEC);
  return memFrequency;
}
