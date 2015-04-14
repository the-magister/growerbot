#include <Streaming.h>
#include <Metro.h>

#define SP 10
// TB304BC = 0, ETEK = 1
#define NPROT 2


// set by ISR.
volatile boolean rxUpdate = false;
volatile unsigned long rxBits = 0;

const int messageLength[NPROT] = { 32, 24 };
const unsigned long pulseLength[NPROT] = { 475UL, 180UL };
const unsigned long oneSeq[NPROT][2] = {
  { 1, 4000UL / pulseLength[0] }, // TB304BC
  { 3, 1 } // ETEK
};
const unsigned long zeroSeq[NPROT][2] = {
  { 1, 2000UL / pulseLength[0] }, // TB304BC
  { 1, 3 } // ETEK
};
const unsigned long syncSeq[NPROT][2] = {
  { 1, 9000UL / pulseLength[0] }, // TB304BC
  { 1, 31 } // ETEK
};

// later, move this block to statics inside ISR0
// have we gotten a valid sync signal?
volatile boolean gotSync = false;
// have we gotten a valid message?
volatile boolean gotMessage = false;
// Rx buffer
volatile unsigned long rxVal = 0; // allows for 32 bits of information to be received
// what kind of protocol are we receiving?
volatile byte protocol = 99;
// last time ISR0 was called
volatile unsigned long lastTime = micros();
// current time; important to do this first so we don't lose time in later calcs.
volatile unsigned long currTime = micros();
// how long since last change?
volatile unsigned long deltaTime = currTime - lastTime;

volatile int rxCounts = 0;
volatile int txCounts = 0;

void setup() {
  Serial.begin(115200);

  // put your setup code here, to run once:
  pinMode(2, INPUT);
  attachInterrupt(0, ISR0, CHANGE); // D2 is int.0

  // use D7 to simulate receipt
  pinMode(SP, OUTPUT);

  Serial << F("Startup.") << endl;
}

Metro simulateTimer(1000UL);
boolean doSimulation = false;
void loop() {
  // simulate traffic
  if ( doSimulation && simulateTimer.check() ) {
    byte prot = random(0, NPROT);
    unsigned long txVal = random(0, pow(2, messageLength[prot] - 1) + 1);

    Serial << endl << F("Tx: prot=") << prot;
    Serial << F(" Val: ") << dec2binWzerofill(txVal, 32) << F("\t") << txVal << endl;

    for( int i=0; i<3; i++) {
      sendNoise(10);
      sendValue(prot, txVal);
      sendNoise(10);
    }
  }

  if ( gotMessage ) {
    Serial << F("Rx: prot=") << protocol;
    Serial << F(" Val: ") << dec2binWzerofill(rxVal, 32) << F("\t") << rxVal << endl;
    delay(1000); // drop repeats
    gotMessage = false;
  }
}

void sendValue(byte prot, unsigned long val) {
  // reset txCounts
  txCounts = 0;
  // send sync
  sendSeq(prot, syncSeq);
  // send MSB first
  for (int b = messageLength[prot] - 1; b >= 0; b-- ) {
    if ( bitRead(val, b) == 1 ) sendSeq(prot, oneSeq);
    else sendSeq(prot, zeroSeq);
    txCounts++;
  }
  // bring pin high to complete Tx
  digitalWrite(SP, HIGH);

}
void sendSeq(byte prot, const unsigned long seq[NPROT][2]) {
  pinSet(HIGH, pulseLength[prot] * seq[prot][0]);
  pinSet(LOW, pulseLength[prot] * seq[prot][1]);
  //  Serial << F("SendSeq: prot=") << prot << F(" high time=") << pulseLength[prot] * seq[prot][0] << F(" low time=") << pulseLength[prot] * seq[prot][1] << endl;
}
void sendNoise(int n) {
  for (int i = 0; i < n; i++) {
    pinSet(HIGH, random(1, 10) * 1000UL);
    pinSet(LOW, random(1, 5) * 1000UL);
  }
}
void pinSet(int state, unsigned long interval) {
  unsigned long tic = micros();
  digitalWrite(SP, state);
  while ( micros() - tic < interval);
}


