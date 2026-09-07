# rBoot OTA and rollback architecture

Production uses rBoot 1.4.2 commit `614f33685d0dd990fc4202f2409b0d2365eeaef3` and esptool2 commit `91759a7c4faec1b7bc8410166d22ed92eb90556c`. Builds enable `BOOT_RTC_ENABLED`, `BOOT_BIG_FLASH`, `BOOT_CONFIG_CHKSUM`, and `BOOT_IROM_CHKSUM`; GPIO boot selection is disabled. GCC 10.3.0 and Arduino Core 3.1.2 are the validated toolchain. `scripts/build-rboot.sh` pins both sources, patches only its build-local checkout, and rejects a bootloader above 4 KiB. The final proven loader is 2,688 bytes.

## Layout

| Start..end | Owner |
|---|---|
| `000000..000fff` | rBoot |
| `001000..001fff` | boot metadata A |
| `002000..0fffff` | slot A, 1,040,384 bytes |
| `100000..100fff` | boot metadata B |
| `101000..201fff` | reserved |
| `202000..2fffff` | slot B, 1,040,384 bytes |
| `300000..3f8fff` | reserved |
| `3f9000..3f9fff` | provisioning A |
| `3fa000..3fafff` | provisioning B |
| `3fb000..3fbfff` | Arduino EEPROM |
| `3fc000..3fcfff` | RF calibration |
| `3fd000..3fffff` | SDK Wi-Fi configuration |

`FlashLayout.h` is authoritative. Static assertions and native tests prevent overlap. No filesystem exists.

## Build and release flow

`controller_1_rboot` links IROM at `0x40202010`. A build-local copy of SDK `libmain.a` weakens `Cache_Read_Enable_New`; the intended big-flash implementation supplies the only strong definition. The ELF verifier checks both facts and map ownership. `elf2rboot.py` emits SDK boot2 EA/04 plus E9 format with full-IROM checksum; CI compares it byte-for-byte with pinned esptool2. The ordinary Arduino `firmware.bin` contains eboot and must never be published as a slot image.

CI signs `rboot-app.bin` with the offline/CI RSA private key and publishes only the signed application plus size and SHA-256 metadata. The server stores it and never receives a private signing key. rBoot and initial metadata are serial-install artifacts, not OTA artifacts.

Semantic-release publishes those files as a ready-to-extract firmware repository archive whenever Conventional Commits on `main` require a release. The archive root includes `manifest.json`, so its extracted directory can be mounted at `MINDFLAYER_FIRMWARE_DIR` without renaming or rewriting paths. Release preparation verifies both the signed trailer and that the signing private key corresponds to the firmware's committed public trust anchor before semantic-release creates the tag or GitHub release.

## Experiment-to-production integration

The separate `mindflayer-keypad-rboot` worktree was evidence, not a source package. The production integration deliberately classified its pieces as follows:

| Experimental piece | Production disposition |
|---|---|
| pinned rBoot/esptool2 build, big-flash linker contract, boot2 encoder, and ELF checks | integrated and hardened with CI byte/symbol/size verification |
| `BootControl`, slot writer, and temporary-boot RTC use | adapted behind application-facing abstractions with strict slot bounds |
| single-sector upstream rBoot config promotion | replaced by redundant transactional metadata with generation, CRC, and last-written commit marker |
| serial experiment harness and `src/rboot_test.cpp` | test-only; intentionally omitted from production firmware |
| experiment `config.h`, private signing key, flash backups, and `.hwtest` state | intentionally omitted; no device/installation secret enters a build or commit |
| experiment slot A/B demo environments and direct flash commands | omitted from normal targets; replaced by the one-time topology-locked installer and authenticated OTA path |

The health gate, server version acknowledgement, provisioning store, restricted-CBOR protocol, serial recovery isolation, and fault-injection builds are production integration work rather than transplanted experimental code. rBoot promotion is never inferred merely from reaching `setup()`.

## Installation

Build the artifacts and committed initial metadata:

```sh
scripts/build-rboot.sh
.venv/bin/pio run -e controller_1_rboot
python scripts/make-rboot-config.py .pio/rboot-artifacts/metadata-a.bin --slot a --generation 1
python scripts/make-rboot-config.py .pio/rboot-artifacts/metadata-b.bin --state erased
```

Then use `scripts/install-rboot.py` with the stable `/dev/serial/by-path/...usb-0:4.1:...` path, the four artifacts, and a new backup directory. The installer rejects any other physical topology, verifies MAC `3c:61:05:cf:d1:54`, backs up only the two provisioning sectors, writes explicit bootloader/metadata/slot-A addresses, rereads provisioning, and requires identical SHA-256 values. esptool controls RTS/DTR, so no button press is required. It does not back up the old application binary and never erases the whole chip.

## Update and rollback

The permanent application accepts an authenticated rollout, downloads over pinned HTTPS, checks exact HTTP length and manifest SHA-256, and streams only the unsigned boot2 body into the inactive slot. It hashes that body for the Core-compatible RSA signature while retaining the fixed signature trailer in RAM. The slot writer protects both the executing and permanent slot, bounds and erases only its target, and validates exact boot2 structure plus full IROM/RAM checksum. Only after all checks succeed does `BootControl` request a one-shot temporary boot.

