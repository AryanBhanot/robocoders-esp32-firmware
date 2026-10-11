#include <Arduino.h>
#include "button.h"
#include "led.h"

// BOOT button reads LOW when pressed
static const int BUTTON_PIN = 0;
static const unsigned long DEBOUNCE_MS = 50;

static bool lastReading = HIGH;
static bool buttonState = HIGH;
static unsigned long lastChange = 0;

void buttonBegin()
{
  // BOOT button has an external pull-up, but enable the internal one too
  pinMode(BUTTON_PIN, INPUT_PULLUP);
}

void buttonUpdate()
{
  bool reading = digitalRead(BUTTON_PIN);

  if (reading != lastReading)
  {
    lastChange = millis();
    lastReading = reading;
  }

  // debounced: only act once the reading has been stable for DEBOUNCE_MS
  if (millis() - lastChange >= DEBOUNCE_MS && reading != buttonState)
  {
    buttonState = reading;
    if (buttonState == LOW)
    {
      ledToggle();
      Serial.println(ledBrightness() > 0 ? "LED ON (button)" : "LED OFF (button)");
    }
  }
}
