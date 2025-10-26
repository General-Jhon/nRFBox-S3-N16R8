#ifndef BLE_JAMMER_MODULE_H
#define BLE_JAMMER_MODULE_H

#include <Arduino.h>
#include <U8g2lib.h>
#include <Adafruit_NeoPixel.h>
#include <RF24.h>

namespace BleJammer {
  void bleJammerSetup();
  void bleJammerLoop();
}

#endif
