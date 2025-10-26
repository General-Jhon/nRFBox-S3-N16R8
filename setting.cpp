#include "setting.h"
#include "config.h"
#include "icon.h"
#include "neopixel.h"  // ✅ funciones NeoPixel desde su propio módulo

// === NeoPixel integrado ===
bool neoPixelActive = true;
uint8_t oledBrightness = 128;

// === NRF Radios ===
RF24 RadioA(NRF_CE_PIN_A, NRF_CSN_PIN_A);
RF24 RadioB(NRF_CE_PIN_B, NRF_CSN_PIN_B);
//RF24 RadioC(NRF_CE_PIN_C, NRF_CSN_PIN_C);

// ==========================
// Funciones de configuración
// ==========================

void setRadiosNeutralState() {
  // Pone ambas radios en estado seguro
  RadioA.stopListening();
  RadioA.setAutoAck(false);
  RadioA.setRetries(0, 0);
  RadioA.powerDown();
  digitalWrite(NRF_CE_PIN_A, LOW);

  RadioB.stopListening();
  RadioB.setAutoAck(false);
  RadioB.setRetries(0, 0);
  RadioB.powerDown();
  digitalWrite(NRF_CE_PIN_B, LOW);
}

void configureNrf(RF24 &radio, const char* tag) {
  if (!radio.begin()) {
    Serial.printf("❌ %s: begin() falló (revisa conexión o alimentación)\n", tag);
    return;
  }

  radio.setAutoAck(false);
  radio.stopListening();
  radio.setRetries(0, 0);
  radio.setPALevel(RF24_PA_LOW, true);
  radio.setDataRate(RF24_2MBPS);
  radio.setCRCLength(RF24_CRC_DISABLED);

  Serial.printf("✅ %s: inicializada correctamente\n", tag);
}

void setupRadioA() {
  configureNrf(RadioA, "Radio A");
}

void setupRadioB() {
  configureNrf(RadioB, "Radio B");
}

void initAllRadios() {
  Serial.println("=== Inicializando todas las radios nRF24 ===");
  setupRadioA();
  setupRadioB();
  Serial.println("============================================");
}

// ==========================
// Utilidades de texto OLED
// ==========================
void Str(uint8_t x, uint8_t y, const uint8_t* asciiArray, size_t len) {
  char buf[64];
  for (size_t i = 0; i < len && i < sizeof(buf) - 1; i++) buf[i] = (char)asciiArray[i];
  buf[len] = '\0';
  u8g2.drawStr(x, y, buf);
}

void CenteredStr(uint8_t screenWidth, uint8_t y, const uint8_t* asciiArray, size_t len, const uint8_t* font) {
  char buf[64];
  for (size_t i = 0; i < len && i < sizeof(buf) - 1; i++) buf[i] = (char)asciiArray[i];
  buf[len] = '\0';
  u8g2.setFont((const uint8_t*)font);
  int16_t w = u8g2.getUTF8Width(buf);
  u8g2.setCursor((screenWidth - w) / 2, y);
  u8g2.print(buf);
}

// ==========================
// Pantalla de arranque
// ==========================
void conf() {
  u8g2.setBitmapMode(1);
  u8g2.clearBuffer();
  CenteredStr(128, 25, txt_n, sizeof(txt_n), u8g2_font_ncenB14_tr);
  CenteredStr(106, 40, txt_c, sizeof(txt_c), u8g2_font_ncenB08_tr);
  CenteredStr(128, 60, txt_v, sizeof(txt_v), u8g2_font_6x10_tf);
  u8g2.sendBuffer();
  delay(2000);
  u8g2.clearBuffer();
  u8g2.drawXBMP(0, 0, 128, 64, cred);
  u8g2.sendBuffer();
  delay(400);
}

// ==========================
// ⚙️ Menú de configuración
// ==========================
namespace Setting {

#define EEPROM_ADDRESS_NEOPIXEL 0
#define EEPROM_ADDRESS_BRIGHTNESS 1

int currentOption = 0;
int totalOptions = 2; // opciones: NeoPixel y Brillo

bool buttonUpPressed = false;
bool buttonDownPressed = false;
bool buttonSelectPressed = false;

void toggleOption(int option) {
  if (option == 0) {
    neoPixelActive = !neoPixelActive;
    EEPROM.write(EEPROM_ADDRESS_NEOPIXEL, neoPixelActive);
    EEPROM.commit();
    Serial.printf("NeoPixel %s\n", neoPixelActive ? "ON" : "OFF");
  }
  else if (option == 1) {
    uint8_t brightnessPercent = map(oledBrightness, 0, 255, 0, 100);
    brightnessPercent += 10;
    if (brightnessPercent > 100) brightnessPercent = 0;
    oledBrightness = map(brightnessPercent, 0, 100, 0, 255);
    u8g2.setContrast(oledBrightness);
    EEPROM.write(EEPROM_ADDRESS_BRIGHTNESS, oledBrightness);
    EEPROM.commit();
    Serial.printf("Brillo: %d%%\n", brightnessPercent);
  }
}

void handleButtons() {
  if (!digitalRead(13)) { // arriba
    if (!buttonUpPressed) { buttonUpPressed = true; currentOption = (currentOption - 1 + totalOptions) % totalOptions; }
  } else buttonUpPressed = false;

  if (!digitalRead(12)) { // abajo
    if (!buttonDownPressed) { buttonDownPressed = true; currentOption = (currentOption + 1) % totalOptions; }
  } else buttonDownPressed = false;

  if (!digitalRead(9)) { // select
    if (!buttonSelectPressed) { buttonSelectPressed = true; toggleOption(currentOption); }
  } else buttonSelectPressed = false;
}

void displayMenu() {
  u8g2.clearBuffer();
  u8g2.setFont(u8g2_font_6x10_tf);
  u8g2.drawStr(0, 10, "Configuración:");

  u8g2.setFont(u8g2_font_5x8_tr);
  u8g2.drawStr(0, 30, currentOption == 0 ? "> NeoPixel:" : "  NeoPixel:");
  u8g2.setCursor(80, 30);
  u8g2.print(neoPixelActive ? "ON" : "OFF");

  u8g2.drawStr(0, 45, currentOption == 1 ? "> Brillo:" : "  Brillo:");
  u8g2.setCursor(80, 45);
  uint8_t brightnessPercent = map(oledBrightness, 0, 255, 0, 100);
  u8g2.printf("%d%%", brightnessPercent);

  u8g2.sendBuffer();
}

void settingSetup() {
  Serial.begin(115200);
  EEPROM.begin(512);

  // 🔹 Cargar valores previos
  neoPixelActive = EEPROM.read(EEPROM_ADDRESS_NEOPIXEL);
  oledBrightness = EEPROM.read(EEPROM_ADDRESS_BRIGHTNESS);
  if (oledBrightness > 255) oledBrightness = 128;
  u8g2.setContrast(oledBrightness);

  // 🔹 Inicializar botones
  pinMode(13, INPUT_PULLUP); // arriba
  pinMode(12, INPUT_PULLUP); // abajo
  pinMode(9, INPUT_PULLUP);  // select

  // 🔹 Inicializar radios A y B
  initAllRadios();
}

void settingLoop() {
  handleButtons();
  displayMenu();
}

} // namespace Setting
