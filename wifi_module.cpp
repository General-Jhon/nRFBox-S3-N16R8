// wifi_module.cpp (sustituir el contenido del namespace WifiScan por esto)

#include "wifi_module.h"

extern void header(const char* title);
extern void drawMainMenu();
extern bool btnPressed(int pin);
extern bool inModule;
extern U8G2_SSD1306_128X64_NONAME_F_HW_I2C oled;

namespace WifiScan {

static int networkCount = 0;
static int scrollIndex = 0;       // índice del primer elemento visible
static int cursor = 0;            // posición dentro de la ventana visible (0..5)
static int detailIndex = -1;      // índice absoluto de la red seleccionada para detalles
static unsigned long lastScan = 0;
static bool scanning = false;
static bool inDetails = false;    // si estamos viendo la pantalla de detalles



// Helper para traducir tipo de encriptación
String getEncryptionType(wifi_auth_mode_t type) {
  switch (type) {
    case WIFI_AUTH_OPEN: return "OPEN";
    case WIFI_AUTH_WEP: return "WEP";
    case WIFI_AUTH_WPA_PSK: return "WPA";
    case WIFI_AUTH_WPA2_PSK: return "WPA2";
    case WIFI_AUTH_WPA_WPA2_PSK: return "WPA/WPA2";
    default: return "?";
  }
}


// Parpadeo del NeoPixel mientras escanea (no bloquear demasiado)
void blinkNeoPixel(uint32_t color, int ms) {
  pixels.setPixelColor(0, color);
  pixels.show();
  delay(ms);
  pixels.clear();
  pixels.show();
  delay(ms);
}

void wifiscanSetup() {
  // 🧹 Reinicia estados del módulo WiFi Scan
  inDetails = false;
  detailIndex = -1;
  scrollIndex = 0;
  cursor = 0;
  scanning = false;

  header("WiFi Scan");
  oled.drawStr(2, 22, "Iniciando...");
  oled.sendBuffer();
  header("WiFi Scan");
  oled.drawStr(2, 22, "Iniciando...");
  oled.sendBuffer();

  WiFi.mode(WIFI_STA);
  WiFi.disconnect(true);
  delay(200);

  // 🔆 Configura brillo suave global del NeoPixel
  pixels.setBrightness(25);           // Brillo 0–255 → 25 = tenue (~10%)
  pixels.clear();
  pixels.show();

  scanning = true;
  pixels.setPixelColor(0, pixels.Color(0, 0, 255)); // Azul inicial tenue
  pixels.show();

  networkCount = WiFi.scanNetworks(true); // escaneo no bloqueante
}


static void drawList() {
  oled.clearBuffer();
  // Línea superior con columnas técnicas
  oled.setFont(u8g2_font_5x8_tr);
  oled.drawStr(2, 12, "SSID       RSSI   CH");
  // Mostrar hasta 6 entradas desde scrollIndex
  int visible = min(6, max(0, networkCount - scrollIndex));
  for (int i = 0; i < visible; i++) {
    int idx = scrollIndex + i;
    String ssid = WiFi.SSID(idx);
    int32_t rssi = WiFi.RSSI(idx);
    int32_t chan = WiFi.channel(idx);

    char line[40];
    snprintf(line, sizeof(line), "%-10s %3ddB CH%02d",
             ssid.substring(0, 10).c_str(), rssi, chan);

    int y = 22 + i * 8;

    // resaltado del cursor
    if (i == cursor) {
      oled.drawBox(0, y-7, 128, 8);
      oled.setDrawColor(0);
      oled.drawStr(2, y, line);
      oled.setDrawColor(1);
    } else {
      oled.drawStr(2, y, line);
    }

    // barra de intensidad
    int bars = constrain(map(rssi, -90, -30, 0, 5), 0, 5);
    for (int b = 0; b < bars; b++) {
      oled.drawBox(110 + b * 3, y-6, 2, 6);
    }
  }

  oled.sendBuffer();
}

static void drawDetails(int idx) {
  oled.clearBuffer();
  oled.setFont(u8g2_font_6x10_tr);
  oled.drawStr(2, 10, "Detalles red:");
  oled.setFont(u8g2_font_5x8_tr);

  String ssid = WiFi.SSID(idx);
  String bssid = WiFi.BSSIDstr(idx);
  int32_t rssi = WiFi.RSSI(idx);
  int chan = WiFi.channel(idx);
  String enc = getEncryptionType(WiFi.encryptionType(idx));

  // SSID (completo, recortamos si es muy largo)
  oled.drawStr(2, 22, "SSID:");
  oled.drawStr(36, 22, ssid.substring(0, 20).c_str());

  // BSSID
  oled.drawStr(2, 34, "BSSID:");
  oled.drawStr(36, 34, bssid.substring(0, 17).c_str()); // A4:F1:...

  // RSSI / Canal / encryption
  char line[40];
  snprintf(line, sizeof(line), "RSSI: %ddB  CH:%d", rssi, chan);
  oled.drawStr(2, 46, line);

  snprintf(line, sizeof(line), "Cifr: %s", enc.c_str());
  oled.drawStr(2, 58, line);

  oled.sendBuffer();
}


void wifiscanLoop() {
  // BACK general: LEFT desde cualquier pantalla vuelve al menú
  if (digitalRead(BTN_LEFT_PIN) == LOW) {
    WiFi.scanDelete();
    WiFi.disconnect(true);
    pixels.clear();
    pixels.show();
    scanning = false;
    scrollIndex = 0;
    cursor = 0;
    detailIndex = -1;
    inDetails = false;
    inModule = false;
    drawMainMenu();
    delay(250);
    return;
  }

  // Mientras se escanea (no bloqueante), animamos y comprobamos resultado
  if (scanning) {
    header("WiFi Scan");
    oled.clearBuffer();
    oled.setFont(u8g2_font_6x10_tr);
    oled.drawStr(2, 10, "Escaneando...");
    // puntos animados
    static uint8_t dots = 0;
    oled.setFont(u8g2_font_5x8_tr);
    char s[5] = {'.', '.', '.', '\0'};
    oled.drawStr(100, 10, s + (dots++ % 3));
    oled.sendBuffer();

    // parpadeo NeoPixel (sin bloquear mucho)
    blinkNeoPixel(pixels.Color(0, 0, 80), 80);

    int16_t result = WiFi.scanComplete();
    if (result >= 0) {
      scanning = false;
      networkCount = result;
      // si no hay redes, no hacemos nada más
      if (networkCount <= 0) {
        scrollIndex = 0;
        cursor = 0;
      }
    }
    return;
  }

  // Si estamos viendo detalles
  if (inDetails) {
    // LEFT vuelve a la lista
    if (digitalRead(BTN_LEFT_PIN) == LOW) {
      inDetails = false;
      delay(200);
      drawList();
      return;
    }
    // SELECT para reescaneo desde detalles
    if (digitalRead(BTN_SELECT_PIN) == LOW) {
      WiFi.scanDelete();
      wifiscanSetup();
      delay(250);
      return;
    }
    // RIGHT y UP/DOWN no hacen nada aquí (podrías usarlos para otras acciones)
    drawDetails(detailIndex);
    return;
  }

  // Lista: navegación con btnPressed (usa tu función de debounce)
  if (btnPressed(BTN_DOWN_PIN)) {
    // si cursor llega al final de la ventana y hay más -> desplazar ventana
    if (cursor < 5 && (scrollIndex + cursor) < networkCount - 1) {
      cursor++;
    } else if (scrollIndex + 6 < networkCount) {
      scrollIndex++;
    }
    drawList();
    return;
  }
  if (btnPressed(BTN_UP_PIN)) {
    if (cursor > 0) {
      cursor--;
    } else if (scrollIndex > 0) {
      scrollIndex--;
    }
    drawList();
    return;
  }

  // RIGHT = ver detalles de la red seleccionada
  if (btnPressed(BTN_RIGHT_PIN)) {
    int absolute = scrollIndex + cursor;
    if (absolute < networkCount) {
      detailIndex = absolute;
      inDetails = true;
      drawDetails(detailIndex);
    }
    delay(150);
    return;
  }

  // SELECT = reescanear
  if (btnPressed(BTN_SELECT_PIN)) {
    WiFi.scanDelete();
    wifiscanSetup();
    delay(250);
    return;
  }

  // Si no hubo botones, redibuja la lista (útil después del escaneo)
  drawList();
}

} // namespace WifiScan

