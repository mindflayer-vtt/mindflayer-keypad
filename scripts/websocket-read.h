// Inserted inside websockets::internals in the pinned dependency by the patcher.
// All header fields and payloads use exact, bounded reads before decoding.
#ifndef _WS_CONFIG_MAX_MESSAGE_SIZE
#error "The device WebSocket transport requires a pre-allocation message limit"
#endif

uint32_t readUntilSuccessfullOrError(network::TcpClient& socket, uint8_t* buffer,
                                     const uint32_t len) {
  uint32_t received = 0;
  const uint32_t started = millis();
  while (received < len && socket.available()) {
    if (static_cast<uint32_t>(millis() - started) >= 1000) {
      socket.close();
      return 0;
    }
    const uint32_t count = socket.read(buffer + received, len - received);
    if (!count || count == static_cast<uint32_t>(-1)) {
      optimistic_yield(1000);
      continue;
    }
    if (count > len - received) {
      socket.close();
      return 0;
    }
    received += count;
  }
  if (received != len) {
    socket.close();
    return 0;
  }
  return received;
}

Header readHeaderFromSocket(network::TcpClient& socket) {
  Header header{};
  readUntilSuccessfullOrError(socket, reinterpret_cast<uint8_t*>(&header), 2);
  return header;
}

uint64_t readExtendedPayloadLength(network::TcpClient& socket, const Header& header) {
  if (header.payload < 126)
    return header.payload;
  uint8_t bytes[8] = {};
  const uint32_t count = header.payload == 126 ? 2 : 8;
  if (readUntilSuccessfullOrError(socket, bytes, count) != count)
    return 0;
  uint64_t length = 0;
  for (uint32_t i = 0; i < count; ++i)
    length = (length << 8) | bytes[i];
  return length;
}

void readMaskingKey(network::TcpClient& socket, uint8_t* outputBuffer) {
  readUntilSuccessfullOrError(socket, outputBuffer, 4);
}

WSString readData(network::TcpClient& socket, uint64_t length) {
  // Defense in depth: _recv checks both frame and aggregate bounds first.
  if (length > _WS_CONFIG_MAX_MESSAGE_SIZE) {
    socket.close();
    return {};
  }
  WSString data(static_cast<size_t>(length), '\0');
  if (length && readUntilSuccessfullOrError(socket, reinterpret_cast<uint8_t*>(&data[0]),
                                            static_cast<uint32_t>(length)) != length)
    return {};
  return data;
}
