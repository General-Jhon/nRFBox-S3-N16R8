#ifndef CONFIG_H
#define CONFIG_H

// -----------------------------
// Pantalla
// -----------------------------
#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
#define OLED_SDA_PIN 17
#define OLED_SCL_PIN 18
#define OLED_ADDR 0x3C

// -----------------------------
// Botones (tu asignación confirmada)
// -----------------------------
#define BTN_UP_PIN 13    // Arriba
#define BTN_DOWN_PIN 12  // Abajo
#define BTN_LEFT_PIN 11  // Izquierda (Back)
#define BTN_RIGHT_PIN 10 // Derecha
#define BTN_SELECT_PIN 9 // Select / Enter

// -----------------------------
// NeoPixel integrado (ESP32-S3)
// -----------------------------
#define NEOPIXEL_PIN 48
#define NUMPIXELS 1

// -----------------------------
// nRF24 - Radios A y B
// -----------------------------
#define NRF_CE_PIN_A 5
#define NRF_CSN_PIN_A 21

#define NRF_CE_PIN_B 15
#define NRF_CSN_PIN_B 7

// (opcional para futura expansión)
// #define NRF_CE_PIN_C 16
// #define NRF_CSN_PIN_C 4

// -----------------------------
// SPI pins para nRF24 (ajusta si cambias cableado)
// -----------------------------
#define SPI_SCK_PIN 40
#define SPI_MOSI_PIN 41
#define SPI_MISO_PIN 42

// -----------------------------
// SD / OTA (si usas SD card)
// -----------------------------
#define SD_CS_PIN 33
#define FIRMWARE_FILE "/firmware.bin"

// -----------------------------
// Includes - headers públicos
// (no instancias de objetos aquí)
// -----------------------------
#include <Arduino.h>
#include <U8g2lib.h>
#include <Wire.h>
#include <SPI.h>
#include <Adafruit_NeoPixel.h>
#include <EEPROM.h>
#include <vector>
#include <string>
#include <SD.h>
#include <Update.h>
#include <BLEDevice.h>
#include <nRF24L01.h>
#include <RF24.h>
#include <WiFi.h>

// -----------------------------
// Estado general del sistema
// -----------------------------
enum AppState
{
    STATE_MENU,
    STATE_WIFI_SCAN,
    STATE_BLE_SCAN,
    STATE_ANALYZER,
    STATE_JAMMER
};

extern AppState currentState;

// -----------------------------
// Externs: instancias definidas en un único .cpp (p. ej. setting.cpp o main .ino)
// -----------------------------
extern U8G2_SSD1306_128X64_NONAME_F_HW_I2C oled;
extern Adafruit_NeoPixel pixels;
extern RF24 RadioA;
extern RF24 RadioB;
extern AppState currentState;

// -----------------------------
// Namespaces / forward declarations de módulos
// -----------------------------
namespace BleJammer { void blejammerSetup(); void blejammerLoop(); }
namespace BleScan   { void blescanSetup();    void blescanLoop(); }
namespace SourApple { void sourappleSetup();  void sourappleLoop(); }
namespace Spoofer   { void spooferSetup();    void spooferLoop(); }
namespace Analyzer  { void analyzerSetup();   void analyzerLoop(); }
namespace ProtoKill { void blackoutSetup();   void blackoutLoop(); }
namespace Scanner   { void scannerSetup();    void scannerLoop(); }
namespace Jammer    { void jammerSetup();     void jammerLoop(); }
namespace WifiScan  { void wifiscanSetup();   void wifiscanLoop(); }
namespace Deauther  { void deautherSetup();   void deautherLoop(); }
namespace Setting   { void settingSetup();    void settingLoop(); }

#endif // CONFIG_H
