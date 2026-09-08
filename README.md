<div align="center">
<img width="460" src="https://raw.githubusercontent.com/mindflayer-vtt/mindflayer-keypad/main/.github/foundryvtt-mindflayer-logo.png">
</div>

# Mind Flayer - Keypad

Firmware for an ESP8266 based keypad that can be used with the Mind Flayer VTT module &amp; server

The firmware source is organized by runtime domain; see [docs/ARCHITECTURE.md](docs/ARCHITECTURE.md).

Native and transport regression-test commands are documented in [docs/TESTING.md](docs/TESTING.md).

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
- Git, Make, and a host C compiler for building esptool2; OpenSSL for release signing

## Setup

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

## Generic firmware and provisioning

The production build is generic: it does not read `config.h` or any installation-specific build input. Device ID, HMAC secret, Wi-Fi settings, direct-server address/port, and pinned server DER/SPKI public key are installed afterward through the serial CBOR provisioning workflow in [docs/PROVISIONING.md](docs/PROVISIONING.md). Two redundant raw 4 KiB flash sectors preserve provisioning across ordinary signed OTA; no filesystem is used.

Existing hardware wires NeoPixel data to GPIO3/RXD0. Provisioned operation uses the ESP8266 DMA backend on that physical pin, so UART RX is intentionally available only in the special unprovisioned/double-reset recovery mode documented in [docs/PROVISIONING.md](docs/PROVISIONING.md). The host provisioning tool enters that mode automatically through two FTDI RTS reset pulses.

## Initial rBoot installation

The production OTA layout requires a one-time serial installation of rBoot, two transactional metadata sectors, and the application in slot A. Connect any supported 4 MiB ESP8266 board over USB serial and identify its port (prefer a stable `/dev/serial/by-id/...` or `/dev/serial/by-path/...` name). The installer accepts any port and MAC address, but asks esptool to identify the target and stops unless it is an ESP8266.

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

This writes only rBoot at `0x000000`, metadata at `0x001000` and `0x100000`, and slot A at `0x002000`. Before writing, it backs up both provisioning sectors (`0x3f9000` and `0x3fa000`); afterward it reads them again and requires identical SHA-256 digests. The backup directory must not already exist. Keep that directory until the keypad has booted and its provisioning has been verified. Installing rBoot replaces the existing application image, so do not interrupt the write or use this command for an ESP8266 with a different flash layout.

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

Extract the release archive into the server's read-only firmware directory. It contains the complete repository layout expected by `mindflayer-server`:

```text
manifest.json
mindflayer-keypad-v1/1.2.3/firmware.bin.signed
```

The manifest's hardware ID, semantic-release version, relative path, size, and SHA-256 match the server's firmware repository contract. The server can then target provisioned keypads at that version; it does not need and must never receive the signing key.

The rBoot production path downloads this signed boot2 image into the inactive slot, validates transport hash, RSA signature, structure, and full IROM/RAM checksum, then boots it once. Promotion requires explicit server acceptance after the complete application health gate. See [docs/OTA_BOOT.md](docs/OTA_BOOT.md). The generic `keypad` environment remains available as the pre-migration eboot build; it is not an rBoot OTA artifact. Legacy `controller_1*` environment names remain as compatibility aliases.

For isolated hardware tests, `scripts/hwtest-network-up.sh` creates a namespaced WPA2 2.4 GHz NetworkManager AP and stores credentials only under ignored `.hwtest/`. `scripts/hwtest-network-down.sh` removes only that generated profile. Always tear it down and confirm the original default route remains.
