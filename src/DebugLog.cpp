#include "DebugLog.h"

namespace {
bool enabled = false;
}

namespace DebugLog {

void setEnabled(bool value) { enabled = value; }
bool isEnabled() { return enabled; }
void print(const char* message) {
  if (enabled)
    Serial.print(message);
}
void println(const char* message) {
  if (enabled)
    Serial.println(message);
}
void flush() {
  if (enabled)
    Serial.flush();
}

} // namespace DebugLog
