#include "RBootSlot.h"

#include <string.h>

namespace RBootSlot {
namespace {

constexpr uint8_t kChecksumInitial = 0xef;
constexpr uint32_t kIramStart = 0x40100000;
constexpr uint32_t kIramEnd = 0x40110000;
constexpr uint32_t kDramStart = 0x3ffe8000;
constexpr uint32_t kDramEnd = 0x40000000;

uint32_t readLe32(const uint8_t* p) {
  return static_cast<uint32_t>(p[0]) | (static_cast<uint32_t>(p[1]) << 8) |
         (static_cast<uint32_t>(p[2]) << 16) | (static_cast<uint32_t>(p[3]) << 24);
}

bool rangeWithin(uint32_t address, uint32_t size, uint32_t start, uint32_t end) {
  return address >= start && size <= end - start && address <= end - size;
}

} // namespace

Geometry geometry(Slot slot) {
  if (slot == Slot::A)
    return {kSlotAAddress, kSlotSize};
  if (slot == Slot::B)
    return {kSlotBAddress, kSlotSize};
  return {0, 0};
}

Slot other(Slot slot) {
  return slot == Slot::A ? Slot::B : slot == Slot::B ? Slot::A : Slot::Invalid;
}

uint32_t slotStart(Slot slot) { return geometry(slot).start; }
uint32_t slotEnd(Slot slot) {
  const Geometry value = geometry(slot);
  return value.size ? value.start + value.size - 1 : 0;
}
uint32_t slotCapacity(Slot slot) { return geometry(slot).size; }
bool validRange(Slot slot, uint32_t offset, uint32_t length) {
  const uint32_t capacity = slotCapacity(slot);
  return length <= capacity && offset <= capacity - length;
}
bool validAbsoluteRange(Slot slot, uint32_t address, uint32_t length) {
  const Geometry value = geometry(slot);
  return value.size && address >= value.start && validRange(slot, address - value.start, length);
}

Writer::Writer(Flash& flash, Slot active, Slot permanent)
    : flash_(flash), active_(active), permanent_(permanent), target_(Slot::Invalid),
      targetGeometry_{0, 0}, imageSize_(0), received_(0), written_(0),
      word_{0xff, 0xff, 0xff, 0xff}, wordSize_(0), state_(State::Idle), error_(Error::None) {}

bool Writer::fail(Error error) {
  error_ = error;
  state_ = State::Failed;
  return false;
}

bool Writer::begin(Slot target, uint32_t imageSize) {
  if (state_ == State::Writing)
    return fail(Error::WrongState);
  targetGeometry_ = geometry(target);
  if (!targetGeometry_.size)
    return fail(Error::InvalidSlot);
  if (target == active_ || target == permanent_)
    return fail(Error::ActiveSlot);
  if (!imageSize || !validRange(target, 0, imageSize))
    return fail(Error::InvalidSize);
  target_ = target;
  imageSize_ = imageSize;
  received_ = written_ = wordSize_ = 0;
  memset(word_, 0xff, sizeof(word_));
  error_ = Error::None;
  state_ = State::Writing;
  const uint32_t sectors = (imageSize + kSectorSize - 1) / kSectorSize;
  for (uint32_t i = 0; i < sectors; ++i) {
    if (!flash_.eraseSector(targetGeometry_.start / kSectorSize + i))
      return fail(Error::FlashFailure);
  }
  return true;
}

bool Writer::flushWord(bool final) {
  if (wordSize_ < sizeof(word_) && !final)
    return true;
  if (!wordSize_)
    return true;
  if (!flash_.write(targetGeometry_.start + written_, word_, sizeof(word_)))
    return fail(Error::FlashFailure);
  written_ += sizeof(word_);
  wordSize_ = 0;
  memset(word_, 0xff, sizeof(word_));
  return true;
}

bool Writer::write(const uint8_t* data, size_t size) {
  if (state_ != State::Writing)
    return fail(Error::WrongState);
  if (!data && size)
    return fail(Error::WrongState);
  if (size > imageSize_ - received_)
    return fail(Error::TooMuchData);
  if (!wordSize_ && size >= 4) {
    const size_t direct = size & ~size_t(3);
    if (!flash_.write(targetGeometry_.start + written_, data, direct))
      return fail(Error::FlashFailure);
    written_ += direct;
    received_ += direct;
    data += direct;
    size -= direct;
  }
  while (size--) {
    word_[wordSize_++] = *data++;
    ++received_;
    if (!flushWord(false))
      return false;
  }
  return true;
}

bool Writer::validate() {
  uint8_t header[8];
  if (!flash_.read(targetGeometry_.start, header, sizeof(header)))
    return false;
  if (header[0] != 0xea || header[1] != 0x04)
    return false;
  uint32_t offset = 8;
  if (!flash_.read(targetGeometry_.start + offset, header, sizeof(header)))
    return false;
  const uint32_t iromLength = readLe32(header + 4);
  if (readLe32(header) != 0 || iromLength == 0 || iromLength > imageSize_ - 16)
    return false;
  uint8_t checksum = kChecksumInitial;
  uint8_t buffer[128];
  offset += 8;
  uint32_t remaining = iromLength;
  while (remaining) {
    const uint32_t chunk = remaining < sizeof(buffer) ? remaining : sizeof(buffer);
    if (!flash_.read(targetGeometry_.start + offset, buffer, chunk))
      return false;
    for (uint32_t i = 0; i < chunk; ++i)
      checksum ^= buffer[i];
    offset += chunk;
    remaining -= chunk;
  }
  offset = (offset + 15) & ~15U;
  if (offset + 8 > imageSize_ ||
      !flash_.read(targetGeometry_.start + offset, header, sizeof(header)))
    return false;
  if (header[0] != 0xe9 || header[1] == 0 || header[1] > 16)
    return false;
  const uint8_t sections = header[1];
  const uint32_t entry = readLe32(header + 4);
  if (!rangeWithin(entry, 1, kIramStart, kIramEnd))
    return false;
  offset += 8;
  for (uint8_t section = 0; section < sections; ++section) {
    if (offset + 8 > imageSize_ ||
        !flash_.read(targetGeometry_.start + offset, header, sizeof(header)))
      return false;
    const uint32_t address = readLe32(header);
    const uint32_t length = readLe32(header + 4);
    if (!length || length > imageSize_ - offset - 8)
      return false;
    if (!rangeWithin(address, length, kIramStart, kIramEnd) &&
        !rangeWithin(address, length, kDramStart, kDramEnd))
      return false;
    offset += 8;
    remaining = length;
    while (remaining) {
      const uint32_t chunk = remaining < sizeof(buffer) ? remaining : sizeof(buffer);
      if (!flash_.read(targetGeometry_.start + offset, buffer, chunk))
        return false;
      for (uint32_t i = 0; i < chunk; ++i)
        checksum ^= buffer[i];
      offset += chunk;
      remaining -= chunk;
    }
  }
  offset = ((offset + 1 + 15) & ~15U) - 1;
  if (offset >= imageSize_ || offset + 1 != imageSize_)
    return false;
  uint8_t stored = 0;
  return flash_.read(targetGeometry_.start + offset, &stored, 1) && stored == checksum;
}

bool Writer::finish() {
  if (state_ != State::Writing)
    return fail(Error::WrongState);
  if (received_ != imageSize_)
    return fail(Error::InvalidSize);
  if (!flushWord(true))
    return false;
  if (!validate())
    return fail(Error::InvalidImage);
  state_ = State::Complete;
  return true;
}

void Writer::abort() {
  if (state_ == State::Writing)
    state_ = State::Failed;
  error_ = Error::WrongState;
}

} // namespace RBootSlot
