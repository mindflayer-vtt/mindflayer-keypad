#include <Arduino.h>
#include <ArduinoWebsockets.h>
#include <ESP8266HTTPClient.h>
#include <ESP8266WiFi.h>
#include <ESP8266httpUpdate.h>
#include <HardwareConfig.h>
#include <KeyboardMatrix.h>
#include <NeoPixelBus.h>
#include <new>
#include <Protocol.h>
#include <Provisioning.h>
#include <ProvisioningStorage.h>
#include <RecoveryMode.h>

#ifndef FIRMWARE_VERSION
#define FIRMWARE_VERSION "0.0.0-dev"
#endif
using namespace websockets;
namespace KeyboardMatrix = com::viromania::vtt::wss::KeyboardMatrix;
namespace protocol = mindflayer::protocol;
namespace provisioning = mindflayer::provisioning;
static uint8_t frameBuffer[protocol::MAX_DEVICE_FRAME_SIZE];
alignas(4) static uint8_t provisioningBuffer[provisioning::MAX_ENVELOPE_SIZE];
static size_t provisioningReceived = 0, provisioningExpected = 0;
static provisioning::Provisioning settings;
static WebsocketsClient client;
static BearSSL::PublicKey* serverPublicKey = nullptr;
static BearSSL::PublicKey firmwareSigningPublicKey(FIRMWARE_SIGNING_PUBLIC_KEY_PEM);
static BearSSL::HashSHA256 firmwareHash;
static BearSSL::SigningVerifier firmwareVerifier(&firmwareSigningPublicKey);
using LedStrip = NeoPixelBus<NeoGrbFeature, Neo800KbpsMethod>;
alignas(LedStrip) static uint8_t ledStripStorage[sizeof(LedStrip)];
static LedStrip* ledStrip = nullptr;
static bool provisioned = false, reconnectRequested = false, authenticated = false;
static uint32_t lastPong = 0;

static void clearRecoveryMarker() { mindflayer::recovery::Marker marker = {}; ESP.rtcUserMemoryWrite(mindflayer::recovery::RTC_OFFSET, (uint32_t*)&marker, sizeof(marker)); }
static bool consumeRecoveryMarker() {
  mindflayer::recovery::Marker marker = {};
  bool requested = ESP.rtcUserMemoryRead(mindflayer::recovery::RTC_OFFSET, (uint32_t*)&marker, sizeof(marker)) && mindflayer::recovery::markerValid(marker);
  clearRecoveryMarker();
  return requested;
}
static void armRecoveryMarker() { auto marker = mindflayer::recovery::makeMarker(); ESP.rtcUserMemoryWrite(mindflayer::recovery::RTC_OFFSET, (uint32_t*)&marker, sizeof(marker)); }

