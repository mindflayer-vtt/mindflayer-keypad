#include "Protocol.h"

#include <stdio.h>
#include <string.h>

namespace mindflayer {
namespace protocol {

bool wasWritten(int length, size_t outputSize) {
  return length >= 0 && static_cast<size_t>(length) < outputSize;
}

bool buildRegistration(char* output, size_t outputSize, const char* controllerId) {
  const int length = snprintf(
    output,
    outputSize,
    "{\"type\":\"registration\",\"controller-id\": \"%s\",\"status\":\"connected\",\"receiver\":false}",
    controllerId
  );
  return wasWritten(length, outputSize);
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
  DynamicJsonDocument& document,
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
