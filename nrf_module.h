#ifndef NRF_MODULE_H
#define NRF_MODULE_H

#include <Arduino.h>
#include <RF24.h>
#include <U8g2lib.h>
#include <Adafruit_NeoPixel.h>

// === Pines de SPI y radios ===
#define CE_A_PIN   5
#define CSN_A_PIN  21
#define CE_B_PIN   15
#define CSN_B_PIN  7
#define SCK_PIN    40
#define MOSI_PIN   41
#define MISO_PIN   42

// === Declaraciones externas (definidas en el .ino principal) ===
extern U8G2_SSD1306_128X64_NONAME_F_HW_I2C oled;
extern Adafruit_NeoPixel pixels;

// === Funciones públicas del módulo nRF ===
void nrfSetup();
void nrfLoop();

#endif
