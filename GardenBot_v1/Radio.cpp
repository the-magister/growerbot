#include "Radio.h"

// TB304BC = 0, ETEK = 1

// bitstream length for each protocol.
const int messageLength[NPROT] = { 32, 24 };

// HIGH pulse duration, in us.
const unsigned long pulseLength[NPROT] = { 475UL, 180UL };

// to transmit a "one", Tx HIGH for duration A, then Tx LOW for duration B.
// units are multiples of pulseLength.
// for example {3,1} means: Tx HIGH for 3*pulseLength, Tx LOW for 1*pulseLength
const unsigned long oneSeq[NPROT][2] = {
  { 1, 4000UL / pulseLength[0] }, // TB304BC
  { 3, 1 } // ETEK
};
// to transmit a "zero", Tx HIGH for duration A, then Tx LOW for duration B.
// units are multiple of pulseLength
// for example {1,3} means: Tx HIGH for 1*pulseLength, Tx LOW for 3*pulseLength
const unsigned long zeroSeq[NPROT][2] = {
  { 1, 2000UL / pulseLength[0] }, // TB304BC
  { 1, 3 } // ETEK
};
// at the start of a bistream/message, send a preamble, following a similar pattern as above
// units are multiples of pulseLength
const unsigned long syncSeq[NPROT][2] = {
  { 1, 9000UL / pulseLength[0] }, // TB304BC
  { 1, 31 } // ETEK
};

// ISR's can't be class member functions unless static.  PITA.
// so, we use some globals as glue between the class and the ISR.
volatile boolean ISR_rxReady;
volatile int ISR_rxPin;
volatile int ISR_rxProt;
volatile unsigned long ISR_rxVal;

// valid rxPins found in http://arduino.cc/en/Reference/attachInterrupt
// for Uno: D2,D3
void Radio::begin(int rxPin, int txPin) {
  Serial << F("Radio: startup.") << endl;

  // assign rxPin
  switch (rxPin) {
    case 2:
      Serial << F("Radio: rxPin 2, int.0") << endl;
      ISR_rxPin = 2;
      pinMode(ISR_rxPin, INPUT);
      attachInterrupt(0, interruptHandler, CHANGE); // D2 is int.0
      break;
    case 3:
      Serial << F("Radio: rxPin 3, int.1") << endl;
      ISR_rxPin = 3;
      pinMode(ISR_rxPin, INPUT);
      attachInterrupt(1, interruptHandler, CHANGE); // D3 is int.1
      break;
    default:
      Serial << F("Radio: error. rxPin must be 2 or 3.  Halting.") << endl;
      while (1);
      break;
  }

  // assign txPin
  this->txPin = txPin;
  pinMode(this->txPin, OUTPUT);
  digitalWrite(this->txPin, LOW);
  Serial << F("Radio. txPin ") << this->txPin << endl;

  // show the timings that we understand
  Serial << F("Protocol information:") << endl;
  for (int p = 0; p < NPROT; p++) {
    Serial << F("Protocol ") << p << F(" pulseLength ") << pulseLength[p] << F(" us.  Message bit length ") << messageLength[p] << F(".") << endl;
    Serial << F("Protocol ") << p << F(" Sync: HIGH ") << pulseLength[p]*syncSeq[p][0] << F(" us, LOW ") << pulseLength[p]*syncSeq[p][1] << F(" us.") << endl;
    Serial << F("Protocol ") << p << F(" Zero: HIGH ") << pulseLength[p]*zeroSeq[p][0] << F(" us, LOW ") << pulseLength[p]*zeroSeq[p][1] << F(" us.") << endl;
    Serial << F("Protocol ") << p << F("  One: HIGH ") << pulseLength[p]*oneSeq[p][0] << F(" us, LOW ") << pulseLength[p]*oneSeq[p][1] << F(" us.") << endl;
    Serial << endl;
  }
  
  // set some defaults
  this->txRepeat(1);
  this->txProtocol(0);
  
  // clear rx buffer
  this->rxClear();

  Serial << F("Radio: startup complete.") << endl;
}

// receiving functions

// is there a message available?
boolean Radio::rxAvailable() {
  return ( ISR_rxReady );
}

// if there is a message, what protocol was is received under?
int Radio::rxProtocol() {
  if ( ISR_rxReady ) return ( ISR_rxProt );
  else return ( -1 );
}

// if there is a message, what is the bit length for the protocol is was received under?
int Radio::rxBitLength() {
  if ( ISR_rxReady ) return ( messageLength[ISR_rxProt] );
  else return ( -1 );
}

// if there is a message, return the value.  Limited to 32 bits.
unsigned long Radio::rxMessage() {
  if ( ISR_rxReady ) return ( ISR_rxVal );
  else return ( 0 );
}

// if there's a message available, no new messages will be received until this is called.
void Radio::rxClear() {
  ISR_rxProt = -1;
  ISR_rxVal = 0;
  ISR_rxReady = false;
}


// transmission functions
// set tx protocol
void Radio::txProtocol(int prot) {
  if ( prot >= 0 && prot < NPROT ) {
    this->txProt = prot;
  } else {
    Serial << F("Radio: error.  Unknown protocol ") << prot << F(". Halting.") << endl;
    while (1);
  }
}

// set number of transmissions
void Radio::txRepeat(int repeats) {
  if ( repeats >= 0 && repeats < 100 ) {
    this->txRepeats = repeats;
  } else {
    Serial << F("Radio: error.  Tx repeats ") << repeats << F(" not in [0,99]. Halting.") << endl;
    while (1);
  }
}

// send a message
void Radio::txMessage(unsigned long val) {
  for ( int i = 0; i < this->txRepeats; i++ ) {
    Radio::sendValue(this->txProt, val);
  }
}

