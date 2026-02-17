/* ============================================================
   nRFBox S3 – Menú jerárquico con submenús (WiFi/BLE/nRF/Analyzer)
   Autor: General_Jhon | 2025
   Hardware:
     OLED SSD1306: SDA=17, SCL=18, addr 0x3C
     Botones: UP=13, DOWN=12, LEFT=11, RIGHT=10, SELECT=9
     nRF24: A(CE=5,CSN=21) B(CE=15,CSN=7)
     SPI: SCK=40, MOSI=41, MISO=42
     NeoPixel: GPIO48 (ESP32-S3 N16R8)
   ============================================================ */

#include <Arduino.h>
#include <Wire.h>
#include <U8g2lib.h>
#include <WiFi.h>
#include <BLEDevice.h>
#include <SPI.h>
#include <Adafruit_NeoPixel.h>
#include <Adafruit_SSD1306.h>

// === MÓDULOS ===
#include "config.h"
#include "icon.h"
#include "boot_animation.h"
#include "wifi_module.h"
#include "analyzer_module.h"
#include "bluetooth_module.h"
#include "ble_jammer_module.h"
#include "setting.h"
#include "nrf_module.h"  // ✅ Nuevo módulo nRF24 (antenas A y B)
#include "captive_portal_module.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include "esp_netif.h"


// === OLED (para el menú principal con U8G2) ===
U8G2_SSD1306_128X64_NONAME_F_HW_I2C oled(U8G2_R0, U8X8_PIN_NONE);

// === OBJETOS GLOBALES COMPARTIDOS (para boot_animation) ===
Adafruit_SSD1306 display(128, 64, &Wire, -1);
Adafruit_NeoPixel pixels(1, 48, NEO_GRB + NEO_KHZ800);

// === PINES ===
#define SDA_PIN     17
#define SCL_PIN     18
#define OLED_ADDR   0x3C
#define BTN_UP      13
#define BTN_DOWN    12
#define BTN_LEFT    11
#define BTN_RIGHT   10
#define BTN_SELECT  9

// ===========================================================
// 🧾 Registro de logs para Captive Portal (y otros módulos)
// ===========================================================
void appendLog(const String &line) {
  // Si tienes SD o SPIFFS puedes reemplazar esto por guardado en archivo.
  // Por ahora lo dejamos simple para depuración:
  Serial.println("[LOG] " + line);
}

void resetWifiStack() {
  Serial.println("🧹 Reiniciando stack WiFi...");
  WiFi.disconnect(true, true);
  delay(100);

  // Detener completamente la interfaz WiFi
  esp_wifi_stop();
  delay(100);

  // Reiniciar el driver WiFi
  esp_wifi_start();
  delay(150);

  // Volver a modo estación
  WiFi.mode(WIFI_STA);
  delay(150);
}



// === VARIABLES GLOBALES ===
String activeModule = "";
bool inModule = false;
unsigned long lastBtn = 0;
const unsigned long DEBOUNCE_MS = 250;
int currentRow = 0;
int currentCol = 0;
int listIndex = 0;

// === MENÚS ===
enum MenuLevel { MAIN_MENU, WIFI_MENU, BLE_MENU };
MenuLevel currentMenu = MAIN_MENU;

const char* mainMenuText[] = {"WiFi Tools","BLE Tools","nRF24","Settings","About"};
const unsigned char* mainIcons[] = {
  bitmap_icon_wifi, bitmap_icon_ble, bitmap_icon_sword,
  bitmap_icon_setting, bitmap_icon_about
};
const char* wifiMenuText[] = {"WiFi Scan","Analyzer","Captive Portal","Back"};
const char* bleMenuText[]  = {"BLE Scan","BLE Analyzer","Beacon Detector","BLE Jammer","Back"};

// === FUNCIONES UTILITARIAS ===
bool btnPressed(int pin) {
  if (digitalRead(pin) == LOW && millis() - lastBtn > DEBOUNCE_MS) {
    lastBtn = millis();
    return true;
  }
  return false;
}

void header(const char* title) {
  oled.clearBuffer();
  oled.setFont(u8g2_font_6x10_tr);
  oled.drawBox(0,0,128,10);
  oled.setDrawColor(0);
  oled.drawStr(2,8,title);
  oled.setDrawColor(1);
}

