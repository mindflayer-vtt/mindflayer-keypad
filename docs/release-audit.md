# Public release audit — 2026-09-08

The original audit found five high-priority defects and a historical credential
requiring a retirement decision. The follow-up below records implemented fixes;
the original findings remain as evidence about the audited revision. Hardware
and deployment validation still determine public release readiness.

Audited firmware revision: `806bba4`. The latest commit was amended during the
audit; the relevant source and signing-key findings were rechecked afterward.
The sibling server was inspected for the provisioning, protocol, and firmware
repository contracts; this was not a complete independent server audit.

## Follow-up fixes — 2026-09-08

- R1: the compiled trust anchor and signer now use the production public key in
  `keys/firmware-signing-public.pem`; the duplicate under `scripts/` was removed.
  Release preparation runs the key-consistency verifier. The owner reports that
  the matching private key is installed in GitHub; its secret value was not read.
- R2: the bootloader script installs the pinned PlatformIO environment packages
  before invoking the compiler. A run with an empty `PLATFORMIO_CORE_DIR`
  installed the toolchain and produced the expected 2,688-byte bootloader.
- R3: an SDK timer now covers startup and yielding network stalls independently
  of application-loop progress. Six scenarios run the actual application source
  with simulated SDK time, and permanent boots remain exempt.
- R4 and R5: the transport now processes one frame per poll, rejects oversized
  frames before payload allocation, tracks aggregate fragment size, and uses
  bounded exact reads. Nine transport scenarios test the actual pinned library
  source. Regression failures were observed before implementing the fixes.
- The approved `npm audit` completed successfully with **zero reported
  vulnerabilities** across 460 dependency entries. The earlier denied attempt
  documented below describes the original audit, not the current scan status.
- Host application/transport regressions and installer tests now run in CI.
  C++ formatting includes their sources and uses portable shell globs.

Follow-up verification passed all 60 existing native/sanitizer cases, six startup
scenarios, nine transport scenarios, and both installer tests. Both production
environments and all three fault variants compiled successfully; the rBoot image
matched pinned esptool2, the signing-key verifier passed, and formatting checks
passed. These are local results, not a claim that a GitHub release has run.

See [TESTING.md](TESTING.md) for commands and coverage. No signing-key rotation,
history rewrite, hardware flashing, or GitHub release publication was performed.
The historical-credential decision, installer safeguards, and remaining hardware
and production-release checks below still require attention.

## Original high-priority findings

### R1: The production public key is not the firmware trust anchor

`keys/firmware-signing-public.pem` contains a different RSA public key from
`scripts/firmware-signing-public.pem` and `FIRMWARE_SIGNING_PUBLIC_KEY_PEM` in
`include/HardwareConfig.h`. The signer defaults to the latter PEM
(`scripts/sign-firmware.sh:16`). Nothing in the build consumes the new `keys/`
copy. `scripts/verify-signing-key.py` succeeds because it compares only the other
two copies.

SHA-256 fingerprints of DER-encoded public keys:

| Copy                                     | Fingerprint                                                        |
| ---------------------------------------- | ------------------------------------------------------------------ |
| Newly committed `keys/` public key       | `aa60b6f834754980d279bfdf94c082c5d013eb7cbbc07843b9249ec0334bc2b1` |
| Signer default and compiled trust anchor | `76766e2ed465a2e774d77a886af54fa79081e56bad190edb4a92c66dc71165c6` |

A GitHub private key corresponding to the new public key will fail the signing
script's comparison. Overriding the script's public-key path alone would produce
an artifact the installed firmware does not trust.

Before production installation, select one authoritative public-key file, make
the compiled key and signer agree with it, and ensure the verifier checks that
authoritative file. Do not treat this as rotation for already deployed devices:
no signing-key rotation mechanism exists, and none was added by this audit.

### R2: A fresh release runner lacks the toolchain when rBoot is built

The release job installs PlatformIO Core, then invokes semantic-release.
`scripts/build-release.sh:39` calls `build-rboot.sh` before its first `pio run`.
The bootloader script immediately needs
`~/.platformio/packages/toolchain-xtensa/bin/xtensa-lx106-elf-gcc`, but neither
script installs that package before use. The README's initial-install sequence
has the same order.

