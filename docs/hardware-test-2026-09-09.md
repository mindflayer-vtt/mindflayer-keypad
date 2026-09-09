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
