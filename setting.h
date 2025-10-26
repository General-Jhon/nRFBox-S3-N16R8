#ifndef SETTING_H
#define SETTING_H

#include <BLEDevice.h>
#include <U8g2lib.h>
#include <Adafruit_NeoPixel.h>
#include <EEPROM.h>
#include <RF24.h>
#include <vector>
#include <string>
#include <SD.h>
#include <Update.h>
#include <SPI.h>

// =====================
// 🟢 Pines del hardware
// =====================
#define OLED_SDA 17
#define OLED_SCL 18

#define NEOPIXEL_PIN 48
#define NUMPIXELS 1

// Pines nRF24
#define NRF_CE_PIN_A 5
#define NRF_CSN_PIN_A 21
#define NRF_CE_PIN_B 15
#define NRF_CSN_PIN_B 7
// #define NRF_CE_PIN_C 16
// #define NRF_CSN_PIN_C 4   // (opcional para futuras expansiones)

// =====================
// 🟢 Objetos principales
// =====================
extern U8G2_SSD1306_128X64_NONAME_F_HW_I2C u8g2;
extern Adafruit_NeoPixel pixels;

// =====================
// 🔧 Variables globales
// =====================
extern bool neoPixelActive;
extern uint8_t oledBrightness;

// === Radios nRF24 ===
extern RF24 RadioA;
extern RF24 RadioB;
// extern RF24 RadioC;   // (dejar preparado si luego agregas una tercera)

// =====================
// 🔧 Funciones principales
// =====================
void neopixelSetup();
void neopixelLoop();

void setNeoPixelColour(const std::string& colour);
void flash(int numberOfFlashes, const std::vector<std::string>& colors, const std::string& finalColour);

// === Radios ===
void configureNrf(RF24 &radio, const char* tag);
void setRadiosNeutralState();
void setupRadioA();
void setupRadioB();
void initAllRadios();

// === Utilidades OLED ===
void Str(uint8_t x, uint8_t y, const uint8_t* asciiArray, size_t len);
void CenteredStr(uint8_t screenWidth, uint8_t y, const uint8_t* asciiArray, size_t len, const uint8_t* font);
void utils();
void conf();

// =====================
// ⚙️ Menú de configuración
// =====================
namespace Setting {
  void settingSetup();
  void settingLoop();
}

#endif


