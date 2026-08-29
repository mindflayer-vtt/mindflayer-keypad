#include "config.h"
#include <Arduino.h>
#include <ArduinoJson.h>
#include <ArduinoWebsockets.h>
#include <ESP8266HTTPClient.h>
#include <ESP8266WiFi.h>
#include <ESP8266httpUpdate.h>
#include <KeyboardMatrix.h>
#include <NeoPixelBus.h>
#include <Protocol.h>

#ifndef FIRMWARE_VERSION
#define FIRMWARE_VERSION "0.0.0-dev"
#endif
#ifndef HARDWARE_ID
#define HARDWARE_ID "mindflayer-keypad-v1"
#endif

using namespace websockets;
namespace KeyboardMatrix = com::viromania::vtt::wss::KeyboardMatrix;
static constexpr size_t BUFFER_SIZE = 512;
static char buffer[BUFFER_SIZE];
static WebsocketsClient client;
static bool reconnectRequested = false;
static bool authenticated = false;
static uint32_t lastPong = 0;
static JsonDocument jsonDocument;
static BearSSL::PublicKey serverPublicKey(SERVER_PUBLIC_KEY_PEM);
static BearSSL::PublicKey firmwareSigningPublicKey(FIRMWARE_SIGNING_PUBLIC_KEY_PEM);
static BearSSL::HashSHA256 firmwareHash;
static BearSSL::SigningVerifier firmwareVerifier(&firmwareSigningPublicKey);
static NeoPixelBus<NeoGrbFeature, Neo800KbpsMethod> ledStrip(2);

void setColors(uint8_t r1, uint8_t g1, uint8_t b1, uint8_t r2, uint8_t g2, uint8_t b2) {
  ledStrip.SetPixelColor(0, RgbColor(r1, g1, b1));
  ledStrip.SetPixelColor(1, RgbColor(r2, g2, b2));
  ledStrip.Show();
}

void performUpdate(const mindflayer::protocol::UpdateAvailable& update) {
  Serial.printf("Starting signed OTA to %s (%lu bytes)\n", update.version, static_cast<unsigned long>(update.size));
  BearSSL::WiFiClientSecure secureClient;
  secureClient.setKnownKey(&serverPublicKey);
  HTTPClient http;
  String url = String(WSS_URL);
  url.replace("wss://", "https://");
  const int pathStart = url.indexOf('/', 8);
  if (pathStart >= 0) url.remove(pathStart);
  url += update.url;
  if (!http.begin(secureClient, url)) { Serial.println("OTA HTTPS setup failed"); return; }
  http.addHeader("Authorization", String("Bearer ") + update.token);
  ESPhttpUpdate.rebootOnUpdate(true);
  ESPhttpUpdate.onError([](int error) { Serial.printf("Signed OTA rejected: %d %s\n", error, ESPhttpUpdate.getLastErrorString().c_str()); });
  const auto result = ESPhttpUpdate.update(http, FIRMWARE_VERSION);
  if (result == HTTP_UPDATE_FAILED) Serial.printf("OTA failed; retaining %s\n", FIRMWARE_VERSION);
  http.end();
}

void onMessage(WebsocketsMessage message) {
  lastPong = millis();
  mindflayer::protocol::AuthChallenge challenge;
  if (mindflayer::protocol::parseAuthChallenge(jsonDocument, message.data().c_str(), challenge)) {
    if (mindflayer::protocol::buildAuthResponse(buffer, sizeof(buffer), ESP_NAME, DEVICE_SECRET_HEX, challenge.challenge)) client.send(buffer);
    return;
  }
  if (!deserializeJson(jsonDocument, message.data()) && strcmp(jsonDocument["type"] | "", "auth-ok") == 0) {
    authenticated = true;
    mindflayer::protocol::buildRegistration(buffer, sizeof(buffer), ESP_NAME, FIRMWARE_VERSION, HARDWARE_ID);
    client.send(buffer);
    Serial.printf("Authenticated as %s; firmware=%s\n", ESP_NAME, FIRMWARE_VERSION);
    return;
  }
  mindflayer::protocol::UpdateAvailable update;
  if (authenticated && mindflayer::protocol::parseUpdateAvailable(jsonDocument, message.data().c_str(), update)) {
    client.close(CloseReason_GoingAway);
    performUpdate(update);
    return;
  }
  mindflayer::protocol::Configuration configuration;
  if (authenticated && mindflayer::protocol::parseConfiguration(jsonDocument, message.data().c_str(), configuration))
    setColors(configuration.led1.r, configuration.led1.g, configuration.led1.b, configuration.led2.r, configuration.led2.g, configuration.led2.b);
}

void onEvent(WebsocketsEvent event, String) {
  if (event == WebsocketsEvent::ConnectionOpened) { authenticated = false; Serial.println("Pinned WSS connected; awaiting challenge"); }
  else if (event == WebsocketsEvent::ConnectionClosed) { authenticated = false; reconnectRequested = true; Serial.println("WSS closed; reconnecting"); }
  else if (event == WebsocketsEvent::GotPing) client.pong();
  else if (event == WebsocketsEvent::GotPong) lastPong = millis();
}

void setupWebSocket() {
  client.onMessage(onMessage); client.onEvent(onEvent);
  client.setKnownKey(&serverPublicKey);
  if (!client.connect(WSS_URL)) { Serial.println("Pinned WSS connection failed"); reconnectRequested = true; return; }
  client.ping(); lastPong = millis();
}

void setup() {
  Serial.begin(115200); Serial.println(); Serial.printf("Mind Flayer %s booting\n", FIRMWARE_VERSION);
  Update.installSignature(&firmwareHash, &firmwareVerifier);
  ledStrip.Begin(); setColors(255, 0, 0, 0, 0, 0);
  WiFi.hostname(ESP_NAME); WiFi.begin(WIFI_SSID, WIFI_PASS);
  Serial.print("Connecting to test Wi-Fi");
  while (WiFi.status() != WL_CONNECTED) { Serial.print('.'); delay(500); }
  Serial.printf(" connected: %s\n", WiFi.localIP().toString().c_str());
  KeyboardMatrix::initMatrix();
  setupWebSocket();
}

void onKeyChange(KeyboardMatrix::KeyState* key) {
  if (mindflayer::protocol::buildKeyEvent(buffer, sizeof(buffer), ESP_NAME, key->key, key->isDown)) client.send(buffer);
}

void loop() {
  client.poll();
  if (authenticated && client.available()) KeyboardMatrix::detectKeys(onKeyChange);
  if (reconnectRequested) { reconnectRequested = false; delay(500); client = WebsocketsClient(); setupWebSocket(); }
  if (client.available() && millis() - lastPong > 15000) { client.ping(); lastPong = millis(); }
}
