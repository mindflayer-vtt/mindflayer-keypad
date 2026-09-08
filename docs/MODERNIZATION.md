# Toolchain and dependency modernization

## 2026-08-29 baseline and upgrade

The current stable PlatformIO ESP8266 platform was retained because 4.2.1 is still the latest stable release. The resolved toolchain remains:

- PlatformIO Core 6.1.19
- espressif8266 4.2.1
- Arduino framework 3.30102.0 / ESP8266 Arduino core 3.1.2
- Xtensa GCC package 2.100300.220621 / GCC 10.3.0

Library changes:

- Removed standalone ArduinoOTA 1.1.0 and adopted the OTA implementation bundled with the ESP8266 Arduino framework.
- Replaced the legacy `ArduinoJson-esphomelib` 6.15.2 fork with official ArduinoJson 7.4.3 and migrated `DynamicJsonDocument` to `JsonDocument`.
- NeoPixelBus: 2.8.0 -> 2.8.4.
- ArduinoWebsockets remains at its latest released version, 0.5.4.

Reproduce with a configured `include/config.h`:

```sh
.venv/bin/pio test -e native
.venv/bin/pio run -e controller_1
```

All six native protocol/configuration tests and a clean firmware compile pass. Final size is 31,632 / 81,920 bytes RAM (38.6%) and 462,888 / 958,448 bytes flash (48.3%). Against the pre-upgrade build this is 104 fewer RAM bytes and 27,765 additional flash bytes. Most flash growth came from the framework-bundled OTA implementation.

The former forced pre-include of the Xtensa configuration header is no longer required. Remaining warnings are dependency/toolchain-owned: ArduinoWebsockets compiles a deprecated `WiFiServer::available()` path, and the bundled `elf2bin.py` emits invalid-escape warnings under local Python 3.14. The CI workflow uses Python 3.13 and pins PlatformIO 6.1.19.

Hardware verification of keypad scanning, LEDs, OTA, and WSS/TLS remains necessary. The embedded server certificate expired in 2025, so certificate/trust replacement is a high-priority follow-up before physical WSS validation.
