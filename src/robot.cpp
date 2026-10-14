#include <Arduino.h>
#include <WiFi.h>
#include <Preferences.h>
#include <WebSocketsServer.h>
#include "robot.h"
#include "led.h"
#include "web.h"

static const uint16_t WS_PORT = 81;
static const size_t MAX_LINE = 64;
static const unsigned long UNPAIRED_DROP_MS = 10000;
static const unsigned long FAILSAFE_MS = 3000;
static const unsigned long TELEMETRY_MS = 50;
static const int MAX_PIN = 48;

static WebSocketsServer ws(WS_PORT);
static Preferences prefs;
static String pairingCode;

// ---- link state ---------------------------------------------------------

static bool usbPaired = false;
static int wsClient = -1; // websocket slot of the controlling client
static bool wsPaired = false;
static unsigned long wsConnectedAt = 0;
static unsigned long lastLineMs = 0;
static bool failsafeTripped = false;

// ---- pin state ----------------------------------------------------------

enum PinMode_
{
  PIN_NONE,
  PIN_DW,
  PIN_PWM,
  PIN_SERVO,
  PIN_WATCH_D,
  PIN_WATCH_A
};

static PinMode_ pinMode_[MAX_PIN + 1];
static int pinValue[MAX_PIN + 1]; // last applied value, -1 = unknown

// Arduino core 2.x LEDC: 8 channels, each pin needs one
static const int LEDC_CHANNELS = 8;
static int8_t pinChannel[MAX_PIN + 1];
static bool channelUsed[LEDC_CHANNELS];

static bool ledcAcquire(int pin, uint32_t freq, uint8_t bits)
{
  for (int ch = 0; ch < LEDC_CHANNELS; ch++)
  {
    if (!channelUsed[ch])
    {
      channelUsed[ch] = true;
      pinChannel[pin] = ch;
      ledcSetup(ch, freq, bits);
      ledcAttachPin(pin, ch);
      return true;
    }
  }
  return false;
}

static void ledcRelease(int pin)
{
  if (pinChannel[pin] >= 0)
  {
    ledcDetachPin(pin);
    channelUsed[pinChannel[pin]] = false;
    pinChannel[pin] = -1;
  }
}

// Pins the robot must never drive: BOOT button, USB, flash/PSRAM, UART0,
// strapping pins and the onboard RGB LED.
static bool pinAllowed(int pin)
{
  if (pin < 0 || pin > MAX_PIN)
    return false;
  if (pin == 0 || pin == 19 || pin == 20 || pin == 45 || pin == 46 || pin == 48)
    return false;
  if (pin >= 26 && pin <= 37)
    return false;
  if (pin == 43 || pin == 44)
    return false;
  return true;
}

// ADC1 only (GPIO 1-10); ADC2 does not work reliably while WiFi is on
static bool pinHasAdc(int pin)
{
  return pin >= 1 && pin <= 10;
}

// give a pin up its current job so it can take a new one
static void releasePin(int pin)
{
  if (pinMode_[pin] == PIN_PWM || pinMode_[pin] == PIN_SERVO)
  {
    ledcRelease(pin);
  }
  pinMode_[pin] = PIN_NONE;
  pinValue[pin] = -1;
}

static void stopOutputs()
{
  for (int pin = 0; pin <= MAX_PIN; pin++)
  {
    if (pinMode_[pin] == PIN_DW && pinValue[pin] != 0)
    {
      digitalWrite(pin, LOW);
      pinValue[pin] = 0;
    }
    else if (pinMode_[pin] == PIN_PWM && pinValue[pin] != 0)
    {
      ledcWrite(pinChannel[pin], 0);
      pinValue[pin] = 0;
    }
  }
}

static void clearWatches()
{
  for (int pin = 0; pin <= MAX_PIN; pin++)
  {
    if (pinMode_[pin] == PIN_WATCH_D || pinMode_[pin] == PIN_WATCH_A)
    {
      pinMode_[pin] = PIN_NONE;
      pinValue[pin] = -1;
    }
  }
}

