#include <Streaming.h>
//#include <Time.h>
//#include <TimeAlarms.h>
#include <Metro.h>
#include <RCSwitch.h>
#include <DS3231.h>
#include <Wire.h>

// see AT24C32_TEST example for I2C memory (32k) for logging

// configure radio
RCSwitch radio = RCSwitch();

// configure RTC
DS3231 rtc;

// sensor and pump abstraction
#include "Bed.h"

// how many sensors are we reading?
const int nSensors = 3;
BIOSDigitalSoilMeter sensor[nSensors];

// how many pumps are we controlling?
const int nPumps = 4;
EtekcityOutlet pump[nPumps];

// what pumps water which sensors?
const boolean ps[nSensors] = {0, 0, 0};

// use LED to indicate status
// solid: one or more sensors is reporting too dry
// blinking: watering in progress
// SOS: a sensor is acting whacky
#define LED 13

void setup() {
  Serial.begin(115200);

  Serial << F("Startup.") << endl;
  Serial.setTimeout(10);

  // LED pin
  pinMode(LED, OUTPUT);

  // RTC
  Wire.begin();

  // Timers
  //  setTime(21, 0, 0, 27, 4, 11);
  //  Alarm.alarmRepeat(21, 0, 1, WateringTime);

  // 9pm. when hour, min, sec match.
  rtc.setClockMode(false); // 24 time.
  rtc.setA1Time(0, 21, 0, 0, B1000, true, false, false);
  rtc.turnOnAlarm(1);
  Serial << F("Watering alarm enabled? ") << rtc.checkAlarmEnabled(1) << endl;

  // Radio module
  radio.enableReceive(0);  // Receiver on interrupt 0 => that is pin D2
  radio.enableTransmit(10); // Transmitter on pin D10
  radio.setRepeatTransmit(3); // repeat a transmission 3 times.
  
  // pumps
  Serial << F("Pumps:") << endl;
  pump[0].begin("Pump 1", 2048, 3048);
  pump[1].begin("Pump 2", 2049, 3049);
  pump[2].begin("Pump 3", 2050, 3050);
  pump[3].begin("Pump 4", 2051, 3051);

  // sensors
  Serial << F("Sensors:") << endl;
  sensor[0].begin("Bed A", 1947, 6, 10);
  sensor[1].begin("Bed B", 1948, 6, 10);
  sensor[2].begin("Bed C", 1949, 4, 8);   // try to keep this bed drier

  // pump and sensor relationships
  Serial << F("Pump waters Sensors:") << endl;
  for ( int i = 0; i < nSensors; i++ ) {
    Serial << pump[ps[i]].name << F(" waters ") << sensor[i].name << endl;
  }

  Serial << F("Startup complete.") << endl;

}

void loop() {
  if (radio.available()) {
    Serial << F("Radio:") << endl;
    output(radio.getReceivedValue(), radio.getReceivedBitlength(), radio.getReceivedDelay(), radio.getReceivedRawdata(), radio.getReceivedProtocol());
    //    radio.resetAvailable();
  }
/*
  // look for sensor data
  getSensorData();

  // look for pump data
  notePumpManualControl();

  // check for time update from Serial
  getTimeUpdate();

  // monitor for too dry
  boolean tooDry = false;
  for (int s = 0; s < nSensors; s++ ) {
    if ( sensor[s].tooDry() ) tooDry |= true; // any tooDry signals tooDry
  }
  if ( tooDry ) { 
    printTime();
    Serial << F(" BAD! one or more sensors reports 'too dry'") << endl;
    ledTooDry();
    delay(1000);
  }

  // check alarm for Watering time
  if ( rtc.checkIfAlarm(1) ) {
    wateringTime();
  }
*/
}

void getTimeUpdate() {
  while (Serial.available() > 0) {
    // wait for everything to come in.
    delay(25);
    // look for the next valid integer in the incoming serial stream:
    int hr = Serial.parseInt();
    int mi = Serial.parseInt();
    int se = Serial.parseInt();
    int da = Serial.parseInt();
    int mo = Serial.parseInt();
    int ye = Serial.parseInt();
    while ( Serial.read() > -1); // dump anything trailing.

    Serial << F("Time update received. Format: hr, min, sec, day, month, year\\n") << endl;
    rtc.setHour(constrain(hr, 0, 24));
    rtc.setMinute(constrain(mi, 0, 60));
    rtc.setSecond(constrain(se, 0, 60));
    rtc.setDate(constrain(da, 1, 31));
    rtc.setMonth(constrain(mo, 1, 12));
    rtc.setYear(constrain(ye, 0, 99));
    Serial << F("Time set to: ");
    printTime();
    Serial << endl;
  }
}

