#include <BootMetadata.h>
#include <unity.h>

#include <algorithm>
#include <cstring>
#include <vector>

namespace {
class Flash : public RBootSlot::Flash {
public:
  Flash() : bytes(0x101000, 0xff), writes(0), failWrite(0) {}
  bool read(uint32_t a, void* d, size_t n) override {
    if (a + n > bytes.size())
      return false;
    memcpy(d, bytes.data() + a, n);
    return true;
  }
  bool eraseSector(uint32_t s) override {
    size_t a = s * 0x1000;
    if (a + 0x1000 > bytes.size())
      return false;
    std::fill(bytes.begin() + a, bytes.begin() + a + 0x1000, 0xff);
    return true;
  }
  bool write(uint32_t a, const void* d, size_t n) override {
    if (++writes == failWrite || a + n > bytes.size())
      return false;
    const uint8_t* p = static_cast<const uint8_t*>(d);
    for (size_t i = 0; i < n; ++i)
      bytes[a + i] &= p[i];
    return true;
  }
  std::vector<uint8_t> bytes;
  unsigned writes, failWrite;
};

BootMetadata::Config config(uint8_t slot) {
  BootMetadata::Config c{};
  c.magic = 0xe1;
  c.version = 1;
  c.currentRom = slot;
  c.count = 2;
  c.roms[0] = RBootSlot::kSlotAAddress;
  c.roms[1] = RBootSlot::kSlotBAddress;
  return c;
}

void test_commit_alternates_and_selects_newest() {
  Flash flash;
  BootMetadata::Store store(flash);
  BootMetadata::Record record;
  TEST_ASSERT_TRUE(store.commit(config(0)));
  TEST_ASSERT_TRUE(store.load(record));
  TEST_ASSERT_EQUAL_UINT32(1, record.generation);
  TEST_ASSERT_EQUAL_UINT8(0, record.config.currentRom);
  TEST_ASSERT_TRUE(store.commit(config(1)));
  TEST_ASSERT_TRUE(store.load(record));
  TEST_ASSERT_EQUAL_UINT32(2, record.generation);
  TEST_ASSERT_EQUAL_UINT8(1, record.config.currentRom);
}

void test_uncommitted_and_torn_body_preserve_previous() {
  Flash flash;
  BootMetadata::Store store(flash);
  BootMetadata::Record record;
  TEST_ASSERT_TRUE(store.commit(config(0)));
  flash.failWrite = flash.writes + 3;
  TEST_ASSERT_FALSE(store.commit(config(1)));
  TEST_ASSERT_TRUE(store.load(record));
  TEST_ASSERT_EQUAL_UINT8(0, record.config.currentRom);
  flash.failWrite = flash.writes + 1;
  TEST_ASSERT_FALSE(store.commit(config(1)));
  TEST_ASSERT_TRUE(store.load(record));
  TEST_ASSERT_EQUAL_UINT8(0, record.config.currentRom);
}

void test_partial_body_and_missing_marker_preserve_previous() {
  Flash flash;
  BootMetadata::Store store(flash);
  BootMetadata::Record record;
  TEST_ASSERT_TRUE(store.commit(config(0)));
  flash.failWrite = flash.writes + 2;
  TEST_ASSERT_FALSE(store.commit(config(1)));
  TEST_ASSERT_TRUE(store.load(record));
  TEST_ASSERT_EQUAL_UINT8(0, record.config.currentRom);
  flash.failWrite = flash.writes + 3;
  TEST_ASSERT_FALSE(store.commit(config(1)));
  TEST_ASSERT_TRUE(store.load(record));
  TEST_ASSERT_EQUAL_UINT8(0, record.config.currentRom);
}

void test_corrupt_newest_falls_back_and_both_invalid_fail() {
  Flash flash;
  BootMetadata::Store store(flash);
  BootMetadata::Record record;
  TEST_ASSERT_TRUE(store.commit(config(0)));
  TEST_ASSERT_TRUE(store.commit(config(1)));
  flash.bytes[RBootSlot::kMetadataBAddress + 12] ^= 1;
  TEST_ASSERT_TRUE(store.load(record));
  TEST_ASSERT_EQUAL_UINT8(0, record.config.currentRom);
  flash.bytes[RBootSlot::kMetadataAAddress] ^= 1;
  TEST_ASSERT_FALSE(store.load(record));
}

void test_generation_comparison_wraps() {
  TEST_ASSERT_TRUE(BootMetadata::newer(0, 0xffffffff));
  TEST_ASSERT_FALSE(BootMetadata::newer(0xffffffff, 0));
}
void test_each_copy_is_independently_usable() {
  Flash flash;
  BootMetadata::Store store(flash);
  BootMetadata::Record record;
  TEST_ASSERT_TRUE(store.commit(config(0)));
  auto a = flash.bytes;
  TEST_ASSERT_TRUE(store.commit(config(1)));
  auto b = flash.bytes;
  std::fill(flash.bytes.begin() + RBootSlot::kMetadataBAddress,
            flash.bytes.begin() + RBootSlot::kMetadataBAddress + 0x1000, 0xff);
  TEST_ASSERT_TRUE(store.load(record));
  TEST_ASSERT_EQUAL_UINT8(0, record.config.currentRom);
  flash.bytes = b;
  std::fill(flash.bytes.begin() + RBootSlot::kMetadataAAddress,
            flash.bytes.begin() + RBootSlot::kMetadataAAddress + 0x1000, 0xff);
  TEST_ASSERT_TRUE(store.load(record));
  TEST_ASSERT_EQUAL_UINT8(1, record.config.currentRom);
}
} // namespace

int main(int, char**) {
  UNITY_BEGIN();
  RUN_TEST(test_commit_alternates_and_selects_newest);
  RUN_TEST(test_uncommitted_and_torn_body_preserve_previous);
  RUN_TEST(test_partial_body_and_missing_marker_preserve_previous);
  RUN_TEST(test_corrupt_newest_falls_back_and_both_invalid_fail);
  RUN_TEST(test_generation_comparison_wraps);
  RUN_TEST(test_each_copy_is_independently_usable);
  return UNITY_END();
}
