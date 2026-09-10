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

The operator measured approximately 0.88 V across the column series resistor
while pressed and questioned whether its intended value was 10 kΩ. Combined
with the approximately 1.4 V input reading, this leaves roughly 0.52 V across
the diode/contact path to the grounded row. The checked-in schematic specifies
12 kΩ for R1–R3; fitted resistance has not been independently measured.

Assuming 12 kΩ is fitted and the readings describe the same steady condition,
the inferred current is about 73 µA and effective internal pull-up resistance
about 29 kΩ. A simple constant-diode-drop model predicts roughly 1.3 V even
with 10 kΩ series resistance, so that small change would not restore adequate
LOW margin. Bypassing one identified column series resistor is a proposed
controlled experiment with power disconnected during soldering and the switch
diodes retained; it has not yet been performed or validated. Lower series
resistance retains more fault-current limiting than a direct bridge.

## Keypad 6 column-resistor bridges

The operator reported that fitting replacement resistors would require extensive
disassembly, bridged all three column series resistors on keypad 6, and reconnected
the board. This is an operator-performed modification, not a change to the
checked-in schematic or confirmation of a validated fix. Post-modification
voltages and normal-scanner behavior remain to be measured. The static-row
diagnostic remains installed; production firmware has not yet been restored.
Serial observation after reconnection confirmed the diagnostic loop remains
active, reporting all three column inputs HIGH during that observation. No
firmware write or reset was performed for this check.

The operator reports approximately 0.53 V ±0.01 V at the pressed column inputs
after bridging the three resistors, compared with approximately 1.4 V before.
At the previously measured 3.5 V supply, even 0.54 V is below the guaranteed
LOW ceiling of 0.875 V, giving approximately 0.335 V of margin. This validates
the static pressed-input voltage improvement and strongly supports the original
series resistance as the source of inadequate LOW-level margin on this board.
It does not yet validate released-input voltages, dynamic scan settling,
multi-key behavior, or the absence of remaining switch faults. Production
firmware is still not restored; a normal-scanner full-key test is the next step.

## Restore normal firmware after measurement

The operator authorized restoring the normal 30 ms debounce build for a full-key
test. The exact previously verified `0.0.4-hwtest.1` production application was
selected, with SHA-256:

```text
dfa5fcba395e9194ef251e37a4073e922a4c1c2cf8e0b9c19e194603c64b7fdc
```

Preflight revalidated application/bootloader formats and initial slot-A metadata,
the production signing trust anchor, and the corrected transactional bootloader
marker. Keypad 6's MAC and exact 4 MiB flash size were checked again immediately
before writing. Restoration replaces the diagnostic with corrected rBoot,
initial slot-A metadata, and the normal application; provisioning sectors are
outside the write regions. The shared server was restarted with unchanged TLS
identity and device credentials, clearing accumulated counters from earlier
sessions for the new controlled test. The physical column-resistor bridges remain.

All written regions passed esptool hash verification. At 00:53:43 UTC keypad 6
authenticated as `hwtest-keypad6` and registered `0.0.4-hwtest.1`, confirming
normal firmware operation and reuse of its retained provisioning. The receiver
is ready for the post-bridge full-key pass; no key-test result is claimed yet.

## Post-bridge full-key result

The operator completed the full-key pass at 00:54:13–00:54:19 UTC. All eleven
keys (Q, W, E, A, S, D, Z, X, C, Shift, Space) produced exactly one down/up
pair in the requested order. There were no missing keys, duplicate transitions,
or observed disconnects, and every key ended released. In particular, X and C
now register normally after being absent in the earlier controlled passes.

Keypad 6 therefore passes this single-key exercise with the three column-series
resistors bridged and `0.0.4-hwtest.1`/30 ms debounce installed. Together with the
pressed-voltage improvement from approximately 1.4 V to 0.53 V, this strongly
supports inadequate electrical LOW margin as a contributor to the earlier
missing-key behavior. The firmware also changed from the earlier non-debounced
baseline, so the disappearance of duplicate transitions cannot be attributed
solely to the resistor bridges. Long-duration and simultaneous-key tests after
the modification remain unverified.

## Keypad 4 flash and provisioning

