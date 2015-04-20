#ifndef Bed_h
#define Bed_h

#define DROP_REPEAT_DELAY 1000UL // messages are received with repeats.  If we get a good one, wait so we drop the extra.

#include <Arduino.h>
#include <Streaming.h> // this needs to be #include'd in the .ino file, too.

class BIOSDigitalSoilMeter {
  public:
    void begin(char name[], unsigned long sensorAddress, byte minMoist=6, byte maxMoist=11);
               
    // accessor functions
    // get current moisture
    byte getMoist();
    // get bed temperature
    float getTemp();
    // does the bed need to be watered?
    boolean tooDry(); // currMoist < minMoist
    boolean tooWet(); // currMoist > maxMoist
    boolean justRight(); // currMoist == maxMoist+1

    // look for sensor update in a received message
    // if address matches, parse (set currMoist and currTemp), and return true
    // if address doesn't match, return false.
    boolean readMessage(unsigned long recv);
 
    // sets targets    
    void setMoistureTargets(byte minMoist, byte maxMoist);
       
    // store sensor name
    char name[20];
  
    // show sensor parameters
    void print();

private:    
    // store the sensor address code
    unsigned long sensorAddress;
    
    // min target, max target, current moisture level. 0-3 (dry), 4-7 (damp), 8-11 (wet)
    byte minMoist, maxMoist, currMoist;
    
    // current temperature
    float currTemp;
    
    // handles the sensor data packet
    unsigned long decodeAddress(unsigned long data);
    float decodeTemp(unsigned long data);
    byte decodeMoist(unsigned long data);
    
};
 
 class EtekcityOutlet {
   public:
    void begin(char name[], unsigned long onCode, unsigned long offCode);
              
    // accessor functions 
    // return current outlet state; true==on, false==off
    boolean on();

    // return message required to outlet on and off
    unsigned long turnOn();
    unsigned long turnOff();

    // look for manual outlet control; if address matches, parse (sets on/off), and return true;
    boolean readMessage(unsigned long recv);
    
    // show outlet parameters
    void print();
    
    // store outlet name
    char name[20];

  private:

    // store the outlet address codes
    unsigned long onCode, offCode;
    
    // store the outlet state
    boolean isOn;

    // handles the outlet data packet
    unsigned long decodeAddress(unsigned long data);
   
};

// helper functions
unsigned long getBits(unsigned long data, int startBit, int nBits) ;
float convertCtoF(float c);
float convertFtoC(float f);
float computeHeatIndex(float tempFahrenheit, float percentHumidity);

#endif
