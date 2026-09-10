# Hardware test — 2026-09-10

## Keypad 6 static-row voltage diagnostic

Following the preceding session's missing-key observations, inspection of the
checked-in schematic confirmed three 12 kΩ column series resistors and individual
switch diodes. Columns GPIO14/12/13 are internally pulled-up inputs; rows
GPIO5/4/0/2 are driven outputs in both the old and current production scanner.
The combination may leave insufficient LOW-level voltage margin. This is an
electrical hypothesis, not yet verified by in-circuit voltage measurements.
The operator's approximately 0.8 V diode reading was from a multimeter and has
not been established as the drop at the circuit's actual scanning current.

At the operator's request, a separate static-row diagnostic was built and
installed on keypad 6. Its MAC `8c:aa:b5:7a:d3:05` and exact 4 MiB flash capacity
were verified before writing. The first USB checks found no serial adapter;
reconnecting restored access before any flash write.

The diagnostic holds GPIO0/D3 (Z/X/C) LOW continuously, leaves other rows HIGH,
and retains `INPUT_PULLUP` on D5/D6/D7. Wi-Fi is disabled without saving network
settings. It has no scanner, debounce, server connection, LED control, OTA, or
serial provisioning. The project is isolated from production sources and release
builds. All 39 host tests and formatting checks passed; the standalone firmware
build succeeded. The code and measurement procedure are committed as `3676abc`.

The 272,368-byte Arduino/eboot image was written at `0x0`, replacing the old
bootloader and low-flash application. Esptool verified the written data hash.
Provisioning sectors `0x3f9000` and `0x3fa000` were not erased or written by the
installation. Image SHA-256:

```text
947a4b9b5d54cbda9fe33a41b64cf6a8d8fd8f8f7d1781eda63faae1f54c9add
```

Restore normal operation by serial-installing the corrected rBoot, initial
slot-A metadata, and production application after measurements; resetting alone
does not exit diagnostic mode. See [STATIC_ROW_DIAGNOSTIC.md](STATIC_ROW_DIAGNOSTIC.md).

A serial reset/read check captured the expected diagnostic boot banner and
repeated `D5/Z=1 D6/X=1 D7/C=1` readings, confirming the diagnostic loop is
running. These are digital observations, not voltage measurements or confirmation
of the operator's switch positions. The board is ready for DC-voltage readings.

The operator reports the tested column inputs only falling to approximately
1.4 V with keys pressed during the static-row diagnostic. Individual per-key
readings were not supplied. At a nominal 3.3 V I/O supply, 1.4 V exceeds the
ESP8266's guaranteed LOW ceiling of 0.825 V; it is not a guaranteed HIGH either.
This establishes insufficient LOW-level margin for the reported measurements,
without establishing the precise pull-up resistance or diode forward voltage.
The result can explain unreliable digital detection and means debounce alone is
not an adequate electrical remedy. Confirm the selected D3/GPIO0 row voltage
relative to GND and the actual 3V3 supply before attributing all of the excess
voltage specifically to the 12 kΩ series resistor and diode.

The operator subsequently measured D3 at GND and the 3V3 rail at approximately
3.5 V. Using that measured supply, the guaranteed LOW ceiling is 0.875 V, so the
approximately 1.4 V pressed inputs still exceed it by about 0.525 V. The selected
row is correctly sinking to ground; an elevated row output does not explain the
input voltage. The pull-up versus the series resistor/diode/contact path remains
the likely explanation. A voltage measurement across the 12 kΩ resistor while
holding the corresponding key can separate its contribution from the remaining
diode/contact drop without changing firmware or components.
