#pragma once

#include <cstdint>
#include <cstring>
#include <string>

using String = std::string;
unsigned long millis();
void delay(unsigned long milliseconds);
void optimistic_yield(uint32_t);

struct FakeSerial {
  template <typename... Args> void printf(const char*, Args...) {}
};
inline FakeSerial Serial;
struct FakeEsp {
  unsigned long getFlashChipRealSize() { return 0x400000; }
  unsigned long getFlashChipSize() { return 0x400000; }
  void restart();
};
inline FakeEsp ESP;
