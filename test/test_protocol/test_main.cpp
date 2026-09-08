#include <FlashLayout.h>
#include <Protocol.h>
#include <Provisioning.h>
#include <ProvisioningStorage.h>
#include <RecoveryMode.h>
#include <array>
#include <string.h>
#include <string>
#include <unity.h>
using namespace mindflayer;
static const char* PUBLIC_DER_HEX =
    "30820122300d06092a864886f70d01010105000382010f003082010a0282010100e30876faef1a62e490ce22679c63"
    "bbc4fa3541f31e9c5f70444fb96c80e26ebd20f636e62b33c25dfee2264aed873371b6e60b3ce673c3c6081f14bf3d"
    "d9152c66bf8688b8dcaaeb04b36e1e59041ead40ae24027296181110ccb2f9133461fa6862d169458b63f4bf5e4726"
    "59879dabccdbf7f468a51d255c7447656398ab2a7536a575d4ba921d24dd5abef184615081a5e419a470f4f060638f"
    "3d920356f1c52a0a24c3131f391baf4e57da756cc314dd01d3fd9e9e5a5768963f831cc3270db8a8474a55191749a7"
    "cfbcfde719129d2eab6b206b862f58dee5db75e4de4114b6c4b2f51a8d383becd40c95a7e89888309234a3c3b3ea19"
    "a1d3f355ccf5470203010001";
