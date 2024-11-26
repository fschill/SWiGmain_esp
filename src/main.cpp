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
uint8_t sMsg[MSG_SIZE];

PacketSerial cobs; 

WiFiUDP udp;
char udp_packet_buffer[255];
unsigned int udp_port = 55502;


bool send_parameter(pb_ostream_t *stream, const pb_field_iter_t *field, void * const *arg) {
  if (!pb_encode_tag_for_field(stream, field))
    return false;
  subseawireless_Parameter parameter = subseawireless_Parameter_init_default;
  parameter.integer = 42; //integer
  parameter.id = subseawireless_Parameter_identifier_swig_version_major; //SWiG version
  return pb_encode_submessage(stream, subseawireless_Parameter_fields, &parameter);
}

void flash_LED(int r, int g, int b) {
  pixels.clear();
  pixels.setPixelColor(0, pixels.Color (r,g,b));
  pixels.show();
  delay(10);
  pixels.setPixelColor(0, pixels.Color (10,10,0));
  pixels.show();
}

void encode_swig_response(pb_ostream_t *pbbuffer) {
  subseawireless_Message message = subseawireless_Message_init_default;
  message.source = 2;
  message.target = 1;
  //message.requests.funcs.encode = &send_request; 
  //message.parameters.funcs.encode = &send_parameter; 
  message.responses.funcs.encode = &send_parameter; 

  pb_encode(pbbuffer, subseawireless_Message_fields , &message);
}
void onPacketReceived(const uint8_t* buffer, size_t size)
{
  flash_LED(0,255,0);
  pb_ostream_t pbbuffer = pb_ostream_from_buffer(sMsg, sizeof(sMsg));
  encode_swig_response(&pbbuffer);
  cobs.send(sMsg, pbbuffer.bytes_written); 

}

void udp_update() {
  int size = udp.parsePacket();
  if (size > 0) {
    IPAddress remote = udp.remoteIP();
    Serial.printf("Received UDP packet from %s,  %i bytes\n", remote.toString(), size);
    flash_LED(0,0,255);
    udp.flush();
    pb_ostream_t pbbuffer = pb_ostream_from_buffer(sMsg, sizeof(sMsg));
    encode_swig_response(&pbbuffer);
    uint8_t cobs_buffer[100] = {0};
    COBS encoder;
    size_t cobs_size = encoder.encode(sMsg, pbbuffer.bytes_written, cobs_buffer);
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
