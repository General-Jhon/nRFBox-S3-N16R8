#include "setting.h"
#include "config.h"
#include "neopixel.h"

#include <Adafruit_NeoPixel.h>
#include <EEPROM.h>

extern Adafruit_NeoPixel pixels;  // ✅ Solo referencia, no redefinir

void neopixelSetup()
{
  EEPROM.begin(512);
  neoPixelActive = EEPROM.read(0);

  if (neoPixelActive)
  {
    pixels.begin();
    pixels.clear();
    pixels.show();
  }
}

void setNeoPixelColour(const std::string &colour)
{
  uint32_t colorValue = 0;

  if (colour == "red")
    colorValue = pixels.Color(5, 0, 0);
  else if (colour == "green")
    colorValue = pixels.Color(0, 5, 0);
  else if (colour == "blue")
    colorValue = pixels.Color(0, 0, 5);
  else if (colour == "yellow")
    colorValue = pixels.Color(5, 5, 0);
  else if (colour == "purple")
    colorValue = pixels.Color(5, 0, 5);
  else if (colour == "cyan")
    colorValue = pixels.Color(0, 5, 5);
  else if (colour == "white")
    colorValue = pixels.Color(5, 5, 5);
  else
    colorValue = pixels.Color(0, 0, 0);

  pixels.setPixelColor(0, colorValue);
  pixels.show();
}

void flash(int numberOfFlashes, const std::vector<std::string> &colors, const std::string &finalColour)
{
  if (numberOfFlashes <= 0 || colors.empty())
  {
    Serial.println("⚠️ Invalid flash parameters");
    return;
  }

  for (int i = 0; i < numberOfFlashes; ++i)
  {
    for (const auto &color : colors)
    {
      setNeoPixelColour(color);
      delay(300);
    }
  }
  setNeoPixelColour(finalColour);
}