// ---- pairing blink (non-blocking): white twice, then restore the LED ----

static const unsigned long BLINK_STEP_MS = 120;
static int blinkStep = -1; // -1 = idle, 0..3 = on/off/on/off
static unsigned long blinkAt = 0;
static int blinkRestore = 0;

static void blinkStart()
{
  blinkRestore = ledBrightness();
  blinkStep = 0;
  blinkAt = millis();
  ledSetLevel(255);
}

static void blinkUpdate()
{
  if (blinkStep < 0 || millis() - blinkAt < BLINK_STEP_MS)
    return;
  blinkAt = millis();
  blinkStep++;
  if (blinkStep >= 4)
  {
    ledSetLevel(blinkRestore);
    blinkStep = -1;
  }
  else
  {
    ledSetLevel(blinkStep % 2 == 0 ? 255 : 0);
  }
}

// ---- output helpers -----------------------------------------------------

static void sendTo(RobotLink link, const String &line)
{
  if (link == LINK_USB)
  {
    Serial.println(line);
  }
  else if (wsClient >= 0)
  {
    String framed = line + "\n";
    ws.sendTXT((uint8_t)wsClient, framed);
  }
}

static bool isPaired(RobotLink link)
{
  return link == LINK_USB ? usbPaired : wsPaired;
}

// ---- command handling ---------------------------------------------------

// parse a whole decimal number; false if the text is not one
static bool parseInt(const String &s, int &out)
{
  if (s.length() == 0 || s.length() > 4)
    return false;
  for (size_t i = 0; i < s.length(); i++)
  {
    if (!isDigit(s[i]))
      return false;
  }
  out = s.toInt();
  return true;
}

static void cmdDigitalWrite(int pin, int value)
{
  if (!pinAllowed(pin))
    return;
  if (pinMode_[pin] != PIN_DW)
  {
    releasePin(pin);
    pinMode(pin, OUTPUT);
    pinMode_[pin] = PIN_DW;
  }
  int level = value ? 1 : 0;
  if (pinValue[pin] != level)
  {
    digitalWrite(pin, level ? HIGH : LOW);
    pinValue[pin] = level;
  }
}

static void cmdPwm(int pin, int value)
{
  if (!pinAllowed(pin))
    return;
  value = constrain(value, 0, 255);
  if (pinMode_[pin] != PIN_PWM)
  {
    releasePin(pin);
    if (!ledcAcquire(pin, 1000, 8))
      return; // out of LEDC channels
    pinMode_[pin] = PIN_PWM;
  }
  if (pinValue[pin] != value)
  {
    ledcWrite(pinChannel[pin], value);
    pinValue[pin] = value;
  }
}

static void cmdServo(int pin, int angle)
{
  if (!pinAllowed(pin))
    return;
  angle = constrain(angle, 0, 180);
  if (pinMode_[pin] != PIN_SERVO)
  {
    releasePin(pin);
    if (!ledcAcquire(pin, 50, 14))
      return;
    pinMode_[pin] = PIN_SERVO;
  }
  if (pinValue[pin] != angle)
  {
    // 500-2400 us pulse in a 20 ms frame at 14-bit resolution
    uint32_t us = 500 + (uint32_t)angle * 1900 / 180;
    ledcWrite(pinChannel[pin], us * 16384UL / 20000UL);
    pinValue[pin] = angle;
  }
}

static void cmdWatch(char kind, int pin)
{
  if (!pinAllowed(pin))
    return;
  if (kind == 'A')
  {
    if (!pinHasAdc(pin))
      return;
    if (pinMode_[pin] != PIN_WATCH_A)
    {
      releasePin(pin);
      pinMode_[pin] = PIN_WATCH_A;
    }
  }
  else
  {
    if (pinMode_[pin] != PIN_WATCH_D)
    {
      releasePin(pin);
      pinMode(pin, INPUT);
      pinMode_[pin] = PIN_WATCH_D;
    }
  }
}

