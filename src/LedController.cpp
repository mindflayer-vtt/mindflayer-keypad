#include "LedController.h"

#include <HardwareConfig.h>
#include <NeoPixelBus.h>
#include <new>

namespace {
using LedStrip = NeoPixelBus<NeoGrbFeature, Neo800KbpsMethod>;
alignas(LedStrip) uint8_t ledStripStorage[sizeof(LedStrip)];
LedStrip* ledStrip = nullptr;
int connectionStatus = -1;
} // namespace

namespace LedController {

void begin() {
  ledStrip = new (ledStripStorage) LedStrip(2, NEOPIXEL_DATA_PIN);
  ledStrip->Begin();
  connectionStatus = -1;
  ledStrip->SetPixelColor(1, RgbColor(0, 0, 0));
}

void showConnectionStatus(bool wifiConnected, bool serverAuthenticated) {
  if (!ledStrip)
    return;
  const int status = !wifiConnected ? 0 : serverAuthenticated ? 2 : 1;
  if (status == connectionStatus)
    return;
  connectionStatus = status;
  ledStrip->SetPixelColor(0, status == 0   ? RgbColor(255, 0, 0)
                             : status == 1 ? RgbColor(255, 255, 0)
                                           : RgbColor(0, 255, 0));
  ledStrip->Show();
}

void setColors(uint8_t r1, uint8_t g1, uint8_t b1, uint8_t r2, uint8_t g2, uint8_t b2) {
  if (!ledStrip)
    return;
  ledStrip->SetPixelColor(0, RgbColor(r1, g1, b1));
  ledStrip->SetPixelColor(1, RgbColor(r2, g2, b2));
  ledStrip->Show();
}

} // namespace LedController
