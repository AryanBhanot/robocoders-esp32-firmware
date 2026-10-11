#include <Arduino.h>
#include <WiFi.h>
#include <WebServer.h>
#include <ESPmDNS.h>
#include "web.h"
#include "web_page.h"
#include "led.h"
#include "secrets.h"

// the board is reachable at http://<HOSTNAME>.local
static const char *HOSTNAME = "ledboard";

// How long to wait for WiFi at startup
static const unsigned long WIFI_TIMEOUT_MS = 15000;

static WebServer server(80);

// current state as JSON for the web page
static void sendStatus()
{
  String json = "{\"brightness\":";
  json += ledBrightness();
  json += ",\"fading\":";
  json += ledIsFading() ? "true" : "false";
  json += "}";
  server.send(200, "application/json", json);
}

static void setupRoutes()
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

static bool connectWiFi()
{
  Serial.print("Connecting to WiFi");
  WiFi.mode(WIFI_STA);
  WiFi.setHostname(HOSTNAME);
  WiFi.setAutoReconnect(true);
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);

  unsigned long start = millis();
  while (WiFi.status() != WL_CONNECTED && millis() - start < WIFI_TIMEOUT_MS)
  {
    delay(500);
    Serial.print(".");
  }
  Serial.println();

  return WiFi.status() == WL_CONNECTED;
}

void webBegin()
{
  if (!connectWiFi())
  {
    Serial.println("WiFi not connected - check src/secrets.h. Serial control still works.");
    return;
  }

  bool nameOk = MDNS.begin(HOSTNAME);
  if (nameOk)
  {
    MDNS.addService("http", "tcp", 80);
  }

  setupRoutes();

  if (nameOk)
  {
    Serial.printf("Open http://%s.local or http://%s\n", HOSTNAME, WiFi.localIP().toString().c_str());
  }
  else
  {
    Serial.printf("Open http://%s (name lookup unavailable)\n", WiFi.localIP().toString().c_str());
  }
}

void webUpdate()
{
  server.handleClient();
}
