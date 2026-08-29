#include <RBootSlot.h>
#include <unity.h>

#include <algorithm>
#include <cstring>
#include <vector>

using RBootSlot::Error;
using RBootSlot::Slot;

namespace {

class FakeFlash : public RBootSlot::Flash {
 public:
  FakeFlash() : bytes(0x300000, 0xff), writes(0), erases(0), failWrites(false) {}
  bool read(uint32_t address, void* data, size_t size) override {
    if (address > bytes.size() || size > bytes.size() - address) return false;
    memcpy(data, bytes.data() + address, size);
    return true;
  }
  bool eraseSector(uint32_t sector) override {
    const size_t address = sector * RBootSlot::kSectorSize;
    if (address + RBootSlot::kSectorSize > bytes.size()) return false;
    std::fill(bytes.begin() + address, bytes.begin() + address + RBootSlot::kSectorSize, 0xff);
    ++erases;
    return true;
  }
  bool write(uint32_t address, const void* data, size_t size) override {
    if (failWrites || (address & 3) || (size & 3) || address + size > bytes.size()) return false;
    const uint8_t* source = static_cast<const uint8_t*>(data);
    for (size_t i = 0; i < size; ++i) bytes[address + i] &= source[i];
    ++writes;
    return true;
  }
  std::vector<uint8_t> bytes;
  unsigned writes;
  unsigned erases;
  bool failWrites;
};

void append32(std::vector<uint8_t>& image, uint32_t value) {
  for (unsigned i = 0; i < 4; ++i) image.push_back(value >> (8 * i));
}

std::vector<uint8_t> image() {
  std::vector<uint8_t> out{0xea, 0x04, 0x02, 0x40};
  append32(out, 0x40100000);
  append32(out, 0);
  append32(out, 16);
  for (unsigned i = 0; i < 16; ++i) out.push_back(static_cast<uint8_t>(i));
  out.insert(out.end(), {0xe9, 1, 2, 0x40});
  append32(out, 0x40100000);
  append32(out, 0x3ffe8000);
  append32(out, 4);
  out.insert(out.end(), {1, 2, 3, 4});
  while ((out.size() + 1) % 16) out.push_back(0);
  uint8_t checksum = 0xef;
  for (unsigned i = 16; i < 32; ++i) checksum ^= out[i];
  checksum ^= 1; checksum ^= 2; checksum ^= 3; checksum ^= 4;
  out.push_back(checksum);
  return out;
}

bool install(RBootSlot::Writer& writer, std::vector<uint8_t> bytes, size_t chunk = 7) {
  if (!writer.begin(Slot::B, bytes.size())) return false;
  for (size_t offset = 0; offset < bytes.size(); offset += chunk) {
    const size_t count = std::min(chunk, bytes.size() - offset);
    if (!writer.write(bytes.data() + offset, count)) return false;
  }
  return writer.finish();
}

void test_geometry_and_active_slot_protection() {
  TEST_ASSERT_EQUAL_HEX32(0x002000, RBootSlot::geometry(Slot::A).start);
  TEST_ASSERT_EQUAL_HEX32(0x202000, RBootSlot::geometry(Slot::B).start);
  TEST_ASSERT_EQUAL_HEX32(0x0fffff, RBootSlot::slotEnd(Slot::A));
  TEST_ASSERT_EQUAL_HEX32(0x2fffff, RBootSlot::slotEnd(Slot::B));
  TEST_ASSERT_EQUAL_HEX32(0x0fe000, RBootSlot::slotCapacity(Slot::A));
  FakeFlash flash;
  RBootSlot::Writer writer(flash, Slot::A);
  TEST_ASSERT_FALSE(writer.begin(Slot::A, 64));
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(Error::ActiveSlot), static_cast<uint8_t>(writer.error()));
  TEST_ASSERT_EQUAL_UINT(0, flash.erases);
}

void test_range_checks_are_overflow_safe() {
  TEST_ASSERT_TRUE(RBootSlot::validRange(Slot::A, RBootSlot::kSlotSize - 4, 4));
  TEST_ASSERT_FALSE(RBootSlot::validRange(Slot::A, RBootSlot::kSlotSize - 3, 4));
  TEST_ASSERT_FALSE(RBootSlot::validRange(Slot::A, 0xffffffff, 2));
  TEST_ASSERT_FALSE(RBootSlot::validAbsoluteRange(Slot::A, RBootSlot::kSlotAAddress - 1, 1));
  TEST_ASSERT_FALSE(RBootSlot::validAbsoluteRange(Slot::A, 0xffffffff, 2));
  TEST_ASSERT_TRUE(RBootSlot::validAbsoluteRange(Slot::B, RBootSlot::kSlotBAddress, RBootSlot::kSlotSize));
}