void drawMainMenu() {
  oled.clearBuffer();
  oled.setFont(u8g2_font_ncenB08_tr);
  oled.drawStr(30, 10, "nRFBox S3");

  const int cellW = 40, cellH = 26, x0 = 12, y0 = 16;
  const int menuRows = 2, menuCols = 3;

  for (int r = 0; r < menuRows; r++) {
    for (int c = 0; c < menuCols; c++) {
      int idx = r * menuCols + c;
      if (idx >= 5) break;
      int x = x0 + c * cellW;
      int y = y0 + r * cellH;
      oled.drawXBMP(x, y, 16, 16, mainIcons[idx]);
      if (r == currentRow && c == currentCol) {
        oled.drawRFrame(x - 3, y - 3, 22, 22, 3);
        oled.setFont(u8g2_font_4x6_tr);
        int w = oled.getUTF8Width(mainMenuText[idx]);
        oled.drawStr((128 - w) / 2, 63, mainMenuText[idx]);
      }
    }
  }
  oled.sendBuffer();
}

void drawWifiMenu() {
  oled.clearBuffer();
  oled.setFont(u8g2_font_6x10_tr);
  oled.drawStr(32, 10, "WiFi Tools");
  for (int i = 0; i < 4; i++) {
    int y = 22 + i * 12;
    if (i == listIndex) {
      oled.drawBox(0, y - 8, 128, 12);
      oled.setDrawColor(0);
    }
    oled.setFont(u8g2_font_5x8_tr);
    oled.drawStr(8, y + 1, wifiMenuText[i]);
    oled.setDrawColor(1);
  }
  oled.sendBuffer();
}

void drawBleMenu() {
  oled.clearBuffer();
  oled.setFont(u8g2_font_6x10_tr);
  oled.drawStr(36, 10, "BLE Tools");
  for (int i = 0; i < 5; i++) {
    int y = 22 + i * 12;
    if (i == listIndex) {
      oled.drawBox(0, y - 8, 128, 12);
      oled.setDrawColor(0);
    }
    oled.setFont(u8g2_font_5x8_tr);
    oled.drawStr(8, y + 1, bleMenuText[i]);
    oled.setDrawColor(1);
  }
  oled.sendBuffer();
}

// === SETTINGS / ABOUT ===
void settingsSetup() {
  header("Settings");
  oled.drawStr(2,22,"(demo) Nada que");
  oled.drawStr(2,34,"configurar aun.");
  oled.sendBuffer();
}
void settingsLoop() {}

void aboutSetup() {
  header("About");
  oled.drawStr(2,22,"nRFBox S3 (demo)");
  oled.drawStr(2,34,"by General_Jhon");
  oled.drawStr(2,46,"WiFi/BLE/nRF tools");
  oled.sendBuffer();
}
void aboutLoop() {}

// === SETUP / LOOP PRINCIPAL ===
void setup() {
  Serial.begin(115200);
  delay(100);

  pinMode(BTN_UP, INPUT_PULLUP);
  pinMode(BTN_DOWN, INPUT_PULLUP);
  pinMode(BTN_LEFT, INPUT_PULLUP);
  pinMode(BTN_RIGHT, INPUT_PULLUP);
  pinMode(BTN_SELECT, INPUT_PULLUP);

  Wire.begin(SDA_PIN, SCL_PIN);
  oled.begin(); 
  oled.setI2CAddress(OLED_ADDR << 1);

  // Inicializa NeoPixel correctamente
  pixels.begin();
  pixels.setBrightness(60);
  pixels.clear();
  pixels.show();

  // 💥 Animación de arranque

  // Inicializa el display de Adafruit (usado por la animación)
  if (!display.begin(SSD1306_SWITCHCAPVCC, 0x3C)) {
    Serial.println("⚠️ No se detectó pantalla OLED (0x3C)");
  } else {
    display.clearDisplay();
    display.display();
    delay(200);
}

  bootAnimation();

  // ✅ Color de "sistema activo"
  pixels.setPixelColor(0, pixels.Color(0, 40, 0)); // Verde tenue
  pixels.show();

  // Limpia OLED y muestra el menú
  oled.clearBuffer();
  oled.sendBuffer();
  drawMainMenu();
}

