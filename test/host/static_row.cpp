#include "../../diagnostics/static-row/src/StaticRow.h"

#include <cassert>

namespace {
int modes[17] = {};
int levels[17] = {};
unsigned writes = 0, configurations = 0, reads = 0;
} // namespace

void digitalWrite(uint8_t pin, int value) {
  assert(pin == 5 || pin == 4 || pin == 0 || pin == 2);
  if (value == LOW) {
    assert(pin == 0);
    assert(configurations == 7); // All rows and pull-ups configured first.
  } else
    assert(value == HIGH);
  levels[pin] = value;
  ++writes;
}

void pinMode(uint8_t pin, int mode) {
  if (pin == 14 || pin == 12 || pin == 13)
    assert(mode == INPUT_PULLUP);
  else {
    assert(pin == 5 || pin == 4 || pin == 0 || pin == 2);
    assert(mode == OUTPUT && levels[pin] == HIGH);
  }
  modes[pin] = mode;
  ++configurations;
}

int digitalRead(uint8_t) {
  ++reads;
  return HIGH;
}

int main() {
  StaticRow::begin();
  assert(writes == 5 && configurations == 7 && reads == 0);
  assert(modes[0] == OUTPUT && levels[0] == LOW);
  for (auto pin : {5, 4, 2})
    assert(modes[pin] == OUTPUT && levels[pin] == HIGH);
  for (auto pin : {14, 12, 13})
    assert(modes[pin] == INPUT_PULLUP);
}
