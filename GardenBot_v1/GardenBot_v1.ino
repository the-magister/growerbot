#include <Streaming.h>
#include <Metro.h>
#include <DS3231.h> // see AT24C32_TEST example for I2C memory (32k) for logging
#include <Wire.h>

// configure RTC
DS3231 rtc;

// radio abstraction
#include "Radio.h"

#define RXPIN 2
#define TXPIN 10
Radio radio;

// sensor and pump abstraction
#include "Bed.h"

// how many sensors are we reading?
const int nSensors = 2;
BIOSDigitalSoilMeter sensor[nSensors];

// how many pumps are we controlling?
const int nPumps = 1;
EtekcityOutlet pump[nPumps];

// what pumps water which sensors?
const boolean ps[nSensors] = {0, 0};

// use LED to indicate status, with morse
// "d": one or more sensors is reporting too dry
// "w": watering in progress
// SOS: a sensor is acting whacky
#define LED 13

// define a maximum watering time, in hours
int maxWaterTime = 4; // hr

// show radio messages.  useful for figuring out addresses.
#define DEBUG_RADIO true

void setup() {
  Serial.begin(115200);

  Serial << F("Startup.") << endl;
  Serial.setTimeout(10);

  // LED pin
  pinMode(LED, OUTPUT);

  // RTC
  // I2C startup
  Wire.begin();
  rtc.setClockMode(false); // 24 time.
  rtc.enable32kHz(false); // don't need 32kHz oscillator pin
  rtc.enableOscillator(true, true, 0); // keep oscillator running on battery mode.
  // maybe needed to clear power cycle flag?
  if( !rtc.oscillatorCheck() ) {
    Serial << F("RTC oscillator problem...") << endl;
    rtc.setSecond(rtc.getSecond());
  }
  Serial << F("RTC: Time=");
  printTime();
  Serial << endl;
  
  // Timers
  // 9pm. when hour, min, sec match.
  rtc.setA1Time(0, 21, 0, 0, B1000, true, false, false);
  // check this
  byte A1Day, A1Hour, A1Minute, A1Second, AlarmBits;
  bool A1Dy, A1h12, A1PM;
  rtc.getA1Time(A1Day, A1Hour, A1Minute, A1Second, AlarmBits, A1Dy, A1h12, A1PM);
  Serial << F("Watering time alarm set for ") << A1Hour << F(":") << A1Minute << F(":") << A1Second << endl;
  // send 20,59,45,4,27,11 to simulate a watering run.
  rtc.turnOnAlarm(1);
  Serial << F("Watering alarm enabled? ") << rtc.checkAlarmEnabled(1) << endl;

  // Radio module
  radio.begin(RXPIN, TXPIN);
  radio.txRepeat(5); // 5 repeats
  radio.txProtocol(1); // always tranmitting using protocol 1 (ETek outlets)
  
  // pumps
  Serial << F("Pumps:") << endl;
  pump[0].begin("Pump 1", 1381683, 1381692);
  radio.txMessage(pump[0].turnOff());
  
  //  pump[1].begin("Pump 2", 1381827, 1381836);
  //  pump[2].begin("Pump 3", 1382147, 1382156);
  //  pump[3].begin("Pump 4", 1383683, 1383692);
  //  pump[4].begin("Pump 5", 1389827, 1389836);

  // sensors
  Serial << F("Sensors:") << endl;
//  sensor[0].begin("South Bed", 324, 6, 10);
//  sensor[1].begin("West Bed", 868, 6, 10);
  sensor[0].begin("South Bed", 910207744, 6, 10);
  sensor[1].begin("West Bed", 339785730, 6, 10);
//  sensor[2].begin("Flower Bed", 1949, 4, 8);   // try to keep this bed drier

  // pump and sensor relationships
  Serial << F("Pump waters Sensors:") << endl;
  for ( int i = 0; i < nSensors; i++ ) {
    Serial << pump[ps[i]].name << F(" waters ") << sensor[i].name << endl;
  }

  Serial << F("Startup complete.") << endl;

}

