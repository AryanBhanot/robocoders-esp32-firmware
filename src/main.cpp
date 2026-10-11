/**
 * Blink
 *
 * Turns on an external LED on pin 9 for two seconds,
 * then off for two seconds, repeatedly.
 */
#include <Arduino.h>

// External LED connected to GPIO 9
const int LED_PIN = 9;

void setup()
{
  // start serial communication so we can print messages to the Serial Monitor
  Serial.begin(115200);

  // initialize LED digital pin as an output.
  pinMode(LED_PIN, OUTPUT);

  Serial.println("Hello! Board started.");
}

void loop()
{
  // turn the LED on (HIGH is the voltage level)
  digitalWrite(LED_PIN, HIGH);
  Serial.println("LED ON");

  // wait for two seconds
  delay(2000);

  // turn the LED off by making the voltage LOW
  digitalWrite(LED_PIN, LOW);
  Serial.println("LED OFF");

  // wait for two seconds
  delay(2000);
}
