# Firmware regression tests

Run the native suites, build the production dependencies, and then run the host regressions:

```sh
.venv/bin/pio test -e native -e native_sanitize
.venv/bin/pio run -e keypad
python3 -m unittest discover -s test -p 'test_*.py'
npm run format:check
```

The Python runner needs a host `g++` with AddressSanitizer and UndefinedBehaviorSanitizer. CI runs these tests after the normal firmware build, using the same patched ArduinoWebsockets sources compiled into that firmware. Missing transport sources fail the test setup rather than skipping coverage.

`test/host/application_deadline.cpp` links the real `src/Application.cpp` against simulated SDK time, flash settings, and network I/O. Its six scenarios check unavailable Wi-Fi, provisioning failure, invalid server-key loading, a yielding connection stall, a yielding poll stall, and a permanent boot that must not time out. Temporary scenarios must restart at exactly 90 seconds even when the application has not returned from `setup()` or `poll()`. These tests failed against the original scheduling logic before the fix.

`test/host/websocket_transport.cpp` compiles the pinned library's actual endpoint, client, message, and support sources with a fake TCP socket. Nine scenarios exercise exactly 512 bytes, oversized 16-bit and 64-bit length declarations, valid and oversized fragmentation with an interleaved ping, multiple buffered messages, one-byte TCP reads, and stalled header/body reads. An allocation guard prevents large advertised lengths from exhausting the test host. The oversized cases must disconnect at the header without reading a body, and two buffered messages must be delivered on separate polls. The corresponding regressions failed against the original transport before the fix.

The SDK and TCP doubles model scheduling and transport behavior; they do not replace hardware verification of the exact candidate image, reset behavior, TLS, flash, keypad scanning, or LEDs. Keep the OTA/recovery hardware matrix in `OTA_BOOT.md` as the release validation requirement.
