/* Convert RF signal into bits (soil moisture sensor version) 
 * Written by : Ray Wang
 * http://rayshobby.net/?p=9413
 */

// ring buffer size has to be large enough to fit
// data between two successive sync signals
#define RING_BUFFER_SIZE  256

#define SYNC_LENGTH  9000
#define SEP_LENGTH   500
#define BIT1_LENGTH  4000
#define BIT0_LENGTH  2000

#define DATAPIN  3  // D3 is interrupt 1

unsigned long timings[RING_BUFFER_SIZE];
unsigned int syncIndex1 = 0;  // index of the first sync signal
unsigned int syncIndex2 = 0;  // index of the second sync signal
bool received = false;

// detect if a sync signal is present
bool isSync(unsigned int idx) {
  unsigned long t0 = timings[(idx+RING_BUFFER_SIZE-1) % RING_BUFFER_SIZE];
  unsigned long t1 = timings[idx];

  // on the temperature sensor, the sync signal
  // is roughtly 9.0ms. Accounting for error
  // it should be within 8.0ms and 10.0ms
  if (t0>(SEP_LENGTH-100) && t0<(SEP_LENGTH+100) &&
    t1>(SYNC_LENGTH-1000) && t1<(SYNC_LENGTH+1000) &&
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
  if (received == true) {
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
      syncIndex1 = (ringIndex+1) % RING_BUFFER_SIZE;
    } 
    else if (syncCount == 2) {
      // second time sync is seen, start bit conversion
      syncCount = 0;
      syncIndex2 = (ringIndex+1) % RING_BUFFER_SIZE;
      unsigned int changeCount = (syncIndex2 < syncIndex1) ? (syncIndex2+RING_BUFFER_SIZE - syncIndex1) : (syncIndex2 - syncIndex1);
      // changeCount must be 66 -- 32 bits x 2 + 2 for sync
      if (changeCount != 76) {
        received = false;
        syncIndex1 = 0;
        syncIndex2 = 0;
      } 
      else {
        received = true;
      }
    }

  }
}


void setup() {
  Serial.begin(9600);
  Serial.println("Started.");
  pinMode(3, INPUT);
  attachInterrupt(1, handler, CHANGE);

}

void loop() {
  if (received == true) {
    // disable interrupt to avoid new data corrupting the buffer
    detachInterrupt(1);
    /*
    // loop over buffer data
    for(unsigned int i=syncIndex1; i!=syncIndex2; i=(i+2)%RING_BUFFER_SIZE) {
      unsigned long t0 = timings[i], t1 = timings[(i+1)%RING_BUFFER_SIZE];
      if (t0>(SEP_LENGTH-100) && t0<(SEP_LENGTH+100)) {
       if (t1>(BIT1_LENGTH-1000) && t1<(BIT1_LENGTH+1000)) {
         Serial.print("1");
       } else if (t1>(BIT0_LENGTH-1000) && t1<(BIT0_LENGTH+1000)) {
         Serial.print("0");
       } else {
         Serial.print("SYNC");  // sync signal
       }
       } else {
       Serial.print("?");  // undefined timing
       }
    }
    Serial.println("");
    */
    // loop over the lowest 12 bits of the middle 2 bytes
    unsigned long temp = 0;
    bool negative = false;
    bool fail = false;
    for(unsigned int i=(syncIndex1+24)%RING_BUFFER_SIZE; i!=(syncIndex1+48)%RING_BUFFER_SIZE; i=(i+2)%RING_BUFFER_SIZE) {
      unsigned long t0 = timings[i], t1 = timings[(i+1)%RING_BUFFER_SIZE];
      if (t0>(SEP_LENGTH-100) && t0<(SEP_LENGTH+100)) {
        if (t1>(BIT1_LENGTH-1000) && t1<(BIT1_LENGTH+1000)) {
          if(i == (syncIndex1+24)%RING_BUFFER_SIZE) negative = true;
          temp = (temp << 1) + 1;
        } 
        else if (t1>(BIT0_LENGTH-1000) && t1<(BIT0_LENGTH+1000)) {
          temp = (temp << 1) + 0;
        } 
        else {
          fail = true;
        }
      } 
      else {
        fail = true;
      }
    }
    byte humidity;
    for(unsigned int i =(syncIndex1+48)%RING_BUFFER_SIZE; i != (syncIndex1+64)%RING_BUFFER_SIZE; i=(i+2)%RING_BUFFER_SIZE){
           unsigned long t0 = timings[i], t1 = timings[(i+1)%RING_BUFFER_SIZE];
      if (t0>(SEP_LENGTH-100) && t0<(SEP_LENGTH+100)) {
        if (t1>(BIT1_LENGTH-1000) && t1<(BIT1_LENGTH+1000)) {
          if(i == (syncIndex1+24)%RING_BUFFER_SIZE) negative = true;
          humidity = (humidity << 1) + 1;
        } 
        else if (t1>(BIT0_LENGTH-1000) && t1<(BIT0_LENGTH+1000)) {
          humidity = (humidity << 1) + 0;
        } 
        else {
          fail = true;
        }
      } 
      else {
        fail = true;
      }
    }

    if (!fail) {
      if (negative) {
        temp = 4096 - temp; 
        Serial.print("-");
      }
      Serial.print((temp+5)/10);  // round to the nearest integer
      Serial.write(176);    // degree symbol
      Serial.print("C/");
      Serial.print((temp+5)*9/50+32);  // convert to F
      Serial.write(176);    // degree symbol
      Serial.println("F");
      Serial.print("Humidity value: ");
      Serial.println(humidity);
    } else {
      Serial.println("Decoding error.");
    }
    // delay for 1 second to avoid repetitions
    delay(1000);
    received = false;
    syncIndex1 = 0;
    syncIndex2 = 0;

    // re-enable interrupt
    attachInterrupt(1, handler, CHANGE);
  }

}

