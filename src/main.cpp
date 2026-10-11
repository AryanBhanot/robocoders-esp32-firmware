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
  // initialize LED digital pin as an output.
  pinMode(LED_PIN, OUTPUT);
}

void loop()
{
  // turn the LED on (HIGH is the voltage level)
  digitalWrite(LED_PIN, HIGH);

  // wait for two seconds
  delay(2000);

  // turn the LED off by making the voltage LOW
  digitalWrite(LED_PIN, LOW);

  // wait for two seconds
  delay(2000);
}
