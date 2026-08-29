#include "Protocol.h"

#include <stdio.h>
#include <string.h>
#ifdef __linux__
#include <openssl/evp.h>
#include <openssl/hmac.h>
#else
extern "C" {
#include <bearssl/bearssl.h>
}
#endif

namespace mindflayer {
namespace protocol {

bool wasWritten(int length, size_t outputSize) {
  return length >= 0 && static_cast<size_t>(length) < outputSize;
}

bool buildRegistration(char* output, size_t outputSize, const char* controllerId, const char* firmware, const char* hardware) {
  const int length = snprintf(
    output,
    outputSize,
    firmware && hardware
      ? "{\"type\":\"registration\",\"controller-id\":\"%s\",\"status\":\"connected\",\"receiver\":false,\"firmware\":\"%s\",\"hardware\":\"%s\"}"
      : "{\"type\":\"registration\",\"controller-id\": \"%s\",\"status\":\"connected\",\"receiver\":false}",
    controllerId, firmware, hardware
  );
  return wasWritten(length, outputSize);
}

bool copyString(char* output, size_t size, const char* input) {
  if (!input || strlen(input) >= size) return false;
  strcpy(output, input); return true;
}

bool parseAuthChallenge(JsonDocument& document, const char* message, AuthChallenge& challenge) {
  if (deserializeJson(document, message) || strcmp(document["type"] | "", "auth-challenge") || document["version"].as<int>() != 1)
    return false;
  return copyString(challenge.challenge, sizeof(challenge.challenge), document["challenge"]);
}

bool decodeSecret(const char* hex, unsigned char output[32]) {
  if (!hex || strlen(hex) != 64) return false;
  for (size_t i = 0; i < 32; i++) {
    unsigned value;
    if (sscanf(hex + i * 2, "%2x", &value) != 1) return false;
    output[i] = static_cast<unsigned char>(value);
  }
  return true;
}

void appendField(unsigned char* input, size_t& offset, const char* value) {
  const size_t length = strlen(value);
  input[offset++] = (length >> 24) & 0xff; input[offset++] = (length >> 16) & 0xff;
  input[offset++] = (length >> 8) & 0xff; input[offset++] = length & 0xff;
  memcpy(input + offset, value, length); offset += length;
}

bool buildAuthResponse(char* output, size_t outputSize, const char* deviceId, const char* secretHex, const char* challenge) {
  static const char* domain = "mindflayer-device-auth-v1";
  if (!deviceId || !challenge || strlen(deviceId) > 64 || strlen(challenge) > 96) return false;
  unsigned char secret[32], input[4 + 25 + 4 + 64 + 4 + 96], digest[32];
  if (!decodeSecret(secretHex, secret)) return false;
  size_t inputLength = 0; appendField(input, inputLength, domain); appendField(input, inputLength, deviceId); appendField(input, inputLength, challenge);
#ifdef __linux__
  unsigned int digestLength = sizeof(digest);
  HMAC(EVP_sha256(), secret, sizeof(secret), input, inputLength, digest, &digestLength);
#else
  br_hmac_key_context key; br_hmac_context context;
  br_hmac_key_init(&key, &br_sha256_vtable, secret, sizeof(secret)); br_hmac_init(&context, &key, 0);
  br_hmac_update(&context, input, inputLength); br_hmac_out(&context, digest);
#endif
  char hex[65]; for (size_t i = 0; i < 32; i++) snprintf(hex + i * 2, 3, "%02x", digest[i]);
  const int length = snprintf(output, outputSize, "{\"type\":\"auth-response\",\"device-id\":\"%s\",\"hmac\":\"%s\"}", deviceId, hex);
  return wasWritten(length, outputSize);
}

bool parseUpdateAvailable(JsonDocument& document, const char* message, UpdateAvailable& update) {
  if (deserializeJson(document, message) || strcmp(document["type"] | "", "update-available")) return false;
  const char* digest = document["sha256"] | "";
  if (strlen(digest) != 64 || !document["size"].is<uint32_t>() || document["size"].as<uint32_t>() == 0) return false;
  update.size = document["size"];
  return copyString(update.version, sizeof(update.version), document["version"])
    && copyString(update.url, sizeof(update.url), document["url"])
    && copyString(update.token, sizeof(update.token), document["token"])
    && copyString(update.sha256, sizeof(update.sha256), digest);
}

bool buildKeyEvent(
  char* output,
  size_t outputSize,
  const char* controllerId,
  const char* key,
  bool isDown
) {
  const int length = snprintf(
    output,
    outputSize,
    "{\"type\":\"key-event\",\"controller-id\": \"%s\",\"key\":\"%s\",\"state\":\"%s\"}",
    controllerId,
    key,
    isDown ? "down" : "up"
  );
  return wasWritten(length, outputSize);
}

bool shouldRestart(bool qIsDown, bool shiftIsDown, bool spaceIsDown) {
  return qIsDown && shiftIsDown && spaceIsDown;
}

bool parseConfiguration(
  JsonDocument& document,
  const char* message,
  Configuration& configuration
) {
  const DeserializationError error = deserializeJson(document, message);
  if (error || strcmp(document["type"].as<const char*>(), "configuration") != 0) {
    return false;
  }
  configuration.led1 = {
    document["led1"]["r"].as<uint8_t>(),
    document["led1"]["g"].as<uint8_t>(),
    document["led1"]["b"].as<uint8_t>()
  };
  configuration.led2 = {
    document["led2"]["r"].as<uint8_t>(),
    document["led2"]["g"].as<uint8_t>(),
    document["led2"]["b"].as<uint8_t>()
  };
  return true;
}

}
}
