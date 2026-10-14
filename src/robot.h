/**
 * BeCre8v Robocoders protocol: pairing handshake, pin commands, telemetry and
 * the safety failsafe. One command parser serves both Serial (USB) and a
 * WebSocket on port 81 (WiFi).
 */
#pragma once
#include <Arduino.h>

enum RobotLink
{
  LINK_USB,
  LINK_WIFI
};

// load/create the pairing code and start the WebSocket server
void robotBegin();

// call every loop(): WebSocket, failsafe, telemetry, address printing
void robotUpdate();

// Feed one trimmed line. Returns true if it was a protocol line (even a
// malformed one), so the caller should not treat it as a text command.
bool robotHandleLine(const String &line, RobotLink link);

// STOP semantics, used before an OTA update starts
void robotStopAll();
