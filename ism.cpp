#include "config.h"
#include "icon.h"
#include "setting.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include "esp_system.h"
#include "nvs_flash.h"


// ============================================================================
// UTILIDADES COMUNES
// ============================================================================
static inline bool btnPressed(int pin, unsigned long &last, unsigned long debounce = 180) {
  if (digitalRead(pin) == LOW) {
    unsigned long now = millis();
    if (now - last > debounce) { last = now; return true; }
  }
  return false;
}

static inline void nrf_writeReg(uint8_t reg, uint8_t val) {
  digitalWrite(NRF_CSN_PIN_A, LOW);
  SPI.transfer((reg & 0x1F) | 0x20);
  SPI.transfer(val);
  digitalWrite(NRF_CSN_PIN_A, HIGH);
}
static inline uint8_t nrf_readReg(uint8_t reg) {
  digitalWrite(NRF_CSN_PIN_A, LOW);
  SPI.transfer(reg & 0x1F);
  uint8_t v = SPI.transfer(0x00);
  digitalWrite(NRF_CSN_PIN_A, HIGH);
  return v;
}
static inline void nrf_powerUp()  { nrf_writeReg(0x00, nrf_readReg(0x00) | 0x02); delayMicroseconds(150); }
static inline void nrf_powerDown(){ nrf_writeReg(0x00, nrf_readReg(0x00) & ~0x02); }
static inline void nrf_rxMode()   { nrf_writeReg(0x00, nrf_readReg(0x00) | 0x01); digitalWrite(NRF_CE_PIN_A, HIGH); delayMicroseconds(100); }
static inline void nrf_ceLow()    { digitalWrite(NRF_CE_PIN_A, LOW); }
static inline void nrf_setCH(uint8_t ch) { nrf_writeReg(0x05, ch); } // RF_CH
static inline bool nrf_carrier()  { return (nrf_readReg(0x09) & 0x01); } // RPD

static void nrf_commonBegin() {
  // SPI y pines
  pinMode(NRF_CE_PIN_A, OUTPUT);
  pinMode(NRF_CSN_PIN_A, OUTPUT);
  digitalWrite(NRF_CE_PIN_A, LOW);
  digitalWrite(NRF_CSN_PIN_A, HIGH);

  SPI.begin(SPI_SCK_PIN, SPI_MISO_PIN, SPI_MOSI_PIN);
  SPI.setFrequency(10'000'000);
  SPI.setDataMode(SPI_MODE0);
  SPI.setBitOrder(MSBFIRST);

  // RF24 (usa la instancia para asegurarnos de que encienda bien)
  RadioA.begin();
  // Config base por registro
  nrf_powerUp();
  nrf_writeReg(0x01, 0x00); // EN_AA off
  nrf_writeReg(0x06, 0x03); // RF_SETUP = 1Mbps, PA min (ajustable con RadioA si gustas)
}

// ============================================================================
// SCANNER (historial de nivel tipo oscilograma)
// ============================================================================
namespace Scanner {
  static const int CHANNELS = 64;
  static uint8_t history[128]; // columnas
  static unsigned long lastSel=0;
  static unsigned long lastSave=0;

  void scannerSetup() {
    Serial.begin(115200);
    esp_bt_controller_deinit();
    esp_wifi_stop();
    esp_wifi_deinit();

    memset(history, 0, sizeof(history));
    nrf_commonBegin();

    u8g2.clearBuffer();
    u8g2.setFont(u8g2_font_6x10_tr);
    u8g2.drawStr(2, 30, "Scanner listo");
    u8g2.sendBuffer();
    delay(600);
  }

  void scannerLoop() {
    if (btnPressed(BTN_SELECT_PIN, lastSel)) return;

    // Mide “nivel” máximo entre 64 canales
    int maxv = 0;
    for (int i = 0; i < CHANNELS; i++) {
      nrf_ceLow();
      nrf_setCH((128 * i) / CHANNELS);
      nrf_rxMode();
      delayMicroseconds(60);
      nrf_ceLow();
      if (nrf_carrier()) maxv++;
    }
    // desplaza historial y mete nueva altura mapeada
    for (int i = 127; i > 0; --i) history[i] = history[i-1];
    history[0] = map(maxv, 0, CHANNELS, 0, 63);

    // Dibujo
    u8g2.clearBuffer();
    for (int x = 0; x < 128; x++) {
      uint8_t h = history[x];
      if (h) u8g2.drawVLine(127 - x, 63 - h, h);
    }
    u8g2.setFont(u8g2_font_5x8_tr);
    u8g2.drawStr(2, 8, "Scanner (SELECT para salir)");
    u8g2.sendBuffer();
  }
}

