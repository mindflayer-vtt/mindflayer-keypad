// Compile and exercise the actual patched ArduinoWebsockets transport on host.
#include <FakeTcpClient.h>
#include <cassert>
#include <cstdio>
#include <cstdlib>
#include <new>
#include <tiny_websockets/client.hpp>

using namespace websockets;
namespace {
uint32_t now = 0;
bool guardAllocations = false;
size_t largestAllocation = 0;
} // namespace

void* operator new(size_t size) {
  if (guardAllocations) {
    largestAllocation = std::max(largestAllocation, size);
    if (size > 2048)
      throw std::bad_alloc();
  }
  if (void* p = std::malloc(size ? size : 1))
    return p;
  throw std::bad_alloc();
}
void operator delete(void* p) noexcept { std::free(p); }
void operator delete(void* p, size_t) noexcept { std::free(p); }
unsigned long millis() { return now; }
void delay(unsigned long ms) {
  now += ms;
  if (now > 10000)
    throw "socket read did not time out";
}
void optimistic_yield(uint32_t) { delay(1); }

static void frame(FakeTcpClient& socket, uint8_t opcode, bool fin, size_t size, bool body = true,
                  bool extended64 = false) {
  socket.input.push_back((fin ? 0x80 : 0) | opcode);
  if (extended64) {
    socket.input.push_back(127);
    for (int shift = 56; shift >= 0; shift -= 8)
      socket.input.push_back(uint64_t(size) >> shift);
  } else if (size >= 126) {
    socket.input.insert(socket.input.end(), {126, uint8_t(size >> 8), uint8_t(size)});
  } else
    socket.input.push_back(size);
  if (body)
    socket.input.insert(socket.input.end(), size, 0x42);
}

int main(int argc, char** argv) {
  assert(argc == 2);
  std::string scenario = argv[1];
  auto socket = std::make_shared<FakeTcpClient>();
  WebsocketsClient client(socket);
  size_t messages = 0, length = 0;
  client.onMessage([&](WebsocketsMessage message) {
    assert(message.isBinary());
    ++messages;
    length = message.length();
  });
  bool oversized = false, stalled = false;
  size_t stopAt = 0;
  if (scenario == "exact-limit")
    frame(*socket, 2, true, 512);
  else if (scenario == "oversized" || scenario == "oversized-64") {
    frame(*socket, 2, true, scenario == "oversized" ? 513 : 65536, false,
          scenario == "oversized-64");
    oversized = true;
    stopAt = socket->input.size();
  } else if (scenario == "fragments-at-limit" || scenario == "fragments-oversized") {
    frame(*socket, 2, false, 300);
    frame(*socket, 9, true, 4); // Interleaved ping must not reset the aggregate limit.
    frame(*socket, 0, true, scenario == "fragments-at-limit" ? 212 : 213,
          scenario == "fragments-at-limit");
    oversized = scenario == "fragments-oversized";
    stopAt = socket->input.size();
  } else if (scenario == "batched") {
    frame(*socket, 2, true, 9);
    frame(*socket, 2, true, 9);
  } else if (scenario == "partial-reads") {
    frame(*socket, 2, true, 512);
    socket->chunk = 1;
  } else if (scenario == "stalled-body") {
    frame(*socket, 2, true, 512, false);
    stalled = true;
  } else if (scenario == "stalled-header") {
    socket->input.push_back(0x82);
    stalled = true;
  } else
    assert(false);

  guardAllocations = true;
  try {
    client.poll();
    if (scenario == "batched") {
      assert(messages == 1); // Allow dispatch before receiving the next message.
      client.poll();
      assert(messages == 2 && client.available());
    } else {
      for (int i = 0; i < 5 && socket->connected && socket->poll(); ++i)
        client.poll();
      if (oversized) {
        assert(!socket->connected && messages == 0 && socket->cursor == stopAt);
        assert(client.getCloseReason() == CloseReason_MessageTooBig);
      } else if (stalled) {
        assert(!socket->connected && messages == 0 && now <= 2000);
      } else
        assert(messages == 1 && length == 512 && socket->connected);
    }
  } catch (...) {
    guardAllocations = false;
    std::fprintf(stderr, "unbounded allocation or socket read (largest allocation: %zu)\n",
                 largestAllocation);
    return 1;
  }
  guardAllocations = false;
  assert(largestAllocation <= 2048);
  std::puts("WebSocket transport regression passed");
}
