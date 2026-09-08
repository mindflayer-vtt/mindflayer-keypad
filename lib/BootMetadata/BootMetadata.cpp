#include "BootMetadata.h"

#include <stddef.h>
#include <string.h>

namespace BootMetadata {
namespace {
constexpr uint8_t kConfigMagic = 0xe1;
constexpr uint8_t kConfigVersion = 1;
constexpr uint8_t kChecksumInitial = 0xef;

uint8_t configChecksum(const Config& config) {
  uint8_t result = kChecksumInitial;
  const uint8_t* p = reinterpret_cast<const uint8_t*>(&config);
  const uint8_t* end = reinterpret_cast<const uint8_t*>(&config.checksum);
  while (p < end)
    result ^= *p++;
  return result;
}
} // namespace

uint32_t crc32(const void* data, size_t size) {
  uint32_t crc = 0xffffffff;
  const uint8_t* p = static_cast<const uint8_t*>(data);
  while (size--) {
    crc ^= *p++;
    for (uint8_t bit = 0; bit < 8; ++bit)
      crc = (crc >> 1) ^ (0xedb88320U & (0U - (crc & 1U)));
  }
  return ~crc;
}

bool valid(const Record& record, uint32_t commit) {
  return commit == kCommit && record.magic == kMagic && record.format == kFormat &&
         record.config.magic == kConfigMagic && record.config.version == kConfigVersion &&
         record.config.count == 2 && record.config.currentRom < 2 &&
         record.config.roms[0] == RBootSlot::kSlotAAddress &&
         record.config.roms[1] == RBootSlot::kSlotBAddress &&
         record.config.checksum == configChecksum(record.config) &&
         record.crc32 == crc32(&record, offsetof(Record, crc32));
}

bool newer(uint32_t left, uint32_t right) { return static_cast<int32_t>(left - right) > 0; }

bool Store::read(uint32_t address, Record& record) {
  uint32_t marker;
  return flash_.read(address, &record, sizeof(record)) &&
         flash_.read(address + kCommitOffset, &marker, sizeof(marker)) && valid(record, marker);
}

bool Store::load(Record& record) {
  Record a, b;
  const bool aValid = read(RBootSlot::kMetadataAAddress, a);
  const bool bValid = read(RBootSlot::kMetadataBAddress, b);
  if (!aValid && !bValid)
    return false;
  if (bValid && (!aValid || newer(b.generation, a.generation))) {
    record = b;
    activeAddress_ = RBootSlot::kMetadataBAddress;
  } else {
    record = a;
    activeAddress_ = RBootSlot::kMetadataAAddress;
  }
  return true;
}

bool Store::commit(const Config& config, Hook hook, void* context) {
  Record current;
  const bool hadCurrent = load(current);
  const uint32_t target = activeAddress_ == RBootSlot::kMetadataAAddress
                              ? RBootSlot::kMetadataBAddress
                              : RBootSlot::kMetadataAAddress;
  Record next{};
  next.magic = kMagic;
  next.format = kFormat;
  next.generation = hadCurrent ? current.generation + 1 : 1;
  next.config = config;
  next.config.checksum = configChecksum(next.config);
  next.crc32 = crc32(&next, offsetof(Record, crc32));
  if (!flash_.eraseSector(target / RBootSlot::kSectorSize))
    return false;
  if (hook)
    hook(Stage::Erased, context);
  const size_t padded = (sizeof(next) + 3) & ~size_t(3);
  uint8_t body[(sizeof(next) + 3) & ~size_t(3)];
  memset(body, 0xff, sizeof(body));
  memcpy(body, &next, sizeof(next));
  if (!flash_.write(target, body, 4))
    return false;
  if (hook)
    hook(Stage::PartialBodyWritten, context);
  if (!flash_.write(target + 4, body + 4, padded - 4))
    return false;
  if (hook)
    hook(Stage::BodyWritten, context);
  Record verify;
  if (!flash_.read(target, &verify, sizeof(verify)) || memcmp(&verify, &next, sizeof(next)))
    return false;
  if (hook)
    hook(Stage::BodyVerified, context);
  const uint32_t marker = kCommit;
  if (!flash_.write(target + kCommitOffset, &marker, sizeof(marker)))
    return false;
  Record committed;
  if (!read(target, committed) || memcmp(&committed, &next, sizeof(next)))
    return false;
  activeAddress_ = target;
  if (hook)
    hook(Stage::Committed, context);
  return true;
}

} // namespace BootMetadata
