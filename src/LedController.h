#pragma once

#include <stdint.h>

namespace LedController {

void begin();
// Changes only the left LED, and only when the connection state changes.
// Server-provided colors remain visible while the connection state is stable.
void showConnectionStatus(bool wifiConnected, bool serverAuthenticated);
// One one-second red fade in/out, followed by two seconds dark. Nonblocking.
void showUnprovisioned(uint32_t now);
void setColors(uint8_t r1, uint8_t g1, uint8_t b1, uint8_t r2, uint8_t g2, uint8_t b2);

} // namespace LedController
