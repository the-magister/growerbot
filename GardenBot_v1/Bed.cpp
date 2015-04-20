#include "Bed.h"

extern void printTime();

void BIOSDigitalSoilMeter::begin(char * name, unsigned long sensorAddress, byte minMoist, byte maxMoist) {
  // set name
  strcpy(this->name, name);
  // set sensor address
  this->sensorAddress = decodeAddress(sensorAddress);
  // set targets
  setMoistureTargets(minMoist, maxMoist);
  // at startup, set the current to not trigger alarms
  this->currMoist = maxMoist - 1;

  this->print();
}

void BIOSDigitalSoilMeter::print() {
  printTime();
  Serial << F(": ") << this->name;
  Serial << F(". Moisture curr[min/max]: ") << this->currMoist << F("[") << this->minMoist << F("/") << this->maxMoist << "]";
  Serial << F(". Temp: ") << convertCtoF(this->currTemp) << F("F.") << endl;
}

void BIOSDigitalSoilMeter::setMoistureTargets(byte minMoist, byte maxMoist) {
  this->minMoist = minMoist;
  this->maxMoist = maxMoist;
}

boolean BIOSDigitalSoilMeter::tooDry() {
  return ( this->currMoist < this->minMoist );
}
boolean BIOSDigitalSoilMeter::tooWet() {
  return ( this->currMoist > this->maxMoist );
}
boolean BIOSDigitalSoilMeter::justRight() {
  // just before reporting "too wet"
  return ( this->currMoist + 1 == this->maxMoist );
}

// parse the message
boolean BIOSDigitalSoilMeter::readMessage(unsigned long recv) {

  if ( decodeAddress(recv) == this->sensorAddress ) {
    this->currMoist = decodeMoist(recv);
    this->currTemp = decodeTemp(recv);
    this->print();
    delay(DROP_REPEAT_DELAY);
    return ( true );
  } else {
    return ( false );
  }
}
// see http://rayshobby.net/reverse-engineer-a-cheap-wireless-soil-moisture-sensor/
unsigned long BIOSDigitalSoilMeter::decodeAddress(unsigned long data) {
  return ( getBits(data, 32, 9) );
}
float BIOSDigitalSoilMeter::decodeTemp(unsigned long data) {
  int temp = getBits(data, 20, 12);
  // stored as an 12-bit number with 2's complement math for negative values 

  // check for sign bit
  if( bitRead(temp, 11) ) {
    // if there is one, left-pad with 1's.
    temp |= 0b1111000000000000;
  }
  
  return ( float(temp) / 10.0 );
}
byte BIOSDigitalSoilMeter::decodeMoist(unsigned long data) {
  return ( getBits(data, 8, 4) );
}

void EtekcityOutlet::begin(char * name, unsigned long onCode, unsigned long offCode) {
  // set name
  strcpy(this->name, name);

  // set pump codes
  this->onCode = onCode;
  this->offCode = offCode;

  // pump state
  this->isOn = false;

  // show it
  this->print();
}

boolean EtekcityOutlet::on() {
  return( this->isOn );
}

// turn outlet on
unsigned long EtekcityOutlet::turnOn() {
  // only update onTime and print if there's a change
  if ( ! this->isOn ) {
    this->isOn = true;
    this->print();
  }
  
 return( this->onCode );
}

// turn outlet off
unsigned long EtekcityOutlet::turnOff() {
  // only print if there's a change
  if ( this->isOn ) {
    this->isOn = false;
    this->print();
  }
  return( this->offCode );
}

boolean EtekcityOutlet::readMessage(unsigned long recv) {
  if ( decodeAddress(recv) == this->onCode ) {
    if ( !this->isOn ) {
      this->isOn = true;
      this->print();
    }
    delay(DROP_REPEAT_DELAY);
    return ( true );
  } else if ( decodeAddress(recv) == this->offCode ) {
    if ( this->isOn ) {
      this->isOn = false;
      this->print();
    }
    delay(DROP_REPEAT_DELAY);
    return ( true );
  }
  return ( false );
}

unsigned long EtekcityOutlet::decodeAddress(unsigned long data) {
  return ( data );
}

// show pump parameters
void EtekcityOutlet::print() {
  printTime();
  Serial << F(": ") << this->name;
  Serial << F(". Status: ");
  if (this->isOn) Serial << F("ON.");
  else Serial << F("OFF.");
  Serial << endl;
}

// helper function; leading and trailing bits; bitshift right
unsigned long getBits(unsigned long data, int startBit, int nBits) {
  const int dataLen = 32;
  return ( ( data << (dataLen - startBit) ) >> (dataLen - nBits) );
}

float convertCtoF(float c) {
  return c * 9.0 / 5.0 + 32.0;
}

float convertFtoC(float f) {
  return (f - 32.0) * 5.0 / 9.0;
}


float computeHeatIndex(float tempFahrenheit, float percentHumidity) {
  // Adapted from equation at: https://github.com/adafruit/DHT-sensor-library/issues/9 and
  // Wikipedia: http://en.wikipedia.org/wiki/Heat_index
  return -42.379 +
         2.04901523 * tempFahrenheit +
         10.14333127 * percentHumidity +
         -0.22475541 * tempFahrenheit * percentHumidity +
         -0.00683783 * pow(tempFahrenheit, 2) +
         -0.05481717 * pow(percentHumidity, 2) +
         0.00122874 * pow(tempFahrenheit, 2) * percentHumidity +
         0.00085282 * tempFahrenheit * pow(percentHumidity, 2) +
         -0.00000199 * pow(tempFahrenheit, 2) * pow(percentHumidity, 2);
}
