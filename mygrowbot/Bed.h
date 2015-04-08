#ifndef Bed_h
#define Bed_h

#include <Arduino.h>

#include <Streaming.h>
#include <RCSwitch.h>

class BIOSDigitalSoilMeter {
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
 
 class EtekcityOutlet {
   public:
    void begin(char name[], int onCode, int offCode);
               
    // store current outlet state
    boolean isOn;

    // turn outlet on and off
    void turnOn();
    void turnOff();

    // look for manual outlet control (to stay in sync).
    void readPump();
    
    // show outlet parameters
    void print();
    
    // store outlet name
    char name[20];

  private:

    // store the outlet address codes
    int onCode, offCode;

     // stores time outlet was turned on
    unsigned long onTime;

    // toggle outlet
    void toggle();
    
    // handles the outlet data packet
    int decodeAddress(unsigned long data);
   
    // stores the last time we got a manual outlet change signal
    unsigned long lastOutletRead;

 };

// helper functions
unsigned long getBits(unsigned long data, int startBit, int nBits) ;
float convertCtoF(float c);
float convertFtoC(float f);
float computeHeatIndex(float tempFahrenheit, float percentHumidity);

#endif
