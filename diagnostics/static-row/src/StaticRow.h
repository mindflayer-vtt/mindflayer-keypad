#pragma once

#include <Arduino.h>

namespace StaticRow {
constexpr uint8_t rows[] = {5, 4, 0, 2};
constexpr uint8_t columns[] = {14, 12, 13};
constexpr uint8_t selectedRow = 0; // GPIO0 / D3: Z, X, C.

inline void begin() {
  for (auto pin : rows) {
    digitalWrite(pin, HIGH); // Preload the latch before enabling output.
    pinMode(pin, OUTPUT);
  }
  for (auto pin : columns)
    pinMode(pin, INPUT_PULLUP);
  digitalWrite(selectedRow, LOW);
}
} // namespace StaticRow