// see http://rayshobby.net/reverse-engineer-a-cheap-wireless-soil-moisture-sensor/
/*
Encoding. Each transmission consists of 8 repetitions.
The above shows one repetition:
it starts with a sync signal (9000us low);
a logic 1 is a impulse (475us) high followed by a 4000us low;
a logic 0 is the same impulse high followed by a 2000us low.
*/

// https://code.google.com/p/rc-switch/wiki/KnowHow_LineCoding
/*
Pulse length: About 300 to 500 microseconds (1/31 time of the synchronization low level)
Synchronization: 1 Pulse High + 31 Pulses Low
Bits:
0: 1 Pulse High + 3 Pulses Low
1: 3 Pulses High + 1 Pulse Low
*/

void ISR0() {

  // set this soonest so we don't lose time from later calcs.
  currTime = micros();

  // if we have a message waiting, exit
  if ( gotMessage ) return;

  deltaTime = currTime - lastTime;
  // update tracking
  lastTime = currTime;

  // track end-of-message
  boolean eom = false;

  // read pin state on D2 quickly.
  byte currPinVal = bitRead(PIND, 2);

  if ( currPinVal == HIGH ) {
    // first, let's establish a sync
    // both protocols start with a long LOW time, so we'll catch HIGH transition.
    if ( ! gotSync ) {
      for ( byte p = 0; p < NPROT; p++ ) {
        if ( isWithin(deltaTime, pulseLength[p]*syncSeq[p][1], 133) ) {
          // that's a sync signal
          gotSync = true;
          protocol = p;
          rxVal = 0; // reset rxVal
          rxCounts = 0; // reset rxCounts
          //Serial << F("S");
        }
      }
    } else {
      // we have a previous sync, so decode bit stream.
      if ( isWithin(deltaTime, pulseLength[protocol]*oneSeq[protocol][1], 133) ) {
        // Rx == 1
        rxCounts++;
        rxVal = (rxVal << 1) + 1; // bitshift current value up and add one at LSB
        //Serial << F("1");

      } else if ( isWithin(deltaTime, pulseLength[protocol]*zeroSeq[protocol][1], 133) ) {
        // Rx == 0
        rxCounts++;
        rxVal = (rxVal << 1) + 0; // bitshift current value up and add zero at LSB
        //Serial << F("0");
      } else {
        // uh oh, we got nonsense.
        eom = true;
      }
    }
  } else if ( gotSync ) {
    // so, we're got a sync, but the pin has just gone LOW
    // let's use this time for some error checking, as we know how long the positive pulses are
    if ( isWithin(deltaTime, pulseLength[protocol]*oneSeq[protocol][0], 133) || // right pulse length to lead "one"
         isWithin(deltaTime, pulseLength[protocol]*zeroSeq[protocol][0], 133) ) { // right pulse length to lead "zero"
      // that's good.
    } else {
      // uh oh, we got nonsense.
      eom = true;
      //Serial << F("?");
    }

    // maybe we've got enough bits?
    if ( rxCounts >= messageLength[protocol] ) {
      eom = true;
      //Serial << F("L");
    }

  }

  // we've reached the end of message, somehow
  if ( eom ) {
    // did we get a good message?
    if ( rxCounts >= messageLength[protocol] ) gotMessage = true;
    // reset
    gotSync = false;
    //Serial << F("E");
  }
}
// helper function to accomodate timing differences
boolean isWithin(unsigned long val, unsigned long test, byte percentOf) {
  unsigned long upper = (test * percentOf) / 100;
  unsigned long lower = (test * 100) / percentOf;
  //  Serial << "isWithin.  val=" << val << " test= " << test << "[" << lower << "," << upper << "]" << endl;
  return ( val <= upper && val >= lower );
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
