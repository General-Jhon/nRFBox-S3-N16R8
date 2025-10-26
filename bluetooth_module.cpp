#include "bluetooth_module.h"
#include "config.h"

extern U8G2_SSD1306_128X64_NONAME_F_HW_I2C oled;
extern Adafruit_NeoPixel pixels;

// =================== Namespace ===================
namespace BluetoothModule {

// --- Variables internas ---
static BLEScan* scanner = nullptr;
static bool initialized = false;
static unsigned long lastScan = 0;
static int foundDevices = 0;

// Buffer de dispositivos BLE
struct BLEInfo {
  String name;
  String address;
  int rssi;
  bool connectable;
};
static BLEInfo bleList[8];

// --- Utilidad común para iniciar BLE ---
static void initBLEBase(const char* title) {
  oled.clearBuffer();
  oled.setFont(u8g2_font_6x10_tr);
  oled.drawStr(30, 28, title);
  oled.setFont(u8g2_font_5x8_tr);
  oled.drawStr(16, 46, "Iniciando BLE...");
  oled.sendBuffer();

  BLEDevice::deinit();
  BLEDevice::init("");
  scanner = BLEDevice::getScan();
  scanner->setActiveScan(true);
  scanner->setInterval(120);
  scanner->setWindow(80);

  pixels.setBrightness(40);
  pixels.setPixelColor(0, pixels.Color(0, 0, 80)); // azul escaneo
  pixels.show();

  initialized = true;
  lastScan = millis();
  foundDevices = 0;
}

// ================== BLE SCAN ==================
void bleScanSetup() {
  initBLEBase("BLE Scan");
}

void bleScanLoop() {
  if (!initialized) return;

  // Salir con botón LEFT
  if (digitalRead(BTN_LEFT_PIN) == LOW) {
    bleStop();
    oled.clearBuffer();
    oled.sendBuffer();
    delay(200);
    return;
  }

  // Escaneo cada 3s
  if (millis() - lastScan > 3000) {
    BLEScanResults* res = scanner->start(2, false);  // ← CORREGIDO
    foundDevices = res->getCount();                  // ← CORREGIDO

    for (int i = 0; i < min(foundDevices, 8); i++) {
      BLEAdvertisedDevice dev = res->getDevice(i);
      bleList[i].name = dev.getName().length() ? dev.getName().c_str() : "Unknown";
      bleList[i].address = dev.getAddress().toString().c_str();
      bleList[i].rssi = dev.getRSSI();
      bleList[i].connectable = dev.isConnectable();
    }

    // Indicador de estado visual
    if (foundDevices > 0)
      pixels.setPixelColor(0, pixels.Color(0, 80, 0));   // verde
    else
      pixels.setPixelColor(0, pixels.Color(80, 0, 0));   // rojo
    pixels.show();

    lastScan = millis();
  }

  // Dibujar resultados
  oled.clearBuffer();
  oled.setFont(u8g2_font_5x8_tr);
  oled.drawStr(2, 8, "BLE Devices:");
  oled.drawLine(0, 10, 128, 10);

  if (foundDevices == 0) {
    oled.drawStr(2, 30, "No devices found");
  } else {
    int show = min(foundDevices, 6);
    for (int i = 0; i < show; i++) {
      int y = 18 + (i * 8);
      char line[64];
      snprintf(line, sizeof(line), "%d) %s %ddB %s",
        i + 1,
        bleList[i].name.substring(0, 8).c_str(),
        bleList[i].rssi,
        bleList[i].connectable ? "C" : "B");
      oled.drawStr(2, y, line);
    }
  }

  oled.sendBuffer();
}

// ================== BLE ANALYZER (DEMO) ==================
void bleAnalyzerSetup() {
  initBLEBase("BLE Analyzer");
  oled.drawStr(12, 46, "(demo) Coming soon...");
  pixels.setPixelColor(0, pixels.Color(80, 40, 0)); // naranja
  pixels.show();
  oled.sendBuffer();
}
void bleAnalyzerLoop() {}

// ================== BEACON DETECTOR (DEMO) ==================
void beaconSetup() {
  initBLEBase("Beacon Detector");
  oled.drawStr(12, 46, "(demo) Coming soon...");
  pixels.setPixelColor(0, pixels.Color(80, 0, 80)); // violeta
  pixels.show();
  oled.sendBuffer();
}
void beaconLoop() {}

// ================== DETENER BLE ==================
void bleStop() {
  if (scanner) scanner->stop();
  pixels.clear();
  pixels.show();
  initialized = false;
}

} // namespace BluetoothModule
