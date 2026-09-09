#pragma once
#include <Arduino.h>

extern bool wifiConnected, publicKeyValid;
constexpr int WL_CONNECTED = 3;
inline unsigned testWifiBegins = 0;
struct FakeWifi {
  void hostname(const char*) {}
  void begin(const char*, const char*) { ++testWifiBegins; }
  int status() { return wifiConnected ? WL_CONNECTED : 0; }
  struct Address {
    String toString() { return "192.0.2.1"; }
  };
  Address localIP() { return {}; }
};
inline FakeWifi WiFi;
namespace BearSSL {
struct PublicKey {
  PublicKey(const uint8_t*, size_t) {}
  bool isRSA() { return publicKeyValid; }
  bool isEC() { return false; }
};
} // namespace BearSSL
