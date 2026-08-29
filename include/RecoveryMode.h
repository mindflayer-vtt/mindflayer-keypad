#pragma once
#include <stdint.h>
namespace mindflayer { namespace recovery {
struct Marker { uint32_t magic; uint32_t inverse; };
constexpr uint32_t MAGIC = 0x4d465250; // MFRP
constexpr uint32_t RTC_OFFSET = 32; // Block offset 32 is byte 128, above eboot's RTC command.
constexpr uint32_t RTC_BYTE_START = RTC_OFFSET * 4;
constexpr uint32_t RTC_BYTE_END = RTC_BYTE_START + sizeof(Marker);
constexpr uint32_t RBOOT_RTC_WORD = 64;
constexpr uint32_t RBOOT_RTC_BYTE_START = RBOOT_RTC_WORD * 4;
constexpr uint32_t RBOOT_RTC_BYTE_END = RBOOT_RTC_BYTE_START + 12;
static_assert(RTC_BYTE_END <= RBOOT_RTC_BYTE_START, "serial recovery overlaps rBoot RTC state");
constexpr uint32_t WINDOW_MS = 1500;
constexpr Marker makeMarker() { return {MAGIC, ~MAGIC}; }
constexpr bool markerValid(const Marker& marker) { return marker.magic == MAGIC && marker.inverse == ~MAGIC; }
} }