void loop() {

  // look for sensor data
  getSensorData();

  // look for pump data
  notePumpManualControl();

  // if we still can't process the message, print it,  probably just need to add it in the setup().  Drop decimel code in.
  if( radio.rxAvailable() && DEBUG_RADIO ) {
    Serial << F("Radio: unknown receipt.  Protocol: ") << radio.rxProtocol();
    Serial << F(" Bit Length: ") << radio.rxBitLength();
    Serial << F(" Message (dec): ") << radio.rxMessage();
    Serial << F(" Message (bin): ") << dec2binWzerofill(radio.rxMessage(), 32);
    Serial << endl;
    radio.rxClear();
  }
  
  // check for time update from Serial
  getTimeUpdate();

  // monitor for too dry
  boolean tooDry = false;
  for (int s = 0; s < nSensors; s++ ) {
    if ( sensor[s].tooDry() ) tooDry |= true; // any tooDry signals tooDry
  }
  static Metro reportSensorDry(30UL * 60UL * 1000UL); // every 30 minutes
  if ( tooDry ) {
    if ( reportSensorDry.check() ) {
      printTime();
      Serial << F(": BAD! one or more sensors reports 'too dry'") << endl;
      reportSensorDry.reset();
    }
    ledTooDry();
  }

  // check alarm for Watering time
  static Metro checkAlarm(1000UL); // every second.
  if ( checkAlarm.check() ) {
    checkAlarm.reset();
    // the RTC needs to be polled on an interval to actually set alarm flag.
    byte toss = rtc.getSecond();
    if ( rtc.checkIfAlarm(1) ) {
      wateringTime();
    }
  }

}

void wateringTime() {
  printTime();
  Serial << F(": Watering time.") << endl;

  // track the start time for this cycle
  unsigned long now = millis();
  Metro maxTimeReached(long(maxWaterTime) * 60UL * 60UL * 1000UL); // hr -> ms
  maxTimeReached.reset();

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
      // so, only turn on the pumps if the soil reads "too dry", but then run the pumps until just short of "too wet" (justRight)

      //      if ( tooWet || !tooDry ) {
      if ( tooWet || justRight ) {
        //        Serial << "too wet or not too dry" << endl;
        radio.txMessage(pump[0].turnOff());
      } else if ( tooDry ) {
        //       Serial << "too dry" << endl;
        radio.txMessage(pump[p].turnOn());
        delay(1000); // power draw on the pumps is high at startup.  stagger them.
      }
    }

    // check to see if all pumps are off.  If they are, we're done for the night.
    keepWatering = false;
    for (int p = 0; p < nPumps; p++ ) keepWatering |= pump[p].on();

    //    delay(1000);
    if ( maxTimeReached.check() ) {
      Serial << F("BAD: maximum watering time reached.  Shutting down...") << endl;
      pumpsAllOff();
      keepWatering = false;
    }
  }

  // just in case
  pumpsAllOff();
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

void notePumpManualControl() {
  if( !radio.rxAvailable() ) return;
  
  // get the message
  unsigned long message = radio.rxMessage();
  
  for (int i = 0; i < nPumps; i++) {
    // push the message to the Outlets; if any can be processed, clear the rxMessage.
    if( pump[i].readMessage(message) ) radio.rxClear();
  }
}

void getSensorData() {
  if( !radio.rxAvailable() ) return;
  
  // get the message
  unsigned long message = radio.rxMessage();
  
  for (int i = 0; i < nSensors; i++) {
    // push the message to the Outlets; if any can be processed, clear the rxMessage.
    if( sensor[i].readMessage(message) ) radio.rxClear();
  }
}


void getTimeUpdate() {
  while (Serial.available() > 0) {
    // wait for everything to come in.
    delay(25);
    // check for valid character
    if( Serial.peek() <= '0' || Serial.peek() >= '9' ) {
      // bad request.  
      Serial << F("Bad time setting.  Format: hr, min, sec, day, month, year") << endl;
      while ( Serial.read() > -1); // dump anything trailing.
      return;
    }
    
    // look for the next valid integer in the incoming serial stream:
    int hr = Serial.parseInt();
    int mi = Serial.parseInt();
    int se = Serial.parseInt();
    int da = Serial.parseInt();
    int mo = Serial.parseInt();
    int ye = Serial.parseInt();
    while ( Serial.read() > -1); // dump anything trailing.

    Serial << F("Time update received. Format: hr, min, sec, day, month, year") << endl;
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


void printSensors() {
  for ( int s = 0; s < nSensors; s++ ) sensor[s].print();
}

void pumpsAllOff() {
  Serial << F("Shutting down all pumps...") << endl;
  for (int p = 0; p < nPumps; p++ ) {
    radio.txMessage(pump[p].turnOff());
    delay(1000);
  }
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

void output(unsigned long decimal, unsigned int length, unsigned int delay, unsigned int * raw, unsigned int protocol) {

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


static char* bin2tristate(char * bin) {
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


