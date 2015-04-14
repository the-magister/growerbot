#include <Streaming.h>
#include <Metro.h>
#include <DS3231.h> // see AT24C32_TEST example for I2C memory (32k) for logging
#include <Wire.h>

// configure RTC
DS3231 rtc;

// radio abstraction
#include "Radio.h"

// sensor and pump abstraction
#include "Bed.h"

// how many sensors are we reading?
const int nSensors = 3;
BIOSDigitalSoilMeter sensor[nSensors];

// how many pumps are we controlling?
const int nPumps = 1;
EtekcityOutlet pump[nPumps];

// what pumps water which sensors?
const boolean ps[nSensors] = {0, 0, 0};

// use LED to indicate status
// solid: one or more sensors is reporting too dry
// blinking: watering in progress
// SOS: a sensor is acting whacky
#define LED 13

// define a maximum watering time, in hours
int maxWaterTime = 4; // hr

// set to true to get all the gory details upon radio receipt
#define DEBUG_RADIO true
#define DATAPIN 2 // D2 is int.0

// track if we've got sensor data
bool sensorReceived = false;

void setup() {
  Serial.begin(115200);

  Serial << F("Startup.") << endl;
  Serial.setTimeout(10);

  // LED pin
  pinMode(LED, OUTPUT);

  // RTC
  Wire.begin();

  // Timers
  rtc.setClockMode(false); // 24 time.
  rtc.setSecond(rtc.getSecond()); // maybe needed to clear power cycle flag?
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
  // receive
  //  radio.enableReceive(0);      // Receiver DATA line on interrupt 0 => that is pin D2
  // can't get RC Switch to work with the Thermor BIOS sensors.  So, we can't listen to the manual switches.  Pity.
  pinMode(DATAPIN, INPUT);
  attachInterrupt(0, handler, CHANGE); // D2 is int.0

  // transmit
  radio.enableTransmit(10);    // Transmitter DATA line on pin D10
  radio.setRepeatTransmit(5);  // repeat a transmission 5 times.

  // pumps
  Serial << F("Pumps:") << endl;
  pump[0].begin("Pump 1", 1381683, 1381692);
  //  pump[1].begin("Pump 2", 1381827, 1381836);
  //  pump[2].begin("Pump 3", 1382147, 1382156);
  //  pump[3].begin("Pump 4", 1383683, 1383692);
  //  pump[4].begin("Pump 5", 1389827, 1389836);

  // sensors
  Serial << F("Sensors:") << endl;
//  sensor[0].begin("South Bed", 324, 6, 10);
//  sensor[1].begin("West Bed", 868, 6, 10);
  sensor[0].begin("South Bed", 320, 6, 10);
  sensor[1].begin("West Bed", 864, 6, 10);
  sensor[2].begin("Flower Bed", 1949, 4, 8);   // try to keep this bed drier

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
    if ( maxTimeReached.check() ) {
      Serial << F("BAD: maximum watering time reached.  Shutting down...") << endl;
      pumpsAllOff();
      keepWatering = false;
    }
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


// ring buffer size has to be large enough to fit
// data between two successive sync signals
//#define RING_BUFFER_SIZE  256
#define RING_BUFFER_SIZE 78

#define SYNC_LENGTH  9000
#define SEP_LENGTH   500
#define BIT1_LENGTH  4000
#define BIT0_LENGTH  2000

unsigned long timings[RING_BUFFER_SIZE];
unsigned int syncIndex1 = 0;  // index of the first sync signal
unsigned int syncIndex2 = 0;  // index of the second sync signal

// detect if a sync signal is present
bool isSync(unsigned int idx) {
  unsigned long t0 = timings[(idx + RING_BUFFER_SIZE - 1) % RING_BUFFER_SIZE];
  unsigned long t1 = timings[idx];

  // on the temperature sensor, the sync signal
  // is roughtly 9.0ms. Accounting for error
  // it should be within 8.0ms and 10.0ms
  if (t0 > (SEP_LENGTH - 100) && t0 < (SEP_LENGTH + 100) &&
      t1 > (SYNC_LENGTH - 1000) && t1 < (SYNC_LENGTH + 1000) &&
      digitalRead(DATAPIN) == HIGH) {
    return true;
  }
  return false;
}

/* Interrupt 1 handler */
void handler() {
  static unsigned long duration = 0;
  static unsigned long lastTime = 0;
  static unsigned int ringIndex = 0;
  static unsigned int syncCount = 0;

  // ignore if we haven't processed the previous received signal
  if (sensorReceived == true) {
    return;
  }
  // calculating timing since last change
  long time = micros();
  duration = time - lastTime;
  lastTime = time;

  // store data in ring buffer
  ringIndex = (ringIndex + 1) % RING_BUFFER_SIZE;
  timings[ringIndex] = duration;

  // detect sync signal
  if (isSync(ringIndex)) {
    syncCount ++;
    // first time sync is seen, record buffer index
    if (syncCount == 1) {
      syncIndex1 = (ringIndex + 1) % RING_BUFFER_SIZE;
    }
    else if (syncCount == 2) {
      // second time sync is seen, start bit conversion
      syncCount = 0;
      syncIndex2 = (ringIndex + 1) % RING_BUFFER_SIZE;
      unsigned int changeCount = (syncIndex2 < syncIndex1) ? (syncIndex2 + RING_BUFFER_SIZE - syncIndex1) : (syncIndex2 - syncIndex1);
      // changeCount must be 66 -- 32 bits x 2 + 2 for sync
//      if (changeCount != 76) {
      if (changeCount != 38*2) {
        sensorReceived = false;
        syncIndex1 = 0;
        syncIndex2 = 0;
      }
      else {
        sensorReceived = true;
      }
    }
  }
}

void getSensorData() {
  unsigned long recv = 0;

  if ( sensorReceived ) {
    // disable interrupt to avoid new data corrupting the buffer
    detachInterrupt(0);

    // loop over buffer data
    byte bitCount = 0;
    unsigned long address = 0;
    unsigned long temp = 0;
    unsigned long hum = 0;
    for (unsigned int i = syncIndex1; i != syncIndex2; i = (i + 2) % RING_BUFFER_SIZE) {
      unsigned long t0 = timings[i], t1 = timings[(i + 1) % RING_BUFFER_SIZE];
      if (t0 > (SEP_LENGTH - 100) && t0 < (SEP_LENGTH + 100)) {
        if (t1 > (BIT1_LENGTH - 1000) && t1 < (BIT1_LENGTH + 1000)) {
  //        Serial << F("1");
          if ( bitCount < 12) {
            address = address << 1;
            address += 1;
          } else if ( bitCount < 12 + 12 ) {
            temp = temp << 1;
            temp += 1;
          } else if ( bitCount < 12 + 12 + 4 ) {
            hum = hum << 1;
            hum += 1;
          }
          bitCount++;
        } else {
 //         Serial << F("0");
          if ( bitCount < 12) {
            address = address << 1;
          } else if ( bitCount < 12 + 12 ) {
            temp = temp << 1;
          } else if ( bitCount < 12 + 12 + 4 ) {
            hum = hum << 1;
          }
          bitCount++;
        }
      } else {
 //       Serial << F("?");
      }
    }
//    Serial << endl;

    if ( bitCount >= 12 + 12 + 4 && DEBUG_RADIO) {
      Serial << dec2binWzerofill(address, 12) << dec2binWzerofill(temp, 12) << dec2binWzerofill(hum, 4) << endl;
      Serial << F("bitcount: ") << bitCount << endl;
      Serial << F("Address: ") << address << F(" ") << dec2binWzerofill(address, 32) << endl;
      Serial << F("Temp: ") << float(temp / 10.0) << F(" ") << dec2binWzerofill(temp, 32) << endl;
      Serial << F("Humidity: ") << hum << F(" ") << dec2binWzerofill(hum, 32) << endl;
    }

    for (int i = 0; i < nSensors; i++) {
      if( sensor[i].sensorAddress == address ) {
        sensor[i].currMoist = hum;
        sensor[i].currTemp = float(temp/10.0);
        sensor[i].lastSensorRead = millis();
        sensor[i].print();
      
        delay(1000); // wait to drop duplicates/
      }
    }

    // reattach
    attachInterrupt(0, handler, CHANGE);
    sensorReceived = false; // clear flag

  }
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


void notePumpManualControl() {
  for (int i = 0; i < nPumps; i++) {
    pump[i].readOutlet();
  }
}

void printSensors() {
  for ( int s = 0; s < nSensors; s++ ) sensor[s].print();
}

void pumpsAllOff() {
  Serial << F("Shutting down all pumps...") << endl;
  for (int p = 0; p < nPumps; p++ ) {
    pump[p].turnOff();
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


