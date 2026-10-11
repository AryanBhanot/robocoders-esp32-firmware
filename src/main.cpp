/**
 * Serial LED control
 *
 * Type "on" or "off" in the Serial Monitor to control
 * the external LED on pin 9.
 */
#include <Arduino.h>

// External LED connected to GPIO 9
const int LED_PIN = 9;

void setup()
{
  // start serial communication so we can talk to the Serial Monitor
  Serial.begin(115200);

  // initialize LED digital pin as an output.
  pinMode(LED_PIN, OUTPUT);

  Serial.println("Hello! Board started.");
  Serial.println("Type 'on' or 'off' and press Enter.");
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
      digitalWrite(LED_PIN, HIGH);
      Serial.println("LED ON");
    }
    else if (command == "off")
    {
      digitalWrite(LED_PIN, LOW);
      Serial.println("LED OFF");
    }
    else if (command.length() > 0)
    {
      Serial.print("Unknown command: ");
      Serial.println(command);
      Serial.println("Type 'on' or 'off'.");
    }
  }
}
