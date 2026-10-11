/**
 * LED control: PWM brightness, on/off, toggle and a non-blocking fade.
 * Everything else (Serial, web page, button) goes through these functions.
 */
#pragma once

// set up PWM on the LED pin, call once from setup()
void ledBegin();

// call every loop() to advance the fade
void ledUpdate();

void ledOn();
void ledOff();
void ledFade();
void ledSetLevel(int value); // 0-255, stops any fade
void ledToggle();            // off if lit or fading, otherwise on

int ledBrightness();
bool ledIsFading();
