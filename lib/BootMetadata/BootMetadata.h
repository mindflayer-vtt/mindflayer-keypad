#pragma once

#include <RBootSlot.h>
#include <stdint.h>

namespace BootMetadata {

constexpr uint32_t kMagic = 0x4d465242;
constexpr uint32_t kFormat = 1;
constexpr uint32_t kCommit = 0x434d4954;
constexpr uint32_t kCommitOffset = RBootSlot::kSectorSize - sizeof(uint32_t);

struct __attribute__((packed)) Config {
  uint8_t magic, version, mode, currentRom, gpioRom, count, unused[2];
  uint32_t roms[4];
  uint8_t checksum;
};

struct __attribute__((packed)) Record {
  uint32_t magic;
  uint32_t format;
  uint32_t generation;
  Config config;
  uint32_t crc32;
};

enum class Stage : uint8_t { Erased, PartialBodyWritten, BodyWritten, BodyVerified, Committed };
using Hook = void (*)(Stage stage, void* context);

uint32_t crc32(const void* data, size_t size);
bool valid(const Record& record, uint32_t commit);
bool newer(uint32_t left, uint32_t right);

class Store {
public:
  explicit Store(RBootSlot::Flash& flash) : flash_(flash), activeAddress_(0) {}
  bool load(Record& record);
  bool commit(const Config& config, Hook hook = nullptr, void* context = nullptr);
  uint32_t activeAddress() const { return activeAddress_; }

private:
  bool read(uint32_t address, Record& record);
  RBootSlot::Flash& flash_;
  uint32_t activeAddress_;
};

} // namespace BootMetadata
