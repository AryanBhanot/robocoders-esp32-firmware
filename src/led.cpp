#include <Arduino.h>
#include "led.h"

// External LED connected to GPIO 9
static const int LED_PIN = 9;

// PWM settings (8-bit: brightness 0-255)
static const int PWM_FREQ = 5000;
static const int PWM_RESOLUTION = 8;
static const int PWM_MAX = 255;

// Fade settings
static const int FADE_STEP = 5;                // brightness change per update
static const unsigned long FADE_DELAY_MS = 20; // time between updates

static bool fading = false;
static int brightness = 0;
static int fadeDirection = 1; // 1 = getting brighter, -1 = getting dimmer
static unsigned long lastFadeUpdate = 0;

// write brightness (0-255) to the pin, works with Arduino-ESP32 core 2.x and 3.x
static void writeBrightness(int value)
{
#if ESP_ARDUINO_VERSION_MAJOR >= 3
  ledcWrite(LED_PIN, value);
#else
  ledcWrite(0, value);
#endif
}

void ledBegin()
{
#if ESP_ARDUINO_VERSION_MAJOR >= 3
  ledcAttach(LED_PIN, PWM_FREQ, PWM_RESOLUTION);
#else
  ledcSetup(0, PWM_FREQ, PWM_RESOLUTION);
  ledcAttachPin(LED_PIN, 0);
#endif
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