static void pair(RobotLink link)
{
  if (link == LINK_WIFI)
    wsPaired = true;
  else
    usbPaired = true;
  clearWatches();
  blinkStart();
  Serial.printf("Paired (%s)\n", link == LINK_USB ? "USB" : "WiFi");
}

static void handleHello(const String &code, RobotLink link)
{
  bool ok = link == LINK_USB || code == pairingCode;
  if (!ok)
  {
    sendTo(link, "HELLO:DENIED");
    return;
  }
  if (!isPaired(link))
    pair(link);
  sendTo(link, "HELLO:OK:" + String(webHostname()));
}

bool robotHandleLine(const String &raw, RobotLink link)
{
  String line = raw;
  line.trim();
  if (line.length() == 0)
    return false;

  // USB-only maintenance commands
  if (link == LINK_USB && line == "CODE?")
  {
    Serial.println("CODE:" + pairingCode);
    return true;
  }
  if (link == LINK_USB && line == "CODE:NEW")
  {
    prefs.begin("robot", false);
    pairingCode = "";
    const char alphabet[] = "ABCDEFGHJKLMNPQRSTUVWXYZ23456789";
    for (int i = 0; i < 6; i++)
      pairingCode += alphabet[esp_random() % (sizeof(alphabet) - 1)];
    prefs.putString("code", pairingCode);
    prefs.end();
    Serial.println("CODE:" + pairingCode);
    return true;
  }

  // is it a protocol line at all? (otherwise leave it to the text commands)
  int c1 = line.indexOf(':');
  String verb = c1 < 0 ? line : line.substring(0, c1);
  bool known = verb == "HELLO" || verb == "DW" || verb == "PWM" || verb == "SERVO" ||
               verb == "WATCH" || verb == "STOP" || verb == "PING";
  if (!known)
    return false;
  if (line.length() >= MAX_LINE)
    return true; // oversized: drop

  if (!isPaired(link) && verb != "HELLO")
    return true; // ignore everything until paired

  if (isPaired(link) || verb == "HELLO")
    lastLineMs = millis();
  failsafeTripped = false;

  String rest = c1 < 0 ? "" : line.substring(c1 + 1);

  if (verb == "HELLO")
  {
    handleHello(rest, link);
  }
  else if (verb == "STOP")
  {
    stopOutputs();
  }
  else if (verb == "PING")
  {
    sendTo(link, "PONG");
  }
  else if (verb == "DW" || verb == "PWM" || verb == "SERVO")
  {
    int c2 = rest.indexOf(':');
    int pin, value;
    if (c2 > 0 && parseInt(rest.substring(0, c2), pin) && parseInt(rest.substring(c2 + 1), value))
    {
      if (verb == "DW")
        cmdDigitalWrite(pin, value);
      else if (verb == "PWM")
        cmdPwm(pin, value);
      else
        cmdServo(pin, value);
    }
  }
  else if (verb == "WATCH")
  {
    int pin;
    if (rest.length() > 2 && (rest[0] == 'D' || rest[0] == 'A') && rest[1] == ':' &&
        parseInt(rest.substring(2), pin))
    {
      cmdWatch(rest[0], pin);
    }
  }
  return true;
}

void robotStopAll()
{
  stopOutputs();
  if (wsClient >= 0)
  {
    ws.disconnect((uint8_t)wsClient);
    wsClient = -1;
    wsPaired = false;
  }
}

// ---- WebSocket ----------------------------------------------------------

