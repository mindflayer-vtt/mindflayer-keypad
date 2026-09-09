# Physical keypad comparison — 2026-09-09

This is an ongoing hardware test, not a release acceptance or signed OTA test.
Both boards use the same production `keypad_rboot` application, reporting
`0.0.0-dev`, built from firmware source at `41f1f9a`. The sibling server is at
`70f0602`; the interactive Foundry-facing receiver is from `7bc8fed`.

Firmware was overwritten through serial without a backup at the owner's request.
Fresh test provisioning supplies Wi-Fi, device credentials, the server address,
its pinned TLS public key, and enabled serial diagnostics. The test hotspot is
on this computer; the Foundry receiver is loopback-only, not an actual Foundry
installation.

Artifact SHA-256:

```text
rboot.bin
fec85189f3afcc86a0071299e9d6043e5b900f18f986a1f00afb95a289dbc7a5
rboot-app.bin
62407f36463e3db4c41ea94a37d18941d7c3a9f338b6ceb19cfe221d502e131b
```

## Keypad 6

The owner identified the first board as keypad 6. Its ESP8266 MAC is
`8c:aa:b5:7a:d3:05`, with 4 MiB detected flash. Its test device ID was
`hwtest-keypad`; local logs are in ignored `.hwtest/e2e-20260909/events.jsonl`.

- Flash writes verified; serial provisioning acknowledged.
- The physical device joined Wi-Fi and authenticated/registered with the real
  server. Key events reached the Foundry-facing receiver.
- The owner confirmed the complete independent red/green/blue LED sequence,
  both white, both off, and both dim green. LED 1 is physically left.
- X never produced an event, including an isolated requested two-second hold.
- Space registered on the first pass but not the final ordered pass.
- Repeated transitions appeared on multiple keys; these were retained in the
  logs, not filtered by the receiver.

Final ordered pass, 18:34:34–18:34:46 UTC, one physical actuation requested per key:

| Key   | Down events | Up events |
| ----- | ----------- | --------- |
| Q     | 2           | 2         |
| W     | 2           | 2         |
| E     | 4           | 4         |
| A     | 4           | 4         |
| S     | 3           | 3         |
| D     | 2           | 2         |
| Z     | 2           | 2         |
| X     | 0           | 0         |
| C     | 1           | 1         |
| Shift | 3           | 3         |
| Space | 0           | 0         |

C's single pair was separated by only 10 ms at the receiver. These results do
not isolate the cause to soldering, switches, electrical behavior, scanning, or
transport. The server remained connected during the pass. Missing keys do not
form one entire matrix row or column.

Git comparison with the pre-rewrite scanner at `76f5db2` confirms the same pin
mapping, key mapping, 5 ms minimum scan interval, and immediate transition
reporting without debounce. Subsequent scanner edits removed `config.h` and
changed formatting only. This does not rule out effects from changed surrounding
execution timing. No scanner fix has been implemented during this test.

## Keypad 2

The owner connected keypad 2 next. Its ESP8266 MAC is `3c:61:05:d0:a0:2d`, with
4 MiB detected flash. It uses test device ID `hwtest-keypad2` and separate local
state under ignored `.hwtest/e2e-20260909-keypad2/`.

The same artifact hashes above were flashed and verified. Serial provisioning
was acknowledged, and the device authenticated and registered with the server
at 18:40:56 UTC. The owner observed the expected red left startup LED.
The owner confirmed the complete LED sequence and both dim green afterward.
All eleven keys reached the Foundry-facing receiver in the requested order;
the server remained connected. The pass ran at 18:42:05–18:42:14 UTC.

| Key   | Down events | Up events |
| ----- | ----------- | --------- |
| Q     | 1           | 1         |
| W     | 2           | 2         |
| E     | 1           | 1         |
| A     | 1           | 1         |
| S     | 7           | 7         |
| D     | 3           | 3         |
| Z     | 5           | 5         |
| X     | 2           | 2         |
| C     | 1           | 1         |
| Shift | 1           | 1         |
| Space | 1           | 1         |

### Keypad 2 repeat pass

The second ordered pass, 18:43:33–18:43:46 UTC, again registered every key and
ended with matching down/up counts and a connected server session. Per-key
press/release pair counts were:

| Keys                        | Pairs per key |
| --------------------------- | ------------- |
| Q, W, A, Z, X, Shift, Space | 1             |
| C                           | 2             |
| S                           | 4             |
| E, D                        | 5             |

X and Space were clean in this pass. The set of keys with extra transitions
changed between passes: E was previously clean, while W, Z, and X became clean.
Thus keypad 2 shows intermittent extra transitions, not consistently missing
keys. This still does not establish their electrical or software cause.

## Comparison so far

Both boards passed the operator-observed LED sequence and authenticated through
the same firmware/server path. X and Space both registered on keypad 2, so their
protocol mappings are not universally broken. Keypad 6's consistently absent X
remains board-dependent in these observations, but its physical cause is unproven.

Extra transitions occur on both boards, especially S. Neither board passes a
one-press/one-release acceptance check. Raw matrix diagnostics are still needed
to distinguish contact behavior from scanning/electrical timing; the lack of
debounce alone does not establish the cause of missing events. No firmware changes
or soldering have been performed to address these findings. Test services and the
hotspot remain running for further investigation.

