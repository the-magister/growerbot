#include <Streaming.h>
#include <Metro.h>
#include "Radio.h"


Metro simulateTimer(1000UL);
boolean doSimulation = true;

void setup() {
  Serial.begin(115200);

  Serial << F("Startup.") << endl;

  radio.begin(RXPIN, TXPIN);
  radio.txRepeat(1);
  
}

void loop() {
  // simulate traffic
  if ( doSimulation && simulateTimer.check() ) {
    byte prot = random(0, NPROT);
    Serial << endl << F("Tx: prot=") << prot;
    
    byte bitLength = prot == 0 ? 32 : 24;    
    unsigned long txVal = random(0, pow(2, bitLength - 1) + 1);

    Serial << F(" Val: ") << dec2binWzerofill(txVal, bitLength) << F("\t") << txVal << endl;

    for( int i=0; i<3; i++) {
 //     radio.txNoise(10);      // intentional noise
      radio.txProtocol(prot); // set the Tx protocol.
      radio.txMessage(txVal); // send the message
//      radio.txNoise(10);      // more intentional noise
    }
  }

  if ( radio.rxAvailable() ) {
    Serial << F("Rx: prot=") << radio.rxProtocol();
    Serial << F(" Val: ") << dec2binWzerofill(radio.rxMessage(), radio.rxBitLength()) << F("\t") << radio.rxMessage() << endl;
    delay(1000); // drop repeats
    radio.rxClear(); // ready for next message
  }
}

// pretty printing of binary stream, with left zero padding.
// Thanks:  http://code.google.com/p/rc-switch/
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
