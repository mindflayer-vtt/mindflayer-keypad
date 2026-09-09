#pragma once

#include <Arduino.h>
#include <cassert>
#include <cstdint>

struct RgbColor {
  uint8_t R, G, B;
  RgbColor(uint8_t r = 0, uint8_t g = 0, uint8_t b = 0) : R(r), G(g), B(b) {}
};
struct NeoGrbFeature {};
struct Neo800KbpsMethod {};
inline RgbColor testLedPixels[2];
inline RgbColor testFirstShown[2];
inline unsigned testLedShows = 0, testLedBegins = 0;

template <typename Feature, typename Method> struct NeoPixelBus {
  NeoPixelBus(unsigned count, unsigned pin) {
    assert(count == 2);
    assert(pin == 3);
  }
  void Begin() { ++testLedBegins; }
  void SetPixelColor(unsigned index, RgbColor color) {
    assert(index < 2);
    testLedPixels[index] = color;
  }
  void Show() {
    if (!testLedShows) {
      testFirstShown[0] = testLedPixels[0];
      testFirstShown[1] = testLedPixels[1];
    }
    ++testLedShows;
  }
};