Earlier attempts to access keypad 4 found no USB serial adapter, preventing
identification or flashing. Following the operator's latest reconnection, the
FTDI enumerated and the board identified as ESP8266EX, MAC `8c:aa:b5:7b:db:35`,
with exactly 4 MiB flash. This restores access but does not by itself prove the
reported intermittent USB concern is permanently resolved.

The operator requested normal firmware installation. The same verified
`0.0.4-hwtest.1` application (30 ms debounce, production signing trust anchor)
was selected, SHA-256
`dfa5fcba395e9194ef251e37a4073e922a4c1c2cf8e0b9c19e194603c64b7fdc`.
Image/metadata preflight and the corrected bootloader marker passed, and the
connected MAC/flash capacity were checked again immediately before writing.
The shared test server retains its TLS identity and existing device credentials,
with a new separate `hwtest-keypad4` credential and private provisioning bundle.
No key/LED exercise was requested for this flash-only step. The operator has
not explicitly confirmed completion of keypad 4's resistor modification, so no
post-modification hardware result is claimed.

The corrected rBoot, initial slot-A metadata, and application were serial-flashed;
all written regions passed esptool hash verification. The normal serial
provisioning tool then entered recovery, received the provisioning acknowledgement,
and rebooted keypad 4. No whole-chip erase was performed.
At 01:28:53 UTC it authenticated as `hwtest-keypad4` and registered
`0.0.4-hwtest.1`. Flash/provisioning and server connectivity are verified;
physical key and LED testing remain deferred.

### Keypad 4 LED and full-key result

The operator subsequently requested testing. The harness sent all nine targeted
LED commands at 01:31:29–01:31:49 UTC, ending with both LEDs dim green. The
operator explicitly confirmed the LED sequence was correct.

The completed key pass at 01:31:55–01:32:01 UTC produced exactly one down/up
pair for each of Q, W, E, A, S, D, Z, X, C, Shift, and Space, in the requested
order. There were no duplicate transitions, missing keys, or observed disconnects;
all keys ended released. Keypad 4 passes this LED and single-key exercise on
`0.0.4-hwtest.1`. This does not establish long-term USB reliability or confirm
which physical resistor modifications were performed on this particular board.

## Keypad 5 repeat setup

The operator connected keypad 5 and requested flashing and testing. It
authenticated on the shared server as `hwtest-keypad5` running `0.0.4-hwtest.1`
before the requested reflash, confirming its existing provisioning still works.
Its known MAC `3c:61:05:d0:53:56` and exact 4 MiB capacity were verified over USB
and checked again immediately before writing.

The same verified `0.0.4-hwtest.1` artifact was selected, SHA-256
`dfa5fcba395e9194ef251e37a4073e922a4c1c2cf8e0b9c19e194603c64b7fdc`,
with 30 ms debounce and the production trust anchor. Corrected rBoot and initial
slot-A metadata accompany the application; provisioning is outside the write
regions. The existing shared server remains running, with keypad 5's counters
initially empty. This is a new controlled test, separate from the earlier keypad 5
passes. No exact physical resistor modification on this board is assumed from
the operator's readiness message alone.

All flash regions passed esptool hash verification. At 01:34:34 UTC keypad 5
authenticated and registered `0.0.4-hwtest.1` after the reflash, retaining its
existing provisioning. The targeted LED sequence was started and the operator
was invited to perform a full-key pass after both LEDs reach dim green. Physical
LED confirmation and the new key-test results remain pending.

The first new key exercise at 01:35:39–01:35:44 UTC captured one pair each for
Q/W/E/A/S/D/Z/X/C, followed by Shift-down without its release and no Space events.
A new registration followed at 01:35:53; a disconnect notification then arrived
at 01:35:59, leaving the harness's connection flag false. This pass is incomplete,
and neither the reconnect cause nor a physical LED pass was established.

On the operator's request to repeat, the harness initially skipped LED commands
because of that connection flag. The test server was restarted with unchanged
TLS identity and credentials to clear session state and event counters. Keypad 5
authenticated again at 01:36:31 UTC; a fresh targeted LED sequence was started.
No firmware or provisioning change was made for this repeat.
