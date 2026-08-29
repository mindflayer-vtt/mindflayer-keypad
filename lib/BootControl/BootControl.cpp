#include "BootControl.h"
#include <BootMetadata.h>

#include <Arduino.h>
extern "C" {
#include <user_interface.h>
}

#include <stddef.h>
#include <string.h>

namespace {

constexpr uint8_t kChecksumInitial = 0xef;
constexpr uint8_t kConfigMagic = 0xe1;
constexpr uint8_t kConfigVersion = 0x01;
constexpr uint8_t kModeStandard = 0x00;
constexpr uint8_t kModeTemporary = 0x02;
constexpr uint32_t kRtcMagic = 0x2334ae68;
constexpr uint8_t kRtcAddress = 64; // RTC byte 256; asserted disjoint from recovery in RecoveryMode.h.

struct __attribute__((packed)) Config {
  uint8_t magic;
  uint8_t version;
  uint8_t mode;
  uint8_t currentRom;
  uint8_t gpioRom;
  uint8_t count;
  uint8_t unused[2];
  uint32_t roms[4];
  uint8_t checksum;
};

struct __attribute__((packed, aligned(4))) RtcData {
  uint32_t magic;
  uint8_t nextMode;
  uint8_t lastMode;
  uint8_t lastRom;
  uint8_t temporaryRom;
  uint8_t checksum;
  uint8_t padding[3];
};

static_assert(sizeof(Config) == sizeof(BootMetadata::Config), "rBoot config layout drift");

Config config;
RtcData rtc;
bool rtcValid = false;

class EspFlash : public RBootSlot::Flash {
 public:
  bool read(uint32_t address, void* data, size_t size) override {
    return ESP.flashRead(address, static_cast<uint8_t*>(data), size);
  }
  bool eraseSector(uint32_t sector) override { return ESP.flashEraseSector(sector); }
  bool write(uint32_t address, const void* data, size_t size) override {
    return ESP.flashWrite(address, const_cast<uint8_t*>(static_cast<const uint8_t*>(data)), size);
  }
};

EspFlash flash;
BootMetadata::Store metadata(flash);

uint8_t checksum(const uint8_t* begin, const uint8_t* end) {
  uint8_t value = kChecksumInitial;
  while (begin < end) value ^= *begin++;
  return value;
}

bool validConfig(const Config& candidate) {
  return candidate.magic == kConfigMagic && candidate.version == kConfigVersion &&
         candidate.count == 2 && candidate.currentRom < candidate.count &&
         candidate.roms[0] == BootControl::kSlotAAddress &&
         candidate.roms[1] == BootControl::kSlotBAddress &&
         candidate.checksum == checksum(reinterpret_cast<const uint8_t*>(&candidate),
                                        reinterpret_cast<const uint8_t*>(&candidate.checksum));
}

bool readRtc() {
  if (!system_rtc_mem_read(kRtcAddress, &rtc, sizeof(rtc))) return false;
  return rtc.magic == kRtcMagic &&
         rtc.checksum == checksum(reinterpret_cast<const uint8_t*>(&rtc),
                                  reinterpret_cast<const uint8_t*>(&rtc.checksum));
}

bool writeRtc() {
  rtc.magic = kRtcMagic;
  rtc.checksum = checksum(reinterpret_cast<const uint8_t*>(&rtc),
                          reinterpret_cast<const uint8_t*>(&rtc.checksum));
  return system_rtc_mem_write(kRtcAddress, &rtc, sizeof(rtc));
}

}  // namespace

namespace BootControl {

bool begin() {
  BootMetadata::Record record;
  if (metadata.load(record)) memcpy(&config, &record.config, sizeof(config));
  else {
    memset(&config, 0, sizeof(config));
    config.magic = kConfigMagic; config.version = kConfigVersion; config.currentRom = 0;
    config.count = 2; config.roms[0] = BootControl::kSlotAAddress; config.roms[1] = BootControl::kSlotBAddress;
    config.checksum = checksum(reinterpret_cast<const uint8_t*>(&config), reinterpret_cast<const uint8_t*>(&config.checksum));
  }
  if (!validConfig(config)) return false;
  rtcValid = readRtc();
  return rtcValid;
}

uint8_t currentSlot() { return rtcValid ? rtc.lastRom : config.currentRom; }
uint8_t permanentSlot() { return config.currentRom; }

bool isTemporaryBoot() { return rtcValid && (rtc.lastMode & kModeTemporary); }

bool bootTemporary(uint8_t slot) {
  if (slot >= config.count || slot == currentSlot()) return false;
  if (!rtcValid) {
    memset(&rtc, 0, sizeof(rtc));
    rtc.lastMode = kModeStandard;
    rtc.lastRom = config.currentRom;
  }
  rtc.nextMode = kModeTemporary;
  rtc.temporaryRom = slot;
  return writeRtc();
}

bool promoteCurrentSlot() {
  if (!rtcValid || !isTemporaryBoot() || currentSlot() >= config.count) return false;
  config.currentRom = currentSlot();
  if (!metadata.commit(*reinterpret_cast<BootMetadata::Config*>(&config))) return false;
  rtc.nextMode = kModeStandard;
  return writeRtc();
}

#ifdef RBOOT_FAULT_INJECTION
namespace {
void resetAtStage(BootMetadata::Stage stage, void* context) {
  if (static_cast<uint8_t>(stage) == *static_cast<uint8_t*>(context)) ESP.restart();
}
}

bool promoteCurrentSlotWithReset(uint8_t stage) {
  if (!rtcValid || !isTemporaryBoot() || currentSlot() >= config.count) return false;
  config.currentRom = currentSlot();
  return metadata.commit(*reinterpret_cast<BootMetadata::Config*>(&config), resetAtStage, &stage);
}
#endif

}  // namespace BootControl