void getSensorData() {
  for (int i = 0; i < nSensors; i++) {
    sensor[i].readSensor();
  }
}

void notePumpManualControl() {
  for (int i = 0; i < nPumps; i++) {
    pump[i].readPump();
  }
}

void printSensors() {
  Serial << F("Sensors:") << endl;
  for ( int s = 0; s < nSensors; s++ ) sensor[s].print();
}

void wateringTime() {
  printTime();
  Serial << F(".  Watering time.") << endl;

  // track the start time for this cycle
  unsigned long now = millis();

  // add some simulation
  for (int s = 0; s < nSensors; s++ ) {
    sensor[s].currMoist = random(0, 8);
  }

  // where are we?
  printSensors();

  boolean keepWatering = true;
  while ( keepWatering ) {
    // watch the radio
    getSensorData();
    notePumpManualControl();

    // indicate watering cycle
    ledWatering();

    // work through each pump
    for (int p = 0; p < nPumps; p++ ) {

      // add some simulation
      for (int s = 0; s < nSensors; s++ ) {
        if ( ps[s] == p && pump[p].isOn ) { // if this pump waters this sensor
          sensor[s].currMoist++;
          sensor[s].print();
        }
      }

      boolean tooDry = false;
      boolean tooWet = false;
      boolean justRight = true;
      // check each sensor assigned to this pump
      for (int s = 0; s < nSensors; s++ ) {
        if ( ps[s] == p ) { // if this pump waters this sensor
          if ( sensor[s].tooDry() ) tooDry |= true;
          if ( sensor[s].tooWet() ) tooWet |= true;
          if ( !sensor[s].justRight() ) justRight &= false; // hard to get them all just right with one pump
        }
      }
      //      Serial << "pump:" << p << " on?" << pump[p].isOn << " tooDry?" << tooDry << " tooWet?" << tooWet << endl;

      // examine the results, and decide what to do.
      //
      // generally, we want to water infrequently, but heavily if we do.
      // so, only turn on the pumps if the soil reads "too dry", but then run the pumps until we (just) hit "too wet"

      //      if ( tooWet || !tooDry ) {
      if ( tooWet || justRight ) {
        //        Serial << "too wet or not too dry" << endl;
        pump[p].turnOff();
      } else if ( tooDry ) {
        //       Serial << "too dry" << endl;
        pump[p].turnOn();
        delay(1000); // power draw on the pumps is high at startup.  stagger them.
      }
    }

    // check to see if all pumps are off.  If they are, we're done for the night.
    keepWatering = false;
    for (int p = 0; p < nPumps; p++ ) keepWatering |= pump[p].isOn;

//    delay(1000);
  }

  Serial << F("All pumps off.  Watering cycle complete.") << endl;

  // see where we ended up.
  boolean tooDry = false;
  for (int s = 0; s < nSensors; s++ ) {
    if ( sensor[s].tooDry() ) tooDry |= true; // any tooDry signals tooDry
  }
  if ( tooDry ) {
    Serial << F("BAD: one or more sensors reports 'too dry' after watering.") << endl;
  } else {
    Serial << F("GOOD: no sensors report 'too dry' after watering.") << endl;
  }
  printSensors();
  Serial << F("Total watering time: ") << (millis() - now) / 1000 / 60 << F(" minutes.") << endl;

}

void printTime() {
  static bool h12 = false;
  static bool PM = false;
  static bool Century = false;

  Serial << rtc.getHour(h12, PM) << F(":") << rtc.getMinute() << F(":") << rtc.getSecond() << F(" ");
  Serial << rtc.getMonth(Century) << F("/") << rtc.getDate() << F("/") << rtc.getYear();
}

// some morse code to indicate what we're doing
void ledTooDry() {
  ledMorse('d');
  ledMorse('.');  
}

