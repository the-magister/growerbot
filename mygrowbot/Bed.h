#ifndef Bed_h
#define Bed_h

#include <Arduino.h>

#include <Streaming.h>
#include <RCSwitch.h>

// Find the three values for your switch by using the Advanced Receieve sketch
// Supplied with the RCSwitch library

#define PUMPBITLENGTH 24    // And the bitlength is 24
#define PUMPPULSELENGTH 166 // Amd the pulse length in microseconds is 166

class Sensor {
  public:
    void begin(char name[], int sensorAddress, byte minMoist=6, byte maxMoist=11);
               
    void setMoistureTargets(byte minMoist, byte maxMoist);
       
    // store bed moisture readings
    int currMoist;

    // store bed temperature readings
    float currTemp;

    // look for sensor update and parse.  sets currMoist and currTemp.
    void readSensor();
 
    // show sensor parameters
    void print();
    
    // does the bed need to be watered?
    boolean tooDry();
    boolean tooWet();
    boolean justRight();

    // store sensor name
    char name[20];
  
  private:
    
   // store the sensor address code
    int sensorAddress;
    
    // define moisture targets for each bed. 0-3 (dry), 4-7 (damp), 8-11 (wet)
    byte minMoist;
    byte maxMoist;

    // handles the sensor data packet
    int decodeAddress(unsigned long data);
    float decodeTemp(unsigned long data);
    byte decodeMoist(unsigned long data);
    
    // stores last sensor update time
    unsigned long lastSensorRead;

 };
 
 class Pump {
  public:
    void begin(char name[], int pumpAddress);
               
    // store current pump state
    boolean isOn;

    // turn pump on and off
    void turnOn();
    void turnOff();

    // look for manual pump control (to stay in sync).
    void readPump();
    
    // show pump parameters
    void print();
    
    // store pump name
    char name[20];

  private:

    // store the pump address code
    int pumpAddress;

     // stores time pump was turned on
    unsigned long onTime;

    // toggle pump
    void toggle();
    
    // handles the sensor data packet
    int decodeAddress(unsigned long data);
   
    // stores the last time we got a manual pump change signal
    unsigned long lastPumpRead;

 };

// helper functions
unsigned long getBits(unsigned long data, int startBit, int nBits) ;
float convertCtoF(float c);
float convertFtoC(float f);
float computeHeatIndex(float tempFahrenheit, float percentHumidity);

#endif