static void onWsEvent(uint8_t num, WStype_t type, uint8_t *payload, size_t length)
{
  switch (type)
  {
  case WStype_CONNECTED:
    if (wsClient >= 0 && wsClient != num)
    {
      ws.disconnect(num); // one controlling client at a time
      break;
    }
    wsClient = num;
    wsPaired = false;
    wsConnectedAt = millis();
    break;

  case WStype_DISCONNECTED:
    if (num == wsClient)
    {
      stopOutputs();
      wsClient = -1;
      wsPaired = false;
    }
    break;

  case WStype_TEXT:
  {
    if (num != wsClient)
      break;
    // a frame may hold several lines; no trailing \n still counts as a line
    String data;
    data.reserve(length);
    for (size_t i = 0; i < length; i++)
      data += (char)payload[i];
    int start = 0;
    while (start <= (int)data.length())
    {
      int nl = data.indexOf('\n', start);
      String piece = nl < 0 ? data.substring(start) : data.substring(start, nl);
      robotHandleLine(piece, LINK_WIFI);
      if (nl < 0)
        break;
      start = nl + 1;
    }
    break;
  }

  default:
    break;
  }
}

// ---- telemetry ----------------------------------------------------------

static unsigned long lastTelemetryMs = 0;

static void sendTelemetry()
{
  String line = "S:";
  bool any = false;
  bool first = true;

  auto flush = [&]() {
    if (!any)
      return;
    if (wsPaired)
      sendTo(LINK_WIFI, line);
    if (usbPaired)
      sendTo(LINK_USB, line);
    line = "S:";
    first = true;
    any = false;
  };

  for (int pin = 0; pin <= MAX_PIN; pin++)
  {
    String item;
    if (pinMode_[pin] == PIN_WATCH_D)
      item = "D" + String(pin) + ":" + String(digitalRead(pin) ? 1 : 0);
    else if (pinMode_[pin] == PIN_WATCH_A)
      item = "A" + String(pin) + ":" + String(analogRead(pin));
    else
      continue;

    if (line.length() + item.length() + 1 >= MAX_LINE - 1)
      flush();
    if (!first)
      line += "|";
    line += item;
    first = false;
    any = true;
  }
  flush();
}

// ---- public API ---------------------------------------------------------

void robotBegin()
{
  for (int pin = 0; pin <= MAX_PIN; pin++)
  {
    pinMode_[pin] = PIN_NONE;
    pinValue[pin] = -1;
    pinChannel[pin] = -1;
  }

  // pairing code: random on first boot, then kept in NVS
  prefs.begin("robot", false);
  pairingCode = prefs.getString("code", "");
  if (pairingCode.length() == 0)
  {
    const char alphabet[] = "ABCDEFGHJKLMNPQRSTUVWXYZ23456789";
    for (int i = 0; i < 6; i++)
      pairingCode += alphabet[esp_random() % (sizeof(alphabet) - 1)];
    prefs.putString("code", pairingCode);
  }
  prefs.end();

  ws.begin();
  ws.onEvent(onWsEvent);
}

void robotUpdate()
{
  ws.loop();
  blinkUpdate();

  // drop a client that hasn't paired in time so it can't lock others out
  if (wsClient >= 0 && !wsPaired && millis() - wsConnectedAt > UNPAIRED_DROP_MS)
  {
    ws.disconnect((uint8_t)wsClient);
    wsClient = -1;
  }

  // failsafe: no line at all for 3 s on a paired link
  if ((wsPaired || usbPaired) && !failsafeTripped && millis() - lastLineMs > FAILSAFE_MS)
  {
    stopOutputs();
    failsafeTripped = true;
  }

  if ((wsPaired || usbPaired) && millis() - lastTelemetryMs >= TELEMETRY_MS)
  {
    lastTelemetryMs = millis();
    sendTelemetry();
  }

  // print the address again whenever the IP changes
  static IPAddress lastIp;
  IPAddress ip = WiFi.localIP();
  if (WiFi.status() == WL_CONNECTED && ip != lastIp)
  {
    lastIp = ip;
    Serial.printf("WIFI:IP:%s HOST:%s.local CODE:%s\n", ip.toString().c_str(), webHostname(),
                  pairingCode.c_str());
  }
}
