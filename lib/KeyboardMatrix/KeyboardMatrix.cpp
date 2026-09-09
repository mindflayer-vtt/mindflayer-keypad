#include "KeyboardMatrix.h"
#include <Arduino.h>

using namespace com::viromania::vtt::wss;

const byte ROWS = 4;               // four rows
const byte COLS = 3;               // three columns
byte rowPins[ROWS] = {5, 4, 0, 2}; // connect to the row pinouts of the kpd
byte colPins[COLS] = {14, 12, 13}; // connect to the column pinouts of the kpd
KeyboardMatrix::KeyState keys[ROWS][COLS] = {
    {{"Q"}, {"W"}, {"E"}},
    {{"A"}, {"S"}, {"D"}},
    {{"Z"}, {"X"}, {"C"}},
    {{"SHI"}, {""}, {"SPC"}},
};
namespace {
struct DebounceState {
  bool candidateDown = false;
  uint32_t changedAt = 0;
};
DebounceState debounce[ROWS][COLS];
uint32_t lastScan = 0;
} // namespace

KeyboardMatrix::KeyState::KeyState(const char* setKey) {
  strncpy(this->key, setKey, 3);
  this->key[3] = 0;
  this->isDown = false;
}

void KeyboardMatrix::initMatrix() {
  for (int i = 0; i < COLS; i++) {
    pinMode(colPins[i], INPUT_PULLUP);
  }
  for (int i = 0; i < ROWS; i++) {
    pinMode(rowPins[i], OUTPUT);
    digitalWrite(rowPins[i], HIGH);
  }
  lastScan = millis();
  for (unsigned row = 0; row < ROWS; ++row) {
    for (unsigned col = 0; col < COLS; ++col) {
      keys[row][col].isDown = false;
      debounce[row][col] = {false, lastScan};
    }
  }
}

void KeyboardMatrix::detectKeys(void (*callback)(KeyState* key)) {
  const uint32_t now = millis();
  if (uint32_t(now - lastScan) < SCAN_INTERVAL_MS) {
    return;
  }
  lastScan = now;
  bool raw[ROWS][COLS], changed[ROWS][COLS] = {};
  // Finish sampling and release all rows before a callback can perform network
  // I/O. Every callback sees the same completed, debounced matrix snapshot.
  for (unsigned row = 0; row < ROWS; ++row) {
    digitalWrite(rowPins[row], LOW);
    for (unsigned col = 0; col < COLS; ++col)
      raw[row][col] = digitalRead(colPins[col]) == LOW;
    digitalWrite(rowPins[row], HIGH);
  }
  for (unsigned row = 0; row < ROWS; ++row) {
    for (unsigned col = 0; col < COLS; ++col) {
      auto& key = keys[row][col];
      auto& filter = debounce[row][col];
      if (!key.key[0])
        continue;
      if (raw[row][col] != filter.candidateDown) {
        filter.candidateDown = raw[row][col];
        filter.changedAt = now;
      } else if (key.isDown != filter.candidateDown &&
                 uint32_t(now - filter.changedAt) >= DEBOUNCE_MS) {
        key.isDown = filter.candidateDown;
        changed[row][col] = true;
      }
    }
  }
  for (unsigned row = 0; row < ROWS; ++row)
    for (unsigned col = 0; col < COLS; ++col)
      if (changed[row][col])
        callback(&keys[row][col]);
}

KeyboardMatrix::KeyState (*KeyboardMatrix::getState())[4][3] { return &keys; }
