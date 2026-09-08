#pragma once
#include <algorithm>
#include <tiny_websockets/network/tcp_client.hpp>
#include <vector>

class FakeTcpClient : public websockets::network::TcpClient {
public:
  std::vector<uint8_t> input;
  size_t cursor = 0, chunk = 512;
  bool connected = true;
  bool poll() override { return cursor < input.size(); }
  bool available() override { return connected; }
  void close() override { connected = false; }
  bool connect(const websockets::WSString&, int) override { return true; }
  void send(const websockets::WSString&) override {}
  void send(const websockets::WSString&&) override {}
  void send(const uint8_t*, uint32_t) override {}
  websockets::WSString readLine() override { return {}; }
  uint32_t read(uint8_t* buffer, uint32_t length) override {
    size_t n = std::min({size_t(length), chunk, input.size() - cursor});
    if (!n) {
      delay(1);
      return uint32_t(-1);
    }
    memcpy(buffer, input.data() + cursor, n);
    cursor += n;
    return n;
  }
  int getSocket() const override { return -1; }
};
