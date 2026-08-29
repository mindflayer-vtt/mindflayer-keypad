#pragma once
#include <stddef.h>
#include <stdint.h>
namespace mindflayer { namespace provisioning {
constexpr size_t MAX_DEVICE_ID = 64, MAX_SSID = 32, MAX_WIFI_PASSWORD = 63, MAX_SERVER_HOST = 253, MAX_SERVER_PUBLIC_KEY = 512;
constexpr size_t MAX_PAYLOAD_SIZE = 1024, ENVELOPE_HEADER_SIZE = 7, ENVELOPE_CRC_SIZE = 4, MAX_ENVELOPE_SIZE = ENVELOPE_HEADER_SIZE + MAX_PAYLOAD_SIZE + ENVELOPE_CRC_SIZE;
constexpr uint8_t ENVELOPE_VERSION = 1, SCHEMA_VERSION = 1;
struct Provisioning {
  char deviceId[MAX_DEVICE_ID + 1]; uint8_t deviceSecret[32]; char ssid[MAX_SSID + 1];
  char wifiPassword[MAX_WIFI_PASSWORD + 1]; char serverHost[MAX_SERVER_HOST + 1]; uint16_t serverPort;
  uint8_t serverPublicKey[MAX_SERVER_PUBLIC_KEY]; uint16_t serverPublicKeyLength;
};
uint32_t crc32(const uint8_t*, size_t);
bool validateServerPublicKey(const uint8_t*, size_t);
bool decodePayload(const uint8_t*, size_t, Provisioning&);
bool decodeEnvelope(const uint8_t*, size_t, Provisioning&);
bool encodeEnvelope(const Provisioning&, uint8_t*, size_t, size_t&);
} }
