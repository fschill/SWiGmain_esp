#include <Arduino.h>
#include <stdbool.h>
#include <Adafruit_NeoPixel.h>

#include <pb_encode.h>
#include <pb_decode.h>
#include "parameters.pb.h"
#include <PacketSerial.h>

#define PIN 48 // ESP32-C3 built-in RGB led
#define NUMPIXELS 1

Adafruit_NeoPixel pixels (NUMPIXELS, PIN, NEO_GRB + NEO_KHZ800);
#define DELAYVAL 500

#define MSG_SIZE 50
uint8_t sMsg[MSG_SIZE];

PacketSerial cobs; 

void setup() {

    // Use serial port
  Serial.begin(115200);
  // set up COBS
  cobs.begin(&Serial);

  pixels.begin();

  pixels.clear();
  pixels.setPixelColor(0, pixels.Color (0,10,0));

  pb_ostream_t sizestream = {0};
  
}

bool send_request(pb_ostream_t *stream, const pb_field_iter_t *field, void * const *arg) {
  subseawireless_Parameter parameter = subseawireless_Parameter_init_default;
  parameter.integer = 42; //integer
  parameter.id = subseawireless_Parameter_identifier_swig_version_major; //SWiG version
  return pb_encode(stream, subseawireless_Parameter_fields, &parameter);
}

bool send_parameter(pb_ostream_t *stream, const pb_field_iter_t *field, void * const *arg) {
  if (!pb_encode_tag_for_field(stream, field))
    return false;
  subseawireless_Parameter parameter = subseawireless_Parameter_init_default;
  parameter.integer = 42; //integer
  parameter.id = subseawireless_Parameter_identifier_swig_version_major; //SWiG version
  return pb_encode_submessage(stream, subseawireless_Parameter_fields, &parameter);
}

int counter = 0;
void loop() {
  // put your main code here, to run repeatedly:

  subseawireless_Message message = subseawireless_Message_init_default;
  message.source = 2;
  message.target = 1;
  //message.requests.funcs.encode = &send_request; 
  //message.parameters.funcs.encode = &send_parameter; 
  message.responses.funcs.encode = &send_parameter; 

  pb_ostream_t buffer = pb_ostream_from_buffer(sMsg, sizeof(sMsg));

  pb_encode(&buffer, subseawireless_Message_fields , &message);
  cobs.send(sMsg, buffer.bytes_written); 

  //Serial.printf("test! %i\n", counter);
  //pixels.setPixelColor(0, pixels.Color (counter, counter/64, counter/4096));
  pixels.show();
  counter+=4;
  delay(500);
  

}

// put function definitions here:
int myFunction(int x, int y) {
  return x + y;
}