The preceding build job installs its own toolchain as a side effect of building
`keypad`, but the release job has no PlatformIO cache restore or artifact transfer.
GitHub-hosted jobs use fresh runner instances, so `needs: build` cannot supply
those files. See [GitHub's hosted-runner documentation](https://docs.github.com/en/actions/concepts/runners/github-hosted-runners).

Install the environment's packages explicitly before invoking the standalone
bootloader build, or build the application first. Verify the entire release
preparation on an empty PlatformIO home. Local success with an existing toolchain
does not prove this path works.

### R3: Unhealthy temporary firmware can bypass the rollback deadline

`src/Application.cpp:76` waits indefinitely for Wi-Fi in `setup()`. Its
`delay(500)` keeps yielding, so this is not a reliable watchdog-reset path.
The 90-second deadline is checked only in `loop()` after startup finishes.
An unavailable AP, invalid credentials, or a candidate Wi-Fi regression can
therefore leave the temporary slot running indefinitely.

There is a second bypass: failed provisioning load returns from `setup()`, then
the unprovisioned branch at `src/Application.cpp:89` returns before the timeout
check on every loop. Blocking WebSocket reads also occur before the timeout check.

Make candidate health enforcement cover startup, failure/recovery branches, and
network operations. Verify AP-unavailable, provisioning-load failure, and stalled
server cases on hardware, proving return to the permanent slot without manual
reset. The current health-gate tests exercise Boolean promotion predicates, not
the application scheduling that must enforce this deadline.

### R4: Valid batched WebSocket messages are rejected

`src/DeviceConnection.cpp:132` marks a message invalid whenever
`receivedFrameSize` is already nonzero. ArduinoWebsockets 0.5.4
`WebsocketsClient::poll()` drains all available messages before returning;
the application's `processCallbacks()` runs only afterward.

Consequently two valid LED configuration frames arriving in one poll cause the
second callback to mark the connection invalid. `processCallbacks()` then drops
the first message as well and closes the connection. Ordinary TCP/TLS batching,
including a configuration message arriving alongside firmware acceptance, can
trigger this without malformed input.

A host harness compiled the exact current `onMessage` function with a minimal
message adapter and reproduced rejection using two valid nine-byte LED frames.
Introduce bounded queueing or a safe one-message polling/dispatch arrangement;
test several messages in one poll, including acceptance plus configuration.

### R5: The WebSocket allocation is not bounded by the protocol limit

The 512-byte check in `src/DeviceConnection.cpp:132` runs after ArduinoWebsockets
has assembled a `WebsocketsMessage`. In the installed, pinned 0.5.4 dependency,
`websockets_endpoint.cpp` calls `WSString data(extendedPayload, '\0')` in
`readData()`. Its earlier optional `_WS_CONFIG_MAX_MESSAGE_SIZE` check is not
enabled anywhere in this repository. Fragment assembly also needs its own total
message bound.

A faulty or compromised pinned server can exhaust the device's small remaining
heap before the application rejects an oversized frame. TLS pinning restricts
the source of such input; it does not enforce memory bounds. The native CBOR
tests do not exercise WebSocket allocation or incomplete socket reads.

Enforce the frame and aggregate-message limit before allocation, close the socket
on violations, and bound incomplete reads. Validate oversized advertised lengths,
fragmented oversized messages, and stalled payloads through the actual transport.

## Credential history requiring a decision

Commit `b5466e7` contains a complete, parseable `client_private_key` in
`src/main.cpp`, beginning at historical line 94. Removing it from the latest
source did not remove it from Git history. Only its public fingerprint was
extracted during the audit; the private bytes are intentionally absent here.

Its DER public-key SHA-256 is
`652dc284e7bfe9d33080045fc01db0fa1330fc2035c389fd3d5a4cecc7a12e4a`, distinct
from both current signing public keys. This evidence does **not** establish that
the production firmware-signing private key was exposed.

Confirm that the historical client credential and associated old deployment
credentials are retired everywhere they were used. Decide whether to remove the
credential from the publication history. No history rewrite, credential change,
or production-private-key access was performed. The targeted history search is
not a substitute for a comprehensive secret scanner.

## Additional release concerns

- **Installer input validation:** `scripts/install-rboot.py:62` checks the chip
  family, but not the detected flash capacity, artifact sizes, metadata validity,
  or boot2 image structure before `write_flash`. In particular, a wrongly selected
  oversized metadata-B file can extend into unrelated flash regions. The
  provisioning hash comparison detects damage only after writing. Validate all
  artifact extents and target capacity before any write; test the complete
  mocked installation flow. The existing two tests cover only chip identity.
- **Toolchain reproducibility:** `platformio.ini:12` uses unversioned
  `espressif8266`, although the integration depends on Core 3.1.2's SDK layout and
  patches its `libmain.a`. Pin the validated platform and deliberately revalidate
  upgrades. rBoot, esptool2, QCBOR, and the two external Arduino library versions
  are already pinned.
- **CI coverage:** formatting and the Python installer tests are not invoked by
  `.github/workflows/build.yml`. The native tests do not compile the application
  orchestration or network adapters. Add the missing gates and transport/startup
  regression coverage alongside the corresponding fixes.
- **Documentation drift:** `docs/MODERNIZATION.md` still instructs readers to
  configure `include/config.h` and describes the superseded JSON/certificate
  implementation; `docs/OTA_BOOT.md` still calls the installer topology-locked in
  its experimental-disposition table. Label historical instructions clearly.
  The README's disposable signing example labels an unversioned build `1.2.3`
  without setting `FIRMWARE_VERSION`, and its newly generated key does not match
  the committed trust anchor automatically. Make the example executable and
  ensure embedded and manifest versions agree.
- **Formatting scope:** the configured Prettier check passes, but
  `.prettierignore` excludes the entire `hardware/case/` directory, including its
  README. The C++ npm scripts rely on shell brace expansion, which is not portable
  to all `/bin/sh` implementations. The top-level format result should not be
  described as covering every document or every host shell.
- **Release directory reuse:** `build-release.sh` archives all of `dist/firmware`,
  while `sign-firmware.sh` replaces its manifest with one release. Repeated local
  runs can therefore bundle stale, unreferenced files. Stage each release in a
  fresh directory before archiving.

## Original verification performed

| Check                             | Evidence/result                                                                                                                                                                                                      |
| --------------------------------- | -------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------- |
| Native and ASan/UBSan suites      | All 60 test cases passed across four suites in both environments.                                                                                                                                                    |
| Installer Python tests            | Both chip-identity tests passed.                                                                                                                                                                                     |
| Production compilation            | `keypad` and `keypad_rboot` passed using PlatformIO 6.1.19, platform 4.2.1, Core 3.1.2, GCC 10.3.0.                                                                                                                  |
| Fault variants                    | `keypad_rboot_fault`, `keypad_rboot_unhealthy`, and `keypad_rboot_corrupt` passed; the promotion-reset build also passed with CI's stage-2 flag.                                                                     |
| Clean-build follow-up             | The initial corrupt-variant build failed; its retry and a subsequent clean rebuild passed. The initial diagnostic was truncated, so its cause remains unestablished.                                                 |
| Bootloader rebuild                | Pinned source and exact metadata patch checked; direct clean rebuild produced the same 2,688-byte bootloader, below 4 KiB. The build wrapper's checkout/reset was not needed.                                        |
| rBoot ELF and image               | Link-time mapping verifier passed; production boot2 image matched pinned esptool2 byte for byte and measured 436,784 bytes, below the 1,040,384-byte slot capacity.                                                  |
| Production fault-marker exclusion | No `TEST:` marker found in the production ELF. This is the CI marker check, not a formal proof of all compiled behavior.                                                                                             |
| Generic firmware proof            | Two clean builds with distinct SSID/device-ID environment inputs were byte-identical. SHA-256: `ec82fb2b453e7446d2634b63294687267c185e6e38f06f95fec1570ecb79cda8`.                                                   |
| Signing smoke test                | Disposable RSA-2048 key outside the repository; signed artifact passed the signing script and independent Node crypto verification. Modified image was rejected; default signer rejected the mismatched private key. |
| Server artifact compatibility     | Sibling server's actual `FirmwareRepository` imported the disposable signed artifact and manifest, validating size and SHA-256. This did not test device acceptance of the disposable key.                           |
| Server protocol/provisioning      | Both targeted Node test files passed. Wi-Fi SSID/password and schema-v2 serial-debug fields agree with firmware; schema-v1 compatibility remains.                                                                    |
| Formatting and local docs         | `npm run format:check`, shell syntax checks, and `git diff --check` passed. No broken local Markdown links found.                                                                                                    |
| Signing anchor verifier           | Passed for the two files it checks; R1 records why that is insufficient.                                                                                                                                             |

The configured v7 GitHub Actions exist: checked against the official releases for
[checkout](https://github.com/actions/checkout/releases),
[setup-python](https://github.com/actions/setup-python/releases), and
[setup-node](https://github.com/actions/setup-node/releases). Their major versions
are not the cause of R2.

## Original limits and release exit criteria

No device was opened, reset, provisioned, or flashed. Earlier hardware results in
`docs/OTA_BOOT.md` describe older test artifacts; they do not prove the current
release candidate. The audit did not validate live GitHub secret contents,
repository protections, or a successful production-signed release job. A full
release archive built from the actual production private key remains unverified.

The online `npm audit` request failed under network restrictions. Automatic
approval review then rejected the request for external access because it would
send dependency metadata to npm. No successful advisory scan is claimed, and no
alternate upload was attempted.

Before public release, resolve R1–R5, settle the historical-credential disposition,
validate the installer guards, and run a fresh-runner release preparation. Then
exercise the exact production candidate on hardware: provisioning/reprovisioning,
key scanning and both LEDs, signed A/B promotion, unavailable-network rollback,
tamper rejection, metadata interruption, and serial recovery. Preserve and compare
the provisioning sectors throughout. Confirm the production signing secret and
release protections separately, and finish the dependency-advisory and secret
scans with approved tooling.
