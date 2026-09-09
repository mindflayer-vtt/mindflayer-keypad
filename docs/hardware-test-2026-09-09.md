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

## Keypad 6 after reseating; debounce preparation

The first post-reseating exercise was not captured because the running test
deployment still used keypad 2's separate server TLS identity. Switching back to
keypad 6's original deployment restored authenticated `0.0.0-dev` registration at
19:23:22 UTC. No firmware or provisioning change was made to keypad 6 for this pass.
The operator then exercised all keys at 19:25:12–19:25:18 UTC. Pair counts were
Q: 2, W: 1, E: 2, A: 2, S: 1, D: 2, Z: 1, X: 0, C: 0, Shift: 4, Space: 6.
Reseating did not restore X/C in this pass, and repeated transitions remain on
several working keys. The operator is investigating switches/soldering on keypad 6;
the exact physical cause has not been established by these server observations.

Firmware `0.0.3-hwtest.1` adds independent 20 ms stable-input debounce to presses
and releases, with no held-key repeats. Seven deterministic scanner scenarios
failed against the old implementation and pass with the new filter. The full
38-method host suite, formatting checks, and both production firmware layouts pass.
This is not yet a physical debounce result.

The test harness now tracks every provisioned ID separately and supports targeted
LED commands. The real server already supports multiple authenticated keypads; the
earlier need to switch deployments was a test-provisioning mistake, not a
single-device server restriction. A new real-TLS integration test exercises two
clients concurrently, independent events/LED routing, and disconnect/reconnect.
The shared physical test deployment now retains keypad 2's TLS identity so new
keypads can join it with separate credentials. Keypad 6 still needs reprovisioning
to this shared identity when it returns from hardware inspection.

## Keypad 5 debounce test setup

The newly connected board identified as ESP8266EX, MAC `3c:61:05:d0:53:56`,
with exactly 4 MiB flash. It was serial-installed with the corrected 2,688-byte
rBoot, fresh slot-A metadata, and production-key `0.0.3-hwtest.1`. Image preflight
validated both image formats and metadata, and the connected MAC/flash size was
checked again immediately before writing. Every written region passed esptool's
hash verification. Application SHA-256:

```text
b628bd50a28121171264e04c08b0d6ed3824e31650ec7794d1e5361f57b1bec1
```

The server created a separate `hwtest-keypad5` credential in keypad 2's existing
test deployment. The serial provisioning tool entered recovery, received an
acknowledgement, and rebooted keypad 5 with the shared test Wi-Fi/server settings.
No firmware-signing key was replaced. Keypad 2's credential and TLS pin remain
valid on the same running server. The full server suite passes all 35 tests,
including concurrent authenticated clients; physical simultaneous operation and
keypad 5's debounce behavior still require operator testing.

At 19:32:37 UTC keypad 5 authenticated and registered `0.0.3-hwtest.1` on the
shared server. The operator was invited to exercise each key once; no physical
debounce pass is claimed until those events and the operator's completion arrive.

### First physical debounce pass

The operator completed all eleven keys at 19:33:11–19:33:16 UTC. Every key
reached the receiver, with exactly one down/up pair for Q, W, E, A, S, D, Z, X,
Shift, and Space. C produced two pairs: down/up at 19:33:15.416/.498 and
19:33:15.522/.592 UTC. These are server receipt timestamps, not raw contact
measurements. The device remained connected and all reported keys ended released.
Thus this pass has no missing keys but still has one duplicate pair on C; the
20 ms filter must not be described as fully resolving physical bounce. An isolated
C repeat is needed to investigate reproducibility before changing the threshold.
No pre-debounce baseline was collected on keypad 5, so comparisons against other
boards cannot quantify the filter's improvement on this particular board.

The isolated five-press C repeat at 19:34:21–19:34:24 UTC produced exactly five
additional down/up pairs, with no duplicate transitions or disconnects. C's
cumulative count rose from two to seven pairs. Observed down-to-up receipt
intervals were 151–199 ms, so this was a clean repeated-tap test, not verification
of the requested one-second holds. The earlier duplicate did not recur in this
sample; it remains an unresolved intermittent observation rather than a fully
eliminated fault.

### 30 ms debounce retry

At the operator's request, the stable-input interval was increased from 20 to
30 ms for both presses and releases (`bf6d648`). Updated regressions check the
new boundary and reject 25 ms press/release glitches; six scenarios fail with
the old 20 ms value, and all seven pass at 30 ms. The full 38-method host suite,
formatting checks, production signing-key consistency check, and both firmware
builds passed. Build `0.0.4-hwtest.1` retains the production trust anchor.

Keypad 5's MAC and 4 MiB flash size were rechecked immediately before serial
installation of the application, corrected bootloader, and initial slot-A
metadata. Provisioning sectors were not rewritten. Application SHA-256:

```text
dfa5fcba395e9194ef251e37a4073e922a4c1c2cf8e0b9c19e194603c64b7fdc
```