void loop() {
  if (!inModule) {
    // --- MENÚ PRINCIPAL ---
    if (currentMenu == MAIN_MENU) {
      if (btnPressed(BTN_UP) && currentRow > 0) { currentRow--; drawMainMenu(); }
      if (btnPressed(BTN_DOWN) && currentRow < 1) { currentRow++; drawMainMenu(); }
      if (btnPressed(BTN_LEFT) && currentCol > 0) { currentCol--; drawMainMenu(); }
      if (btnPressed(BTN_RIGHT) && currentCol < 2) { currentCol++; drawMainMenu(); }

      if (btnPressed(BTN_SELECT)) {
        int sel = currentRow * 3 + currentCol;
        if (sel == 0) { currentMenu = WIFI_MENU; listIndex = 0; drawWifiMenu(); return; }
        else if (sel == 1) { currentMenu = BLE_MENU; listIndex = 0; drawBleMenu(); return; }
        else if (sel == 2) { inModule = true; activeModule = "nrf"; nrfSetup(); return; }
        else if (sel == 3) { inModule = true; activeModule = "settings"; settingsSetup(); return; }
        else if (sel == 4) { inModule = true; activeModule = "about"; aboutSetup(); return; }
      }
    }

    // --- WIFI MENU ---
    else if (currentMenu == WIFI_MENU) {
      if (btnPressed(BTN_UP) && listIndex > 0) { listIndex--; drawWifiMenu(); }
      if (btnPressed(BTN_DOWN) && listIndex < 3) { listIndex++; drawWifiMenu(); }
      if (btnPressed(BTN_SELECT)) {
        if (listIndex == 0) { inModule = true; activeModule = "wifiscan"; WifiScan::wifiscanSetup(); return; }
        if (listIndex == 1) { inModule = true; activeModule = "analyzer"; Analyzer::analyzerSetup(); return; }

        // --- Captive Portal ---
        if (listIndex == 2) {
          inModule = true;
          activeModule = "captiveportal";

          // 🔧 Reinicio forzado del stack Wi-Fi (previene “Iniciando...” colgado)
          resetWifiStack();   // 🧩 reinicia completamente el módulo WiFi

          CaptivePortal::setup();
          return;
        }

        if (listIndex == 3) { currentMenu = MAIN_MENU; drawMainMenu(); return; }
      }

      if (btnPressed(BTN_LEFT)) { currentMenu = MAIN_MENU; drawMainMenu(); }
    }

    // --- BLE MENU ---
    else if (currentMenu == BLE_MENU) {
      if (btnPressed(BTN_UP) && listIndex > 0) { listIndex--; drawBleMenu(); }
      if (btnPressed(BTN_DOWN) && listIndex < 4) { listIndex++; drawBleMenu(); }

      if (btnPressed(BTN_SELECT)) {
        if (listIndex == 0) { inModule = true; activeModule = "blescan"; BluetoothModule::bleScanSetup(); return; }
        if (listIndex == 1) { inModule = true; activeModule = "bleanalyzer"; BluetoothModule::bleAnalyzerSetup(); return; }
        if (listIndex == 2) { inModule = true; activeModule = "beacon"; BluetoothModule::beaconSetup(); return; }
        if (listIndex == 3) { inModule = true; activeModule = "blejammer"; BleJammer::bleJammerSetup(); return; }
        if (listIndex == 4) { currentMenu = MAIN_MENU; drawMainMenu(); return; }
      }

      if (btnPressed(BTN_LEFT)) { currentMenu = MAIN_MENU; drawMainMenu(); }
    }
  } 
  else {
    // --- MÓDULOS ACTIVOS ---
    if (btnPressed(BTN_LEFT)) {
      // 🧩 Si estás dentro de Captive Portal, apágalo completamente
      if (activeModule == "captiveportal") {
        CaptivePortal::stop();         // Cierra servidor, DNS y AP
        delay(200);                    // Espera a liberar Wi-Fi
      }

      // 🔙 Regresa al menú principal
      inModule = false;
      activeModule = "";
      pixels.clear();
      pixels.show();
      drawMainMenu();
    } 
    else {
      // 🔁 Ejecuta el bucle correspondiente al módulo activo
      if (activeModule == "wifiscan")        WifiScan::wifiscanLoop();
      else if (activeModule == "analyzer")   Analyzer::analyzerLoop();
      else if (activeModule == "captiveportal") CaptivePortal::loop();
      else if (activeModule == "blescan")    BluetoothModule::bleScanLoop();
      else if (activeModule == "bleanalyzer")BluetoothModule::bleAnalyzerLoop();
      else if (activeModule == "beacon")     BluetoothModule::beaconLoop();
      else if (activeModule == "blejammer")  BleJammer::bleJammerLoop();
      else if (activeModule == "nrf")        nrfLoop();

      delay(120);
    }
  }
}



