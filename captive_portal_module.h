#ifndef CAPTIVE_PORTAL_MODULE_H
#define CAPTIVE_PORTAL_MODULE_H

#include <Arduino.h>

// =============================================================
//  Captive Portal Module - nRFBox S3 by General_Jhon
//  Escanea redes, permite seleccionar una y crear un portal cautivo
//  con el mismo SSID. Guarda logs en SPIFFS y muestra último registro
//  en OLED.
// =============================================================

namespace CaptivePortal {

  // --- Inicialización del módulo ---
  void setup();

  // --- Bucle principal del módulo ---
  void loop();

  // --- Iniciar portal cautivo manualmente (usado desde menú principal) ---
  void startCaptivePortalWithSSID(const String &ssid);

  // --- Control interno del portal (DNS + servidor web) ---
  void captivePortalLoop();

  // --- Guardar log manualmente (por si otros módulos lo usan) ---
  void appendLog(const String &line);

  // --- Refrescar OLED con último registro ---
  void updateOLED();

  void stop();

} // namespace CaptivePortal

#endif // CAPTIVE_PORTAL_MODULE_H