// Tx random noise.  Useful for simulation.
void Radio::txNoise(int n) {
  for (int i = 0; i < n; i++) {
    pinSet(HIGH, random(1, 10) * 1000UL);
    pinSet(LOW, random(1, 5) * 1000UL);
  }
}

// send a message
void Radio::sendValue(int prot, unsigned long val) {
//  Serial << F("sendValue: prot=") << prot << F(" value=") << val << F(" bitLength=") << messageLength[prot] << endl;
  // send sync
  sendSeq(prot, syncSeq);
  // send MSB first
  for (int b = messageLength[prot] - 1; b >= 0; b-- ) {
    if ( bitRead(val, b) == 1 ) sendSeq(prot, oneSeq);
    else sendSeq(prot, zeroSeq);
  }
  // toggle pin complete Tx
  digitalWrite(this->txPin, HIGH);
  digitalWrite(this->txPin, LOW); // leave it in the low state to not jam airwaves
}

// Tx a sync, zero, and one sequence
void Radio::sendSeq(int prot, const unsigned long seq[NPROT][2]) {
  pinSet(HIGH, pulseLength[prot] * seq[prot][0]);
  pinSet(LOW, pulseLength[prot] * seq[prot][1]);
//  Serial << F("SendSeq: prot=") << prot << F(" high time=") << pulseLength[prot] * seq[prot][0] << F(" low time=") << pulseLength[prot] * seq[prot][1] << endl;
}

// Tx helper.  BLOCKING function to hold the Tx signals HIGH and LOW for the proper intervals
void Radio::pinSet(int state, unsigned long interval) {
  unsigned long tic = micros();
  digitalWrite(this->txPin, state);
  while ( micros() - tic < interval);
  // note: we don't use delayMicroseconds, as that's inaccurate with ISR's running the background.
}

// ISR.  Thar be dragons--prepare for battle.
void interruptHandler() {

  // have we gotten a valid sync signal?
  static boolean gotSync = false;
  // last time ISR was called
  static unsigned long lastTime = micros();
  // current time; important to do this first so we don't lose time in later calcs.
  static unsigned long currTime = micros();
  // current number of bits received since sync;
  static byte rxCounts = 0;
  
  // set this soonest so we don't lose time from later calcs.
  currTime = micros();
  
  // how long since last change?
  unsigned long deltaTime = currTime - lastTime;

  // update tracking
  lastTime = currTime;

  // if we have a message waiting, exit
  if ( ISR_rxReady ) return;

  // track end-of-message
  boolean eom = false;

  // read rxPin state, quickly
  // PIND gives the bitWise pin states for D0-D7.  
  // D2 is bit 2, D3 is bit 3
  byte currPinVal = bitRead(PIND, ISR_rxPin);

  if ( currPinVal == HIGH ) {
    // first, let's establish a sync
    // both protocols start with a long LOW time, so we'll catch HIGH transition.
    //Serial << F("H");
    if ( ! gotSync ) {
      //Serial << F("s");
      for ( byte p = 0; p < NPROT; p++ ) {
        if ( isWithin(deltaTime, pulseLength[p]*syncSeq[p][1], 133) ) {
          // that's a sync signal
          gotSync = true;
          ISR_rxProt = p; // store receiving protocol
          ISR_rxVal = 0; // reset rxVal
          rxCounts = 0; // reset rxCounts
          //Serial << F("S");
        } else {
          //Serial << F("n");
        }
      }
    } else {
      // we have a previous sync, so decode bit stream.
      if ( isWithin(deltaTime, pulseLength[ISR_rxProt]*oneSeq[ISR_rxProt][1], 133) ) {
        // Rx == 1
        rxCounts++;
        ISR_rxVal = (ISR_rxVal << 1) + 1; // bitshift current value up and add one at LSB
        //Serial << F("1");

      } else if ( isWithin(deltaTime, pulseLength[ISR_rxProt]*zeroSeq[ISR_rxProt][1], 133) ) {
        // Rx == 0
        rxCounts++;
        ISR_rxVal = (ISR_rxVal << 1) + 0; // bitshift current value up and add zero at LSB
        //Serial << F("0");
      } else {
        // uh oh, we got nonsense.
        eom = true;
      }
    }
  } else if ( gotSync ) {
    // so, we're got a sync, but the pin has just gone LOW
    // let's use this time for some error checking, as we know how long the positive pulses are
    if ( isWithin(deltaTime, pulseLength[ISR_rxProt]*oneSeq[ISR_rxProt][0], 133) || // right pulse length to lead "one"
         isWithin(deltaTime, pulseLength[ISR_rxProt]*zeroSeq[ISR_rxProt][0], 133) ) { // right pulse length to lead "zero"
      // that's good.
    } else {
      // uh oh, we got nonsense.
      eom = true;
      //Serial << F("?");
    }

    // maybe we've got enough bits?
    if ( rxCounts >= messageLength[ISR_rxProt] ) {
      eom = true;
      //Serial << F("L");
    }

  }

  // we've reached the end of message, somehow
  if ( eom ) {
    // did we get a good message?
    if ( rxCounts >= messageLength[ISR_rxProt] ) ISR_rxReady = true;
    // reset sync for next time.
    gotSync = false;
    //Serial << F("E");
  }
}

// Rx helper to allow deviation from the protocol timings
boolean isWithin(unsigned long val, unsigned long test, byte percentOf) {
  unsigned long upper = (test * percentOf) / 100;
  unsigned long lower = (test * 100) / percentOf;
//  Serial << "isWithin.  val=" << val << " test= " << test << "[" << lower << "," << upper << "]" << endl;
  return ( val <= upper && val >= lower );
}




