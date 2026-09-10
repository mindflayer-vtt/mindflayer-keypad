# Static Z/X/C row diagnostic

This separate, serial-only firmware holds **D3 / GPIO0 LOW continuously** after
boot. D1/GPIO5, D2/GPIO4, and D4/GPIO2 remain HIGH. D5/GPIO14, D6/GPIO12, and
D7/GPIO13 remain `INPUT_PULLUP`, matching the production scanner's column bias.
It does not scan or debounce, and it disables Wi-Fi without saving Wi-Fi settings.
Serial output once per second reports digital readings only, not voltages.

The standalone project is outside the production source tree and release builds:

```sh
.venv/bin/pio run -d diagnostics/static-row -e static_row
python3 -m unittest discover -s test -p test_static_row.py
clang-format --dry-run --Werror diagnostics/static-row/src/*.cpp diagnostics/static-row/src/*.h
```

Only install on the explicitly identified 4 MiB ESP8266 test board with the owner's
permission. The `firmware.bin` is a normal Arduino/eboot image flashed at `0x0`,
not an rBoot slot image. Installation replaces the resident bootloader and low-flash
application. Do not erase the whole chip or write the provisioning sectors at
`0x3f9000` and `0x3fa000`. It provides no server connection, LED control, OTA,
firmware signature verification, or serial provisioning; LED colors may remain
latched from previous firmware and are not a diagnostic status indication.

## Measure

Use **DC-voltage mode**, not diode/resistance mode, on the powered board. Connect
the black probe to board GND. Avoid bridging adjacent pins with the probe tip.

1. Verify D3 is near 0 V; the other row outputs should be near 3.3 V.
2. With all keys released, measure D5, D6, and D7 relative to GND.
3. Hold Z alone and measure D5; hold X alone and measure D6; hold C alone and
   measure D7. Record each pressed and released voltage at the MCU side of the
   12 kΩ resistor. Test only one key at a time.

The ESP8266's guaranteed LOW ceiling is `0.25 × VIO` (0.825 V when VIO is 3.3 V).
An input above this ceiling is not guaranteed LOW, but is not necessarily HIGH.
See the [Espressif electrical characteristics](https://documentation.espressif.com/0a-esp8266ex_datasheet_en.html).
This static test removes scan-settling timing as a variable. It cannot measure the
production scanner's timing, Wi-Fi noise, or prove all switches are healthy.

## Restore

After measurement, serial-install the production rBoot bootloader, initial slot-A
metadata, and production `rboot-app.bin` using the normal validated installation
workflow. Preserve the provisioning sectors. A reset alone does **not** exit this
diagnostic; normal keypad functionality requires reinstalling production firmware.