// ============================================================================
// PROTOKILL (selector multi-modo simplificado, una radio)
// ============================================================================
namespace ProtoKill {
  enum Mode { WIFI_M, VIDEO_TX_M, RC_M, BLE_M, BT_M, USBW_M, ZIGBEE_M, NRF24_M, MODES_N };
  static Mode mode = WIFI_M;
  static bool active = false;
  static unsigned long lastUp=0, lastDown=0, lastLeft=0, lastRight=0, lastSel=0;

  // Listas demo de canales (nRF24 escala 0..127)
  const uint8_t wifi_ch[]   = {  2,  8, 16, 26, 32, 40, 48, 56, 64, 72, 84, 96 };
  const uint8_t ble_ch[]    = {  2, 26, 80 };
  const uint8_t bt_ch[]     = { 32, 34, 46, 48, 50, 52, 74, 76, 78, 80 };
  const uint8_t zigbee_ch[] = { 32, 40, 48, 56, 64 };
  const uint8_t rc_ch[]     = {  6, 12, 18, 24 };
  const uint8_t vtx_ch[]    = { 70, 75, 80 };
  const uint8_t nrf_ch[]    = { 76, 78, 79 };

  template<typename T, size_t N>
  uint8_t pick(const T (&arr)[N]) { return arr[random(0, (int)N)]; }

  void blackoutSetup() {
    Serial.begin(115200);
    esp_bt_controller_deinit();
    esp_wifi_stop();
    esp_wifi_deinit();

    nrf_commonBegin();
    setNeoPixelColour("blue");

    u8g2.clearBuffer();
    u8g2.setFont(u8g2_font_6x10_tr);
    u8g2.drawStr(2, 10, "ProtoKill listo");
    u8g2.drawStr(2, 24, "LEFT/RIGHT: modo");
    u8g2.drawStr(2, 36, "UP: activar/desactivar");
    u8g2.drawStr(2, 50, "SELECT: salir");
    u8g2.sendBuffer();
    delay(900);
    setNeoPixelColour("0");
  }

  void draw() {
    u8g2.clearBuffer();
    u8g2.setFont(u8g2_font_6x10_tr);
    u8g2.drawStr(2, 10, "ProtoKill (1 radio)");
    u8g2.drawStr(2, 24, active ? "Estado: ACTIVE" : "Estado: DEACTIVE");

    const char* names[MODES_N] = { "WiFi", "Video TX", "RC", "BLE", "Bluetooth",
                                   "USB Wireless", "Zigbee", "nRF24" };
    u8g2.drawStr(2, 38, "Modo:");
    u8g2.drawStr(40, 38, names[mode]);

    u8g2.drawStr(2, 52, "LEFT/RIGHT modo, UP toggle, SEL exit");
    u8g2.sendBuffer();
  }

  void blackoutLoop() {
    if (btnPressed(BTN_SELECT_PIN, lastSel)) return;

    if (btnPressed(BTN_LEFT_PIN, lastLeft))  { mode = (Mode)((mode + MODES_N - 1) % MODES_N); draw(); }
    if (btnPressed(BTN_RIGHT_PIN, lastRight)){ mode = (Mode)((mode + 1) % MODES_N);          draw(); }
    if (btnPressed(BTN_UP_PIN, lastUp)) {
      active = !active;
      setNeoPixelColour(active ? "red" : "0");
      draw();
    }

    if (!active) return;

    // “actividad” simbólica: saltar entre canales del modo seleccionado
    uint8_t ch = 0;
    switch (mode) {
      case WIFI_M:    ch = pick(wifi_ch);   break;
      case VIDEO_TX_M:ch = pick(vtx_ch);    break;
      case RC_M:      ch = pick(rc_ch);     break;
      case BLE_M:     ch = pick(ble_ch);    break;
      case BT_M:      ch = pick(bt_ch);     break;
      case USBW_M:    ch = pick(wifi_ch);   break;
      case ZIGBEE_M:  ch = pick(zigbee_ch); break;
      case NRF24_M:   ch = pick(nrf_ch);    break;
    }
    nrf_ceLow();
    nrf_setCH(ch);
    nrf_rxMode();
    delay(5);
  }
}

