#pragma once

#include <stdint.h>

namespace LedController {

void begin();
void setColors(uint8_t r1, uint8_t g1, uint8_t b1, uint8_t r2, uint8_t g2, uint8_t b2);

} // namespace LedController
