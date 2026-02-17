#include "bluetooth_module.h"
#include "config.h"
#include <RF24.h>
#include <SPI.h>

extern U8G2_SSD1306_128X64_NONAME_F_HW_I2C oled;
extern Adafruit_NeoPixel pixels;
extern void header(const char* title);

namespace BleJammer {

// Definiciones de pines y canales
#define CE_PIN_1  5
#define CSN_PIN_1 21

#define CE_PIN_2  15
#define CSN_PIN_2 7

#define MODE_BUTTON 9

class MyRF24 : public RF24 {
public:
    MyRF24(uint8_t ce_pin, uint8_t csn_pin, uint32_t speed = 0) : RF24(ce_pin, csn_pin, speed) {}
    uint8_t getStatus() {
        return read_register(NRF_STATUS);
    }
};

MyRF24 radio1(CE_PIN_1, CSN_PIN_1, 16000000);
MyRF24 radio2(CE_PIN_2, CSN_PIN_2, 16000000);

enum OperationMode { DEACTIVE_MODE, BLE_MODULE, Bluetooth_MODULE };
OperationMode currentMode = DEACTIVE_MODE;

int bluetooth_channels[] = {32, 34, 46, 48, 50, 52, 0, 1, 2, 4, 6, 8, 22, 24, 26, 28, 30, 74, 76, 78, 80};
int ble_channels[] = {2, 26, 80};

const byte BLE_channels[] = {2, 26, 80};
byte channelGroup1[] = {2, 5, 8, 11};
byte channelGroup2[] = {26, 29, 32, 35};

volatile bool modeChangeRequested = false;

unsigned long lastJammingTime = 0;
const unsigned long jammingInterval = 10;

unsigned long lastButtonPressTime = 0;
const unsigned long debounceDelay = 500;

void IRAM_ATTR handleButtonPress() {
  unsigned long currentTime = millis();
  if (currentTime - lastButtonPressTime > debounceDelay) {
    modeChangeRequested = true;
    lastButtonPressTime = currentTime;
  }
}

void configureRadio(MyRF24 &radio) {
  radio.setAutoAck(false);
  radio.stopListening();
  radio.setRetries(0, 0);
  radio.setPALevel(RF24_PA_MAX, true);
  radio.setDataRate(RF24_2MBPS);
  radio.setCRCLength(RF24_CRC_DISABLED);
  radio.setAddressWidth(5);
  radio.setPayloadSize(32);
  radio.openWritingPipe(0xE7E7E7E7E7LL);
  radio.openReadingPipe(1, 0xC2C2C2C2C2LL);
  radio.printPrettyDetails();
}

void initializeRadiosMultiMode() {
  if (radio1.begin()) {
    configureRadio(radio1);
  }
  if (radio2.begin()) {
    configureRadio(radio2);
  }
}

void initializeRadios() {
  if (currentMode == !DEACTIVE_MODE) {
    initializeRadiosMultiMode();
  } else if (currentMode == DEACTIVE_MODE) {
    radio1.powerDown();
    radio2.powerDown();
    delay(100);
  }
}

void jammer(MyRF24 &radio, const byte* channels, size_t size) {
  const char text[] = "xxxxxxxxxxxxxxxx";
  for (size_t i = 0; i < size; i++) {
    radio.setChannel(channels[i]);
    if (radio.isChipConnected()) {
      bool success = radio.write(&text, sizeof(text));
      if (success) {
        Serial.print("Paquete enviado en canal ");
        Serial.println(channels[i]);
      } else {
        Serial.print("Error al enviar paquete en canal ");
        Serial.println(channels[i]);
        Serial.print("Estado de la radio: ");
        Serial.println(radio.getStatus(), HEX);
        Serial.print("Registro de estado: ");
        Serial.println(radio.getStatus(), BIN);
      }
    } else {
      Serial.print("Radio no conectada en canal ");
      Serial.println(channels[i]);
    }
  }
}

void updateOLED() {
  oled.clearBuffer();
  oled.setFont(u8g2_font_ncenB08_tr);

  oled.setCursor(0, 10);
  oled.print("Mode ");
  oled.print(" ....... ");
  oled.setCursor(65, 10);
  oled.print("[");
  oled.print(currentMode == BLE_MODULE ? "BLE" : currentMode == Bluetooth_MODULE ? "Bluetooth" : "Deactive");
  oled.print("]");

  oled.setCursor(0, 35);
  oled.print("Radio 1: ");
  oled.setCursor(70, 35);
  oled.print(radio1.isChipConnected() ? "Active" : "Inactive");

  oled.setCursor(0, 50);
  oled.print("Radio 2: ");
  oled.setCursor(70, 50);
  oled.print(radio2.isChipConnected() ? "Active" : "Inactive");

  oled.sendBuffer();
}

void checkModeChange() {
  if (modeChangeRequested) {
    modeChangeRequested = false;
    currentMode = static_cast<OperationMode>((currentMode + 1) % 3);
    initializeRadios();
    updateOLED();
  }
}

void bleJammerSetup() {
  header("BLE Jammer");
  oled.drawStr(8, 28, "Inicializando antenas...");
  oled.sendBuffer();

  SPI.begin(40, 42, 41); // SCK, MISO, MOSI
  delay(10);

  pinMode(MODE_BUTTON, INPUT_PULLUP);
  attachInterrupt(digitalPinToInterrupt(MODE_BUTTON), handleButtonPress, FALLING);

  Serial.begin(115200); // Inicializar el monitor serie
  initializeRadios();
  updateOLED();
}

void bleJammerLoop() {
  checkModeChange();

  if (currentMode == BLE_MODULE) {
    int randomIndex = random(0, sizeof(ble_channels) / sizeof(ble_channels[0]));
    byte channel = ble_channels[randomIndex];
    radio1.setChannel(channel);
    radio2.setChannel(channel);
    jammer(radio1, &channel, 1);
    jammer(radio2, &channel, 1);
  } else if (currentMode == Bluetooth_MODULE) {
    int randomIndex = random(0, sizeof(bluetooth_channels) / sizeof(bluetooth_channels[0]));
    byte channel = bluetooth_channels[randomIndex];
    radio1.setChannel(channel);
    radio2.setChannel(channel);
    jammer(radio1, &channel, 1);
    jammer(radio2, &channel, 1);
  }
}

} // namespace BleJammer