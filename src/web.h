/**
 * WiFi connection, the .local address (mDNS) and the web page / API.
 * WiFi name and password come from secrets.h.
 */
#pragma once

// connect to WiFi, advertise the address and start the web server
void webBegin();

// call every loop() to answer browser requests
void webUpdate();

// the mDNS / OTA / robot name, e.g. "ledboard"
const char *webHostname();
