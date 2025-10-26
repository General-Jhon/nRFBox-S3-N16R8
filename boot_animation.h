#ifndef BOOT_ANIMATION_H
#define BOOT_ANIMATION_H

#include <Arduino.h>
#include <Adafruit_SSD1306.h>
#include <Adafruit_NeoPixel.h>

// OLED y NeoPixel externos
extern Adafruit_SSD1306 display;
extern Adafruit_NeoPixel pixel;

// Función principal de arranque
void bootAnimation();

#endif
