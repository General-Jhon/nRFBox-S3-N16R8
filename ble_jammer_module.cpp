#include "bluetooth_module.h"
#include "config.h"
#include <RF24.h>
#include <SPI.h>

extern U8G2_SSD1306_128X64_NONAME_F_HW_I2C oled;
extern Adafruit_NeoPixel pixels;
extern void header(const char* title);

namespace BleJammer {

// Instancias globales de las dos radios (A y B)
static RF24 radioA(NRF_CE_PIN_A, NRF_CSN_PIN_A);
static RF24 radioB(NRF_CE_PIN_B, NRF_CSN_PIN_B);

enum JamMode { JAM_OFF = 0, JAM_BLE, JAM_BT };
static JamMode jamMode = JAM_OFF;

static unsigned long lastJammed = 0;
static unsigned long jamPacketsSent = 0;
static int jamChannel = -1;

// Configurar radio individual
static void configureRadio(RF24 &r) {
  r.setAutoAck(false);
  r.stopListening();
  r.setRetries(0, 0);
  r.setPALevel(RF24_PA_LOW);  // potencia baja para entorno controlado
  r.setDataRate(RF24_2MBPS);
  r.setCRCLength(RF24_CRC_DISABLED);
}

// =========================================================
//                         SETUP
// =========================================================
void bleJammerSetup() {
  header("BLE Jammer");
  oled.drawStr(8, 28, "Inicializando antenas...");
  oled.sendBuffer();

  SPI.begin(SPI_SCK_PIN, SPI_MISO_PIN, SPI_MOSI_PIN);
  delay(10);

  bool okA = radioA.begin();
  bool okB = radioB.begin();

  if (okA) configureRadio(radioA);
  if (okB) configureRadio(radioB);

  oled.clearBuffer();
  oled.setFont(u8g2_font_5x8_tr);
  oled.drawStr(6, 16, "Estado de antenas:");
  oled.drawStr(6, 30, okA ? "Radio A: OK" : "Radio A: FAIL");
  oled.drawStr(6, 42, okB ? "Radio B: OK" : "Radio B: FAIL");
  oled.sendBuffer();

  pixels.setBrightness(40);
  pixels.setPixelColor(0, okA ? pixels.Color(0, 0, 60) : pixels.Color(80, 0, 0));
  pixels.show();

  jamMode = JAM_OFF;
  lastJammed = millis();
  jamPacketsSent = 0;
  jamChannel = -1;
  delay(800);
}

// =========================================================
//                         LOOP
// =========================================================
void bleJammerLoop() {
  // salir con LEFT
  if (digitalRead(BTN_LEFT_PIN) == LOW) {
    jamMode = JAM_OFF;
    pixels.clear();
    pixels.show();
    oled.clearBuffer();
    oled.sendBuffer();
    delay(200);
    return;
  }

  // cambiar modo con SELECT
  if (digitalRead(BTN_SELECT_PIN) == LOW) {
    jamMode = static_cast<JamMode>((jamMode + 1) % 3);
    oled.clearBuffer();
    oled.setFont(u8g2_font_6x10_tr);
    oled.drawStr(10, 18, "BLE Jammer");
    oled.setFont(u8g2_font_5x8_tr);
    oled.drawStr(10, 38, jamMode == JAM_OFF ? "Modo: OFF" :
                       jamMode == JAM_BLE ? "Modo: BLE" : "Modo: BT");
    oled.drawStr(10, 54, "LEFT = salir");
    oled.sendBuffer();

    if (jamMode == JAM_OFF) pixels.setPixelColor(0, pixels.Color(20, 20, 20));
    else if (jamMode == JAM_BLE) pixels.setPixelColor(0, pixels.Color(0, 80, 0));
    else pixels.setPixelColor(0, pixels.Color(80, 80, 0));
    pixels.show();

    delay(250);
  }

  // operación periódica (envío)
  if (jamMode != JAM_OFF && millis() - lastJammed > 200) {
    int bleChs[] = {2, 26, 80};
    int btChs[]  = {32, 34, 46, 48, 50, 52};
    int ch = (jamMode == JAM_BLE)
      ? bleChs[random(0, 3)]
      : btChs[random(0, 6)];

    jamChannel = ch;
    const char payload[] = "xxxxxxxxxxxxx";

    // Transmitir con ambas si están conectadas
    if (radioA.isChipConnected()) {
      radioA.setChannel(ch);
      radioA.write(&payload, sizeof(payload));
    }
    if (radioB.isChipConnected()) {
      radioB.setChannel(ch);
      radioB.write(&payload, sizeof(payload));
    }

    jamPacketsSent++;
    lastJammed = millis();
  }

  // mostrar info cada 500 ms
  static unsigned long lastDisplay = 0;
  if (millis() - lastDisplay > 500) {
    oled.clearBuffer();
    oled.setFont(u8g2_font_6x10_tr);
    oled.drawStr(10, 10, "BLE Jammer");
    oled.setFont(u8g2_font_5x8_tr);
    oled.drawStr(10, 24, jamMode == JAM_OFF ? "Modo: OFF" :
                       jamMode == JAM_BLE ? "Modo: BLE" : "Modo: BT");

    char buf[32];
    snprintf(buf, sizeof(buf), "Canal: %d", jamChannel);
    oled.drawStr(10, 38, buf);

    snprintf(buf, sizeof(buf), "Paquetes: %lu", (unsigned long)jamPacketsSent);
    oled.drawStr(10, 52, buf);
    oled.sendBuffer();
    lastDisplay = millis();
  }
}

} // namespace BleJammer