static size_t fromHex(const char* hex, uint8_t* out, size_t cap) {
  size_t n = strlen(hex) / 2;
  if (n > cap)
    return 0;
  for (size_t i = 0; i < n; i++) {
    unsigned v;
    sscanf(hex + i * 2, "%2x", &v);
    out[i] = v;
  }
  return n;
}
static void assertBytes(const uint8_t* actual, size_t size, const char* hex) {
  uint8_t expected[512];
  size_t n = fromHex(hex, expected, sizeof(expected));
  TEST_ASSERT_EQUAL_UINT32(n, size);
  TEST_ASSERT_EQUAL_UINT8_ARRAY(expected, actual, n);
}
static provisioning::Provisioning sample(const char* id = "controller1") {
  provisioning::Provisioning p = {};
  strcpy(p.deviceId, id);
  memset(p.deviceSecret, 0x11, 32);
  strcpy(p.ssid, "test-ap");
  strcpy(p.wifiPassword, "correct horse battery staple");
  strcpy(p.serverHost, "10.42.0.1");
  p.serverPort = 10443;
  p.serverPublicKeyLength = fromHex(PUBLIC_DER_HEX, p.serverPublicKey, sizeof(p.serverPublicKey));
  return p;
}
class FakeFlash : public provisioning::FlashBackend {
public:
  std::array<uint8_t, flashlayout::SECTOR_SIZE> a, b;
  bool failErase = false, failDataWrite = false, failCommitWrite = false;
  FakeFlash() {
    a.fill(0xff);
    b.fill(0xff);
  }
  std::array<uint8_t, flashlayout::SECTOR_SIZE>* sector(uint32_t address) {
    if (address >= flashlayout::PROVISIONING_A &&
        address < flashlayout::PROVISIONING_A + flashlayout::SECTOR_SIZE)
      return &a;
    if (address >= flashlayout::PROVISIONING_B &&
        address < flashlayout::PROVISIONING_B + flashlayout::SECTOR_SIZE)
      return &b;
    return nullptr;
  }
  bool read(uint32_t address, void* out, size_t n) override {
    auto* s = sector(address);
    if (!s || address % flashlayout::SECTOR_SIZE + n > flashlayout::SECTOR_SIZE)
      return false;
    memcpy(out, s->data() + address % flashlayout::SECTOR_SIZE, n);
    return true;
  }
  bool eraseSector(uint32_t s) override {
    if (failErase) {
      failErase = false;
      return false;
    }
    if (s == flashlayout::PROVISIONING_A / flashlayout::SECTOR_SIZE) {
      a.fill(0xff);
      return true;
    }
    if (s == flashlayout::PROVISIONING_B / flashlayout::SECTOR_SIZE) {
      b.fill(0xff);
      return true;
    }
    return false;
  }
  bool write(uint32_t address, const void* data, size_t n) override {
    auto* s = sector(address);
    if (!s || address % 4 || n % 4 ||
        address % flashlayout::SECTOR_SIZE + n > flashlayout::SECTOR_SIZE)
      return false;
    size_t offset = address % flashlayout::SECTOR_SIZE;
    if (failCommitWrite && offset == flashlayout::SECTOR_SIZE - 4) {
      failCommitWrite = false;
      return false;
    }
    const uint8_t* in = (const uint8_t*)data;
    size_t limit = failDataWrite && offset == 0 ? n / 2 : n;
    for (size_t i = 0; i < limit; i++) {
      if (((*s)[offset + i] & in[i]) != in[i])
        return false;
      (*s)[offset + i] &= in[i];
    }
    if (failDataWrite && offset == 0) {
      failDataWrite = false;
      return false;
    }
    return true;
  }
};
static size_t envelope(const provisioning::Provisioning& p, uint8_t* out) {
  size_t written = 0;
  TEST_ASSERT_TRUE(provisioning::encodeEnvelope(p, out, provisioning::MAX_ENVELOPE_SIZE, written));
  return written;
}
static size_t findBytes(const uint8_t* data, size_t size, const uint8_t* needle, size_t count) {
  for (size_t i = 0; i + count <= size; i++)
    if (!memcmp(data + i, needle, count))
      return i;
  return size;
}
static void refreshEnvelopeCrc(uint8_t* e, size_t n) {
  uint32_t crc = provisioning::crc32(e, n - 4);
  e[n - 4] = crc >> 24;
  e[n - 3] = crc >> 16;
  e[n - 2] = crc >> 8;
  e[n - 1] = crc;
}
void test_protocol_exact_fixtures() {
  uint8_t out[512];
  size_t n;
  TEST_ASSERT_TRUE(
      protocol::buildRegistration(out, sizeof(out), n, "1.2.3", "mindflayer-keypad-v1"));
  assertBytes(out, n, "84030265312e322e33746d696e64666c617965722d6b65797061642d7631");
  TEST_ASSERT_TRUE(protocol::buildKeyEvent(out, sizeof(out), n, "W", true));
  assertBytes(out, n, "8404020101");
  uint8_t challengeFrame[37];
  fromHex((std::string("8300015820") + std::string(64, '1')).c_str(), challengeFrame,
          sizeof(challengeFrame));
  protocol::AuthChallenge challenge;
  TEST_ASSERT_TRUE(protocol::parseAuthChallenge(challengeFrame, sizeof(challengeFrame), challenge));
  uint8_t secret[32];
  memset(secret, 0x11, 32);
  TEST_ASSERT_TRUE(
      protocol::buildAuthResponse(out, sizeof(out), n, "controller1", secret, challenge.challenge));
  assertBytes(out, n,
              "8401026b636f6e74726f6c6c6572315820e371e039a5fa8d68355af25c83deece181accbbd6f3cac420d"
              "63b9b1ec194c7e");
}
void test_protocol_decodes_server_fixtures() {
  uint8_t f[512];
  protocol::Configuration c;
  size_t n = fromHex("880502010203040506", f, sizeof(f));
  TEST_ASSERT_TRUE(protocol::parseConfiguration(f, n, c));
  TEST_ASSERT_EQUAL_UINT8(6, c.led2.b);
  protocol::AuthResult auth;
  n = fromHex("840202006b636f6e74726f6c6c657231", f, sizeof(f));
  TEST_ASSERT_TRUE(protocol::parseAuthResult(f, n, auth));
  TEST_ASSERT_EQUAL_STRING("controller1", auth.deviceId);
  const char* update = "87060265312e322e33187b58200000000000000000000000000000000000000000000000000"
                       "000000000000000712f6669726d776172652f612f312e322e33582033333333333333333333"
                       "33333333333333333333333333333333333333333333";
  n = fromHex(update, f, sizeof(f));
  protocol::UpdateAvailable u;
  TEST_ASSERT_TRUE(protocol::parseUpdateAvailable(f, n, u));
  TEST_ASSERT_EQUAL_UINT32(123, u.size);
  TEST_ASSERT_EQUAL_STRING("/firmware/a/1.2.3", u.path);
  protocol::FirmwareAccepted accepted;
  n = fromHex("83070265312e322e33", f, sizeof(f));
  TEST_ASSERT_TRUE(protocol::parseFirmwareAccepted(f, n, accepted));
  TEST_ASSERT_EQUAL_STRING("1.2.3", accepted.version);
}
void test_restricted_protocol_rejects_malformed_corpus() {
  protocol::Configuration c;
  const uint8_t* cases[] = {(const uint8_t*)"",
                            (const uint8_t*)"\x87",
                            (const uint8_t*)"\x9f\x05\x01\x02\x03\x04\x05\x06\xff",
                            (const uint8_t*)"\x87\xc0\x05\x01\x02\x03\x04\x05",
                            (const uint8_t*)"\x87\x05\xf9\x00\x00\x02\x03\x04\x05\x06",
                            (const uint8_t*)"\x87\x05\x01\x02\x03\x04\x05\x06\x00"};
  size_t sizes[] = {0, 1, 9, 8, 10, 9};
  for (size_t i = 0; i < 6; i++)
    TEST_ASSERT_FALSE(protocol::parseConfiguration(cases[i], sizes[i], c));
  uint8_t huge[protocol::MAX_DEVICE_FRAME_SIZE + 1] = {0};
  TEST_ASSERT_FALSE(protocol::parseConfiguration(huge, sizeof(huge), c));
  const uint8_t invalidUtf8[] = {0x84, 0x02, 0x02, 0x00, 0x61, 0xff};
  protocol::AuthResult result;
  TEST_ASSERT_FALSE(protocol::parseAuthResult(invalidUtf8, sizeof(invalidUtf8), result));
  const uint8_t wrongVersion[] = {0x88, 0x05, 0x03, 1, 2, 3, 4, 5, 6};
  TEST_ASSERT_FALSE(protocol::parseConfiguration(wrongVersion, sizeof(wrongVersion), c));
  const uint8_t missingVersion[] = {0x87, 0x05, 1, 2, 3, 4, 5, 6};
  TEST_ASSERT_FALSE(protocol::parseConfiguration(missingVersion, sizeof(missingVersion), c));
  const uint8_t nonShortestVersion[] = {0x88, 0x05, 0x18, 0x02, 1, 2, 3, 4, 5, 6};
  TEST_ASSERT_FALSE(
      protocol::parseConfiguration(nonShortestVersion, sizeof(nonShortestVersion), c));
  const uint8_t embeddedNul[] = {0x84, 0x02, 0x02, 0x00, 0x63, 'a', 0, 'b'};
  TEST_ASSERT_FALSE(protocol::parseAuthResult(embeddedNul, sizeof(embeddedNul), result));
}
void test_crc_and_envelope() {
  TEST_ASSERT_EQUAL_HEX32(0xcbf43926, provisioning::crc32((const uint8_t*)"123456789", 9));
  uint8_t e[provisioning::MAX_ENVELOPE_SIZE];
  auto p = sample();
  const char* legacyFixture =
      "4d465031010194a80001016b636f6e74726f6c6c6572310258201111111111111111111111111111111111111111"
      "1111111111111111111111110367746573742d617004781c636f727265637420686f727365206261747465727920"
      "737461706c65056931302e34322e302e31061928cb0759012630820122300d06092a864886f70d01010105000382"
      "010f003082010a0282010100e30876faef1a62e490ce22679c63bbc4fa3541f31e9c5f70444fb96c80e26ebd20f6"
      "36e62b33c25dfee2264aed873371b6e60b3ce673c3c6081f14bf3dd9152c66bf8688b8dcaaeb04b36e1e59041ead"
      "40ae24027296181110ccb2f9133461fa6862d169458b63f4bf5e472659879dabccdbf7f468a51d255c7447656398"
      "ab2a7536a575d4ba921d24dd5abef184615081a5e419a470f4f060638f3d920356f1c52a0a24c3131f391baf4e57"
      "da756cc314dd01d3fd9e9e5a5768963f831cc3270db8a8474a55191749a7cfbcfde719129d2eab6b206b862f58de"
      "e5db75e4de4114b6c4b2f51a8d383becd40c95a7e89888309234a3c3b3ea19a1d3f355ccf5470203010001fa6fb8"
      "d9";
  provisioning::Provisioning decoded;
  size_t n = fromHex(legacyFixture, e, sizeof(e));
  TEST_ASSERT_TRUE(provisioning::decodeEnvelope(e, n, decoded));
  TEST_ASSERT_EQUAL_STRING(p.deviceId, decoded.deviceId);
  TEST_ASSERT_FALSE(decoded.serialDebug);
  n = envelope(p, e);
  TEST_ASSERT_TRUE(provisioning::decodeEnvelope(e, n, decoded));
  TEST_ASSERT_FALSE(decoded.serialDebug);
  p.serialDebug = true;
  n = envelope(p, e);
  TEST_ASSERT_TRUE(provisioning::decodeEnvelope(e, n, decoded));
  TEST_ASSERT_TRUE(decoded.serialDebug);
  e[20] ^= 1;
  TEST_ASSERT_FALSE(provisioning::decodeEnvelope(e, n, decoded));
}
void test_redundant_store_selection_and_updates() {
  FakeFlash flash;
  provisioning::ProvisioningStore store(flash);
  provisioning::Provisioning out;
  provisioning::Selection s;
  TEST_ASSERT_FALSE(store.load(out, &s));
  uint8_t e[provisioning::MAX_ENVELOPE_SIZE];
  auto p = sample("controllerA");
  size_t n = envelope(p, e);
  TEST_ASSERT_TRUE(store.writeEnvelope(e, n, &s));
  TEST_ASSERT_EQUAL(provisioning::COPY_A, s.copy);
  TEST_ASSERT_EQUAL_UINT32(1, s.generation);
  p = sample("controllerB");
  n = envelope(p, e);
  TEST_ASSERT_TRUE(store.writeEnvelope(e, n, &s));
  TEST_ASSERT_EQUAL(provisioning::COPY_B, s.copy);
  TEST_ASSERT_EQUAL_UINT32(2, s.generation);
  TEST_ASSERT_TRUE(store.load(out, &s));
  TEST_ASSERT_EQUAL_STRING("controllerB", out.deviceId);
}
void test_newer_uncommitted_or_bad_crc_falls_back() {
  FakeFlash flash;
  provisioning::ProvisioningStore store(flash);
  uint8_t e[provisioning::MAX_ENVELOPE_SIZE];
  auto p = sample("old");
  size_t n = envelope(p, e);
  TEST_ASSERT_TRUE(store.writeEnvelope(e, n));
  p = sample("new");
  n = envelope(p, e);
  TEST_ASSERT_TRUE(store.writeEnvelope(e, n));
  flash.b[flashlayout::SECTOR_SIZE - 1] = 0xff;
  provisioning::Provisioning out;
  provisioning::Selection s;
  TEST_ASSERT_TRUE(store.load(out, &s));
  TEST_ASSERT_EQUAL_STRING("old", out.deviceId);
  TEST_ASSERT_TRUE(store.writeEnvelope(e, n));
  flash.b[20] ^= 1;
  TEST_ASSERT_TRUE(store.load(out, &s));
  TEST_ASSERT_EQUAL_STRING("old", out.deviceId);
}
void test_generation_serial_arithmetic() {
  TEST_ASSERT_TRUE(provisioning::generationNewer(2, 1));
  TEST_ASSERT_TRUE(provisioning::generationNewer(0, 0xffffffff));
  TEST_ASSERT_FALSE(provisioning::generationNewer(1, 2));
  TEST_ASSERT_FALSE(provisioning::generationNewer(0x80000000, 0));
}
void test_recovery_marker_is_strict_and_avoids_eboot_rtc_words() {
  auto marker = recovery::makeMarker();
  TEST_ASSERT_TRUE(recovery::markerValid(marker));
  marker.inverse ^= 1;
  TEST_ASSERT_FALSE(recovery::markerValid(marker));
  TEST_ASSERT_GREATER_OR_EQUAL_UINT32(32, recovery::RTC_OFFSET);
}
void test_power_loss_failures_preserve_previous_copy() {
  FakeFlash flash;
  provisioning::ProvisioningStore store(flash);
  uint8_t e[provisioning::MAX_ENVELOPE_SIZE];
  auto old = sample("old");
  size_t n = envelope(old, e);
  TEST_ASSERT_TRUE(store.writeEnvelope(e, n));
  auto committedA = flash.a;
  auto freshB = flash.b;
  auto newer = sample("new");
  n = envelope(newer, e);
  provisioning::Provisioning out;
  provisioning::Selection selected;
  flash.failErase = true;
  TEST_ASSERT_FALSE(store.writeEnvelope(e, n));
  TEST_ASSERT_EQUAL_UINT8_ARRAY(committedA.data(), flash.a.data(), flashlayout::SECTOR_SIZE);
  TEST_ASSERT_TRUE(store.load(out, &selected));
  TEST_ASSERT_EQUAL_STRING("old", out.deviceId);
  flash.b = freshB;
  flash.failDataWrite = true;
  TEST_ASSERT_FALSE(store.writeEnvelope(e, n));
  TEST_ASSERT_EQUAL_UINT8_ARRAY(committedA.data(), flash.a.data(), flashlayout::SECTOR_SIZE);
  TEST_ASSERT_TRUE(store.load(out, &selected));
  TEST_ASSERT_EQUAL_STRING("old", out.deviceId);
  flash.b = freshB;
  flash.failCommitWrite = true;
  TEST_ASSERT_FALSE(store.writeEnvelope(e, n));
  TEST_ASSERT_EQUAL_UINT8_ARRAY(committedA.data(), flash.a.data(), flashlayout::SECTOR_SIZE);
  TEST_ASSERT_TRUE(store.load(out, &selected));
  TEST_ASSERT_EQUAL_STRING("old", out.deviceId);
}
void test_envelope_bounds_schema_and_semantics() {
  uint8_t e[provisioning::MAX_ENVELOPE_SIZE];
  auto p = sample();
  size_t n = envelope(p, e);
  provisioning::Provisioning out;
  uint8_t original = e[0];
  e[0] = 'X';
  TEST_ASSERT_FALSE(provisioning::decodeEnvelope(e, n, out));
  e[0] = original;
  e[4] = 2;
  refreshEnvelopeCrc(e, n);
  TEST_ASSERT_FALSE(provisioning::decodeEnvelope(e, n, out));
  e[4] = 1;
  e[5] = 4;
  e[6] = 1;
  TEST_ASSERT_FALSE(provisioning::decodeEnvelope(e, n, out));
  n = envelope(p, e);
  TEST_ASSERT_FALSE(provisioning::decodeEnvelope(e, n - 1, out));
  size_t payload = provisioning::ENVELOPE_HEADER_SIZE;
  e[payload + 2] = 3;
  refreshEnvelopeCrc(e, n);
  TEST_ASSERT_FALSE(provisioning::decodeEnvelope(e, n, out));
  n = envelope(p, e);
  payload = provisioning::ENVELOPE_HEADER_SIZE;
  e[payload + 2] = 1;
  refreshEnvelopeCrc(e, n);
  TEST_ASSERT_FALSE(provisioning::decodeEnvelope(e, n, out));
  n = envelope(p, e);
  const uint8_t debugHeader[] = {8, 0xf4};
  size_t at = findBytes(e, n, debugHeader, sizeof(debugHeader));
  TEST_ASSERT_TRUE(at < n);
  e[at + 1] = 0;
  refreshEnvelopeCrc(e, n);
  TEST_ASSERT_FALSE(provisioning::decodeEnvelope(e, n, out));
  n = envelope(p, e);
  const uint8_t secretHeader[] = {2, 0x58, 0x20};
  at = findBytes(e, n, secretHeader, sizeof(secretHeader));
  TEST_ASSERT_TRUE(at < n);
  e[at + 2] = 31;
  refreshEnvelopeCrc(e, n);
  TEST_ASSERT_FALSE(provisioning::decodeEnvelope(e, n, out));
  n = envelope(p, e);
  const uint8_t ssidHeader[] = {3, 0x67, 't', 'e', 's', 't'};
  at = findBytes(e, n, ssidHeader, sizeof(ssidHeader));
  TEST_ASSERT_TRUE(at < n);
  e[at + 3] = 0;
  refreshEnvelopeCrc(e, n);
  TEST_ASSERT_FALSE(provisioning::decodeEnvelope(e, n, out));
  n = envelope(p, e);
  const uint8_t keyHeader[] = {7, 0x59, 0x01, 0x26, 0x30};
  at = findBytes(e, n, keyHeader, sizeof(keyHeader));
  TEST_ASSERT_TRUE(at < n);
  e[at + 4] ^= 1;
  refreshEnvelopeCrc(e, n);
  TEST_ASSERT_FALSE(provisioning::decodeEnvelope(e, n, out));
}
void test_copy_selection_all_validity_combinations() {
  FakeFlash flash;
  provisioning::ProvisioningStore store(flash);
  uint8_t e[provisioning::MAX_ENVELOPE_SIZE];
  auto p = sample("A");
  size_t n = envelope(p, e);
  TEST_ASSERT_TRUE(store.writeEnvelope(e, n));
  p = sample("B");
  n = envelope(p, e);
  TEST_ASSERT_TRUE(store.writeEnvelope(e, n));
  provisioning::Provisioning out;
  provisioning::Selection s;
  auto savedA = flash.a;
  flash.a.fill(0xff);
  TEST_ASSERT_TRUE(store.load(out, &s));
  TEST_ASSERT_EQUAL(provisioning::COPY_B, s.copy);
  flash.a = savedA;
  auto savedB = flash.b;
  flash.b.fill(0xff);
  TEST_ASSERT_TRUE(store.load(out, &s));
  TEST_ASSERT_EQUAL(provisioning::COPY_A, s.copy);
  flash.b = savedB;
  std::swap(flash.a, flash.b);
  TEST_ASSERT_TRUE(store.load(out, &s));
  TEST_ASSERT_EQUAL(provisioning::COPY_A, s.copy);
  flash.a.fill(0xff);
  flash.b.fill(0xff);
  TEST_ASSERT_FALSE(store.load(out, &s));
  TEST_ASSERT_EQUAL(provisioning::NO_COPY, s.copy);
}
void test_bounded_random_corpus_is_memory_safe() {
  uint32_t state = 0x5eed1234;
  uint8_t bytes[provisioning::MAX_ENVELOPE_SIZE + 1];
  for (size_t round = 0; round < 10000; round++) {
    state ^= state << 13;
    state ^= state >> 17;
    state ^= state << 5;
    size_t n = state % sizeof(bytes);
    for (size_t i = 0; i < n; i++) {
      state ^= state << 13;
      state ^= state >> 17;
      state ^= state << 5;
      bytes[i] = state;
    }
    protocol::AuthChallenge challenge;
    protocol::AuthResult auth;
    protocol::Configuration configuration;
    protocol::UpdateAvailable update;
    protocol::FirmwareAccepted accepted;
    provisioning::Provisioning p;
    protocol::parseAuthChallenge(bytes, n, challenge);
    protocol::parseAuthResult(bytes, n, auth);
    protocol::parseConfiguration(bytes, n, configuration);
    protocol::parseUpdateAvailable(bytes, n, update);
    protocol::parseFirmwareAccepted(bytes, n, accepted);
    provisioning::decodeEnvelope(bytes, n, p);
  }
}
void setUp() {}
void tearDown() {}
int main() {
  UNITY_BEGIN();
  RUN_TEST(test_protocol_exact_fixtures);
  RUN_TEST(test_protocol_decodes_server_fixtures);
  RUN_TEST(test_restricted_protocol_rejects_malformed_corpus);
  RUN_TEST(test_crc_and_envelope);
  RUN_TEST(test_redundant_store_selection_and_updates);
  RUN_TEST(test_newer_uncommitted_or_bad_crc_falls_back);
  RUN_TEST(test_generation_serial_arithmetic);
  RUN_TEST(test_recovery_marker_is_strict_and_avoids_eboot_rtc_words);
  RUN_TEST(test_power_loss_failures_preserve_previous_copy);
  RUN_TEST(test_envelope_bounds_schema_and_semantics);
  RUN_TEST(test_copy_selection_all_validity_combinations);
  RUN_TEST(test_bounded_random_corpus_is_memory_safe);
  return UNITY_END();
}
