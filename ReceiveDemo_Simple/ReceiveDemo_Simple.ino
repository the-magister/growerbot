/*
  Simple example for receiving
  
  http://code.google.com/p/rc-switch/
*/

#include <RCSwitch.h>

RCSwitch mySwitch = RCSwitch();

void setup() {
  Serial.begin(115200);
  mySwitch.enableReceive(0);  // Receiver on inerrupt 0 => that is pin #2
//  mySwitch.setReceiveTolerance(100);
}

boolean rxFailed = false;
boolean recProt = false;
boolean largeCount = false;

void loop() {
  if (mySwitch.available()) {
    
    int value = mySwitch.getReceivedValue();
    
    if (value == 0) {
      Serial.print("Unknown encoding");
    } else {
      Serial.print("Received ");
      Serial.print( mySwitch.getReceivedValue() );
      Serial.print(" / ");
      Serial.print( mySwitch.getReceivedBitlength() );
      Serial.print("bit ");
      Serial.print("Protocol: ");
      Serial.println( mySwitch.getReceivedProtocol() );
    }

    mySwitch.resetAvailable();
    
    Serial.println("rec:");
    for(int i=0; i<RCSWITCH_MAX_CHANGES;i++) {
      Serial.print(mySwitch.timings[i]);
      Serial.print(" ");
    }
    Serial.println("");
  } 
  else if( recProt && mySwitch.timings[0]>0) {
    Serial.println("recProt:");
    for(int i=0; i<RCSWITCH_MAX_CHANGES;i++) {
      Serial.print(mySwitch.timings[i]);
      Serial.print(" ");
      mySwitch.timings[i]=0;
   }
    Serial.println("");
    recProt = false;
  } 
  else if( largeCount && mySwitch.timings[0]>0) {
    Serial.println("largeCount:");
    for(int i=0; i<RCSWITCH_MAX_CHANGES;i++) {
      Serial.print(mySwitch.timings[i]);
      Serial.print(" ");
      mySwitch.timings[i]=0;
   }
    Serial.println("");
    largeCount = false;
  } 
  delay(1000);
}
