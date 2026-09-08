#pragma once

#include <Arduino.h>

namespace DebugLog {

void setEnabled(bool enabled);
bool isEnabled();
void print(const char* message);
void println(const char* message);
void flush();

template <typename... Args> void printf(const char* format, Args... args) {
  if (isEnabled())
    Serial.printf(format, args...);
}

} // namespace DebugLog