static void setColors(uint8_t r1, uint8_t g1, uint8_t b1, uint8_t r2, uint8_t g2, uint8_t b2) {
  if (!ledStrip) return;
  ledStrip->SetPixelColor(0, RgbColor(r1, g1, b1)); ledStrip->SetPixelColor(1, RgbColor(r2, g2, b2)); ledStrip->Show();
}
static void base64Url(const uint8_t input[32], char output[44]) {
  static const char alphabet[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789-_"; size_t in = 0, out = 0;
  while (in + 3 <= 32) { uint32_t n = ((uint32_t)input[in] << 16) | ((uint32_t)input[in+1] << 8) | input[in+2]; in += 3; output[out++] = alphabet[n >> 18]; output[out++] = alphabet[(n >> 12) & 63]; output[out++] = alphabet[(n >> 6) & 63]; output[out++] = alphabet[n & 63]; }
  uint32_t n = (uint32_t)input[in] << 16 | (uint32_t)input[in+1] << 8; output[out++] = alphabet[n >> 18]; output[out++] = alphabet[(n >> 12) & 63]; output[out++] = alphabet[(n >> 6) & 63]; output[out] = 0;
}
static void performUpdate(const protocol::UpdateAvailable& update) {
  Serial.printf("Starting signed OTA to %s (%lu bytes)\n", update.version, (unsigned long)update.size); BearSSL::WiFiClientSecure secureClient; secureClient.setKnownKey(serverPublicKey); HTTPClient http;
  String url = String("https://") + settings.serverHost + ':' + settings.serverPort + update.path; if (!http.begin(secureClient, url)) { Serial.println("OTA HTTPS setup failed"); return; }
  char bearer[44]; base64Url(update.token, bearer); http.addHeader("Authorization", String("Bearer ") + bearer); ESPhttpUpdate.rebootOnUpdate(true);
  ESPhttpUpdate.onError([](int error) { Serial.printf("Signed OTA rejected: %d %s\n", error, ESPhttpUpdate.getLastErrorString().c_str()); });
  const auto result = ESPhttpUpdate.update(http, FIRMWARE_VERSION); if (result == HTTP_UPDATE_FAILED) Serial.printf("OTA failed; retaining %s\n", FIRMWARE_VERSION); http.end();
}
static void sendFrame(size_t size) { if (size && size <= sizeof(frameBuffer)) client.sendBinary((const char*)frameBuffer, size); }
static void onMessage(WebsocketsMessage message) {
  lastPong = millis(); if (!message.isBinary() || message.length() == 0 || message.length() > protocol::MAX_DEVICE_FRAME_SIZE) { Serial.println("Rejected non-binary or oversized device frame"); client.close(CloseReason_ProtocolError); return; }
  const uint8_t* data = (const uint8_t*)message.c_str(); size_t size = message.length(); protocol::AuthChallenge challenge; size_t written;
  if (protocol::parseAuthChallenge(data, size, challenge)) { if (protocol::buildAuthResponse(frameBuffer, sizeof(frameBuffer), written, settings.deviceId, settings.deviceSecret, challenge.challenge)) sendFrame(written); return; }
  protocol::AuthResult auth;
  if (protocol::parseAuthResult(data, size, auth)) {
    if (auth.status != protocol::AUTH_OK || strcmp(auth.deviceId, settings.deviceId)) { client.close(CloseReason_PolicyViolation); return; }
    authenticated = true; if (protocol::buildRegistration(frameBuffer, sizeof(frameBuffer), written, FIRMWARE_VERSION, HARDWARE_ID)) sendFrame(written);
    Serial.printf("Authenticated as %s; firmware=%s; heap=%u\n", settings.deviceId, FIRMWARE_VERSION, ESP.getFreeHeap()); return;
  }
  protocol::UpdateAvailable update;
  if (authenticated && protocol::parseUpdateAvailable(data, size, update)) { client.close(CloseReason_GoingAway); performUpdate(update); return; }
  protocol::Configuration configuration;
  if (authenticated && protocol::parseConfiguration(data, size, configuration)) { setColors(configuration.led1.r, configuration.led1.g, configuration.led1.b, configuration.led2.r, configuration.led2.g, configuration.led2.b); return; }
  Serial.println("Rejected malformed or unauthorized CBOR message"); client.close(CloseReason_ProtocolError);
}
static void onEvent(WebsocketsEvent event, String) {
  if (event == WebsocketsEvent::ConnectionOpened) { authenticated = false; Serial.printf("Pinned binary WSS connected; heap=%u\n", ESP.getFreeHeap()); }
  else if (event == WebsocketsEvent::ConnectionClosed) { authenticated = false; reconnectRequested = true; Serial.println("WSS closed; reconnecting"); }
  else if (event == WebsocketsEvent::GotPing) client.pong(); else if (event == WebsocketsEvent::GotPong) lastPong = millis();
}
static void setupWebSocket() {
  client.onMessage(onMessage); client.onEvent(onEvent); client.setKnownKey(serverPublicKey);
  if (!client.connectSecure(settings.serverHost, settings.serverPort, "/device/v1")) { Serial.println("Pinned WSS connection failed"); reconnectRequested = true; return; }
  client.ping(); lastPong = millis();
}
static void processSerialProvisioning() {
  static const uint8_t magic[4] = {'M','F','P','1'};
  while (Serial.available()) {
    uint8_t byte = Serial.read();
    if (provisioningReceived < 4 && byte != magic[provisioningReceived]) { provisioningReceived = byte == magic[0] ? 1 : 0; provisioningExpected = 0; continue; }
    if (provisioningReceived >= sizeof(provisioningBuffer)) { provisioningReceived = provisioningExpected = 0; continue; }
    provisioningBuffer[provisioningReceived++] = byte;
    if (provisioningReceived == provisioning::ENVELOPE_HEADER_SIZE) {
      size_t payload = ((size_t)provisioningBuffer[5] << 8) | provisioningBuffer[6];
      if (payload == 0 || payload > provisioning::MAX_PAYLOAD_SIZE) { Serial.println("PROVISIONING ERROR length"); provisioningReceived = provisioningExpected = 0; continue; }
      provisioningExpected = provisioning::ENVELOPE_HEADER_SIZE + payload + provisioning::ENVELOPE_CRC_SIZE;
    }
    if (provisioningExpected && provisioningReceived == provisioningExpected) {
      if (!provisioning::storeAtomically(provisioningBuffer, provisioningReceived)) Serial.println("PROVISIONING ERROR invalid envelope or flash store");
      else { Serial.println("PROVISIONING OK; rebooting"); delay(100); ESP.restart(); }
      provisioningReceived = provisioningExpected = 0;
    }
  }
}
void setup() {
  Serial.setRxBufferSize(provisioning::MAX_ENVELOPE_SIZE + 16); Serial.begin(115200); Serial.println(); Serial.printf("Mind Flayer %s booting; heap=%u; flash-real=%lu; flash-configured=%lu\n", FIRMWARE_VERSION, ESP.getFreeHeap(), (unsigned long)ESP.getFlashChipRealSize(), (unsigned long)ESP.getFlashChipSize()); Update.installSignature(&firmwareHash, &firmwareVerifier);
  provisioning::Selection selected;
  if (!provisioning::loadStored(settings, &selected)) { clearRecoveryMarker(); Serial.println("UNPROVISIONED; NeoPixel DMA disabled; awaiting MFP1 serial provisioning envelope"); return; }
  Serial.printf("Provisioning copy %c generation %lu loaded\n", selected.copy == provisioning::COPY_A ? 'A' : 'B', (unsigned long)selected.generation);
  serverPublicKey = new BearSSL::PublicKey(settings.serverPublicKey, settings.serverPublicKeyLength); if (!serverPublicKey->isRSA() && !serverPublicKey->isEC()) { Serial.println("UNPROVISIONED; invalid server public key"); return; }
  if (consumeRecoveryMarker()) { Serial.println("SERIAL PROVISIONING MODE; NeoPixel DMA disabled on GPIO3/RXD0"); return; }
  armRecoveryMarker(); Serial.println("Double-reset recovery window open"); delay(mindflayer::recovery::WINDOW_MS); clearRecoveryMarker();
  ledStrip = new (ledStripStorage) LedStrip(2, NEOPIXEL_DATA_PIN); ledStrip->Begin(); setColors(255, 0, 0, 0, 0, 0); Serial.println("NeoPixel DMA active on physical GPIO3/RXD0; serial RX disabled");
  provisioned = true; WiFi.hostname(settings.deviceId); WiFi.begin(settings.ssid, settings.wifiPassword);
  Serial.print("Connecting to provisioned Wi-Fi"); while (WiFi.status() != WL_CONNECTED) { Serial.print('.'); delay(500); }
  Serial.printf(" connected: %s; heap=%u\n", WiFi.localIP().toString().c_str(), ESP.getFreeHeap()); KeyboardMatrix::initMatrix(); setupWebSocket();
}
void onKeyChange(KeyboardMatrix::KeyState* key) { size_t written; if (protocol::buildKeyEvent(frameBuffer, sizeof(frameBuffer), written, key->key, key->isDown)) sendFrame(written); }
void loop() {
  if (!provisioned) { processSerialProvisioning(); delay(10); return; } client.poll(); if (authenticated && client.available()) KeyboardMatrix::detectKeys(onKeyChange);
  if (reconnectRequested) { reconnectRequested = false; delay(500); client = WebsocketsClient(); setupWebSocket(); }
  if (client.available() && millis() - lastPong > 15000) { client.ping(); lastPong = millis(); }
}
