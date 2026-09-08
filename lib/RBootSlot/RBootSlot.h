#pragma once

#include <FlashLayout.h>
#include <stddef.h>
#include <stdint.h>

namespace RBootSlot {

constexpr uint32_t kSectorSize = mindflayer::flashlayout::SECTOR_SIZE;
constexpr uint32_t kSlotAAddress = mindflayer::flashlayout::SLOT_A_START;
constexpr uint32_t kSlotBAddress = mindflayer::flashlayout::SLOT_B_START;
constexpr uint32_t kSlotSize = mindflayer::flashlayout::SLOT_SIZE;
constexpr uint32_t kMetadataAAddress = mindflayer::flashlayout::BOOT_METADATA_A;
constexpr uint32_t kMetadataBAddress = mindflayer::flashlayout::BOOT_METADATA_B;
static_assert(kSlotAAddress + kSlotSize <= kMetadataBAddress, "slot A overlaps metadata B");
static_assert(kSlotBAddress + kSlotSize <= mindflayer::flashlayout::PROVISIONING_A,
              "slot B overlaps provisioning");

enum class Slot : uint8_t { A = 0, B = 1, Invalid = 0xff };

struct Geometry {
  uint32_t start;
  uint32_t size;
};

Geometry geometry(Slot slot);
Slot other(Slot slot);
uint32_t slotStart(Slot slot);
uint32_t slotEnd(Slot slot);
uint32_t slotCapacity(Slot slot);
bool validRange(Slot slot, uint32_t offset, uint32_t length);
bool validAbsoluteRange(Slot slot, uint32_t address, uint32_t length);

class Flash {
public:
  virtual ~Flash() {}
  virtual bool read(uint32_t address, void* data, size_t size) = 0;
  virtual bool eraseSector(uint32_t sector) = 0;
  virtual bool write(uint32_t address, const void* data, size_t size) = 0;
};

enum class Error : uint8_t {
  None,
  InvalidSlot,
  ActiveSlot,
  InvalidSize,
  WrongState,
  TooMuchData,
  FlashFailure,
  InvalidImage,
};

class Writer {
public:
  explicit Writer(Flash& flash, Slot active, Slot permanent = Slot::Invalid);
  bool begin(Slot target, uint32_t imageSize);
  bool write(const uint8_t* data, size_t size);
  bool finish();
  void abort();
  bool bootable() const { return state_ == State::Complete; }
  Error error() const { return error_; }

private:
  enum class State : uint8_t { Idle, Writing, Failed, Complete };
  bool flushWord(bool final);
  bool validate();
  bool fail(Error error);

  Flash& flash_;
  Slot active_;
  Slot permanent_;
  Slot target_;
  Geometry targetGeometry_;
  uint32_t imageSize_;
  uint32_t received_;
  uint32_t written_;
  uint8_t word_[4];
  uint8_t wordSize_;
  State state_;
  Error error_;
};

} // namespace RBootSlot
