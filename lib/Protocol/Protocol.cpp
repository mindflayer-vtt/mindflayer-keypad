#include "Protocol.h"
#include <qcbor/qcbor_decode.h>
#include <qcbor/qcbor_encode.h>
#include <string.h>
#ifdef __linux__
#include <openssl/evp.h>
#include <openssl/hmac.h>
#else
extern "C" {
#include <bearssl/bearssl.h>
}
#endif
namespace mindflayer {
namespace protocol {
static const char* KEYS[] = {"Q", "W", "E", "A", "S", "D", "Z", "X", "C", "SHI", "SPC"};
static bool preferredArgument(const uint8_t* frame, size_t size, size_t& offset,
                              uint8_t expectedMajor) {
  if (offset >= size)
    return false;
  const uint8_t initial = frame[offset++], major = initial >> 5, additional = initial & 31;
  if (major != expectedMajor || additional >= 28)
    return false;
  uint64_t value = additional;
  if (additional >= 24) {
    const size_t width = static_cast<size_t>(1) << (additional - 24);
    if (offset + width > size)
      return false;
    value = 0;
    for (size_t i = 0; i < width; ++i)
      value = (value << 8) | frame[offset++];
    const uint64_t minimum = additional == 24 ? 24 : (static_cast<uint64_t>(1) << (width * 4));
    if (value < minimum)
      return false;
  }
  if ((major == 2 || major == 3) && (value > size - offset))
    return false;
  if (major == 2 || major == 3)
    offset += static_cast<size_t>(value);
  return true;
}
static bool preferredRestrictedFrame(const uint8_t* frame, size_t size) {
  if (!frame || !size)
    return false;
  size_t offset = 0;
  if (!preferredArgument(frame, size, offset, 4))
    return false;
  const size_t arity = frame[0] & 31;
  if (arity >= 24)
    return false;
  for (size_t i = 0; i < arity; ++i) {
    if (offset >= size)
      return false;
    const uint8_t major = frame[offset] >> 5;
    if (major != 0 && major != 2 && major != 3)
      return false;
    if (!preferredArgument(frame, size, offset, major))
      return false;
  }
  return offset == size;
}
static bool startArray(QCBORDecodeContext& d, const uint8_t* frame, size_t size, uint16_t arity) {
  if (!frame || size == 0 || size > MAX_DEVICE_FRAME_SIZE || !preferredRestrictedFrame(frame, size))
    return false;
  QCBORDecode_Init(&d, {frame, size}, QCBOR_DECODE_MODE_NORMAL);
  QCBORItem item;
  return QCBORDecode_GetNext(&d, &item) == QCBOR_SUCCESS && item.uDataType == QCBOR_TYPE_ARRAY &&
         item.val.uCount == arity;
}
static bool getUInt(QCBORDecodeContext& d, uint64_t max, uint64_t& value) {
  QCBORItem item;
  if (QCBORDecode_GetNext(&d, &item) != QCBOR_SUCCESS)
    return false;
  if (item.uDataType == QCBOR_TYPE_INT64) {
    if (item.val.int64 < 0 || (uint64_t)item.val.int64 > max)
      return false;
    value = item.val.int64;
    return true;
  }
  if (item.uDataType != QCBOR_TYPE_UINT64 || item.val.uint64 > max)
    return false;
  value = item.val.uint64;
  return true;
}
static bool getBytes(QCBORDecodeContext& d, uint8_t* output, size_t exact) {
  QCBORItem item;
  if (QCBORDecode_GetNext(&d, &item) != QCBOR_SUCCESS || item.uDataType != QCBOR_TYPE_BYTE_STRING ||
      item.val.string.len != exact)
    return false;
  memcpy(output, item.val.string.ptr, exact);
  return true;
}
static bool validUtf8(const uint8_t* s, size_t n) {
  for (size_t i = 0; i < n;) {
    uint8_t c = s[i++];
    if (c < 0x80)
      continue;
    size_t more;
    uint32_t cp;
    if ((c & 0xe0) == 0xc0) {
      more = 1;
      cp = c & 0x1f;
      if (cp < 2)
        return false;
    } else if ((c & 0xf0) == 0xe0) {
      more = 2;
      cp = c & 0x0f;
    } else if ((c & 0xf8) == 0xf0) {
      more = 3;
      cp = c & 7;
    } else
      return false;
    if (i + more > n)
      return false;
    for (size_t j = 0; j < more; j++) {
      uint8_t x = s[i++];
      if ((x & 0xc0) != 0x80)
        return false;
      cp = (cp << 6) | (x & 0x3f);
    }
    if ((more == 2 && cp < 0x800) || (more == 3 && cp < 0x10000) || cp > 0x10ffff ||
        (cp >= 0xd800 && cp <= 0xdfff))
      return false;
  }
  return true;
}
static bool getText(QCBORDecodeContext& d, char* output, size_t capacity, bool allowEmpty = false) {
  QCBORItem item;
  if (QCBORDecode_GetNext(&d, &item) != QCBOR_SUCCESS || item.uDataType != QCBOR_TYPE_TEXT_STRING ||
      item.val.string.len >= capacity || (!allowEmpty && item.val.string.len == 0) ||
      memchr(item.val.string.ptr, 0, item.val.string.len) ||
      !validUtf8((const uint8_t*)item.val.string.ptr, item.val.string.len))
    return false;
  memcpy(output, item.val.string.ptr, item.val.string.len);
  output[item.val.string.len] = 0;
  return true;
}
static bool finish(QCBORDecodeContext& d) { return QCBORDecode_Finish(&d) == QCBOR_SUCCESS; }
static bool startEncode(QCBOREncodeContext& e, uint8_t* output, size_t size) {
  if (!output || size == 0 || size > MAX_DEVICE_FRAME_SIZE)
    return false;
  QCBOREncode_Init(&e, {output, size});
  QCBOREncode_OpenArray(&e);
  return true;
}
static bool finishEncode(QCBOREncodeContext& e, size_t& written) {
  QCBOREncode_CloseArray(&e);
  UsefulBufC encoded;
  if (QCBOREncode_Finish(&e, &encoded) != QCBOR_SUCCESS || encoded.len > MAX_DEVICE_FRAME_SIZE)
    return false;
  written = encoded.len;
  return true;
}
static void addText(QCBOREncodeContext& e, const char* value) {
  QCBOREncode_AddText(&e, UsefulBuf_FromSZ(value));
}
static bool startMessage(QCBORDecodeContext& d, const uint8_t* frame, size_t size, uint16_t arity,
                         MessageType expected, uint8_t expectedVersion = PROTOCOL_VERSION) {
  uint64_t type, version;
  return startArray(d, frame, size, arity) && getUInt(d, 255, type) && type == expected &&
         getUInt(d, 255, version) && version == expectedVersion;
}
static void addMessageHeader(QCBOREncodeContext& e, MessageType type) {
  QCBOREncode_AddUInt64(&e, type);
  QCBOREncode_AddUInt64(&e, PROTOCOL_VERSION);
}
bool parseAuthChallenge(const uint8_t* frame, size_t size, AuthChallenge& challenge) {
  QCBORDecodeContext d;
  return startMessage(d, frame, size, 3, AUTH_CHALLENGE, AUTH_CHALLENGE_VERSION) &&
         getBytes(d, challenge.challenge, 32) && finish(d);
}
static void appendField(uint8_t* input, size_t& offset, const void* value, size_t length) {
  input[offset++] = (length >> 24) & 0xff;
  input[offset++] = (length >> 16) & 0xff;
  input[offset++] = (length >> 8) & 0xff;
  input[offset++] = length & 0xff;
  memcpy(input + offset, value, length);
  offset += length;
}
bool buildAuthResponse(uint8_t* output, size_t outputSize, size_t& written, const char* deviceId,
                       const uint8_t secret[32], const uint8_t challenge[32]) {
  static const char domain[] = "mindflayer-device-auth-v1";
  if (!deviceId || !secret || !challenge || strlen(deviceId) == 0 ||
      strlen(deviceId) > MAX_DEVICE_ID)
    return false;
  uint8_t input[4 + sizeof(domain) - 1 + 4 + MAX_DEVICE_ID + 4 + 32], digest[32];
  size_t inputLength = 0;
  appendField(input, inputLength, domain, sizeof(domain) - 1);
  appendField(input, inputLength, deviceId, strlen(deviceId));
  appendField(input, inputLength, challenge, 32);
#ifdef __linux__
  unsigned int digestLength = sizeof(digest);
  HMAC(EVP_sha256(), secret, 32, input, inputLength, digest, &digestLength);
#else
  br_hmac_key_context key;
  br_hmac_context context;
  br_hmac_key_init(&key, &br_sha256_vtable, secret, 32);
  br_hmac_init(&context, &key, 0);
  br_hmac_update(&context, input, inputLength);
  br_hmac_out(&context, digest);
#endif
  QCBOREncodeContext e;
  if (!startEncode(e, output, outputSize))
    return false;
  addMessageHeader(e, AUTH_RESPONSE);
  addText(e, deviceId);
  QCBOREncode_AddBytes(&e, {digest, sizeof(digest)});
  return finishEncode(e, written);
}
bool parseAuthResult(const uint8_t* frame, size_t size, AuthResult& result) {
  QCBORDecodeContext d;
  uint64_t status;
  if (!startMessage(d, frame, size, 4, AUTH_RESULT) || !getUInt(d, 1, status) ||
      !getText(d, result.deviceId, sizeof(result.deviceId), status == AUTH_FAILED) || !finish(d))
    return false;
  result.status = static_cast<AuthStatus>(status);
  return true;
}
bool buildRegistration(uint8_t* output, size_t outputSize, size_t& written, const char* firmware,
                       const char* hardware) {
  if (!firmware || !hardware || strlen(firmware) == 0 || strlen(firmware) > MAX_VERSION ||
      strlen(hardware) == 0 || strlen(hardware) > MAX_HARDWARE_ID)
    return false;
  QCBOREncodeContext e;
  if (!startEncode(e, output, outputSize))
    return false;
  addMessageHeader(e, REGISTRATION);
  addText(e, firmware);
  addText(e, hardware);
  return finishEncode(e, written);
}
bool buildKeyEvent(uint8_t* output, size_t outputSize, size_t& written, const char* key,
                   bool isDown) {
  size_t code = 0;
  while (code < sizeof(KEYS) / sizeof(KEYS[0]) && strcmp(KEYS[code], key))
    code++;
  if (code == sizeof(KEYS) / sizeof(KEYS[0]))
    return false;
  QCBOREncodeContext e;
  if (!startEncode(e, output, outputSize))
    return false;
  addMessageHeader(e, KEY_EVENT);
  QCBOREncode_AddUInt64(&e, code);
  QCBOREncode_AddUInt64(&e, isDown ? ACTION_DOWN : ACTION_UP);
  return finishEncode(e, written);
}
bool parseConfiguration(const uint8_t* frame, size_t size, Configuration& configuration) {
  QCBORDecodeContext d;
  uint64_t c[6];
  if (!startMessage(d, frame, size, 8, CONFIGURATION))
    return false;
  for (uint8_t i = 0; i < 6; i++)
    if (!getUInt(d, 255, c[i]))
      return false;
  if (!finish(d))
    return false;
  configuration = {{(uint8_t)c[0], (uint8_t)c[1], (uint8_t)c[2]},
                   {(uint8_t)c[3], (uint8_t)c[4], (uint8_t)c[5]}};
  return true;
}
bool parseUpdateAvailable(const uint8_t* frame, size_t size, UpdateAvailable& update) {
  QCBORDecodeContext d;
  uint64_t firmwareSize;
  if (!startMessage(d, frame, size, 7, UPDATE_AVAILABLE) ||
      !getText(d, update.version, sizeof(update.version)) ||
      !getUInt(d, 0xffffffff, firmwareSize) || firmwareSize == 0 ||
      !getBytes(d, update.sha256, 32) || !getText(d, update.path, sizeof(update.path)) ||
      !getBytes(d, update.token, 32) || !finish(d))
    return false;
  update.size = (uint32_t)firmwareSize;
  return true;
}
bool parseFirmwareAccepted(const uint8_t* frame, size_t size, FirmwareAccepted& accepted) {
  QCBORDecodeContext d;
  return startMessage(d, frame, size, 3, FIRMWARE_ACCEPTED) &&
         getText(d, accepted.version, sizeof(accepted.version)) && finish(d);
}
bool shouldRestart(bool q, bool shift, bool space) { return q && shift && space; }
} // namespace protocol
} // namespace mindflayer
