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

bool buildRegistration(char* output, size_t outputSize, const char* controllerId);
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