void ledWatering() {
  ledMorse('w');
  ledMorse('.');
}

void ledSOS() {
  ledMorse('s');
  ledMorse('o');
  ledMorse('s');
  ledMorse('.');
}

void ledMorse(char letter) {
  const int dot = 200;
  const int dash = 3 * dot;
  const int pauseSpace = dot;
  const int letterSpace = dash;
  const int wordSpace = 7 * dot;

  switch (letter) {
    case 's':  // "S".  three dots.
      for (int i = 0; i < 3; i++) {
        digitalWrite(LED, HIGH);
        delay(dot);
        digitalWrite(LED, LOW);
        delay(pauseSpace);
      }
      delay(letterSpace);
      break;
    case 'o': // "O".  three dashes.
      for (int i = 0; i < 3; i++) {
        digitalWrite(LED, HIGH);
        delay(dash);
        digitalWrite(LED, LOW);
        delay(pauseSpace);
      }
      delay(letterSpace);

      break;
    case 'w': // "W".  dot dash dash
      digitalWrite(LED, HIGH);
      delay(dot);
      digitalWrite(LED, LOW);
      delay(pauseSpace);
      for (int i = 0; i < 2; i++) {
        digitalWrite(LED, HIGH);
        delay(dash);
        digitalWrite(LED, LOW);
        delay(pauseSpace);
      }
      delay(letterSpace);

      break;
    case 'd': // "D".  dash dot dot
      digitalWrite(LED, HIGH);
      delay(dash);
      digitalWrite(LED, LOW);
      delay(pauseSpace);
      for (int i = 0; i < 2; i++) {
        digitalWrite(LED, HIGH);
        delay(dot);
        digitalWrite(LED, LOW);
        delay(pauseSpace);
      }
      delay(letterSpace);

      break;
    case '.': // end of word
      delay(wordSpace);
      break;
    default:
      Serial << F("Don't know morse for: ") << letter << endl;
      break;
  }
}

static char * dec2binWzerofill(unsigned long Dec, unsigned int bitLength) {
  static char bin[64];
  unsigned int i = 0;

  while (Dec > 0) {
    bin[32 + i++] = (Dec & 1 > 0) ? '1' : '0';
    Dec = Dec >> 1;
  }

  for (unsigned int j = 0; j < bitLength; j++) {
    if (j >= bitLength - i) {
      bin[j] = bin[ 31 + i - (j - (bitLength - i)) ];
    } else {
      bin[j] = '0';
    }
  }
  bin[bitLength] = '\0';

  return bin;
}

void output(unsigned long decimal, unsigned int length, unsigned int delay, unsigned int* raw, unsigned int protocol) {

  if (decimal == 0) {
    Serial.print("Unknown encoding.");
  } else {
    char* b = dec2binWzerofill(decimal, length);
    Serial.print("Decimal: ");
    Serial.print(decimal);
    Serial.print(" (");
    Serial.print( length );
    Serial.print("Bit) Binary: ");
    Serial.print( b );
    Serial.print(" Tri-State: ");
    Serial.print( bin2tristate( b) );
    Serial.print(" PulseLength: ");
    Serial.print(delay);
    Serial.print(" microseconds");
    Serial.print(" Protocol: ");
    Serial.println(protocol);
  }

  Serial.print("Raw data: ");
  for (int i = 0; i <= length * 2; i++) {
    Serial.print(raw[i]);
    Serial.print(",");
  }
  Serial.println();
  Serial.println();
}


static char* bin2tristate(char* bin) {
  char returnValue[50];
  int pos = 0;
  int pos2 = 0;
  while (bin[pos] != '\0' && bin[pos + 1] != '\0') {
    if (bin[pos] == '0' && bin[pos + 1] == '0') {
      returnValue[pos2] = '0';
    } else if (bin[pos] == '1' && bin[pos + 1] == '1') {
      returnValue[pos2] = '1';
    } else if (bin[pos] == '0' && bin[pos + 1] == '1') {
      returnValue[pos2] = 'F';
    } else {
      return "not applicable";
    }
    pos = pos + 2;
    pos2++;
  }
  returnValue[pos2] = '\0';
  return returnValue;
}


