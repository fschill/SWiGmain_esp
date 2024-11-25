#include <Arduino.h>
#include <stdbool.h>
#include <Adafruit_NeoPixel.h>

#include <pb_encode.h>
#include <pb_decode.h>
#include "parameters.pb.h"


#define PIN 48 // ESP32-C3 built-in RGB led
#define NUMPIXELS 1

Adafruit_NeoPixel pixels (NUMPIXELS, PIN, NEO_GRB + NEO_KHZ800);
#define DELAYVAL 500

void setup() {

    // Use serial port
  Serial.begin(115200);
  pixels.begin();

  pixels.clear();
  pixels.setPixelColor(0, pixels.Color (0,10,0));

  subseawireless_Message message = subseawireless_Message_init_default;
  message.source = 42;
  message.target = 1;
  
  pb_ostream_t sizestream = {0};
  pb_encode(&sizestream, subseawireless_Message_fields , &message);
  Serial.printf("Encoded size is %d\n", sizestream.bytes_written);
  
}

int counter = 0;
void loop() {
  // put your main code here, to run repeatedly:
  Serial.printf("test! %i\n", counter);
  //pixels.setPixelColor(0, pixels.Color (counter, counter/64, counter/4096));
  pixels.show();
  counter+=4;
  delay(500);
  

}

// put function definitions here:
int myFunction(int x, int y) {
  return x + y;
}