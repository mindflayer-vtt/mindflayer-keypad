#pragma once
namespace websockets {
struct WebsocketsClient {
  bool connected = true;
  bool available() { return connected; }
};
} // namespace websockets
