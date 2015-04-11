#ifndef Bed_h
#define Bed_h

#include <Arduino.h>

#include <Streaming.h>
#include <RCSwitch.h>

class BIOSDigitalSoilMeter {
  public:
    void begin(char name[], unsigned long sensorAddress, byte minMoist=6, byte maxMoist=11);
               
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
    unsigned long sensorAddress;
    
    // define moisture targets for each bed. 0-3 (dry), 4-7 (damp), 8-11 (wet)
    byte minMoist;
    byte maxMoist;

    // handles the sensor data packet
    unsigned long decodeAddress(unsigned long data);
    float decodeTemp(unsigned long data);
    byte decodeMoist(unsigned long data);
    
    // stores last sensor update time
    unsigned long lastSensorRead;

 };
 
 class EtekcityOutlet {
   public:
    void begin(char name[], unsigned long onCode, unsigned long offCode, int bitLength=24, int pulseLength=180, int protocol=1);
               
    // store current outlet state
    boolean isOn;

    // turn outlet on and off
    void turnOn();
    void turnOff();

    // look for manual outlet control (to stay in sync).
    void readOutlet();
    
    // show outlet parameters
    void print();
    
    // store outlet name
    char name[20];

  private:

    // store the outlet address codes
    unsigned long onCode, offCode;

     // stores time outlet was turned on
    unsigned long onTime;
    
    // handles the outlet data packet
    unsigned long decodeAddress(unsigned long data);
   
    // stores the last time we got a manual outlet change signal
    unsigned long lastOutletRead;
    
    // store bit length, pulse length and protocol for sending
    int bitLength, pulseLength, protocol;
    
    // handles transmission
    void send(unsigned long code);

 };

// helper functions
unsigned long getBits(unsigned long data, int startBit, int nBits) ;
float convertCtoF(float c);
float convertFtoC(float f);
float computeHeatIndex(float tempFahrenheit, float percentHumidity);

#endif
