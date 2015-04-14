/*

Radio interface module.  Knows the following protocols:

Thermor BIOS Wireless Moisture and Temperature Sensor (TB304BC)
see http://rayshobby.net/reverse-engineer-a-cheap-wireless-soil-moisture-sensor/
Encoding. Each transmission consists of 8 repetitions.
The above shows one repetition:
it starts with a sync signal (9000us low);
a logic 1 is a impulse (475us) high followed by a 4000us low;
a logic 0 is the same impulse high followed by a 2000us low.

Etekcity Outlet

// https://code.google.com/p/rc-switch/wiki/KnowHow_LineCoding

Pulse length: 180us (1/31 time of the synchronization low level)
Synchronization: 1 Pulse High + 31 Pulses Low
Bits:
0: 1 Pulse High + 3 Pulses Low
1: 3 Pulses High + 1 Pulse Low

*/

#ifndef Radio_h
#define Radio_h

// TB304BC = 0, ETEK = 1
#define NPROT 2

#include <Arduino.h>
#include <Streaming.h> // this needs to be #include'd in the .ino file, too.

class Radio {
  public:
    // valid rxPins found in http://arduino.cc/en/Reference/attachInterrupt
    // for Uno: D2,D3
    void begin(int rxPin, int txPin);

    // receiving functions
    // is there a message available?
    boolean rxAvailable();
    // if there is a message, what protocol was is received under?
    int rxProtocol();
    // if there is a message, what is the bit length for the protocol is was received under?
    int rxBitLength();
    // if there is a message, return the value.  Limited to 32 bits.
    unsigned long rxMessage();
    // if there's a message available, no new messages will be received until this is called.
    void rxClear();

    // transmission functions
    // set tx protocol
    void txProtocol(int prot);
    // set number of transmissions
    void txRepeat(int repeats);
    // send a message
    void txMessage(unsigned long val);

    // Tx random noise.  Useful for simulation.
    void txNoise(int counts);

  private:
    // store pins
    int rxPin, txPin;
    // store protocol
    int txProt;

   // store tx repeats
    int txRepeats;

    // low-level functionality
    // send a message
    void sendValue(int prot, unsigned long val);
    // Tx a sync, zero, and one sequence
    void sendSeq(int prot, const unsigned long seq[NPROT][2]);
    // Tx helper.  BLOCKING function to hold the Tx signals HIGH and LOW for the proper intervals
    void pinSet(int state, unsigned long interval);
};


// ISR for Rx.  Can't attach interrupt to a class member function directly.
void interruptHandler();
// Rx helper to allow deviation from the protocol timings
boolean isWithin(unsigned long val, unsigned long test, byte percentOf);

#endif
