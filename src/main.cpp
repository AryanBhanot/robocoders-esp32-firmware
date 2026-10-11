/**
 * LED control over Serial and WiFi
 *
 * Serial Monitor: type "on", "off" or "fade".
 * Web page: open the IP address printed at startup in a browser
 * on the same WiFi network.
 *
 * Put your WiFi name/password in src/secrets.h (see secrets.h.example).
 */
#include <Arduino.h>
#include <WiFi.h>
#include <WebServer.h>
#include "secrets.h"

// External LED connected to GPIO 9
const int LED_PIN = 9;

// PWM settings (8-bit: brightness 0-255)
const int PWM_FREQ = 5000;
const int PWM_RESOLUTION = 8;
const int PWM_MAX = 255;

// Fade settings
const int FADE_STEP = 5;                // brightness change per update
const unsigned long FADE_DELAY_MS = 20; // time between updates

// How long to wait for WiFi at startup
const unsigned long WIFI_TIMEOUT_MS = 15000;

// Onboard BOOT button (GPIO 0, reads LOW when pressed)
const int BUTTON_PIN = 0;
const unsigned long DEBOUNCE_MS = 50;

bool lastButtonReading = HIGH;
bool buttonState = HIGH;
unsigned long lastButtonChange = 0;

bool fading = false;
int brightness = 0;
int fadeDirection = 1; // 1 = getting brighter, -1 = getting dimmer
unsigned long lastFadeUpdate = 0;

WebServer server(80);

// the web page, stored in flash
const char INDEX_HTML[] PROGMEM = R"HTML(
<!doctype html>
<html>
<head>
  <meta charset="utf-8">
  <meta name="viewport" content="width=device-width, initial-scale=1">
  <title>LED Control</title>
  <style>
    body { font-family: system-ui, sans-serif; max-width: 420px; margin: 0 auto;
           padding: 24px 16px; background: #111; color: #eee; text-align: center; }
    h1 { font-size: 1.4rem; }
    .bulb { width: 90px; height: 90px; border-radius: 50%; margin: 20px auto;
            background: #ffd54a; opacity: 0.1; box-shadow: 0 0 40px #ffd54a; }
    .buttons { display: flex; gap: 10px; }
    button { flex: 1; padding: 16px 0; font-size: 1.1rem; border: 0;
             border-radius: 10px; background: #2d6cdf; color: #fff; }
    button:active { background: #1d4fa8; }
    input[type=range] { width: 100%; margin: 28px 0 8px; }
    #status { color: #999; }
  </style>
</head>
<body>
  <h1>LED Control</h1>
  <div class="bulb" id="bulb"></div>
  <div class="buttons">
    <button onclick="send('on')">On</button>
    <button onclick="send('off')">Off</button>
    <button onclick="send('fade')">Fade</button>
  </div>
  <input type="range" id="slider" min="0" max="255" value="0">
  <div id="status">connecting...</div>
<script>
  const slider = document.getElementById('slider');
  const bulb = document.getElementById('bulb');
  const status = document.getElementById('status');
  let dragging = false;

  function show(s) {
    bulb.style.opacity = 0.1 + 0.9 * s.brightness / 255;
    status.textContent = (s.fading ? 'Fading' : 'Manual') + ' - brightness ' + s.brightness;
    if (!dragging) slider.value = s.brightness;
  }
  function call(url) {
    return fetch(url).then(r => r.json()).then(show).catch(() => status.textContent = 'disconnected');
  }
  function send(cmd) { call('/' + cmd); }

  slider.addEventListener('input', () => { dragging = true; call('/brightness?value=' + slider.value); });
  slider.addEventListener('change', () => { dragging = false; });

  setInterval(() => { if (!dragging) call('/status'); }, 500);
  call('/status');
</script>
</body>
</html>
)HTML";

// set LED brightness (0-255), works with both Arduino-ESP32 core 2.x and 3.x
void setBrightness(int value)
{
#if ESP_ARDUINO_VERSION_MAJOR >= 3
  ledcWrite(LED_PIN, value);
#else
  ledcWrite(0, value);
#endif
}

// the actions below are shared by the Serial commands and the web page
void ledOn()
{
  fading = false;
  brightness = PWM_MAX;
  setBrightness(brightness);
}

void ledOff()
{
  fading = false;
  brightness = 0;
  setBrightness(brightness);
}

void ledFade()
{
  fading = true;
}

void ledSetLevel(int value)
{
  fading = false;
  brightness = constrain(value, 0, PWM_MAX);
  setBrightness(brightness);
}

// turn the LED off if it is lit or fading, otherwise turn it on
void ledToggle()
{
  if (fading || brightness > 0)
  {
    ledOff();
    Serial.println("LED OFF (button)");
  }
  else
  {
    ledOn();
    Serial.println("LED ON (button)");
  }
}

// debounced button check: toggles the LED once per press
void checkButton()
{
  bool reading = digitalRead(BUTTON_PIN);

  if (reading != lastButtonReading)
  {
    lastButtonChange = millis();
    lastButtonReading = reading;
  }

  if (millis() - lastButtonChange >= DEBOUNCE_MS && reading != buttonState)
  {
    buttonState = reading;
    if (buttonState == LOW)
    {
      ledToggle();
    }
  }
}

// current state as JSON for the web page
void sendStatus()
{
  String json = "{\"brightness\":";
  json += brightness;
  json += ",\"fading\":";
  json += fading ? "true" : "false";
  json += "}";
  server.send(200, "application/json", json);
}

void setupWebServer()
{
  server.on("/", []() { server.send_P(200, "text/html", INDEX_HTML); });
  server.on("/on", []() { ledOn(); sendStatus(); });
  server.on("/off", []() { ledOff(); sendStatus(); });
  server.on("/fade", []() { ledFade(); sendStatus(); });
  server.on("/status", sendStatus);
  server.on("/brightness", []() {
    if (server.hasArg("value"))
    {
      ledSetLevel(server.arg("value").toInt());
    }
    sendStatus();
  });
  server.onNotFound([]() { server.send(404, "text/plain", "Not found"); });
  server.begin();
}

void connectWiFi()
{
  Serial.print("Connecting to WiFi");
  WiFi.mode(WIFI_STA);
  WiFi.setAutoReconnect(true);
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);

  unsigned long start = millis();
  while (WiFi.status() != WL_CONNECTED && millis() - start < WIFI_TIMEOUT_MS)
  {
    delay(500);
    Serial.print(".");
  }
  Serial.println();

  if (WiFi.status() == WL_CONNECTED)
  {
    Serial.print("Open http://");
    Serial.print(WiFi.localIP());
    Serial.println(" in your browser.");
  }
  else
  {
    Serial.println("WiFi not connected - check src/secrets.h. Serial control still works.");
  }
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

  // BOOT button has an external pull-up, but enable the internal one too
  pinMode(BUTTON_PIN, INPUT_PULLUP);

  Serial.println("Hello! Board started.");
  connectWiFi();
  setupWebServer();
  Serial.println("Type 'on', 'off' or 'fade' and press Enter, or press the BOOT button to toggle.");
}

void loop()
{
  // answer any browser requests
  server.handleClient();

  checkButton();

  // only do something when the user has typed something
  if (Serial.available() > 0)
  {
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
