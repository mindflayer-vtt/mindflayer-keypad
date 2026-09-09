# Firmware regression tests

For an interactive physical keypad test through the real server and a Foundry-facing
receiver, see [HARDWARE_TESTING.md](HARDWARE_TESTING.md).

Run the native suites, build the production dependencies, and then run the host regressions:

```sh
.venv/bin/pio test -e native -e native_sanitize
.venv/bin/pio run -e keypad
python3 -m unittest discover -s test -p 'test_*.py'
npm run format:check
```

The Python runner needs a host `g++` with AddressSanitizer and UndefinedBehaviorSanitizer. CI runs these tests after the normal firmware build, using the same patched ArduinoWebsockets sources compiled into that firmware. Missing transport sources fail the test setup rather than skipping coverage.

`test/host/application_deadline.cpp` links the real `src/Application.cpp` against simulated SDK time, flash settings, and network I/O. Its six scenarios check unavailable Wi-Fi, provisioning failure, invalid server-key loading, a yielding connection stall, a yielding poll stall, and a permanent boot that must not time out. Temporary scenarios must restart at exactly 90 seconds even when the application has not returned from `setup()` or `poll()`. These tests failed against the original scheduling logic before the fix.

The same host runner checks the restored Shift + Space + E restart shortcut for
all 4,096 matrix-state combinations, both provisioned and unprovisioned. E and
both modifiers are required; the old Q-based combination and incomplete chords
must not restart. This regression failed before restoring the handler. It tests
the application decision, not physical scanning or switch debounce.

`test/test_rboot_bootloader.py` checks the bootloader build guard: the binary must
fit 4 KiB, contain the transactional patch's safe-default marker, and omit the
legacy config-writing marker. `build-rboot.sh` runs this guard before copying its
output. The guard rejected the unpatched binary exposed by physical OTA testing.
This is a build-provenance check, not a substitute for testing metadata behavior.

`test/host/websocket_transport.cpp` compiles the pinned library's actual endpoint, client, message, and support sources with a fake TCP socket. Nine scenarios exercise exactly 512 bytes, oversized 16-bit and 64-bit length declarations, valid and oversized fragmentation with an interleaved ping, multiple buffered messages, one-byte TCP reads, and stalled header/body reads. An allocation guard prevents large advertised lengths from exhausting the test host. The oversized cases must disconnect at the header without reading a body, and two buffered messages must be delivered on separate polls. The corresponding regressions failed against the original transport before the fix.

`test/test_install_rboot.py` runs the installer with a mocked esptool interface and metadata produced by the real generator. It covers artifact size boundaries, corrupt images and metadata, unsafe slot selection, flash-capacity checks, backup failures, private backup permissions, pre-write manifests, failed writes, provisioning readback, and validated snapshots surviving source-file changes. Invalid local inputs must fail before any serial access; failed target checks or incomplete backups must prevent every flash write. A layout regression checks the installer's constants against the firmware header. Run this suite alone without a compiler or hardware:

```sh
python3 -m unittest discover -s test -p test_install_rboot.py
```

The SDK, TCP, and esptool doubles do not replace hardware verification of the exact candidate image, reset behavior, TLS, flash, keypad scanning, or LEDs. The owner has deferred the hardware matrix in `OTA_BOOT.md` and remaining production signing/archive/server-import checks until after the first release; they remain unverified for that release. See [release-audit.md](release-audit.md) for the recorded decision.