The shared server was restarted with the same credentials and TLS identity to
start the retry with fresh per-device event counters.
All flashed regions passed esptool hash verification. At 19:37:57 UTC keypad 5
authenticated and registered `0.0.4-hwtest.1`, with its existing provisioning.
The physical keypress retry is pending operator input.

At 19:45:26–19:45:31 UTC, the completed full-key pass produced exactly one
down/up pair for each of all eleven keys, including C. There were no duplicate
transitions, missing keys in that pass, or observed disconnects. All keys ended
released. This is a clean full-key pass on keypad 5 with 30 ms debounce, not a
guarantee against every intermittent contact fault. The five additional requested
C presses were not present in the received log as of 19:45:42 UTC; that isolated
repeat remains unverified.

## Keypad 6 after reflow

The operator reported reflowing keypad 6 and reconnecting it. The USB-connected
ESP identified with the same MAC `8c:aa:b5:7a:d3:05` and 4 MiB flash. Firmware
was deliberately left unchanged for the initial post-reflow comparison.

A separate `hwtest-keypad6` credential was created in the shared test deployment
alongside keypad 2 and 5. This replaces keypad 6's former `hwtest-keypad` identity
when provisioned; the other boards' credentials and shared TLS identity were
retained. The standard fixed-delay double-reset provisioning attempt timed out.
Boot diagnostics still showed `0.0.0-dev`, permanent slot A, original provisioning
copy A generation 1, and pinned WSS failures against the new server identity.

A retry waited for the actual `Double-reset recovery window open` diagnostic
before issuing the second reset, then waited for `SERIAL PROVISIONING MODE`
before sending the validated bundle. This received `PROVISIONING OK`. Only
provisioning was updated; keypad 6 still has its earlier firmware without debounce
and its earlier bootloader. The fixed-delay failure is recorded as a provisioning
timing observation, not evidence that the reflow failed or a firmware fix was
installed. Post-reflow key behavior remains to be checked after authentication.

At 19:49:57 UTC keypad 6 authenticated on the shared server as `hwtest-keypad6`
and registered `0.0.0-dev`. The receiver is ready for a post-reflow full-key pass.

The operator completed the post-reflow pass at 19:51:08–19:51:14 UTC. X and C
again produced no events. The other nine keys registered with matched down/up
counts: Q/W/E each 2 pairs, A/S/D/Z/Shift each 1, and Space 3. The connection
remained active and all reported keys ended released. Reflow therefore did not
restore X/C in this test. Firmware remains the original non-debounced build, so
the 30 ms filter cannot explain their absence. These observations do not identify
whether the remaining physical fault is in a switch, connection, or PCB trace.

## Keypad 1 test setup

The next attached board identified as ESP8266EX, MAC `3c:61:05:d0:7d:b3`, with
4 MiB flash. The existing `0.0.4-hwtest.1` artifact was reused after validating
its image format, metadata, production trust anchor, corrected bootloader marker,
and SHA-256 against the previously tested keypad 5 build:

```text
dfa5fcba395e9194ef251e37a4073e922a4c1c2cf8e0b9c19e194603c64b7fdc
```

The MAC and flash capacity were checked again immediately before installation.
The shared server was no longer running when this test began; its ports were
confirmed free and the hotspot remained active at `10.42.0.1`. It was restarted
with the same TLS identity and existing credentials, adding a separate
`hwtest-keypad1` credential and generating a private provisioning bundle for
this board. No firmware-signing keys were changed.

Serial installation wrote the corrected bootloader, initial slot-A metadata,
and application without a whole-chip erase. All written regions passed esptool's
hash verification. The normal server provisioning tool then entered serial
recovery, received acknowledgement, and rebooted keypad 1 with its new settings.

At 21:39:39 UTC keypad 1 authenticated as `hwtest-keypad1` and registered
`0.0.4-hwtest.1`. The receiver began the targeted LED sequence, and the operator
was asked to exercise all eleven keys after both LEDs reach dim green. Physical
LED confirmation and keypress results are pending; successful command transmission
alone is not counted as a hardware pass.

### Keypad 1 first pass

All nine LED commands were sent at 21:40:02–21:40:22 UTC, ending with both LEDs
commanded dim green. Physical color confirmation is still pending. A single Q
pair arrived at 21:40:03 during the sequence; it is separate from the later pass,
not evidence of Q bounce.

The completed full-key exercise at 21:40:23–21:40:30 UTC registered Q, W, E, S,
D, Z, X, C, and Space, but no A or Shift events. Q/W/E/S/D/Z/X/C each produced
one down/up pair in that pass. Space produced two pairs at 21:40:30.020/.060 and
21:40:30.150/.195 UTC. The device remained connected and all reported keys ended
released. Thus this board does not yet pass: A/Shift are absent and Space has an
extra pair despite 30 ms debounce. An isolated repeat is needed before assigning
the cause to filtering or particular hardware components.
