#include "Provisioning.h"
#include <string.h>
#include <qcbor/qcbor_decode.h>
#include <qcbor/qcbor_encode.h>
#ifdef __linux__
#include <openssl/evp.h>
#include <openssl/x509.h>
#elif defined(ARDUINO)
#include <BearSSLHelpers.h>
#endif
namespace mindflayer { namespace provisioning {
static const uint8_t MAGIC[4] = {'M', 'F', 'P', '1'};
enum Field : uint8_t { VERSION = 0, DEVICE_ID = 1, DEVICE_SECRET = 2, WIFI_SSID = 3, WIFI_PASSWORD = 4, SERVER_HOST = 5, SERVER_PORT = 6, SERVER_PUBLIC_KEY = 7 };
uint32_t crc32(const uint8_t* data, size_t size) {
  uint32_t crc = 0xffffffff; for (size_t i = 0; i < size; i++) { crc ^= data[i]; for (uint8_t bit = 0; bit < 8; bit++) crc = (crc >> 1) ^ (0xedb88320 & (0u - (crc & 1))); } return crc ^ 0xffffffff;
}
static bool derLength(const uint8_t* data, size_t size, size_t& header, size_t& content) {
  if (size < 2 || data[0] != 0x30) return false;
  uint8_t first = data[1];
  if (first < 0x80) { header = 2; content = first; return header + content == size; }
  uint8_t count = first & 0x7f; if (count == 0 || count > 2 || size < (size_t)2 + count) return false;
  content = 0;
  for (uint8_t i = 0; i < count; i++) content = (content << 8) | data[2 + i];
  header = 2 + count;
  return content >= 128 && header + content == size;
}
bool validateServerPublicKey(const uint8_t* data, size_t size) {
  static const uint8_t rsaAlgorithm[] = {0x30,0x0d,0x06,0x09,0x2a,0x86,0x48,0x86,0xf7,0x0d,0x01,0x01,0x01,0x05,0x00};
  size_t header, content; if (!data || size < 32 || size > MAX_SERVER_PUBLIC_KEY || !derLength(data, size, header, content) || header + sizeof(rsaAlgorithm) >= size || memcmp(data + header, rsaAlgorithm, sizeof(rsaAlgorithm))) return false;
#ifdef __linux__
  const unsigned char* cursor = data; EVP_PKEY* key = d2i_PUBKEY(nullptr, &cursor, size); bool valid = key && cursor == data + size; EVP_PKEY_free(key); return valid;
#elif defined(ARDUINO)
  BearSSL::PublicKey key(data, size); return key.isRSA();
#else
  return true;
#endif
}
static bool validUtf8(const uint8_t* s, size_t n) {
  for(size_t i=0;i<n;){uint8_t c=s[i++];if(c<0x80)continue;size_t more;uint32_t cp;if((c&0xe0)==0xc0){more=1;cp=c&0x1f;if(cp<2)return false;}else if((c&0xf0)==0xe0){more=2;cp=c&0x0f;}else if((c&0xf8)==0xf0){more=3;cp=c&7;}else return false;if(i+more>n)return false;for(size_t j=0;j<more;j++){uint8_t x=s[i++];if((x&0xc0)!=0x80)return false;cp=(cp<<6)|(x&0x3f);}if((more==2&&cp<0x800)||(more==3&&cp<0x10000)||cp>0x10ffff||(cp>=0xd800&&cp<=0xdfff))return false;}return true;
}
static bool copyText(const QCBORItem& item, char* output, size_t maximum, bool password = false) {
  if (item.uDataType != QCBOR_TYPE_TEXT_STRING || item.val.string.len > maximum || (!password && item.val.string.len == 0) || !validUtf8((const uint8_t*)item.val.string.ptr,item.val.string.len)) return false;
  memcpy(output, item.val.string.ptr, item.val.string.len); output[item.val.string.len] = 0; return true;
}
bool decodePayload(const uint8_t* payload, size_t size, Provisioning& output) {
  if (!payload || size == 0 || size > MAX_PAYLOAD_SIZE) return false;
  QCBORDecodeContext decoder; QCBORDecode_Init(&decoder, {payload, size}, QCBOR_DECODE_MODE_NORMAL); QCBORItem map;
  if (QCBORDecode_GetNext(&decoder, &map) != QCBOR_SUCCESS || map.uDataType != QCBOR_TYPE_MAP || map.val.uCount < 8 || map.val.uCount > 16) return false;
  static Provisioning candidate;
  memset(&candidate, 0, sizeof(candidate));
  uint16_t seen = 0;
  for (uint16_t i = 0; i < map.val.uCount; i++) {
    QCBORItem item; if (QCBORDecode_GetNext(&decoder, &item) != QCBOR_SUCCESS || item.uLabelType != QCBOR_TYPE_INT64 || item.label.int64 < 0 || item.label.int64 > 15) return false;
    uint8_t key = item.label.int64; if (seen & (1u << key)) return false; seen |= 1u << key;
    switch (key) {
      case VERSION: if (item.uDataType != QCBOR_TYPE_INT64 || item.val.int64 != SCHEMA_VERSION) return false; break;
      case DEVICE_ID: if (!copyText(item, candidate.deviceId, MAX_DEVICE_ID)) return false; break;
      case DEVICE_SECRET: if (item.uDataType != QCBOR_TYPE_BYTE_STRING || item.val.string.len != 32) return false; memcpy(candidate.deviceSecret, item.val.string.ptr, 32); break;
      case WIFI_SSID: if (!copyText(item, candidate.ssid, MAX_SSID)) return false; break;
      case WIFI_PASSWORD: if (!copyText(item, candidate.wifiPassword, MAX_WIFI_PASSWORD, true)) return false; break;
      case SERVER_HOST: if (!copyText(item, candidate.serverHost, MAX_SERVER_HOST)) return false; break;
      case SERVER_PORT: if (item.uDataType != QCBOR_TYPE_INT64 || item.val.int64 <= 0 || item.val.int64 > 65535) return false; candidate.serverPort = item.val.int64; break;
      case SERVER_PUBLIC_KEY:
        if (item.uDataType != QCBOR_TYPE_BYTE_STRING || !validateServerPublicKey((const uint8_t*)item.val.string.ptr, item.val.string.len)) return false;
        candidate.serverPublicKeyLength = item.val.string.len; memcpy(candidate.serverPublicKey, item.val.string.ptr, item.val.string.len); break;
      default: return false;
    }
  }
  if ((seen & 0xff) != 0xff || QCBORDecode_Finish(&decoder) != QCBOR_SUCCESS) return false;
  output = candidate;
  return true;
}
bool decodeEnvelope(const uint8_t* envelope, size_t size, Provisioning& output) {
  if (!envelope || size < ENVELOPE_HEADER_SIZE + ENVELOPE_CRC_SIZE || size > MAX_ENVELOPE_SIZE || memcmp(envelope, MAGIC, 4) || envelope[4] != ENVELOPE_VERSION) return false;
  size_t payloadSize = ((size_t)envelope[5] << 8) | envelope[6]; if (payloadSize == 0 || payloadSize > MAX_PAYLOAD_SIZE || size != ENVELOPE_HEADER_SIZE + payloadSize + ENVELOPE_CRC_SIZE) return false;
  size_t crcOffset = ENVELOPE_HEADER_SIZE + payloadSize; uint32_t stored = ((uint32_t)envelope[crcOffset] << 24) | ((uint32_t)envelope[crcOffset + 1] << 16) | ((uint32_t)envelope[crcOffset + 2] << 8) | envelope[crcOffset + 3];
  return stored == crc32(envelope, crcOffset) && decodePayload(envelope + ENVELOPE_HEADER_SIZE, payloadSize, output);
}
static void addText(QCBOREncodeContext& e, int64_t key, const char* value) { QCBOREncode_AddTextToMapN(&e, key, UsefulBuf_FromSZ(value)); }
bool encodeEnvelope(const Provisioning& p, uint8_t* output, size_t capacity, size_t& written) {
  if (!output || capacity < ENVELOPE_HEADER_SIZE + ENVELOPE_CRC_SIZE) return false;
  uint8_t payload[MAX_PAYLOAD_SIZE]; QCBOREncodeContext e; QCBOREncode_Init(&e, {payload, sizeof(payload)}); QCBOREncode_OpenMap(&e);
  QCBOREncode_AddUInt64ToMapN(&e, VERSION, SCHEMA_VERSION); addText(e, DEVICE_ID, p.deviceId); QCBOREncode_AddBytesToMapN(&e, DEVICE_SECRET, {p.deviceSecret, 32}); addText(e, WIFI_SSID, p.ssid); addText(e, WIFI_PASSWORD, p.wifiPassword); addText(e, SERVER_HOST, p.serverHost); QCBOREncode_AddUInt64ToMapN(&e, SERVER_PORT, p.serverPort); QCBOREncode_AddBytesToMapN(&e, SERVER_PUBLIC_KEY, {p.serverPublicKey, p.serverPublicKeyLength}); QCBOREncode_CloseMap(&e);
  UsefulBufC encoded; Provisioning validation;
  if (QCBOREncode_Finish(&e, &encoded) != QCBOR_SUCCESS || encoded.len > MAX_PAYLOAD_SIZE || !decodePayload(payload, encoded.len, validation)) return false;
  written = ENVELOPE_HEADER_SIZE + encoded.len + ENVELOPE_CRC_SIZE;
  if (written > capacity) return false;
  memcpy(output, MAGIC, 4); output[4] = ENVELOPE_VERSION; output[5] = encoded.len >> 8; output[6] = encoded.len; memcpy(output + ENVELOPE_HEADER_SIZE, payload, encoded.len);
  uint32_t crc = crc32(output, ENVELOPE_HEADER_SIZE + encoded.len); size_t o = ENVELOPE_HEADER_SIZE + encoded.len; output[o] = crc >> 24; output[o+1] = crc >> 16; output[o+2] = crc >> 8; output[o+3] = crc; return true;
}
} }
