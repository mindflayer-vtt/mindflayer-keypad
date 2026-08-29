#pragma once

#include <stddef.h>

namespace mindflayer {
namespace protocol {

bool buildRegistration(char* output, size_t outputSize, const char* controllerId);
bool buildKeyEvent(
  char* output,
  size_t outputSize,
  const char* controllerId,
  const char* key,
  bool isDown
);
bool shouldRestart(bool qIsDown, bool shiftIsDown, bool spaceIsDown);

}
}