void test_size_edges_and_wrong_target() {
  FakeFlash flash;
  RBootSlot::Writer exact(flash, Slot::A);
  TEST_ASSERT_TRUE(exact.begin(Slot::B, RBootSlot::kSlotSize));
  RBootSlot::Writer zero(flash, Slot::A);
  TEST_ASSERT_FALSE(zero.begin(Slot::B, 0));
  RBootSlot::Writer wrong(flash, Slot::A);
  TEST_ASSERT_FALSE(wrong.begin(Slot::Invalid, 4));
}

void test_permanent_slot_is_protected_during_temporary_boot() {
  FakeFlash flash;
  RBootSlot::Writer writer(flash, Slot::B, Slot::A);
  TEST_ASSERT_FALSE(writer.begin(Slot::A, 64));
  TEST_ASSERT_EQUAL_UINT(0, flash.erases);
}

void test_rejects_bounds_without_touching_flash() {
  FakeFlash flash;
  RBootSlot::Writer writer(flash, Slot::A);
  TEST_ASSERT_FALSE(writer.begin(Slot::B, RBootSlot::kSlotSize + 1));
  TEST_ASSERT_EQUAL_UINT(0, flash.erases);
}

void test_streams_unaligned_chunks_and_validates() {
  FakeFlash flash;
  RBootSlot::Writer writer(flash, Slot::A);
  TEST_ASSERT_TRUE(install(writer, image(), 3));
  TEST_ASSERT_TRUE(writer.bootable());
  TEST_ASSERT_GREATER_THAN(0, flash.writes);
}

void test_incomplete_or_excess_data_never_becomes_bootable() {
  FakeFlash flash;
  auto bytes = image();
  RBootSlot::Writer shortWriter(flash, Slot::A);
  TEST_ASSERT_TRUE(shortWriter.begin(Slot::B, bytes.size()));
  TEST_ASSERT_TRUE(shortWriter.write(bytes.data(), bytes.size() - 1));
  TEST_ASSERT_FALSE(shortWriter.finish());
  TEST_ASSERT_FALSE(shortWriter.bootable());
  RBootSlot::Writer longWriter(flash, Slot::A);
  TEST_ASSERT_TRUE(longWriter.begin(Slot::B, bytes.size()));
  TEST_ASSERT_FALSE(longWriter.write(bytes.data(), bytes.size() + 1));
  TEST_ASSERT_FALSE(longWriter.bootable());
}

void test_finish_abort_and_terminal_states() {
  FakeFlash flash; auto bytes = image();
  RBootSlot::Writer finished(flash, Slot::A);
  TEST_ASSERT_TRUE(install(finished, bytes));
  TEST_ASSERT_FALSE(finished.finish());
  TEST_ASSERT_FALSE(finished.write(bytes.data(), 1));
  RBootSlot::Writer aborted(flash, Slot::A);
  TEST_ASSERT_TRUE(aborted.begin(Slot::B, bytes.size()));
  aborted.abort();
  TEST_ASSERT_FALSE(aborted.write(bytes.data(), 1));
  TEST_ASSERT_FALSE(aborted.finish());
}

void test_rejects_bad_header_section_bounds_and_checksum() {
  FakeFlash flash;
  auto badHeader = image(); badHeader[0] = 0;
  RBootSlot::Writer a(flash, Slot::A); TEST_ASSERT_FALSE(install(a, badHeader));
  auto badAddress = image(); badAddress[40] = 0; badAddress[41] = 0; badAddress[42] = 0; badAddress[43] = 0;
  RBootSlot::Writer b(flash, Slot::A); TEST_ASSERT_FALSE(install(b, badAddress));
  auto badChecksum = image(); badChecksum.back() ^= 1;
  RBootSlot::Writer c(flash, Slot::A); TEST_ASSERT_FALSE(install(c, badChecksum));
}

void test_propagates_flash_failure() {
  FakeFlash flash;
  auto bytes = image();
  RBootSlot::Writer writer(flash, Slot::A);
  TEST_ASSERT_TRUE(writer.begin(Slot::B, bytes.size()));
  flash.failWrites = true;
  TEST_ASSERT_FALSE(writer.write(bytes.data(), 4));
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(Error::FlashFailure), static_cast<uint8_t>(writer.error()));
}

}  // namespace

int main(int, char**) {
  UNITY_BEGIN();
  RUN_TEST(test_geometry_and_active_slot_protection);
  RUN_TEST(test_range_checks_are_overflow_safe);
  RUN_TEST(test_size_edges_and_wrong_target);
  RUN_TEST(test_permanent_slot_is_protected_during_temporary_boot);
  RUN_TEST(test_rejects_bounds_without_touching_flash);
  RUN_TEST(test_streams_unaligned_chunks_and_validates);
  RUN_TEST(test_incomplete_or_excess_data_never_becomes_bootable);
  RUN_TEST(test_finish_abort_and_terminal_states);
  RUN_TEST(test_rejects_bad_header_section_bounds_and_checksum);
  RUN_TEST(test_propagates_flash_failure);
  return UNITY_END();
}

