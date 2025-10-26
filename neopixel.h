#ifndef NEOPIXEL_H
#define NEOPIXEL_H

#include <Arduino.h>
#include <Adafruit_NeoPixel.h>
#include <vector>
#include <string>

extern Adafruit_NeoPixel pixels;  // ✅ referencia global

void neopixelSetup();
void setNeoPixelColour(const std::string &colour);
void flash(int numberOfFlashes, const std::vector<std::string> &colors, const std::string &finalColour);

#endif
