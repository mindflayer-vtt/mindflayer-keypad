#include <Arduino.h>
#include <ArduinoWebsockets.h>
#include <ESP8266HTTPClient.h>
#include <ESP8266WiFi.h>
#include <ESP8266httpUpdate.h>
#include <HardwareConfig.h>
#include <HealthGate.h>
#include <KeyboardMatrix.h>
#include <NeoPixelBus.h>
#include <new>
#include <Protocol.h>
#include <Provisioning.h>
#include <ProvisioningStorage.h>
#include <RecoveryMode.h>
#ifdef RBOOT_INTEGRATION
#include <BootControl.h>
#include <RBootSlot.h>
#endif

#ifndef FIRMWARE_VERSION
#define FIRMWARE_VERSION "0.0.0-dev"
#endif
using namespace websockets;
namespace KeyboardMatrix = com::viromania::vtt::wss::KeyboardMatrix;
namespace protocol = mindflayer::protocol;
namespace provisioning = mindflayer::provisioning;
static uint8_t frameBuffer[protocol::MAX_DEVICE_FRAME_SIZE];
static uint8_t pendingFrame[protocol::MAX_DEVICE_FRAME_SIZE];
static size_t pendingFrameSize = 0;
static uint8_t receivedFrame[protocol::MAX_DEVICE_FRAME_SIZE];
static size_t receivedFrameSize = 0;
static bool receivedFrameInvalid = false;
static bool eventOpened = false, eventClosed = false, eventPing = false, eventPong = false;
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
static bool wifiHealthy = false, wssHealthy = false, registered = false;
static bool temporaryBoot = false;
static uint32_t lastPong = 0;
#ifdef RBOOT_INTEGRATION
static uint32_t temporaryStarted = 0;
constexpr uint32_t TEMPORARY_HEALTH_TIMEOUT_MS = 90000;
class EspSlotFlash : public RBootSlot::Flash {
 public:
  bool read(uint32_t a, void* d, size_t n) override { return ESP.flashRead(a, static_cast<uint8_t*>(d), n); }
  bool eraseSector(uint32_t s) override { return ESP.flashEraseSector(s); }
  bool write(uint32_t a, const void* d, size_t n) override { return ESP.flashWrite(a, const_cast<uint8_t*>(static_cast<const uint8_t*>(d)), n); }
};
static EspSlotFlash slotFlash;
#endif

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
  char bearer[44]; base64Url(update.token, bearer); http.addHeader("Authorization", String("Bearer ") + bearer);
#ifdef RBOOT_INTEGRATION
  const uint32_t signatureSize = firmwareVerifier.length(), trailerSize = signatureSize + 4;
  if (temporaryBoot || update.size <= trailerSize) { Serial.println("OTA rejected: no safe inactive slot or invalid signed size"); http.end(); return; }
  const auto active = static_cast<RBootSlot::Slot>(BootControl::currentSlot()); const auto target = RBootSlot::other(active);
  RBootSlot::Writer writer(slotFlash, active, static_cast<RBootSlot::Slot>(BootControl::permanentSlot()));
  const uint32_t imageSize = update.size - trailerSize;
  if (!writer.begin(target, imageSize)) { Serial.println("OTA rejected before erase by slot bounds/state"); http.end(); return; }
  const int status = http.GET();
  if (status != HTTP_CODE_OK || http.getSize() != static_cast<int>(update.size)) { Serial.printf("OTA HTTPS response rejected: %d\n", status); writer.abort(); http.end(); return; }
  WiFiClient& stream = http.getStream(); uint8_t buffer[512], signature[512]; uint32_t received = 0; unsigned long lastData = millis();
  br_sha256_context artifactHash; br_sha256_init(&artifactHash); firmwareHash.begin(); bool ok = true;
  while (ok && received < update.size) {
    size_t available = stream.available(); if (!available) { if (!stream.connected() || millis()-lastData>60000) { ok=false; break; } delay(1); continue; }
    size_t count = min(available, min(sizeof(buffer), static_cast<size_t>(update.size-received))); int got=stream.readBytes(buffer,count); if(got<=0){ok=false;break;} count=got; lastData=millis(); br_sha256_update(&artifactHash,buffer,count);
    size_t offset=0; if(received<imageSize){size_t body=min(count,static_cast<size_t>(imageSize-received)); firmwareHash.add(buffer,body); ok=writer.write(buffer,body); offset=body;}
    if(ok&&offset<count){size_t tailOffset=received+offset-imageSize;if(tailOffset+count-offset>sizeof(signature)){ok=false;break;}memcpy(signature+tailOffset,buffer+offset,count-offset);} received+=count; delay(0);
  }
  uint8_t digest[32]; br_sha256_out(&artifactHash,digest); firmwareHash.end(); uint32_t encodedLength=0;
  if (received==update.size) memcpy(&encodedLength, signature+signatureSize, 4);
  ok = ok && received==update.size && memcmp(digest,update.sha256,32)==0 && encodedLength==signatureSize &&
       firmwareVerifier.verify(&firmwareHash,signature,signatureSize) && writer.finish();
  if (!ok) { Serial.println("Signed rBoot OTA rejected; permanent slot unchanged"); writer.abort(); http.end(); return; }
  Serial.printf("Validated candidate in slot %c; requesting temporary boot\n", target==RBootSlot::Slot::A?'A':'B'); http.end();
  if (!BootControl::bootTemporary(static_cast<uint8_t>(target))) { Serial.println("Temporary boot request failed"); return; }
  Serial.flush(); ESP.restart();
