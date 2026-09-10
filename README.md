<div align="center">
<img width="460" src="https://raw.githubusercontent.com/mindflayer-vtt/mindflayer-keypad/main/.github/foundryvtt-mindflayer-logo.png">
</div>

# Mind Flayer - Keypad

Firmware for an ESP8266 based keypad that can be used with the Mind Flayer VTT module &amp; server

<div align="center">
<img width="460" src="https://raw.githubusercontent.com/mindflayer-vtt/mindflayer-keypad/main/.github/keypad.png">
</div>

## Schematic and PCB

See [hardware/board/README.md](https://github.com/mindflayer-vtt/mindflayer-keypad/tree/main/hardware/board) for more information.

## Case

See [hardware/case/README.md](https://github.com/mindflayer-vtt/mindflayer-keypad/tree/main/hardware/case) for more information.

## Requirements

You have to have set up the following software in order to compile an flash the controller:

- Python 3
- PlatformIO 6.1.19
- Git, Make, patch, and a host C compiler for building esptool2; OpenSSL for release signing

## Setup

For a complete new-device walkthrough, see
[Initial keypad setup over USB](docs/initial-keypad-setup.md), covering firmware
installation, server registration, Wi-Fi provisioning, and LED/key verification.

1. Use the following command to setup a python virtual environment:
   ```bash
   python -m venv .venv
   ```
2. Activate the python virtual environment
   ```bash
   source .venv/bin/activate
   ```
3. Install platformio
   ```bash
   pip install platformio==6.1.19
   ```

## Context

The firmware source is organized by runtime domain; see [docs/ARCHITECTURE.md](docs/ARCHITECTURE.md).

Native and transport regression-test commands are documented in [docs/TESTING.md](docs/TESTING.md).

## Restart shortcut

While the keypad is connected and authenticated, hold **Shift + Space + E**
together to restart it. Release the keys when it restarts to avoid triggering
another restart after reconnection. This does not erase provisioning or perform
a factory reset. The shortcut uses the normal key scanner; it is not available
in serial provisioning mode or while startup is waiting for Wi-Fi. The keys are
also sent to the server as ordinary key events.

The key matrix is sampled at a minimum interval of 5 ms. Each key independently
requires 30 ms of stable sampled input before a press or release is reported;
shorter glitches are ignored and holding a key does not generate repeats. The
restart shortcut uses this debounced state too.

## Left LED connection status

On a normally provisioned boot, the left LED shows:

| Color  | State                                       |
| ------ | ------------------------------------------- |
| Red    | Booting; Wi-Fi is not connected             |
| Yellow | Wi-Fi connected; waiting for the server     |
| Green  | Connected and authenticated with the server |

Loss of the server connection changes it to yellow; loss of Wi-Fi changes it to
red. Status transitions leave the right LED unchanged. Server configuration can
set both LEDs, including the left one; those colors remain until the connection
state changes.

An unprovisioned keypad pulses **both LEDs red once every three seconds**: a
one-second fade in/out, then two seconds dark. The pulse starts after the
double-reset recovery window. Serial provisioning/recovery mode disables the
animation and does not initialize the LED driver, because its data pin is shared
with serial RX. The provisioning tool selects that mode automatically.

## Generic firmware and provisioning

The production build is generic: it does not read `config.h` or any installation-specific build input. Device ID, HMAC secret, Wi-Fi settings, direct-server address/port, and pinned server DER/SPKI public key are installed afterward through the serial CBOR provisioning workflow in [docs/PROVISIONING.md](docs/PROVISIONING.md). Two redundant raw 4 KiB flash sectors preserve provisioning across ordinary signed OTA; no filesystem is used.

Existing hardware wires NeoPixel data to GPIO3/RXD0. Normal operation, including the unprovisioned pulse, uses the ESP8266 DMA backend on that physical pin, so UART RX is intentionally available only in the double-reset serial recovery mode documented in [docs/PROVISIONING.md](docs/PROVISIONING.md). The host provisioning tool enters that mode automatically through two FTDI RTS reset pulses, whether or not the device has stored provisioning.

## Initial rBoot installation

The production OTA layout requires a one-time serial installation of rBoot, two transactional metadata sectors, and the application in slot A. Connect any supported 4 MiB ESP8266 board over USB serial and identify its port (prefer a stable `/dev/serial/by-id/...` or `/dev/serial/by-path/...` name). The installer accepts any port and MAC address, but asks esptool to identify the target and stops unless it is an ESP8266 with exactly 4 MiB of detected flash.

Build the pinned bootloader and initial application, then generate the two metadata sectors. The bootloader script installs the ESP8266 packages, including the Xtensa compiler, before building:

```sh
./scripts/build-rboot.sh
.venv/bin/pio run -e keypad_rboot
.venv/bin/python scripts/make-rboot-config.py .pio/rboot-artifacts/metadata-a.bin --slot a --generation 1
.venv/bin/python scripts/make-rboot-config.py .pio/rboot-artifacts/metadata-b.bin --state erased
```

Install them, replacing `PORT` with the connected ESP8266 serial device and choosing a new backup directory:

```sh
.venv/bin/python scripts/install-rboot.py \
  --port PORT \
  --rboot .pio/rboot-artifacts/rboot.bin \
  --metadata-a .pio/rboot-artifacts/metadata-a.bin \
  --metadata-b .pio/rboot-artifacts/metadata-b.bin \
  --slot-a .pio/build/keypad_rboot/rboot-app.bin \
  --backup-dir .hwtest/initial-rboot-backup
```

Before touching the serial port, the installer validates artifact sizes, image headers, RAM ranges, entry points, and checksums. Metadata A must be committed and select slot A; metadata B must be erased or also validly select slot A. Both records must use the production layout. Slot A must be the unsigned `rboot-app.bin`, not Arduino's `firmware.bin` or a signed OTA artifact. Only the validated snapshots are flashed.

This writes only rBoot at `0x000000`, metadata at `0x001000` and `0x100000`, and slot A at `0x002000`. Before writing, it backs up both provisioning sectors (`0x3f9000` and `0x3fa000`), requires complete 4 KiB reads, saves `sha256.txt`, and rechecks the chip family and flash size. Afterward it reads provisioning again and requires identical SHA-256 digests. The backup directory must not already exist. It is created owner-only because the backups contain provisioning credentials; keep it private and retain it until the keypad has booted and its provisioning has been verified, including after an interrupted or failed installation.

Use trusted build artifacts: these structural checks do not authenticate the serial-install images. Installing rBoot replaces the existing application image and is not power-loss safe; an interrupted write may require another serial installation. Do not disconnect or swap the target during installation, or use this command for a different flash layout.

For a blank device, install rBoot first and then perform the serial provisioning workflow in [docs/PROVISIONING.md](docs/PROVISIONING.md). For an already provisioned keypad, the preserved sectors allow the rBoot application to reuse its existing settings.

## Secure server-managed updates

The keypad validates the provisioned direct-server key through BearSSL `setKnownKey`; it never disables TLS validation, depends on public CA roots, pins a certificate fingerprint, or needs a device clock for server authentication. The global firmware-signing public key remains compiled into every generic artifact; its private key remains outside Git, the server, and the keypad.

Override `FIRMWARE_VERSION` at build time to produce rollout versions without editing source. After pinned WSS connects to `/device/v1`, the keypad exchanges only restricted binary CBOR, completes HMAC-SHA256 authentication, and registers its hardware ID and version. See [docs/DEVICE_PROTOCOL.md](docs/DEVICE_PROTOCOL.md).

For a disposable local signing key and a framework-compatible signed artifact plus server manifest:

```sh
./scripts/generate-test-signing-key.sh
.venv/bin/pio run -e keypad
./scripts/build-rboot.sh
.venv/bin/pio run -e keypad_rboot
./scripts/sign-firmware.sh .pio/build/keypad_rboot/rboot-app.bin keys/signing-private.pem mindflayer-keypad-v1 1.2.3 artifacts
```

`keys/firmware-signing-public.pem` is the authoritative release public key. Its compiled copy in `include/HardwareConfig.h` must match; `scripts/verify-signing-key.py` checks this and release preparation runs the check automatically. Generating a replacement keypair requires deliberately updating that global public key and rebuilding all generic firmware. Provisioning never changes this key.

The private key and artifacts are ignored. Generate the production RSA signing key separately, protect and back it up offline or in a dedicated CI secret, and never copy it into the server container. Only its public key belongs in firmware. The code installs the ESP8266 core `SigningVerifier`, so modified or unsigned OTA artifacts are rejected.

## Public firmware releases

CI validates every push and pull request. On `main`, semantic-release analyzes Conventional Commits, chooses the next semantic version, and—only when a release is due—builds the rBoot application with that version, signs it, verifies the signature against the public key compiled into the keypad, creates the `vX.Y.Z` tag and GitHub release, and publishes `mindflayer-keypad-X.Y.Z-server-firmware.tar.gz`. Configure the GitHub Actions secret `FIRMWARE_SIGNING_PRIVATE_KEY` with the PEM-encoded production RSA private key before merging a releasable commit. A missing or mismatched key fails before a release is published. See [docs/repository-setup.md](docs/repository-setup.md) for key generation, verification, upload, storage, and rotation precautions.

With automatic firmware updates enabled (the production server default),
`mindflayer-server` discovers stable releases, downloads this archive, verifies
the production signature, and offers newer firmware to all eligible connected
keypads. Keypads install it and reboot automatically. The server checks at startup
and hourly; per-device opt-outs and exact version pins are available. See the
[server update guide](https://github.com/mindflayer-vtt/mindflayer-server/blob/main/docs/firmware-updates.md).

For a manually targeted/offline rollout, extract the release archive into the
server's read-only firmware directory. It contains the complete repository layout:

```text
manifest.json
mindflayer-keypad-v1/1.2.3/firmware.bin.signed
```

The manifest's hardware ID, semantic-release version, relative path, size, and SHA-256 match the server's firmware repository contract. The server can then target provisioned keypads at that version; it does not need and must never receive the signing key.

The rBoot production path downloads this signed boot2 image into the inactive slot, validates transport hash, RSA signature, structure, and full IROM/RAM checksum, then boots it once. Promotion requires explicit server acceptance after the complete application health gate. See [docs/OTA_BOOT.md](docs/OTA_BOOT.md). The generic `keypad` environment remains available as the pre-migration eboot build; it is not an rBoot OTA artifact. Legacy `controller_1*` environment names remain as compatibility aliases.

For isolated hardware tests, `scripts/hwtest-network-up.sh` creates a namespaced WPA2 2.4 GHz NetworkManager AP and stores credentials only under ignored `.hwtest/`. `scripts/hwtest-network-down.sh` removes only that generated profile. Always tear it down and confirm the original default route remains.
