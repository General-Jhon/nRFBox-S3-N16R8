#ifndef BLUETOOTH_MODULE_H
#define BLUETOOTH_MODULE_H

#include <Arduino.h>
#include <U8g2lib.h>
#include <BLEDevice.h>
#include <Adafruit_NeoPixel.h>

/*
 * Módulo Bluetooth (BLE) profesional
 * ----------------------------------
 * Permite tres modos:
 *  - BLE Scan: escaneo activo con RSSI, nombre y tipo.
 *  - BLE Analyzer (demo): placeholder para análisis de canal y potencia.
 *  - Beacon Detector (demo): placeholder para detección de beacons BLE.
 * 
 * NeoPixel:
 *  🔵 Azul → Escaneando
 *  🟢 Verde → Dispositivos detectados
 *  🔴 Rojo → Sin dispositivos / error
 */

namespace BluetoothModule {
  void bleScanSetup();
  void bleScanLoop();

  void bleAnalyzerSetup();
  void bleAnalyzerLoop();

  void beaconSetup();
  void beaconLoop();

  void bleStop();
}

#endif
