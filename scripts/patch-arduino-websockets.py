"""Harden the pinned ArduinoWebsockets 0.5.4 dependency before compilation.

Run as a PlatformIO post script (after dependency installation, before SCons
workers start), or pass a library src directory when preparing host tests.
Original sources and the last generated hash are kept in the ignored dependency
directory. Unexpected upstream sources or local edits fail closed.
"""

import hashlib
from pathlib import Path


def digest(source):
    return hashlib.sha256(source.encode()).hexdigest()


def replace_once(source, before, after):
    if source.count(before) != 1:
        raise RuntimeError("Pinned ArduinoWebsockets source shape changed")
    return source.replace(before, after, 1)


def patch_file(path, expected, transform):
    current = path.read_text()
    original_path = path.with_name(path.name + ".mindflayer-original")
    stamp = path.with_name(path.name + ".mindflayer-sha256")
    original = original_path.read_text() if original_path.exists() else current
    if digest(original) != expected:
        raise RuntimeError(f"Unexpected upstream source: {path}")
    if current != original and (not stamp.exists() or digest(current) != stamp.read_text()):
        raise RuntimeError(f"Refusing to overwrite local dependency edits: {path}")
    patched = transform(original)
    if not original_path.exists():
        original_path.write_text(original)
    if current != patched:
        path.write_text(patched)
    stamp.write_text(digest(patched))


def endpoint(source):
    start = source.index("    uint32_t readUntilSuccessfullOrError")
    end = source.index("    void remaskData(WSString& data, const uint8_t*")
    helpers = (SCRIPT_DIR / "websocket-read.h").read_text()
    source = source[:start] + helpers + "\n\n" + source[end:]
    source = replace_once(source, "uint8_t maskingKey[4];", "uint8_t maskingKey[4] = {};")
    source = replace_once(source, "WebsocketsFrame frame;", "WebsocketsFrame frame{};")
    source = replace_once(source, """#ifdef _WS_CONFIG_MAX_MESSAGE_SIZE
        if(payloadLength > _WS_CONFIG_MAX_MESSAGE_SIZE) {
            return WebsocketsFrame();
        }
#endif""", """        // Reject before allocating or reading the payload. Control frames do
        // not consume/reset the aggregate budget of an interrupted message.
        const bool control = (header.opcode & 8) != 0;
        if (header.flags || header.mask ||
            (control && (!header.fin || payloadLength > 125 ||
                         (header.opcode != 8 && header.opcode != 9 && header.opcode != 10))) ||
            (!control && ((header.opcode != 0 && header.opcode != 1 && header.opcode != 2) ||
                          (header.opcode == 0 && _recvMode != RecvMode_Streaming) ||
                          (header.opcode != 0 && _recvMode == RecvMode_Streaming)))) {
            close(CloseReason_ProtocolError);
            return {};
        }
        if (payloadLength > _WS_CONFIG_MAX_MESSAGE_SIZE ||
            (!control && payloadLength > _WS_CONFIG_MAX_MESSAGE_SIZE - _receivedMessageSize)) {
            close(CloseReason_MessageTooBig);
            return {};
        }
        if (!control)
            _receivedMessageSize = header.fin ? 0 : _receivedMessageSize + payloadLength;""")
    source = source.replace("_closeReason(other._closeReason),", "_receivedMessageSize(other._receivedMessageSize),\n        _closeReason(other._closeReason),")
    source = source.replace("this->_closeReason = other._closeReason;", "this->_receivedMessageSize = other._receivedMessageSize;\n        this->_closeReason = other._closeReason;")
    return source


def client(source):
    start = source.index("    bool WebsocketsClient::poll()")
    end = source.index("    WebsocketsMessage WebsocketsClient::readBlocking()", start)
    poll = source[start:end]
    poll = replace_once(poll, "while(available() && _endpoint.poll())", "if(available() && _endpoint.poll())")
    poll = replace_once(poll, "continue;", "return false;")
    return source[:start] + poll + source[end:]


def patch_library(root):
    header = root / "tiny_websockets/network/generic_esp/generic_esp_clients.hpp"
    source = header.read_text()
    # Accept both pristine 0.5.4 and the earlier yield-only project patch.
    if source.count("yield();") == 10 and source.count("optimistic_yield(1000);") == 0:
        header.write_text(source.replace("yield();", "optimistic_yield(1000);"))
    elif source.count("yield();") != 0 or source.count("optimistic_yield(1000);") != 10:
        raise RuntimeError("Unexpected ArduinoWebsockets ESP yield implementation")
    patch_file(root / "websockets_endpoint.cpp",
               "ecdc4e53657e939ba00db7324ac15be884e8adf2692634d405c72bf0591bf3c6", endpoint)
    patch_file(root / "websockets_client.cpp",
               "9650f4b5b34082d48f14c8576e82eebe7f1eb047b92d1c7eaf46652782b75abd", client)
    patch_file(root / "tiny_websockets/internals/websockets_endpoint.hpp",
               "b7932c7cbb9d36e96df5fb03bf9845253bde3c1661153a0c518f6c75029f4320",
               lambda s: replace_once(s, "        CloseReason _closeReason;",
                                     "        size_t _receivedMessageSize = 0;\n        CloseReason _closeReason;"))


if "Import" in globals():
    Import("env")
    SCRIPT_DIR = Path(env.subst("$PROJECT_DIR")) / "scripts"
    patch_library(Path(env.subst("$PROJECT_LIBDEPS_DIR")) / env.subst("$PIOENV") / "ArduinoWebsockets/src")
elif __name__ == "__main__":
    import sys
    SCRIPT_DIR = Path(__file__).resolve().parent
    patch_library(Path(sys.argv[1]))
