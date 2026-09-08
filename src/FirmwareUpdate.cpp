#include "FirmwareUpdate.h"

#include "ApplicationState.h"
#include "BuildConfig.h"
#include "DebugLog.h"

#include <Arduino.h>
#include <ESP8266HTTPClient.h>
#include <ESP8266httpUpdate.h>
#include <HardwareConfig.h>
#ifdef RBOOT_INTEGRATION
#include <BootControl.h>
#include <RBootSlot.h>
#include <RBootTestHooks.h>
#endif

namespace {
BearSSL::PublicKey firmwareSigningPublicKey(FIRMWARE_SIGNING_PUBLIC_KEY_PEM);
BearSSL::HashSHA256 firmwareHash;
BearSSL::SigningVerifier firmwareVerifier(&firmwareSigningPublicKey);

#ifdef RBOOT_INTEGRATION
uint8_t otaBuffer[512], otaSignature[512], otaDigest[32];
br_sha256_context otaArtifactHash;

class EspSlotFlash : public RBootSlot::Flash {
public:
  bool read(uint32_t address, void* data, size_t size) override {
    return ESP.flashRead(address, static_cast<uint8_t*>(data), size);
  }
  bool eraseSector(uint32_t sector) override { return ESP.flashEraseSector(sector); }
  bool write(uint32_t address, const void* data, size_t size) override {
    return ESP.flashWrite(address, const_cast<uint8_t*>(static_cast<const uint8_t*>(data)), size);
  }
};

EspSlotFlash slotFlash;
#endif

void base64Url(const uint8_t input[32], char output[44]) {
  static const char alphabet[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789-_";
  size_t in = 0, out = 0;
  while (in + 3 <= 32) {
    const uint32_t value =
        ((uint32_t)input[in] << 16) | ((uint32_t)input[in + 1] << 8) | input[in + 2];
    in += 3;
    output[out++] = alphabet[value >> 18];
    output[out++] = alphabet[(value >> 12) & 63];
    output[out++] = alphabet[(value >> 6) & 63];
    output[out++] = alphabet[value & 63];
  }
  const uint32_t value = (uint32_t)input[in] << 16 | (uint32_t)input[in + 1] << 8;
  output[out++] = alphabet[value >> 18];
  output[out++] = alphabet[(value >> 12) & 63];
  output[out++] = alphabet[(value >> 6) & 63];
  output[out] = 0;
}
} // namespace

namespace FirmwareUpdate {

void installSignatureVerifier() { Update.installSignature(&firmwareHash, &firmwareVerifier); }

void perform(const mindflayer::protocol::UpdateAvailable& update) {
  ApplicationState& state = applicationState();
  DebugLog::printf("Starting signed OTA to %s (%lu bytes)\n", update.version,
                   (unsigned long)update.size);
  BearSSL::WiFiClientSecure secureClient;
  secureClient.setKnownKey(state.serverPublicKey);
  HTTPClient http;
  const String url = String("https://") + state.settings.serverHost + ':' +
                     state.settings.serverPort + update.path;
  if (!http.begin(secureClient, url)) {
    DebugLog::println("OTA HTTPS setup failed");
    return;
  }
  char bearer[44];
  base64Url(update.token, bearer);
  http.addHeader("Authorization", String("Bearer ") + bearer);
#ifdef RBOOT_INTEGRATION
  const uint32_t signatureSize = firmwareVerifier.length(), trailerSize = signatureSize + 4;
  if (state.temporaryBoot || update.size <= trailerSize) {
    DebugLog::println("OTA rejected: no safe inactive slot or invalid signed size");
    http.end();
    return;
  }
  const auto active = static_cast<RBootSlot::Slot>(BootControl::currentSlot());
  const auto target = RBootSlot::other(active);
  RBootSlot::Writer writer(slotFlash, active,
                           static_cast<RBootSlot::Slot>(BootControl::permanentSlot()));
  const uint32_t imageSize = update.size - trailerSize;
  if (!writer.begin(target, imageSize)) {
    DebugLog::println("OTA rejected before erase by slot bounds/state");
    http.end();
    return;
  }
  const int status = http.GET();
  if (status != HTTP_CODE_OK || http.getSize() != static_cast<int>(update.size)) {
    DebugLog::printf("OTA HTTPS response rejected: %d\n", status);
    writer.abort();
    http.end();
    return;
  }
  WiFiClient& stream = http.getStream();
  uint32_t received = 0;
  unsigned long lastData = millis();
  br_sha256_init(&otaArtifactHash);
  firmwareHash.begin();
  bool ok = true;
  while (ok && received < update.size) {
    size_t available = stream.available();
    if (!available) {
      if (!stream.connected() || millis() - lastData > 60000) {
        ok = false;
        break;
      }
      delay(1);
      continue;
    }
    size_t count =
        min(available, min(sizeof(otaBuffer), static_cast<size_t>(update.size - received)));
    const int got = stream.readBytes(otaBuffer, count);
    if (got <= 0) {
      ok = false;
      break;
    }
    count = got;
    lastData = millis();
    br_sha256_update(&otaArtifactHash, otaBuffer, count);
    size_t offset = 0;
    if (received < imageSize) {
      const size_t body = min(count, static_cast<size_t>(imageSize - received));
      firmwareHash.add(otaBuffer, body);
      ok = writer.write(otaBuffer, body);
      offset = body;
    }
    if (ok && offset < count) {
      const size_t tailOffset = received + offset - imageSize;
      if (tailOffset + count - offset > sizeof(otaSignature)) {
        ok = false;
        break;
      }
      memcpy(otaSignature + tailOffset, otaBuffer + offset, count - offset);
    }
    received += count;
    delay(0);
  }
  br_sha256_out(&otaArtifactHash, otaDigest);
  firmwareHash.end();
  uint32_t encodedLength = 0;
  if (received == update.size)
    memcpy(&encodedLength, otaSignature + signatureSize, 4);
  ok = ok && received == update.size && memcmp(otaDigest, update.sha256, 32) == 0 &&
       encodedLength == signatureSize &&
       firmwareVerifier.verify(&firmwareHash, otaSignature, signatureSize) && writer.finish();
  if (!ok) {
    DebugLog::println("Signed rBoot OTA rejected; permanent slot unchanged");
    writer.abort();
    http.end();
    return;
  }
  if (!RBootTestHooks::corruptCandidateAfterValidation(slotFlash, target)) {
    http.end();
    return;
  }
  DebugLog::printf("Validated candidate in slot %c; requesting temporary boot\n",
                   target == RBootSlot::Slot::A ? 'A' : 'B');
  http.end();
  if (!BootControl::bootTemporary(static_cast<uint8_t>(target))) {
    DebugLog::println("Temporary boot request failed");
    return;
  }
  DebugLog::flush();
  ESP.restart();
#else
  ESPhttpUpdate.rebootOnUpdate(true);
  ESPhttpUpdate.onError([](int error) {
    DebugLog::printf("Signed OTA rejected: %d %s\n", error,
                     ESPhttpUpdate.getLastErrorString().c_str());
  });
  const auto result = ESPhttpUpdate.update(http, FIRMWARE_VERSION);
  if (result == HTTP_UPDATE_FAILED)
    DebugLog::printf("OTA failed; retaining %s\n", FIRMWARE_VERSION);
  http.end();
#endif
}

} // namespace FirmwareUpdate
