/**
 * Onboard BOOT button (GPIO 0): one press toggles the LED.
 */
#pragma once

void buttonBegin();

// call every loop() to check for presses
void buttonUpdate();
