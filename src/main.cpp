/**
 * LED control over Serial, WiFi and the BOOT button
 *
 * Serial Monitor: type "on", "off" or "fade".
 * Web page: open http://ledboard.local (or the IP printed at startup)
 * from a device on the same WiFi network.
 * Button: press BOOT to toggle the LED.
 *
 * Put your WiFi name/password in src/secrets.h (see secrets.h.example).
 */
#include <Arduino.h>
#include "led.h"
#include "button.h"
#include "web.h"

// handle one typed line from the Serial Monitor
static void handleSerialCommand()
{
  if (Serial.available() <= 0)
  {
    return;
  }

  // read the whole line and clean it up (remove spaces/newlines, make lowercase)
  String command = Serial.readStringUntil('\n');
  command.trim();
  command.toLowerCase();

  if (command == "on")
  {
    ledOn();
    Serial.println("LED ON");
  }
  else if (command == "off")
  {
    ledOff();
    Serial.println("LED OFF");
  }
  else if (command == "fade")
  {
    ledFade();
    Serial.println("LED FADING (type 'on' or 'off' to stop)");
  }
  else if (command.length() > 0)
  {
    Serial.print("Unknown command: ");
    Serial.println(command);
    Serial.println("Type 'on', 'off' or 'fade'.");
  }
}

void setup()
{
  // start serial communication so we can talk to the Serial Monitor
  Serial.begin(115200);

  ledBegin();
  buttonBegin();

  Serial.println("Hello! Board started.");
  webBegin();
  Serial.println("Type 'on', 'off' or 'fade' and press Enter, or press the BOOT button to toggle.");
}

void loop()
{
  webUpdate();
  buttonUpdate();
  handleSerialCommand();
  ledUpdate();
}
