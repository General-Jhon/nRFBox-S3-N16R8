#include "nrf_module.h"

// === Radios ===
RF24 radioA(CE_A_PIN, CSN_A_PIN);
RF24 radioB(CE_B_PIN, CSN_B_PIN);

bool radioA_ok = false;
bool radioB_ok = false;
unsigned long nrf_last_update = 0;
const unsigned long NRF_UPDATE_MS = 800;

// --- Funciones auxiliares ---
bool initRadio(RF24 &r, const char *tag) {
  bool ok = r.begin();
  Serial.printf("[%s] begin() -> %s\n", tag, ok ? "OK" : "FAIL");
  if (!ok) return false;

  r.setPALevel(RF24_PA_MIN);
  r.setDataRate(RF24_250KBPS);
  r.setRetries(3, 5);
  r.setChannel(40);
  r.stopListening();
  return true;
}

void drawNrfStatus(bool a_ok, bool b_ok, int ach, int apa, uint8_t arpd,
                   int bch, int bpa, uint8_t brpd) {
  oled.clearBuffer();
  oled.setFont(u8g2_font_6x10_tr);
  oled.drawBox(0, 0, 128, 10);
  oled.setDrawColor(0);
  oled.drawStr(2, 8, "nRF24 Status");
  oled.setDrawColor(1);

  char buf[40];

  oled.setFont(u8g2_font_5x8_tr);
  snprintf(buf, sizeof(buf), "A: %s  CH:%3d PA:%d RPD:%u",
           a_ok ? "OK" : "FAIL", ach, apa, arpd);
  oled.drawStr(2, 22, buf);

  snprintf(buf, sizeof(buf), "B: %s  CH:%3d PA:%d RPD:%u",
           b_ok ? "OK" : "FAIL", bch, bpa, brpd);
  oled.drawStr(2, 36, buf);

  oled.setFont(u8g2_font_4x6_tr);
  oled.drawStr(2, 52, "LEFT = exit");
  oled.sendBuffer();
}

void updateNeoPixel(bool a_ok, bool b_ok) {
  if (a_ok && b_ok) pixels.setPixelColor(0, pixels.Color(0, 120, 0));       // verde
  else if (a_ok || b_ok) pixels.setPixelColor(0, pixels.Color(120, 120, 0)); // amarillo
  else pixels.setPixelColor(0, pixels.Color(120, 0, 0));                    // rojo
  pixels.show();
}

// === SETUP ===
void nrfSetup() {
  oled.clearBuffer();
  oled.setFont(u8g2_font_6x10_tr);
  oled.drawStr(2, 22, "Inicializando...");
  oled.sendBuffer();

  SPI.begin(SCK_PIN, MISO_PIN, MOSI_PIN, CSN_A_PIN);
  delay(50);

  radioA_ok = initRadio(radioA, "Radio A");
  radioB_ok = initRadio(radioB, "Radio B");

  oled.drawStr(2, 36, radioA_ok ? "A: OK" : "A: FAIL");
  oled.drawStr(2, 46, radioB_ok ? "B: OK" : "B: FAIL");
  oled.sendBuffer();

  pixels.begin();
  pixels.setBrightness(80);
  updateNeoPixel(radioA_ok, radioB_ok);

  Serial.println("nRF module initialized (A/B):");
  Serial.printf(" A: %s\n", radioA_ok ? "OK" : "FAIL");
  Serial.printf(" B: %s\n", radioB_ok ? "OK" : "FAIL");

  nrf_last_update = millis();
}

// === LOOP ===
void nrfLoop() {
  if (millis() - nrf_last_update < NRF_UPDATE_MS) return;
  nrf_last_update = millis();

  if (!radioA_ok) radioA_ok = initRadio(radioA, "Radio A (retry)");
  if (!radioB_ok) radioB_ok = initRadio(radioB, "Radio B (retry)");

  int ach = -1, apa = -1;
  uint8_t arpd = 0;
  if (radioA_ok) {
    ach = radioA.getChannel();
    apa = radioA.getPALevel();
    arpd = radioA.testRPD();
  }

  int bch = -1, bpa = -1;
  uint8_t brpd = 0;
  if (radioB_ok) {
    bch = radioB.getChannel();
    bpa = radioB.getPALevel();
    brpd = radioB.testRPD();
  }

  drawNrfStatus(radioA_ok, radioB_ok, ach, apa, arpd, bch, bpa, brpd);
  updateNeoPixel(radioA_ok, radioB_ok);

  const char payload[] = "P";
  if (radioA_ok) {
    bool ok = radioA.write(&payload, sizeof(payload));
    if (!ok) {
      Serial.println("[Radio A] TX FAIL");
      radioA_ok = false;
    } else {
      Serial.printf("[Radio A] TX OK  CH=%d RPD=%u\n", ach, arpd);
    }
  }
  if (radioB_ok) {
    bool ok = radioB.write(&payload, sizeof(payload));
    if (!ok) {
      Serial.println("[Radio B] TX FAIL");
      radioB_ok = false;
    } else {
      Serial.printf("[Radio B] TX OK  CH=%d RPD=%u\n", bch, brpd);
    }
  }
}
