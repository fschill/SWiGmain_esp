#include <Arduino.h>
#include <stdbool.h>
#include <Adafruit_NeoPixel.h>

#include <pb_encode.h>
#include <pb_decode.h>
#include "parameters.pb.h"

#include <PacketSerial.h>
#include <Encoding/COBS.h>

#include <WiFi.h>
#include <WiFiUdp.h>

#include "credentials.h"

#define PIN 48 // ESP32-C3 built-in RGB led
#define NUMPIXELS 1

Adafruit_NeoPixel pixels (NUMPIXELS, PIN, NEO_GRB + NEO_KHZ800);
#define DELAYVAL 500

#define MSG_SIZE 50
// calculate COBS overhead for max. buffer size
#define COBS_SIZE (MSG_SIZE+MSG_SIZE/254+1)

uint8_t sMsg[MSG_SIZE];

PacketSerial cobs; 

WiFiUDP udp;
char udp_packet_buffer[255];
unsigned int udp_port = 55502;

void flash_LED(int r, int g, int b) {
  pixels.clear();
  pixels.setPixelColor(0, pixels.Color (r,g,b));
  pixels.show();
  delay(10);
  pixels.setPixelColor(0, pixels.Color (10,10,0));
  pixels.show();
}

// nanopb callback for sending parameter payloads
bool send_parameter(pb_ostream_t *stream, const pb_field_iter_t *field, void * const *arg) {
  if (!pb_encode_tag_for_field(stream, field))
    return false;
  subseawireless_Parameter parameter = subseawireless_Parameter_init_default;
  parameter.integer = 42; //integer
  parameter.id = subseawireless_Parameter_identifier_swig_version_major; //SWiG version
  return pb_encode_submessage(stream, subseawireless_Parameter_fields, &parameter);
}

// convenience function to send a swig response message
void encode_swig_response(pb_ostream_t *pbbuffer) {
  subseawireless_Message message = subseawireless_Message_init_default;
  message.source = 2;
  message.target = 1;
  //message.requests.funcs.encode = &send_request; 
  //message.parameters.funcs.encode = &send_parameter; 
  message.responses.funcs.encode = &send_parameter; 

  pb_encode(pbbuffer, subseawireless_Message_fields , &message);
}

bool decode_parameter(pb_istream_t *stream, const pb_field_iter_t *field, void **arg) {
  subseawireless_Parameter parameter = subseawireless_Parameter_init_default;
  bool eof = false;
  pb_wire_type_t wiretype;
  uint32_t tag;
  if (!pb_decode_tag(stream, &wiretype, &tag, &eof)) {
    return false;
  }
  Serial.printf("got wiretype %i, tag %i\n",wiretype, tag);
  bool success = pb_decode(stream, subseawireless_Parameter_fields, &parameter);
  if (!success)
  {
      const char * error = PB_GET_ERROR(stream);
      Serial.printf("pb_decode error: %s\n", error);
      return false;
  }
  Serial.printf("Received parameter %i\n", parameter.id);
  return success;
}


bool decode_swig_message(pb_istream_t *pbbuffer_in, subseawireless_Message *message){
  //message->responses.funcs.decode = &decode_parameter;
  pb_decode(pbbuffer_in, subseawireless_Message_fields, message);
}

/** handler for received serial packets (COBS callback)*/
void onPacketReceived(const uint8_t* buffer, size_t size)
{
  subseawireless_Message message = subseawireless_Message_init_default;
  bool success = false;
  flash_LED(0,255,0);
  pb_istream_t pbbuffer_in = pb_istream_from_buffer(buffer, size);
  // decode SWiG
  success = success && decode_swig_message(&pbbuffer_in, &message);
  if (!success) return;
  // send response
  pb_ostream_t pbbuffer = pb_ostream_from_buffer(sMsg, sizeof(sMsg));
  encode_swig_response(&pbbuffer);
  cobs.send(sMsg, pbbuffer.bytes_written); 

}

