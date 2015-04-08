#include "Bed.h"

extern RCSwitch radio;
extern void printTime();

// Find the three values for your switch by using the Advanced Receieve sketch
// Supplied with the RCSwitch library
const int EtekcityOutletBitLength=24;    // the pump bitlength is 24
const int EtekcityOutletPulseLength=166; // the pump pulse length in microseconds is 166

void BIOSDigitalSoilMeter::begin(char * name, int sensorAddress, byte minMoist, byte maxMoist) {
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
  return( !this->tooDry() && !this->tooWet() );
}

// see http://rayshobby.net/reverse-engineer-a-cheap-wireless-soil-moisture-sensor/
int BIOSDigitalSoilMeter::decodeAddress(unsigned long data) {
  return ( getBits(data, 32, 11) );
}
float BIOSDigitalSoilMeter::decodeTemp(unsigned long data) {
  return ( float(getBits(data, 20, 12)) / 10.0 );
}
byte BIOSDigitalSoilMeter::decodeMoist(unsigned long data) {
  return ( getBits(data, 8, 4) );
}

void EtekcityOutlet::begin(char * name, int onCode, int offCode){
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

// turn pump on and off
void EtekcityOutlet::turnOn(){
  if( ! this->isOn ) {
    this->onTime = millis();
  
    this->toggle();
    
    this->print();
  }
}
void EtekcityOutlet::turnOff(){
  if( this->isOn ) {
    this->toggle();
    
    this->print();
  }
}

void EtekcityOutlet::toggle() {
  this->isOn = !this->isOn;
  radio.setPulseLength(EtekcityOutletPulseLength);  // set the pulse length to talk to the pumps

  if( this->isOn ) {
     radio.send(this->onCode, EtekcityOutletBitLength); // Turn switch on/off
  } else {
     radio.send(this->offCode, EtekcityOutletBitLength); // Turn switch on/off
  }
}

void EtekcityOutlet::readPump(){
  if (radio.available()) {
    // pull radio data
    unsigned long recv = radio.getReceivedValue();
 
    if( decodeAddress(recv) == this->onCode ) {
      this->isOn = true;
      this->lastOutletRead = millis();
      this->print();
    
      radio.resetAvailable(); // clear radio buffer
    }
    else if( decodeAddress(recv) == this->offCode ) {
      this->isOn = false;
      this->lastOutletRead = millis();
      this->print();
    
      radio.resetAvailable(); // clear radio buffer
    }
  }

}

int EtekcityOutlet::decodeAddress(unsigned long data) {
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
