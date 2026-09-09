// Exercise the actual GPIO scanner with deterministic switch traces and time.
#include <Arduino.h>
#include <KeyboardMatrix.h>
#include <cassert>
#include <cstdio>
#include <vector>

namespace matrix = com::viromania::vtt::wss::KeyboardMatrix;
namespace {
uint32_t now = 0;
constexpr uint8_t rows[] = {5, 4, 0, 2}, cols[] = {14, 12, 13};
bool pressed[4][3] = {};
bool rowLow[4] = {};
struct Event {
  std::string key;
  bool down;
  uint32_t time;
};
std::vector<Event> events;
bool expectChordSnapshot = false;

void onKey(matrix::KeyState* key) {
  if (expectChordSnapshot) {
    // Network callbacks must run only after releasing every matrix row.
    for (bool low : rowLow)
      assert(!low);
    const auto& keys = *matrix::getState();
    assert(keys[0][2].isDown && keys[3][0].isDown && keys[3][2].isDown);
  }
  events.push_back({key->key, key->isDown, now});
}
void advance(uint32_t duration) {
  for (uint32_t elapsed = 0; elapsed < duration; ++elapsed) {
    ++now;
    matrix::detectKeys(onKey);
  }
}
void expectPair(const char* key) {
  assert(events.size() == 2);
  assert(events[0].key == key && events[0].down);
  assert(events[1].key == key && !events[1].down);
}
} // namespace

unsigned long millis() { return now; }
void pinMode(uint8_t pin, int mode) {
  bool found = false;
  for (auto row : rows)
    if (pin == row) {
      assert(mode == OUTPUT);
      found = true;
    }
  for (auto col : cols)
    if (pin == col) {
      assert(mode == INPUT_PULLUP);
      found = true;
    }
  assert(found);
}
void digitalWrite(uint8_t pin, int value) {
  for (unsigned row = 0; row < 4; ++row) {
    if (pin == rows[row]) {
      rowLow[row] = value == LOW;
      return;
    }
  }
  assert(false);
}
int digitalRead(uint8_t pin) {
  unsigned active = 4;
  for (unsigned row = 0; row < 4; ++row)
    if (rowLow[row]) {
      assert(active == 4);
      active = row;
    }
  assert(active < 4);
  for (unsigned col = 0; col < 3; ++col)
    if (pin == cols[col])
      return pressed[active][col] ? LOW : HIGH;
  assert(false);
  return HIGH;
}

int main(int argc, char** argv) {
  assert(argc == 2);
  const std::string scenario = argv[1];
  if (scenario == "wrap")
    now = UINT32_MAX - 12;
  matrix::initMatrix();
  if (scenario == "all-keys") {
    const char* names[4][3] = {
        {"Q", "W", "E"}, {"A", "S", "D"}, {"Z", "X", "C"}, {"SHI", "", "SPC"}};
    for (unsigned row = 0; row < 4; ++row) {
      for (unsigned col = 0; col < 3; ++col) {
        events.clear();
        pressed[row][col] = true;
        advance(34);
        assert(events.empty());
        advance(1);
        assert(events.size() == (names[row][col][0] ? 1u : 0u));
        advance(1000);
        pressed[row][col] = false;
        advance(34);
        assert(events.size() == (names[row][col][0] ? 1u : 0u));
        advance(1);
        if (names[row][col][0])
          expectPair(names[row][col]);
        else
          assert(events.empty());
      }
    }
  } else if (scenario == "bounce") {
    for (bool level : {true, false, true, false, true}) {
      pressed[1][1] = level;
      advance(5);
      assert(events.empty());
    }
    advance(29);
    assert(events.empty());
    advance(1);
    assert(events.size() == 1 && events[0].time == 55);
    advance(1000);
    for (bool level : {false, true, false, true, false}) {
      pressed[1][1] = level;
      advance(5);
      assert(events.size() == 1);
    }
    advance(30);
    expectPair("S");
  } else if (scenario == "glitches") {
    pressed[0][0] = true;
    advance(25); // Longer than the old threshold, shorter than 30 ms.
    pressed[0][0] = false;
    advance(30);
    assert(events.empty());
    pressed[0][0] = true;
    advance(35);
    assert(events.size() == 1);
    pressed[0][0] = false;
    advance(25);
    pressed[0][0] = true;
    advance(30);
    assert(events.size() == 1);
    pressed[0][0] = false;
    advance(35);
    expectPair("Q");
  } else if (scenario == "independent") {
    pressed[3][0] = true;
    advance(5);
    pressed[3][2] = true;
    advance(5);
    pressed[0][2] = true;
    for (unsigned i = 0; i < 8; ++i) {
      pressed[1][1] = !pressed[1][1]; // Noise on S must not delay modifiers/E.
      advance(5);
    }
    assert(events.size() == 3);
    assert(events[0].key == "SHI" && events[0].time == 35);
    assert(events[1].key == "SPC" && events[1].time == 40);
    assert(events[2].key == "E" && events[2].time == 45);
  } else if (scenario == "snapshot") {
    pressed[0][2] = pressed[3][0] = pressed[3][2] = true;
    expectChordSnapshot = true;
    advance(35);
    assert(events.size() == 3);
  } else if (scenario == "wrap") {
    const uint32_t started = now;
    pressed[2][1] = true;
    advance(34);
    assert(events.empty());
    advance(1);
    assert(events.size() == 1 && uint32_t(events[0].time - started) == 35);
    pressed[2][1] = false;
    advance(35);
    expectPair("X");
  } else if (scenario == "reinitialize") {
    pressed[0][0] = true;
    advance(35);
    assert((*matrix::getState())[0][0].isDown);
    matrix::initMatrix();
    events.clear();
    assert(!(*matrix::getState())[0][0].isDown);
    advance(34);
    assert(events.empty());
    advance(1);
    assert(events.size() == 1 && events[0].down);
  } else
    assert(false);
  std::puts("keyboard matrix regression passed");
}