## Restart shortcut and Wi-Fi OTA

Commit `5a19e7d` restores Shift + Space + E and adds an exhaustive application-loop
regression. The operator authorized a temporary serial-installed local-key build
because the production private key was not locally available. Its version is
`0.0.0-localkey`. The OTA payload is `0.0.1-hwtest.1`, signed with the local key but
containing the production public key. The temporary source-key substitution was
reverted without committing it. No signature verification was disabled.

The first OTA attempt served 437,300 bytes over the authorized HTTPS endpoint.
The device validated the signed image, booted slot B temporarily, authenticated,
and received server acceptance. On the promotion restart, however, rBoot printed
`Writing default boot config.` and booted the old slot A. Update offers were
stopped while investigating; this attempt did not pass promotion.

The cause was the rBoot build script's `git apply` command inside an exported
source subdirectory of the keypad repository. Git silently skipped `rboot.c`,
leaving a legacy bootloader that did not understand transactional metadata.
The reproduction reported `Skipped patch 'rboot.c'.` with success status. The
build now applies the patch with `patch --batch --fuzz=0` directly to the exported
tree and checks the final binary before copying it. A new regression guard rejects
the missing safe-default marker and any legacy config-writing marker.

The corrected bootloader is 2,688 bytes, SHA-256:

```text
a6838d9fdb2afc2f31690342e7f839915f7e0f7fbed7eef7ce0c4cf7c0011ee3
```

It was serial-installed on keypad 2 with fresh slot-A boot metadata, without
rewriting the intermediate application or provisioning. A retry uses the exact
same signed OTA payload. At 18:58:43 UTC the server completed the 437,300-byte
response. The device validated slot B, booted it temporarily, received server
acceptance, and rebooted with `slot=B permanent=B mode=PERMANENT`. It authenticated
again at 18:58:56 UTC as `0.0.1-hwtest.1`, still loading provisioning copy A,
generation 1. Thus Wi-Fi download and permanent promotion passed on retry; no
serial application write occurred between the intermediate and that OTA boot.
The active firmware contains the production public key again. The inactive slot
still contains the local-key intermediate; it has not been erased.

Physical shortcut testing subsequently passed: the serial monitor captured
`Shift+Space+E: restarting`, followed by another permanent slot-B boot of
`0.0.1-hwtest.1` and successful authentication with the same provisioning.
Keypad 6 still has the earlier bootloader until explicitly reflashed. Future OTA
on the active keypad-2 firmware again
requires production-key signing; this local-key test does not validate GitHub
release signing or establish a general key-rotation mechanism.

Separately, the owner found the ESP module on keypad 6 was not fully seated.
This is a plausible hardware contributor, not yet confirmed by a post-reseating
test. It does not remove the firmware's missing debounce behavior.

## Connection colors and unprovisioned pulse

The next build, `0.0.2-hwtest.1`, restores left-LED red before Wi-Fi, yellow once
Wi-Fi connects, and green after server authentication. An unprovisioned normal
boot pulses both LEDs red every three seconds (one-second fade, two seconds dark).
Serial recovery is selected by the existing host double reset before DMA starts;
the pulse path never polls UART while DMA owns GPIO3. Host regressions verify
three pulse cycles, status transitions, preservation of server colors/right LED,
and separate recovery behavior with and without stored settings. All 37 host
tests passed and both firmware layouts compiled.

For the owner-requested unprovisioned test, keypad 2 was serial-flashed with
the corrected bootloader, slot-A metadata/application, and erased provisioning
sectors at `0x3f9000` and `0x3fa000`. Every written region passed esptool's hash
verification, and the device was reset. The test bundle remains available on the host
for later reprovisioning. The new application retains the production trust anchor;
its SHA-256 is:

```text
a762b4d8ac624cfb8ccdcb2a2f2bd26ac75f545e1111f7fc107a4e2a6eb52071
```

The owner confirmed the physical pulse works. The unchanged server serial
provisioning tool then double-reset the pulsing device, reported serial recovery
mode, received its provisioning acknowledgement, and reset it into normal boot.
This physically verifies that pulse mode does not prevent entry to serial
provisioning on the shared GPIO3 pin.

The server observed authenticated registration of `0.0.2-hwtest.1` at 19:12:52
UTC. The owner confirmed that the red/yellow/green boot LED progression worked.
Both the unprovisioned pulse and transition back to provisioned operation have
therefore been checked on physical keypad 2.

### Concurrent LED commands and button input

At 19:14:04–19:14:24 UTC, the harness sent its complete LED sequence while the
operator pressed all eleven keys on keypad 2 running `0.0.2-hwtest.1`. Every key
reached the Foundry-facing receiver with matching down/up counts, and the session
remained connected. Per-key pair counts were Q/W/A/D/Z/Shift/Space: 1, S/X: 2,
C: 4, E: 6. Thus no key was wholly missing during this pass, but repeated
transitions remain unresolved. The sequence ended with both LEDs commanded dim
green. The operator confirmed the LEDs worked and the button sequence was complete.