An unsigned/tampered artifact is never selected. A corrupt/incomplete inactive image is never selected. A structurally valid but unhealthy candidate runs temporarily and returns to the committed slot on software/watchdog reset or after the 90-second health timeout. The timeout exceeds measured Wi-Fi/TLS/WSS authentication startup by a broad margin and can only reboot; it cannot promote.

Promotion requires every signal: valid provisioning, normal mode, Wi-Fi, pinned TLS, WSS, HMAC authentication, registration, reported candidate version, and matching type-7 server acceptance. Promotion alternates metadata sectors: erase inactive copy, write and verify body, write `0x434D4954` at `0xFFC` last, and reread. Each record carries format, generation, checked rBoot config, and CRC-32. Selection independently validates both and uses wrap-safe 32-bit serial arithmetic. If both are invalid, rBoot uses compiled safe slot A without writing flash.

rBoot RTC data occupies bytes 256..267 (word 64). Serial-recovery state occupies bytes 128..135 (word 32); a static assertion keeps them disjoint. Temporary candidates clear but never consume/arm the double-reset marker. Permanent firmware retains normal double-reset serial recovery and GPIO3 DMA remains unconstructed whenever UART recovery owns RXD0.

## Upgrade regression requirements

CI covers native/sanitizer state machines, layout, boot2 equivalence, strong-symbol ownership, IROM VMA, and bootloader size. Toolchain/Core/rBoot upgrades still require hardware checks for A-to-B promotion, unhealthy rollback, tamper rejection, inactive-IROM rejection, representative metadata interruption, provisioning hashes, and permanent/temporary/promoted recovery behavior.

## Integrated resource measurements

The final local Core 3.1.2/GCC 10.3.0 production rBoot build uses 40,920/81,920 bytes static RAM and 435,883 bytes of linked flash. Its boot2 image is 435,968/1,040,384 bytes (41.90%), leaving 604,416 bytes (590.25 KiB) in either slot. The retained normal build uses 39,384 bytes RAM and 446,239 linked flash bytes. rBoot is 2,688/4,096 bytes; each transactional metadata copy uses one 4 KiB sector, with its commit marker in the final four bytes.

Existing lifecycle messages report free heap, largest free block, and fragmentation without periodic noisy logging. The integrated hardware run measured: boot and post-provisioning-load `32,080 / 31,392 / 3%`; post-Wi-Fi `30,472 / 29,688 / 3%`; pinned WSS plus authenticated registration `9,088 / 8,928 / 2%`; and immediately after releasing WSS for OTA `29,832 / 29,184 / 3%` (free / largest / fragmentation). A later rejected-download retry observed `29,720 / 20,872 / 25%`, documenting the transient fragmentation case as well.

## Integrated hardware validation

The production integration was exercised on the Phase 1 ESP8266 only: USB topology `4.1`, stable `/dev/serial/by-path/pci-0000:2a:00.3-usb-0:4.1:1.0-port0`, MAC `3c:61:05:cf:d1:54`, 4 MiB flash, kernel `7.2.2-1-cachyos`. Port `4.2` was never opened, reset, or flashed. FTDI RTS supplied all resets without physical intervention.

The observed matrix covered:

- one-time installation and permanent slot-A boot;
- signed A-to-B streaming, temporary B health, server acceptance, transactional promotion, and permanent B reboot;
- software reset before acceptance, which returned an unhealthy temporary candidate to the committed slot without entering serial recovery;
- an artifact signed by an unauthorized key, which was rejected before selection;
- a correctly signed artifact modified at byte 256 after signing, with its repository manifest size/SHA-256 recomputed, which reached the device signature check and printed `Signed rBoot OTA rejected; permanent slot unchanged`; metadata still selected generation-5 slot A and provisioning was unchanged;
- reset after metadata body verification but before its commit marker, which retained the older committed slot;
- reset immediately after the metadata commit marker, which retained the newly committed slot;
- one-bit corruption inside the inactive boot2 IROM payload after application validation, for which rBoot printed `Temp boot rom (1) is bad` and watchdog-fell back to the permanent slot;
- automated permanent-firmware double reset, which entered serial provisioning mode with GPIO3 DMA disabled, followed by a normal single-reset boot;
- final signed `0.2.1-rboot-metrics.1` A-to-B run, temporary-B health and acceptance, generation-6 promotion, permanent-B reboot, and a separate FTDI RTS reset that again booted permanent B and completed provisioning, Wi-Fi, pinned TLS/WSS, HMAC authentication, and registration;
- a second post-signing mutation offered from the instrumented permanent B, which captured the OTA heap measurements above, was rejected cryptographically, and left permanent B running.

Provisioning copies were read directly before and after the full OTA/fault matrix and compared byte-for-byte. Copy A remained SHA-256 `0ef48340f03ca22a368a85108cf797f593d6b651430dab76f013123ecba30cee`; copy B remained `6da5088e22014dce3af2e7a4b532ccb340661685a9629f46cb1937c7d819bb19`.
