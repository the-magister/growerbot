#include "Bed.h"

extern RCSwitch radio;
extern void printTime();

// Find the three values for your switch by using the Advanced Receieve sketch
// Supplied with the RCSwitch library
const int EtekcityOutletBitLength=24;    // the pump bitlength is 24
const int EtekcityOutletPulseLength=180; // the pump pulse length in microseconds is 180

void BIOSDigitalSoilMeter::begin(char * name, unsigned long sensorAddress, byte minMoist, byte maxMoist) {
  // set name
  strcpy(this->name, name);
  // set sensor address
  this->sensorAddress = sensorAddress;
  // set targets
  setMoistureTargets(minMoist, maxMoist);

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

void BIOSDigitalSoilMeter::readSensor() {
  if (radio.available()) {
    // pull radio data
    unsigned long recv = radio.getReceivedValue();
 
    if( decodeAddress(recv) == this->sensorAddress ) {
      this->currMoist = decodeMoist(recv);
      this->currTemp = decodeTemp(recv);
      this->lastSensorRead = millis();
      this->print();
    
      radio.resetAvailable(); // clear radio buffer
    }
  }
}

boolean BIOSDigitalSoilMeter::tooDry() {
  return( this->currMoist < this->minMoist );
}
boolean BIOSDigitalSoilMeter::tooWet(){
  return( this->currMoist > this->maxMoist );
}
boolean BIOSDigitalSoilMeter::justRight() {
  // just before reporting "too wet"
  return( this->currMoist+1 == this->maxMoist );
}

// see http://rayshobby.net/reverse-engineer-a-cheap-wireless-soil-moisture-sensor/
unsigned long BIOSDigitalSoilMeter::decodeAddress(unsigned long data) {
  return ( getBits(data, 32, 11) );
}
float BIOSDigitalSoilMeter::decodeTemp(unsigned long data) {
  return ( float(getBits(data, 20, 12)) / 10.0 );
}
byte BIOSDigitalSoilMeter::decodeMoist(unsigned long data) {
  return ( getBits(data, 8, 4) );
}

void EtekcityOutlet::begin(char * name, unsigned long onCode, unsigned long offCode, int bitLength, int pulseLength, int protocol){
  // set name
  strcpy(this->name, name);
  
  // set pump codes
  this->onCode = onCode;
  this->offCode = offCode;

  // set protocol for transmission
  this->bitLength = bitLength;
  this->pulseLength = pulseLength;
  this->protocol = protocol;

  // pump state
  this->isOn = false;
  this->turnOff();
  
  // show it
  this->print();
}

// turn outlet on 
void EtekcityOutlet::turnOn(){
  // only update onTime and print if there's a change
  if( ! this->isOn ) {
    this->isOn = true;
    this->onTime = millis();
    this->print();
  }
  this->send(this->onCode);
}

// turn outlet off
void EtekcityOutlet::turnOff(){
  // only print if there's a change
  if( this->isOn ) {
    this -> isOn = false;
    this->print();
  }
  this->send(this->offCode);
}

// handles transmission
void EtekcityOutlet::send(unsigned long code) {
  radio.setProtocol(this->protocol);        // protocol 1
  radio.setPulseLength(this->pulseLength);   // 180 us pulse length
  radio.send(code, this->bitLength);
}

void EtekcityOutlet::readOutlet(){
  if (radio.available()) {
    // pull radio data
    unsigned long recv = radio.getReceivedValue();
 
    if( decodeAddress(recv) == this->onCode ) {
      if( !this->isOn ) {
        this->isOn = true;
        this->print();
      }
    
      this->lastOutletRead = millis();
      radio.resetAvailable(); // clear radio buffer
    }
    else if( decodeAddress(recv) == this->offCode ) {
      if( this->isOn ) {
        this->isOn = false;
        this->print();
      }

      this->lastOutletRead = millis();  
      radio.resetAvailable(); // clear radio buffer
    }
  }

}

unsigned long EtekcityOutlet::decodeAddress(unsigned long data) {
  return( data );
}

// show pump parameters
void EtekcityOutlet::print(){
  printTime();
  Serial << F(": ") << this->name;
  Serial << F(". Status: ");
  if(this->isOn) Serial << F("ON.");
  else Serial << F("OFF.");
  Serial << endl;
}

// helper function
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
