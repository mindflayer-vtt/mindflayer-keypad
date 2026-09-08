# Pre-upgrade baseline

Recorded with PlatformIO Core 6.1.19. No platform or library version was changed.

## Toolchain and commands

- Platform: `espressif8266@4.2.1`
- Framework: `framework-arduinoespressif8266@3.30102.0` (Arduino core 3.1.2)
- Compiler: `toolchain-xtensa@2.100300.220621` (GCC 10.3.0)
- Libraries: ArduinoWebsockets 0.5.4, ArduinoJson-esphomelib 6.15.2,
  NeoPixelBus 2.8.0, ArduinoOTA 1.1.0
- Host tests:
  `PLATFORMIO_CORE_DIR=/tmp/mindflayer-platformio .venv/bin/pio test -e native`
- Firmware build:
  `PLATFORMIO_CORE_DIR=/tmp/mindflayer-platformio .venv/bin/pio run -e controller_1`

The ignored `include/config.h` must exist. For non-hardware verification it was
created from `include/config.example.h` with dummy Wi-Fi, OTA, and WebSocket
values.

## Missing-config/Xtensa failure

A clean build without `include/config.h` fails. Because the source uses the
generic quoted include `#include "config.h"`, the compiler resolves another
`config.h` from its include search path instead of reporting a missing file.
The expected `WIFI_SSID`, `WIFI_PASS`, `OTA_PASSWORD`, and `WSS_URL`
macros are therefore absent. It also reaches `Arduino.h` before the Xtensa
address macros and reports:

```text
mmu_iram.h:116:43: error: 'XCHAL_INSTRAM1_VADDR' was not declared in this scope
mmu_iram.h:132:41: error: 'XCHAL_INSTRAM0_VADDR' was not declared in this scope
mmu_iram.h:140:45: error: 'XCHAL_INSTROM0_VADDR' was not declared in this scope
src/main.cpp:18:20: error: 'WIFI_SSID' was not declared in this scope
```

The temporary command-line workaround used during initial investigation was:

```sh
PLATFORMIO_BUILD_FLAGS='-include xtensa/config/core.h -DESP_NAME="controller1"' \
  .venv/bin/pio run -e controller_1
```

That workaround is not required when the documented `include/config.h`
prerequisite exists, so it has not been added to project configuration.

## Results and existing warnings

- Native tests: 6 passed.
- Firmware: RAM 31,736/81,920 bytes (38.7%).
- Firmware: flash 435,123/958,448 bytes (45.4%).
- ArduinoWebsockets 0.5.4 and ArduinoOTA 1.1.0 call the deprecated
  `WiFiServer::available()`; ESP8266 core 3.1.2 recommends `accept()`.
- Under host Python 3.14, the framework's `elf2bin.py` emits two
  `SyntaxWarning` messages for the `'\s+'` escape.

Hardware Wi-Fi, TLS, OTA, GPIO scanning, LEDs, and physical debounce behavior
remain manual-test boundaries.