/** handler for received UDP packets*/
void udp_update() {
  int size = udp.parsePacket();
  COBS cobs_coder;
  subseawireless_Message message = subseawireless_Message_init_default;
  subseawireless_Parameter parameter = subseawireless_Parameter_init_default;
  bool success = false;
  uint8_t cobs_buffer[COBS_SIZE];

  if (size > 0) {
    IPAddress remote = udp.remoteIP();
    Serial.printf("Received UDP packet from %s,  %i bytes\n", remote.toString(), size);
    flash_LED(0,0,255);
    // read UDP data
    int len = udp.read(cobs_buffer, COBS_SIZE);
    if (len>0) {
      success = true;
      // decode packet
      uint8_t in_buffer[MSG_SIZE];
      size_t msg_size = cobs_coder.decode(cobs_buffer, size, in_buffer);
      Serial.printf("received COBS message: raw size %i, decoded size %i\n", len, msg_size);
      pb_istream_t pbbuffer_in = pb_istream_from_buffer(in_buffer, msg_size);
      // decode SWiG
      message.requests.funcs.decode = &decode_parameter;
      message.requests.arg = &parameter;
      message.parameters.funcs.decode = &decode_parameter;
      message.parameters.arg = &parameter;
      message.responses.funcs.decode = &decode_parameter;
      message.responses.arg = &parameter;
      pb_decode(&pbbuffer_in, subseawireless_Message_fields, &message);

      //success = success && decode_swig_message(&pbbuffer_in, &message);
      Serial.printf("SWiG message from %i \n", message.source);
    }
    udp.flush();

    if (!success) return;
    
    // send response back to sender
    pb_ostream_t pbbuffer = pb_ostream_from_buffer(sMsg, sizeof(sMsg));
    encode_swig_response(&pbbuffer);
    size_t cobs_size = cobs_coder.encode(sMsg, pbbuffer.bytes_written, cobs_buffer);
    // add 0 as delimiter 
    cobs_size++;
    udp.beginPacket(remote, 55501);
    udp.write(cobs_buffer,cobs_size);
    udp.endPacket();
    Serial.printf("Sent response, %i bytes\n", cobs_size);
  }
}

// Connect to Wifi
bool setup_wifi()
{
  delay(10);
  Serial.println();
  Serial.print("Connecting to ");
  Serial.println(ssid);

  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password);
  
  Serial.print("Signal strength: ");
  Serial.println(WiFi.RSSI());

  int connect_timeout = 30; // if it can't connect in 10 seconds give up
  while ((WiFi.status() != WL_CONNECTED) && (connect_timeout>0))
  {

    Serial.printf("Status %i time-out %i\n", WiFi.status(), connect_timeout);
    delay(1000);
    connect_timeout--;
    flash_LED(50, 0,0);
  }

  if (connect_timeout==0) {
    Serial.println("Connecting to Wifi failed.\n");
    return false; // WiFi connection failed
  }
  randomSeed(micros());

  Serial.println("");
  Serial.println("WiFi connected");
  Serial.println("IP address: ");
  Serial.println(WiFi.localIP());
  return true;
}

void setup() {
  pixels.clear();
  pixels.setPixelColor(0, pixels.Color (0,0,0));
    // Use serial port
  Serial.begin(115200);
  // set up COBS
  cobs.begin(&Serial);
  cobs.setPacketHandler(&onPacketReceived);

  setup_wifi();

  udp.begin(udp_port);
  
  pixels.begin();

  pixels.clear();
  pixels.setPixelColor(0, pixels.Color (10,10,0));

  pb_ostream_t sizestream = {0};
  
}

bool send_request(pb_ostream_t *stream, const pb_field_iter_t *field, void * const *arg) {
  subseawireless_Parameter parameter = subseawireless_Parameter_init_default;
  parameter.integer = 42; //integer
  parameter.id = subseawireless_Parameter_identifier_swig_version_major; //SWiG version
  return pb_encode(stream, subseawireless_Parameter_fields, &parameter);
}


int counter = 0;
void loop() {
  // put your main code here, to run repeatedly:


  //Serial.printf("test! %i\n", counter);
  //pixels.setPixelColor(0, pixels.Color (counter, counter/64, counter/4096));
  pixels.show();
  cobs.update();

  udp_update();
  counter+=4;
  delay(10);
  

}
