// analyzer_module.cpp
#include "analyzer_module.h"
#include "config.h"
#include <SPI.h>
#include <EEPROM.h>

extern U8G2_SSD1306_128X64_NONAME_F_HW_I2C oled;
extern Adafruit_NeoPixel pixels;

namespace Analyzer {

// =================== CONFIG ===================
const int ANALYZER_CHANNELS = 64;
const int SAMPLES_PER_CHANNEL = 24;
const int GRAPH_COLUMNS = 128;
const unsigned long SAVE_INTERVAL_MS = 5000UL;
const uint8_t EEPROM_BASE_ADDR = 100;

// pines base del SPI
const uint8_t SCK_PIN  = SPI_SCK_PIN;
const uint8_t MOSI_PIN = SPI_MOSI_PIN;
const uint8_t MISO_PIN = SPI_MISO_PIN;

// se actualizan según la radio viva
static uint8_t CE_PIN  = NRF_CE_PIN_A;
static uint8_t CSN_PIN = NRF_CSN_PIN_A;

static int channelValues[ANALYZER_CHANNELS];
static uint8_t graphBuffer[GRAPH_COLUMNS];
static unsigned long lastSave = 0;
static unsigned long lastScanMs = 0;
static bool initialized = false;

// =================== LOW LEVEL ===================
static void ceHigh()  { digitalWrite(CE_PIN, HIGH); }
static void ceLow()   { digitalWrite(CE_PIN, LOW);  }

static uint8_t readRegister(uint8_t reg) {
  digitalWrite(CSN_PIN, LOW);
  SPI.transfer(reg & 0x1F);
  uint8_t val = SPI.transfer(0x00);
  digitalWrite(CSN_PIN, HIGH);
  return val;
}

static void writeRegister(uint8_t reg, uint8_t value) {
  digitalWrite(CSN_PIN, LOW);
  SPI.transfer((reg & 0x1F) | 0x20);
  SPI.transfer(value);
  digitalWrite(CSN_PIN, HIGH);
}

// =================== RADIO DETECTION ===================
static bool probeRadio(uint8_t ce, uint8_t csn) {
  pinMode(ce, OUTPUT);
  pinMode(csn, OUTPUT);
  digitalWrite(ce, LOW);
  digitalWrite(csn, HIGH);
  delay(2);
  digitalWrite(csn, LOW);
  SPI.transfer(0x07 & 0x1F);
  uint8_t st = SPI.transfer(0x00);
  digitalWrite(csn, HIGH);
  return (st != 0x00);
}

static void selectAliveRadio() {
  bool a_ok = probeRadio(NRF_CE_PIN_A, NRF_CSN_PIN_A);
  bool b_ok = probeRadio(NRF_CE_PIN_B, NRF_CSN_PIN_B);
  if (!a_ok && b_ok) {
    CE_PIN  = NRF_CE_PIN_B;
    CSN_PIN = NRF_CSN_PIN_B;
  }
}

// =================== EEPROM ===================
static void loadGraphFromEEPROM() {
  EEPROM.begin(512);
  for (int i = 0; i < GRAPH_COLUMNS; ++i)
    graphBuffer[i] = EEPROM.read(EEPROM_BASE_ADDR + i);
  EEPROM.end();
}

static void saveGraphToEEPROM() {
  EEPROM.begin(512);
  for (int i = 0; i < GRAPH_COLUMNS; ++i)
    EEPROM.write(EEPROM_BASE_ADDR + i, graphBuffer[i]);
  EEPROM.commit();
  EEPROM.end();
}

// =================== SCAN ===================
static void scanChannels() {
  ceLow();
  for (int i = 0; i < ANALYZER_CHANNELS; ++i) channelValues[i] = 0;

  for (int ch = 0; ch < ANALYZER_CHANNELS; ++ch) {
    uint8_t rf_ch = (uint8_t)((125u * ch) / (ANALYZER_CHANNELS - 1));
    writeRegister(0x05, rf_ch);

    for (int s = 0; s < SAMPLES_PER_CHANNEL; ++s) {
      uint8_t cfg = readRegister(0x00) | 0x03; // PWR_UP + PRIM_RX
      writeRegister(0x00, cfg);
      delayMicroseconds(120);
      ceHigh();
      delayMicroseconds(160);
      ceLow();

      uint8_t rpd = readRegister(0x09) & 0x01;
      channelValues[ch] += rpd;
      delayMicroseconds(60);
    }
  }

  int maxHits = 0;
  for (int i = 0; i < ANALYZER_CHANNELS; ++i)
    if (channelValues[i] > maxHits) maxHits = channelValues[i];

  for (int i = GRAPH_COLUMNS - 1; i > 0; --i)
    graphBuffer[i] = graphBuffer[i - 1];

  uint8_t height = (uint8_t)map(maxHits, 0, SAMPLES_PER_CHANNEL, 0, 63);
  graphBuffer[0] = height;

  lastScanMs = millis();

  // DEBUG opcional
  static unsigned long dbgLast = 0;
  if (millis() - dbgLast > 1000) {
    uint8_t st = readRegister(0x07);
    uint8_t ch = readRegister(0x05);
    Serial.printf("[Analyzer] CE=%u CSN=%u STATUS=0x%02X RF_CH=%u\n", CE_PIN, CSN_PIN, st, ch);
    dbgLast = millis();
  }
}

// =================== DRAW ===================
static void drawAnalyzer() {
  oled.clearBuffer();
  int peak = 0;
  for (int i = 0; i < GRAPH_COLUMNS; ++i)
    if (graphBuffer[i] > peak) peak = graphBuffer[i];

  int dBm = map(peak, 0, 63, -90, -40);
  if (dBm < -90) dBm = -90;
  if (dBm > -40) dBm = -40;

  oled.drawLine(0, 0, 0, 63);
  oled.drawLine(127, 63, 0, 63);
  oled.setFont(u8g2_font_4x6_tr);
  oled.drawStr(2, 10, "-50");
  oled.drawStr(2, 30, "-70");
  oled.drawStr(2, 50, "-90");

  for (int x = 0; x < GRAPH_COLUMNS; ++x) {
    uint8_t h = graphBuffer[x];
    int sx = 127 - x;
    oled.drawLine(sx, 63, sx, 63 - h);
  }

  oled.setFont(u8g2_font_5x8_tr);
  char buf[32];
  snprintf(buf, sizeof(buf), "Power: %ddBm", dBm);
  oled.drawStr(60, 10, buf);

  if (dBm < -65) pixels.setPixelColor(0, pixels.Color(0, 60, 0));
  else if (dBm < -50) pixels.setPixelColor(0, pixels.Color(80, 60, 0));
  else pixels.setPixelColor(0, pixels.Color(100, 0, 0));
  pixels.show();

  oled.sendBuffer();
}

// =================== SETUP & LOOP ===================
void analyzerSetup() {
  pixels.clear();
  pixels.show();
  SPI.begin(SCK_PIN, MISO_PIN, MOSI_PIN);

  selectAliveRadio();
  pinMode(CE_PIN, OUTPUT);
  pinMode(CSN_PIN, OUTPUT);
  digitalWrite(CE_PIN, LOW);
  digitalWrite(CSN_PIN, HIGH);

  pixels.setBrightness(25);
  loadGraphFromEEPROM();

  oled.clearBuffer();
  oled.setFont(u8g2_font_6x10_tr);
  oled.drawStr(10, 20, "Analyzer");
  oled.setFont(u8g2_font_5x8_tr);
  oled.drawStr(10, 36, (CE_PIN==NRF_CE_PIN_B) ? "Radio usada: B" : "Radio usada: A");
  oled.drawStr(10, 52, "LEFT para salir");
  oled.sendBuffer();

  initialized = true;
  lastSave = millis();
  scanChannels();
  drawAnalyzer();
}

void analyzerLoop() {
  if (!initialized) return;

  if (digitalRead(BTN_LEFT_PIN) == LOW) {
    oled.clearBuffer();
    oled.sendBuffer();
    pixels.clear();
    pixels.show();
    saveGraphToEEPROM();
    initialized = false;
    delay(300);
    return;
  }

  unsigned long now = millis();
  if (now - lastScanMs > 180) {
    scanChannels();
    drawAnalyzer();
  }

  if (now - lastSave > SAVE_INTERVAL_MS) {
    saveGraphToEEPROM();
    lastSave = now;
  }
}

} // namespace Analyzer


