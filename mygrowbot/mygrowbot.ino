#include <Streaming.h>
#include <Time.h>
#include <TimeAlarms.h>
#include <Metro.h>

// define moisture targets
double targetMoist=100;
double currentMoist=0;

// maximum watering time allowed.
unsigned long maxWaterTime = 4 ; // hr

void setup() {
  Serial.begin(115200);
  
  // Timers
  setTime(21, 0, 0, 27, 4, 11);  
  Alarm.alarmRepeat(21, 0, 1, WateringTime);
  
}

void loop() {
  printTime();
  // also the Alarm servicing function
  Alarm.delay(10000);
}

void WateringTime() {
  Serial << F("Watering time. At: ");
  printTime();

  // get current reading.
  getMoisture();

  if( currentMoist >= targetMoist ) {
    Serial << F("Target moisture reached. Stopping.")  << endl;
    return;
  }

  Serial << F("Maximum watering time: ") << maxWaterTime << F("h") << endl;
  Metro maxWaterTimer(maxWaterTime*60*60*1000);
  maxWaterTimer.reset();
  
  unsigned long now = millis();
  
  boolean keepWatering = true;
  togglePump();
  while( keepWatering ) {
    // get current reading.
    getMoisture();
    
    if( maxWaterTimer.check() ) {
      Serial << F("Maximum watering time reached. Stopping.")  << endl;
      keepWatering = false;
    }
       
    if( currentMoist >= targetMoist ) {
      Serial << F("Target moisture reached. Stopping.")  << endl;
      keepWatering = false;
    }
    
    delay(25);
    
  }
  
  togglePump();
  
  Serial << F("Total watering time: ") << (millis() - now)/1000/60 << F(" minutes.") << endl;
}

void printTime() {
  Serial << hour() << F(":") << minute() << F(":") << second() << F(" ");
  Serial << month() << F("/") << day() << F("/") << year();
  Serial << endl;
}

void togglePump() {
  Serial << F("Pump toggled at: ");
  printTime();
}

void getMoisture() {
  currentMoist+=10;
  
  Serial << F("Moisture: current [") << currentMoist << F("] -> Target[") << targetMoist << F("].") << endl;
}
