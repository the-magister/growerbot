#include <Streaming.h> // pretty printing
#include "Pixels.h" // where I do some fancy ASCII-art to emulate the NeoPixel library

// so, I've got a couple of strips
LEDStrip stripA;
LEDStrip stripB;

// I'd like to animate them with a variety of different functions, so let's make some glue to stick strips and animations together.
class Animation {
  public:
    LEDStrip * strip; // pointer to strip.  NeoPixel class, likely.
    
     // pointer to the animation function, which returns nothing and take the strip as an argument
    void (*animationFunction)(LEDStrip &strip);

    // how often do I want to do a .show() of the strip?  units of ms!
    unsigned long showInterval;
    // provide a function that both calculates the next frame and does the showing on the aforementioned timer
    void update();
    
  private:
    // when did we last .show()?
    unsigned long lastShow;
    // is the next frame calculated and ready?
    boolean nextFrameReady;
};
void Animation::update() {
  // first, check to see if we need to advance the animation a frame.  
  if(! nextFrameReady) {
    // call the animation function to update the pixels for the next frame
    (*this->animationFunction)(* this->strip);
    // signal that the next frame is ready
    nextFrameReady = true;
  } 
  
  // have showInterval ms have elapsed since the last .show()?
  unsigned long now = millis();
  if( now - lastShow >= this->showInterval ) {
    // call .show
    this->strip->show();
    
    // did we get it done quickly enough?
 //   Serial << this->strip->name << F(": update interval=") << now - lastShow << endl;
    
    // signal that we need the next frame calculated
    nextFrameReady = false;
    // reset the timer.
    lastShow = now;
  }
}

Animation animStripA;
Animation animStripB;

#define BLACK 0
#define WHITE 1

void setup() {
  // open serial
  Serial.begin(115200);
  
  randomSeed(analogRead(A0));

  // name the strips
  stripA.name = 'A';
  stripB.name = 'B';
  
  // just exercise the animations, for a check.
  animationSetAll(stripA, BLACK);
  stripA.show();
  animationSetAll(stripA, WHITE);
  stripA.show();
  animationSetRandom(stripA);
  stripA.show();
  animationShiftRight(stripA);
  stripA.show();
  animationSetRandom(stripB);
  stripB.show();
  
  // set up an animation.  probably better done in a constructor-something-something
  animStripA.strip = &stripA;
  animStripA.animationFunction = &animationShiftRight;
  animStripA.showInterval = 10000UL;
 
  // set up an animation.  probably better done in a constructor-something-something
  animStripB.strip = &stripB;
  animStripB.animationFunction = &animationShiftLeft;

  // you're going to ask me what a reasonable showInterval is.
  // Anything smaller than 25UL ms is not appreciable for humans
  
  int FPS = 30; // 30 frames per second is "smooth" to humans.
  unsigned long updateInterval = 1000UL / FPS; // == 33.33 ms.
  
  animStripB.showInterval = updateInterval;
}

void loop() {
  // and, that's it.  
  animStripA.update();
  animStripB.update();
  
  // from here, you can change the animation mid-stream, change the update interval (speeding up and slowing down the animation).
}


// Animation functions, for example.

// Fill strip with a color.  Note that this one _wouldn't_ work.  Animations need to be self contained,
// meaning frame i+1 must be derived entirely from frame i.
void animationSetAll(LEDStrip & strip, int color) {
  for (uint16_t i = 0; i < strip.numPixels(); i++) {
    strip.setPixelColor(i, color);
    // don't do this now.
    //   strip.show();
    //   delay(wait);
  }
}
// Fill strip with random colors
void animationSetRandom(LEDStrip & strip) {
  for (uint16_t i = 0; i < strip.numPixels(); i++) {
    strip.setPixelColor(i, random(0,1+1));
    // don't do this now.
    //   strip.show();
    //   delay(wait);
  }
}
// Shift strip Right
void animationShiftRight(LEDStrip & strip) {
  int firstColor = strip.getPixelColor(0);
  strip.pixels = strip.pixels >> 1;
  strip.setPixelColor(strip.numPixels()-1, firstColor);
}
// Shift strip Left
void animationShiftLeft(LEDStrip & strip) {
  int lastColor = strip.getPixelColor(strip.numPixels()-1);
  strip.pixels = strip.pixels << 1;
  strip.setPixelColor(0, lastColor);
}




