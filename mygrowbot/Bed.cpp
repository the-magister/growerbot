#include "Bed.h"

extern RCSwitch radio;
extern void printTime();

void Sensor::begin(char * name, int sensorAddress, byte minMoist, byte maxMoist) {
  // set name
  strcpy(this->name, name);
  // set sensor address
  this->sensorAddress = sensorAddress;
  // set targets
  setMoistureTargets(minMoist, maxMoist);

  this->print();
}

void Sensor::print() {
  printTime();
  Serial << F(": ") << this->name;
  Serial << F(". Moisture curr[min/max]: ") << this->currMoist << F("[") << this->minMoist << F("/") << this->maxMoist << "]";
  Serial << F(". Temp: ") << convertCtoF(this->currTemp) << F("F.") << endl;
}

void Sensor::setMoistureTargets(byte minMoist, byte maxMoist) {
  this->minMoist = minMoist;
  this->maxMoist = maxMoist;
}

void Sensor::readSensor() {
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

boolean Sensor::tooDry() {
  return( this->currMoist < this->minMoist );
}
boolean Sensor::tooWet(){
  return( this->currMoist > this->maxMoist );
}
boolean Sensor::justRight() {
  return( !this->tooDry() && !this->tooWet() );
}

// see http://rayshobby.net/reverse-engineer-a-cheap-wireless-soil-moisture-sensor/
int Sensor::decodeAddress(unsigned long data) {
  return ( getBits(data, 32, 11) );
}
float Sensor::decodeTemp(unsigned long data) {
  return ( float(getBits(data, 20, 12)) / 10.0 );
}
byte Sensor::decodeMoist(unsigned long data) {
  return ( getBits(data, 8, 4) );
}

void Pump::begin(char * name, int pumpAddress){
  // set name
  strcpy(this->name, name);
  // set sensor address
  this->pumpAddress = pumpAddress;
  // pump state
  this->isOn = false;
  
  // show it
  this->print();
}

// turn pump on and off
void Pump::turnOn(){
  if( ! this->isOn ) {
    this->onTime = millis();
  
    this->toggle();
    
    this->print();
  }
}
void Pump::turnOff(){
  if( this->isOn ) {
    this->toggle();
    
    this->print();
  }
}

void Pump::toggle() {
  this->isOn = !this->isOn;
  radio.send(this->pumpAddress, PUMPBITLENGTH); // Turn switch on/off
}

void Pump::readPump(){
  if (radio.available()) {
    // pull radio data
    unsigned long recv = radio.getReceivedValue();
 
    if( decodeAddress(recv) == this->pumpAddress ) {
      this->isOn = !this->isOn;
      this->lastPumpRead = millis();
      this->print();
    
      radio.resetAvailable(); // clear radio buffer
    }
  }

}

int Pump::decodeAddress(unsigned long data) {
  return( data );
}

// show pump parameters
void Pump::print(){
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
