#include "LedController.h"

#include <HardwareConfig.h>
#include <NeoPixelBus.h>
#include <new>

namespace {
using LedStrip = NeoPixelBus<NeoGrbFeature, Neo800KbpsMethod>;
alignas(LedStrip) uint8_t ledStripStorage[sizeof(LedStrip)];
LedStrip* ledStrip = nullptr;
} // namespace

namespace LedController {

void begin() {
  ledStrip = new (ledStripStorage) LedStrip(2, NEOPIXEL_DATA_PIN);
  ledStrip->Begin();
}

void setColors(uint8_t r1, uint8_t g1, uint8_t b1, uint8_t r2, uint8_t g2, uint8_t b2) {
  if (!ledStrip)
    return;
  ledStrip->SetPixelColor(0, RgbColor(r1, g1, b1));
  ledStrip->SetPixelColor(1, RgbColor(r2, g2, b2));
  ledStrip->Show();
}

} // namespace LedController
