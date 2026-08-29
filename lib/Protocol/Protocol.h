#pragma once

#include <stddef.h>
#include <stdint.h>

#include <ArduinoJson.h>

namespace mindflayer {
namespace protocol {

struct LedColor {
  uint8_t r;
  uint8_t g;
  uint8_t b;
};

struct Configuration {
  LedColor led1;
  LedColor led2;
};

struct AuthChallenge { char challenge[96]; };
struct UpdateAvailable {
  char version[48];
  char url[192];
  char token[96];
  char sha256[65];
  uint32_t size;
};

bool buildRegistration(char* output, size_t outputSize, const char* controllerId, const char* firmware = nullptr, const char* hardware = nullptr);
bool parseAuthChallenge(JsonDocument& document, const char* message, AuthChallenge& challenge);
bool buildAuthResponse(char* output, size_t outputSize, const char* deviceId, const char* secretHex, const char* challenge);
bool parseUpdateAvailable(JsonDocument& document, const char* message, UpdateAvailable& update);
bool buildKeyEvent(
  char* output,
  size_t outputSize,
  const char* controllerId,
  const char* key,
  bool isDown
);
bool shouldRestart(bool qIsDown, bool shiftIsDown, bool spaceIsDown);
bool parseConfiguration(
  JsonDocument& document,
  const char* message,
  Configuration& configuration
);

}
}