// ============================================================================
// JAMMER (menú simple: canal, PA, data rate, toggle) - 1 radio
// ============================================================================
namespace Jammer {
  static unsigned long lastUp=0, lastDown=0, lastLeft=0, lastRight=0, lastSel=0;
  static int menuIndex=0; // 0: Canal, 1: PA, 2: DataRate, 3: Jamming
  static uint8_t channel = 6; // 1..14 -> mapeado a 0..127
  static uint8_t paIdx = 3;   // 0..3
  static uint8_t drIdx = 1;   // 0..2
  static bool jamming=false;

  void applyParams() {
    switch (paIdx) {
      case 0: RadioA.setPALevel(RF24_PA_MIN);  break;
      case 1: RadioA.setPALevel(RF24_PA_LOW);  break;
      case 2: RadioA.setPALevel(RF24_PA_HIGH); break;
      case 3: RadioA.setPALevel(RF24_PA_MAX);  break;
    }
    switch (drIdx) {
      case 0: RadioA.setDataRate(RF24_250KBPS); break;
      case 1: RadioA.setDataRate(RF24_1MBPS);   break;
      case 2: RadioA.setDataRate(RF24_2MBPS);   break;
    }
  }

  void jammerSetup() {
    Serial.begin(115200);
    esp_bt_controller_deinit();
    esp_wifi_stop();
    esp_wifi_deinit();

    nrf_commonBegin();
    RadioA.stopListening();
    applyParams();

    u8g2.clearBuffer();
    u8g2.setFont(u8g2_font_6x10_tr);
    u8g2.drawStr(2, 10, "Jammer listo");
    u8g2.sendBuffer();
    delay(600);
  }

  void draw() {
    u8g2.clearBuffer();
    u8g2.setFont(u8g2_font_profont11_tf);
    const char* items[4] = { "Channel", "PA Level", "Data Rate", "Jamming" };

    for (int i=0;i<4;i++) {
      int y = (i==0)?12:(i==1)?28:(i==2)?44:60;
      if (menuIndex==i) u8g2.drawStr(0,y,">");
      u8g2.drawStr(10,y,items[i]);
      u8g2.setCursor(80,y);
      if (i==0) {
        u8g2.print((int)channel);
      } else if (i==1) {
        const char* pal[]={"MIN","LOW","HIGH","MAX"};
        u8g2.print(pal[paIdx]);
      } else if (i==2) {
        const char* drs[]={"250K","1M","2M"};
        u8g2.print(drs[drIdx]);
      } else {
        u8g2.print(jamming?"Active":"Off");
      }
    }
    u8g2.sendBuffer();
  }

  void jammerLoop() {
    if (btnPressed(BTN_SELECT_PIN, lastSel)) return;

    if (btnPressed(BTN_UP_PIN, lastUp))   { menuIndex = (menuIndex + 3) % 4; draw(); }
    if (btnPressed(BTN_DOWN_PIN, lastDown)){ menuIndex = (menuIndex + 1) % 4; draw(); }

    if (btnPressed(BTN_RIGHT_PIN, lastRight)) {
      if (menuIndex==0) { channel = (channel % 14) + 1; }
      else if (menuIndex==1) { paIdx = (paIdx + 1) % 4; applyParams(); }
      else if (menuIndex==2) { drIdx = (drIdx + 1) % 3; applyParams(); }
      else if (menuIndex==3) { jamming = !jamming; setNeoPixelColour(jamming?"red":"0"); }
      draw();
    }
    if (btnPressed(BTN_LEFT_PIN, lastLeft)) {
      if (menuIndex==0) { channel = (channel + 12) % 14 + 1; }
      else if (menuIndex==1) { paIdx = (paIdx + 3) % 4; applyParams(); }
      else if (menuIndex==2) { drIdx = (drIdx + 2) % 3; applyParams(); }
      else if (menuIndex==3) { jamming = !jamming; setNeoPixelColour(jamming?"red":"0"); }
      draw();
    }

    if (jamming) {
      // Patrón placeholder (NO transmite paquetes válidos; evita uso indebido)
      uint8_t dummy[16] = {0xAA,0x55,0xAA,0x55,0xAA,0x55,0xAA,0x55,0xAA,0x55,0xAA,0x55,0xAA,0x55,0xAA,0x55};
      uint8_t mapped = map(channel, 1, 14, 2, 96); // aproximado al rango 0..127
      RadioA.setChannel(mapped);
      RadioA.write(&dummy, sizeof(dummy)); // requerirá módulo con PA estable y buen 3V3 + cap
      delay(2);
    }
  }
}
