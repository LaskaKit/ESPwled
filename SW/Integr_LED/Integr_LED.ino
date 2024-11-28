/*
* This example code is used for LaskaKit ESP32-S3-DEVKit board  https://www.laskakit.cz/laskakit-esp32-s3-devkit/
* ESP32-S3-DEVKit just changing LED color 
* 
* Made by (c) laskakit.cz 2023
* 
*/

#include <Adafruit_NeoPixel.h>
#define PIN_LED 9

Adafruit_NeoPixel pixels = Adafruit_NeoPixel(1, PIN_LED, NEO_GRB + NEO_KHZ800);

void setup() {
  Serial.begin(115200);

  pixels.begin();
  pixels.setBrightness(10);
}

void loop() {
     
  pixels.setPixelColor(0, pixels.Color(255, 0, 0)); // R, G, B
  pixels.show();

  delay(1000);

  pixels.setPixelColor(0, pixels.Color(0, 255, 0)); // R, G, B
  pixels.show();

  delay(1000);

  pixels.setPixelColor(0, pixels.Color(0, 0, 255)); // R, G, B
  pixels.show();

  delay(1000);

  pixels.setPixelColor(0, pixels.Color(255, 255, 255)); // R, G, B
  pixels.show();

  delay(1000);

}