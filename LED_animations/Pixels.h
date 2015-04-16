// I'm just emulating the NeoPixel library functions so some animations work easily.


// help function to display the led strip values
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


// you would, of course, replace this with the LED library object.
class LEDStrip {
  public:
    char name;
    unsigned long pixels; // gonna use 32 bits of this variable to simulate 32 pixels that can be on and off.
    int numPixels();
    void setPixelColor(int p, int c);
    int getPixelColor(int p);

    void show();
};
void LEDStrip::show() {
  Serial << this->name << F("->") << dec2binWzerofill(this->pixels, 32) << endl;
}
int LEDStrip::numPixels() {
  return ( 32 );
}
void LEDStrip::setPixelColor(int p, int c) {
  bitWrite(this->pixels, p, c);
}
int LEDStrip::getPixelColor(int p) {
  return( bitRead(this->pixels, p) );
}