#else
  ESPhttpUpdate.rebootOnUpdate(true);
  ESPhttpUpdate.onError([](int error) { Serial.printf("Signed OTA rejected: %d %s\n", error, ESPhttpUpdate.getLastErrorString().c_str()); });
  const auto result = ESPhttpUpdate.update(http, FIRMWARE_VERSION); if (result == HTTP_UPDATE_FAILED) Serial.printf("OTA failed; retaining %s\n", FIRMWARE_VERSION); http.end();
#endif
}
static void sendFrame(size_t size) { if (size && size <= sizeof(frameBuffer) && !pendingFrameSize) { memcpy(pendingFrame,frameBuffer,size); pendingFrameSize=size; } }
static void flushFrame() { if (pendingFrameSize && client.available()) { size_t size=pendingFrameSize; pendingFrameSize=0; client.sendBinary((const char*)pendingFrame,size); } }
static void processMessage(const uint8_t* data, size_t size) {
  lastPong = millis(); protocol::AuthChallenge challenge; size_t written;
  if (protocol::parseAuthChallenge(data, size, challenge)) { if (protocol::buildAuthResponse(frameBuffer, sizeof(frameBuffer), written, settings.deviceId, settings.deviceSecret, challenge.challenge)) sendFrame(written); return; }
  protocol::AuthResult auth;
  if (protocol::parseAuthResult(data, size, auth)) {
    if (auth.status != protocol::AUTH_OK || strcmp(auth.deviceId, settings.deviceId)) { client.close(CloseReason_PolicyViolation); return; }
    authenticated = true; if (protocol::buildRegistration(frameBuffer, sizeof(frameBuffer), written, FIRMWARE_VERSION, HARDWARE_ID)) { sendFrame(written); registered = true; }
    Serial.printf("Authenticated as %s; firmware=%s; heap=%u\n", settings.deviceId, FIRMWARE_VERSION, ESP.getFreeHeap()); return;
  }
  protocol::UpdateAvailable update;
  if (authenticated && protocol::parseUpdateAvailable(data, size, update)) { client.close(CloseReason_GoingAway); performUpdate(update); return; }
  protocol::FirmwareAccepted accepted;
  if (authenticated && protocol::parseFirmwareAccepted(data, size, accepted)) {
#ifdef RBOOT_INTEGRATION
    const mindflayer::health::State health = {temporaryBoot, provisioned, true, wifiHealthy, serverPublicKey != nullptr, wssHealthy, authenticated, registered, true, !strcmp(accepted.version, FIRMWARE_VERSION)};
    if (mindflayer::health::shouldPromote(health)) { Serial.println("Server accepted candidate; promoting transactionally"); if (BootControl::promoteCurrentSlot()) { Serial.flush(); ESP.restart(); } else Serial.println("Candidate promotion failed"); }
#endif
    return;
  }
  protocol::Configuration configuration;
  if (authenticated && protocol::parseConfiguration(data, size, configuration)) { setColors(configuration.led1.r, configuration.led1.g, configuration.led1.b, configuration.led2.r, configuration.led2.g, configuration.led2.b); return; }
  Serial.println("Rejected malformed or unauthorized CBOR message"); client.close(CloseReason_ProtocolError);
}
static void onMessage(WebsocketsMessage message) {
  const size_t size = message.length();
  if (!message.isBinary() || !size || size > sizeof(receivedFrame) || receivedFrameSize) { receivedFrameInvalid = true; return; }
  memcpy(receivedFrame, message.c_str(), size); receivedFrameSize = size;
}
static void onEvent(WebsocketsEvent event, String) {
  if (event == WebsocketsEvent::ConnectionOpened) eventOpened = true;
  else if (event == WebsocketsEvent::ConnectionClosed) eventClosed = true;
  else if (event == WebsocketsEvent::GotPing) eventPing = true;
  else if (event == WebsocketsEvent::GotPong) eventPong = true;
}
static void processWebSocketCallbacks() {
  if (eventOpened) { eventOpened = false; authenticated = false; wssHealthy = true; Serial.printf("Pinned binary WSS connected; heap=%u\n", ESP.getFreeHeap()); }
  if (eventClosed) { eventClosed = false; authenticated = false; wssHealthy = registered = false; reconnectRequested = true; Serial.println("WSS closed; reconnecting"); }
  if (eventPing) { eventPing = false; client.pong(); }
  if (eventPong) { eventPong = false; lastPong = millis(); }
  if (receivedFrameInvalid) { receivedFrameInvalid = false; receivedFrameSize = 0; Serial.println("Rejected non-binary, oversized, or overlapping device frame"); client.close(CloseReason_ProtocolError); return; }
  if (receivedFrameSize) { const size_t size = receivedFrameSize; receivedFrameSize = 0; processMessage(receivedFrame, size); }
}
static void setupWebSocket() {
  client.onMessage(onMessage); client.onEvent(onEvent); client.setKnownKey(serverPublicKey);
  if (!client.connectSecure(settings.serverHost, settings.serverPort, "/device/v1")) { Serial.println("Pinned WSS connection failed"); reconnectRequested = true; return; }
  lastPong = millis();
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
#ifdef RBOOT_INTEGRATION
  if (!BootControl::begin()) { Serial.println("FATAL: invalid rBoot metadata/RTC state"); return; }
  temporaryBoot = BootControl::isTemporaryBoot(); temporaryStarted = millis();
  Serial.printf("rBoot slot=%c permanent=%c mode=%s\n", BootControl::currentSlot()?'B':'A', BootControl::permanentSlot()?'B':'A', temporaryBoot?"TEMPORARY":"PERMANENT");
#endif
  provisioning::Selection selected;
  if (!provisioning::loadStored(settings, &selected)) { clearRecoveryMarker(); Serial.println("UNPROVISIONED; NeoPixel DMA disabled; awaiting MFP1 serial provisioning envelope"); return; }
  Serial.printf("Provisioning copy %c generation %lu loaded\n", selected.copy == provisioning::COPY_A ? 'A' : 'B', (unsigned long)selected.generation);
  serverPublicKey = new BearSSL::PublicKey(settings.serverPublicKey, settings.serverPublicKeyLength); if (!serverPublicKey->isRSA() && !serverPublicKey->isEC()) { Serial.println("UNPROVISIONED; invalid server public key"); return; }
  if (mindflayer::health::shouldArmSerialRecovery(temporaryBoot, !temporaryBoot) && consumeRecoveryMarker()) { Serial.println("SERIAL PROVISIONING MODE; NeoPixel DMA disabled on GPIO3/RXD0"); return; }
  if (!mindflayer::health::shouldArmSerialRecovery(temporaryBoot, !temporaryBoot)) clearRecoveryMarker(); else { armRecoveryMarker(); Serial.println("Double-reset recovery window open"); delay(mindflayer::recovery::WINDOW_MS); clearRecoveryMarker(); }
  ledStrip = new (ledStripStorage) LedStrip(2, NEOPIXEL_DATA_PIN); ledStrip->Begin(); setColors(255, 0, 0, 0, 0, 0); Serial.println("NeoPixel DMA active on physical GPIO3/RXD0; serial RX disabled");
  provisioned = true; WiFi.hostname(settings.deviceId); WiFi.begin(settings.ssid, settings.wifiPassword);
  Serial.print("Connecting to provisioned Wi-Fi"); while (WiFi.status() != WL_CONNECTED) { Serial.print('.'); delay(500); }
  wifiHealthy = true; Serial.printf(" connected: %s; heap=%u\n", WiFi.localIP().toString().c_str(), ESP.getFreeHeap()); KeyboardMatrix::initMatrix(); setupWebSocket();
}
void onKeyChange(KeyboardMatrix::KeyState* key) { size_t written; if (protocol::buildKeyEvent(frameBuffer, sizeof(frameBuffer), written, key->key, key->isDown)) client.sendBinary((const char*)frameBuffer,written); }
void loop() {
  if (!provisioned) { processSerialProvisioning(); delay(10); return; } client.poll(); processWebSocketCallbacks(); flushFrame(); if (authenticated && client.available()) KeyboardMatrix::detectKeys(onKeyChange);
#ifdef RBOOT_INTEGRATION
  if (temporaryBoot && millis()-temporaryStarted > TEMPORARY_HEALTH_TIMEOUT_MS) { Serial.println("Temporary candidate health timeout; rebooting for rollback"); Serial.flush(); ESP.restart(); }
#ifdef TEST_FAIL_BEFORE_SERVER_ACK
  if (temporaryBoot && millis()-temporaryStarted > 3000) { Serial.println("TEST: failing temporary candidate before server acknowledgement"); Serial.flush(); ESP.restart(); }
#endif
#endif
  if (reconnectRequested) { reconnectRequested = false; delay(500); client = WebsocketsClient(); setupWebSocket(); }
  if (client.available() && millis() - lastPong > 15000) { client.ping(); lastPong = millis(); }
}
