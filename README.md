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

The production build is generic: it does not read `config.h` or any installation-specific build input. Device ID, HMAC secret, Wi-Fi settings, direct-server address/port, and pinned server DER/SPKI public key are installed afterward through the serial CBOR provisioning workflow in [PROVISIONING.md](PROVISIONING.md). Two redundant raw 4 KiB flash sectors preserve provisioning across ordinary signed OTA; no filesystem is used.

Existing hardware wires NeoPixel data to GPIO3/RXD0. Provisioned operation uses the ESP8266 DMA backend on that physical pin, so UART RX is intentionally available only in the special unprovisioned/double-reset recovery mode documented in [PROVISIONING.md](PROVISIONING.md). The host provisioning tool enters that mode automatically through two FTDI RTS reset pulses.

## Flashing

1. Connect the wemos d1 mini to the PC with USB
2. Run the following command to flash the controller
   ```bash
   platformio run -e controller_1 -t upload
   ```

## Secure server-managed updates

The keypad validates the provisioned direct-server key through BearSSL `setKnownKey`; it never disables TLS validation, depends on public CA roots, pins a certificate fingerprint, or needs a device clock for server authentication. The global firmware-signing public key remains compiled into every generic artifact; its private key remains outside Git, the server, and the keypad.

Override `FIRMWARE_VERSION` at build time to produce rollout versions without editing source. After pinned WSS connects to `/device/v1`, the keypad exchanges only restricted binary CBOR, completes HMAC-SHA256 authentication, and registers its hardware ID and version. See [DEVICE_PROTOCOL.md](DEVICE_PROTOCOL.md).

For a disposable local signing key and a framework-compatible signed artifact plus server manifest:

```sh
./scripts/generate-test-signing-key.sh
.venv/bin/pio run -e controller_1
./scripts/build-rboot.sh
.venv/bin/pio run -e controller_1_rboot
./scripts/sign-firmware.sh .pio/build/controller_1_rboot/rboot-app.bin keys/signing-private.pem mindflayer-keypad-v1 1.2.3 artifacts
```

`include/HardwareConfig.h` is the global firmware-signing trust domain. Its public key must match the private key used by the signing command; generating a replacement keypair requires deliberately updating that global public key and rebuilding all generic firmware. Provisioning never changes this key.

The private key and artifacts are ignored. Generate the production RSA signing key separately, protect and back it up offline or in a dedicated CI secret, and never copy it into the server container. Only its public key belongs in firmware. The code installs the ESP8266 core `SigningVerifier`, so modified or unsigned OTA artifacts are rejected.

## Public firmware releases

CI validates every push and pull request. On `main`, semantic-release analyzes Conventional Commits, chooses the next semantic version, and—only when a release is due—builds the rBoot application with that version, signs it, verifies the signature against the public key compiled into the keypad, creates the `vX.Y.Z` tag and GitHub release, and publishes `mindflayer-keypad-X.Y.Z-server-firmware.tar.gz`. Configure the GitHub Actions secret `FIRMWARE_SIGNING_PRIVATE_KEY` with the PEM-encoded production RSA private key before merging a releasable commit. A missing or mismatched key fails before a release is published.

Extract the release archive into the server's read-only firmware directory. It contains the complete repository layout expected by `mindflayer-server`:

```text
manifest.json
mindflayer-keypad-v1/1.2.3/firmware.bin.signed
```

The manifest's hardware ID, semantic-release version, relative path, size, and SHA-256 match the server's firmware repository contract. The server can then target provisioned keypads at that version; it does not need and must never receive the signing key.

The rBoot production path downloads this signed boot2 image into the inactive slot, validates transport hash, RSA signature, structure, and full IROM/RAM checksum, then boots it once. Promotion requires explicit server acceptance after the complete application health gate. See [OTA_BOOT.md](OTA_BOOT.md). The additive `controller_1` environment remains available as the pre-migration eboot build; it is not an rBoot OTA artifact.

For isolated hardware tests, `scripts/hwtest-network-up.sh` creates a namespaced WPA2 2.4 GHz NetworkManager AP and stores credentials only under ignored `.hwtest/`. `scripts/hwtest-network-down.sh` removes only that generated profile. Always tear it down and confirm the original default route remains.
