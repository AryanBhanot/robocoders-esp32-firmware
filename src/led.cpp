#include <Arduino.h>
#include "led.h"

// Onboard WS2812 RGB LED (GPIO 48 on DevKitC-1 v1.0, GPIO 38 on v1.1).
// RGB_BUILTIN comes from the board definition; fall back to 48.
#ifdef RGB_BUILTIN
static const int LED_PIN = RGB_BUILTIN;
#else
static const int LED_PIN = 48;
#endif

// brightness range 0-255
static const int PWM_MAX = 255;

// Fade settings
static const int FADE_STEP = 5;                // brightness change per update
static const unsigned long FADE_DELAY_MS = 20; // time between updates

static bool fading = false;
static int brightness = 0;
static int fadeDirection = 1; // 1 = getting brighter, -1 = getting dimmer
static unsigned long lastFadeUpdate = 0;

// write brightness (0-255) to the onboard RGB LED as white
static void writeBrightness(int value)
{
  neopixelWrite(LED_PIN, value, value, value);
}

void ledBegin()
{
  writeBrightness(0);
}

void ledUpdate()
{
  // step the brightness up and down between 0 and 255
  if (!fading || millis() - lastFadeUpdate < FADE_DELAY_MS)
  {
    return;
  }
  lastFadeUpdate = millis();

  brightness += fadeDirection * FADE_STEP;
  if (brightness >= PWM_MAX)
  {
    brightness = PWM_MAX;
    fadeDirection = -1;
  }
  else if (brightness <= 0)
  {
    brightness = 0;
    fadeDirection = 1;
  }

  writeBrightness(brightness);
}

void ledOn()
{
  ledSetLevel(PWM_MAX);
}

void ledOff()
{
  ledSetLevel(0);
}

void ledFade()
{
  fading = true;
}

void ledSetLevel(int value)
{
  fading = false;
  brightness = constrain(value, 0, PWM_MAX);
  writeBrightness(brightness);
}

void ledToggle()
{
  if (fading || brightness > 0)
  {
    ledOff();
  }
  else
  {
    ledOn();
  }
}

int ledBrightness()
{
  return brightness;
}

bool ledIsFading()
{
  return fading;
}
