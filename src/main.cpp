/**
 * Serial LED control
 *
 * Type "on", "off" or "fade" in the Serial Monitor to control
 * the external LED on pin 9.
 */
#include <Arduino.h>

// External LED connected to GPIO 9
const int LED_PIN = 9;

// PWM settings (8-bit: brightness 0-255)
const int PWM_FREQ = 5000;
const int PWM_RESOLUTION = 8;
const int PWM_MAX = 255;

// Fade settings
const int FADE_STEP = 5;                // brightness change per update
const unsigned long FADE_DELAY_MS = 20; // time between updates

bool fading = false;
int brightness = 0;
int fadeDirection = 1; // 1 = getting brighter, -1 = getting dimmer
unsigned long lastFadeUpdate = 0;

// set LED brightness (0-255), works with both Arduino-ESP32 core 2.x and 3.x
void setBrightness(int value)
{
#if ESP_ARDUINO_VERSION_MAJOR >= 3
  ledcWrite(LED_PIN, value);
#else
  ledcWrite(0, value);
#endif
}

void setup()
{
  // start serial communication so we can talk to the Serial Monitor
  Serial.begin(115200);

  // set up PWM on the LED pin so we can dim it
#if ESP_ARDUINO_VERSION_MAJOR >= 3
  ledcAttach(LED_PIN, PWM_FREQ, PWM_RESOLUTION);
#else
  ledcSetup(0, PWM_FREQ, PWM_RESOLUTION);
  ledcAttachPin(LED_PIN, 0);
#endif

  Serial.println("Hello! Board started.");
  Serial.println("Type 'on', 'off' or 'fade' and press Enter.");
}

void loop()
{
  // only do something when the user has typed something
  if (Serial.available() > 0)
  {
    // read the whole line and clean it up (remove spaces/newlines, make lowercase)
    String command = Serial.readStringUntil('\n');
    command.trim();
    command.toLowerCase();

    if (command == "on")
    {
      fading = false;
      brightness = PWM_MAX;
      setBrightness(brightness);
      Serial.println("LED ON");
    }
    else if (command == "off")
    {
      fading = false;
      brightness = 0;
      setBrightness(brightness);
      Serial.println("LED OFF");
    }
    else if (command == "fade")
    {
      fading = true;
      Serial.println("LED FADING (type 'on' or 'off' to stop)");
    }
    else if (command.length() > 0)
    {
      Serial.print("Unknown command: ");
      Serial.println(command);
      Serial.println("Type 'on', 'off' or 'fade'.");
    }
  }

  // non-blocking fade: step the brightness up and down between 0 and 255
  if (fading && millis() - lastFadeUpdate >= FADE_DELAY_MS)
  {
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

    setBrightness(brightness);
  }
}
