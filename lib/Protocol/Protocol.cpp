#include "Protocol.h"

#include <stdio.h>

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

}
}
