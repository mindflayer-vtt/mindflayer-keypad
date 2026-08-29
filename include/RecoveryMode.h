#pragma once
#include <stdint.h>
namespace mindflayer { namespace recovery {
struct Marker { uint32_t magic; uint32_t inverse; };
constexpr uint32_t MAGIC = 0x4d465250; // MFRP
constexpr uint32_t RTC_OFFSET = 32; // Block offset 32 is byte 128, above eboot's RTC command.
constexpr uint32_t WINDOW_MS = 1500;
constexpr Marker makeMarker() { return {MAGIC, ~MAGIC}; }
constexpr bool markerValid(const Marker& marker) { return marker.magic == MAGIC && marker.inverse == ~MAGIC; }
} }
