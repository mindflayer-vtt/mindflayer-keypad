# Mind Flayer Provisioning Format v1

One generic signed firmware contains hardware definitions, QCBOR, the device protocol, and the firmware-signing public key. Installation data is supplied later over trusted serial and stored in two dedicated raw flash sectors. No filesystem, device ID, HMAC secret, Wi-Fi credential, server address, or server TLS key participates in compiling or signing `firmware.bin`.

## Serial request envelope

The host sends exactly: bytes 0..3 magic `MFP1`; byte 4 envelope version `1`; bytes 5..6 unsigned big-endian CBOR payload length (1..1024); the payload; then a big-endian CRC32. CRC covers magic, version, length, and payload. The variant is CRC-32/ISO-HDLC (reflected polynomial `0xEDB88320`, init/xorout `0xFFFFFFFF`), whose `123456789` check is `0xCBF43926`. Maximum envelope size is 1,035 bytes.

The definite-length CBOR map uses integer keys: `0` schema version 1; `1` device ID text 1..64; `2` exactly 32 secret bytes; `3` SSID text 1..32; `4` Wi-Fi password text 0..63; `5` server host text 1..253; `6` port 1..65535; `7` DER/SPKI RSA server public key bytes 32..512. All keys are required. Duplicate, unsupported, missing, mistyped or oversized fields, embedded NUL text, unsupported versions, invalid/non-RSA DER, indefinite encodings, trailing bytes, or invalid CRC are rejected. The host emits canonical CBOR and its verifier decodes, validates, and byte-for-byte canonical-round-trips the payload before serial transmission. Version 1 rejects unknown keys; a future schema version must define its own policy.

## Physical flash layout

The target has 4 MiB flash. The addresses are centralized and compile-time asserted in `lib/Provisioning/FlashLayout.h`:

| Range | Owner |
|---|---|
| `0x000000..0x000FFF` | rBoot 1.4.2 |
| `0x001000..0x001FFF` | transactional boot metadata A |
| `0x002000..0x0FFFFF` | rBoot slot A |
| `0x100000..0x100FFF` | transactional boot metadata B |
| `0x101000..0x201FFF` | reserved/free |
| `0x202000..0x2FFFFF` | rBoot slot B |
| `0x300000..0x3F8FFF` | reserved/free |
| `0x3F9000..0x3F9FFF` | provisioning copy A |
| `0x3FA000..0x3FAFFF` | provisioning copy B |
| `0x3FB000..0x3FBFFF` | future 4 MiB Arduino EEPROM |
| `0x3FC000..0x3FCFFF` | RF calibration |
| `0x3FD000..0x3FFFFF` | SDK Wi-Fi parameters |

Core 3.1.2 uses `ld/eagle.flash.4m-rboot.ld` for rBoot application images and `ld/eagle.flash.4m0m.ld` only for the retained pre-migration build. Both configure no filesystem. EEPROM is explicitly at `0x3FB000`, with RF calibration and SDK Wi-Fi state above it. Compile-time assertions encode bootloader, metadata, slot, provisioning, and framework-tail boundaries. Runtime provisioning flash operations still refuse every address outside the two provisioning sectors.

No filesystem space is configured, mounted, generated, or parsed.

## Redundant raw records

Each 4 KiB sector independently contains: bytes 0..3 `MFR1`; byte 4 record version 1; bytes 5..8 generation uint32 big-endian; bytes 9..10 payload length big-endian; CBOR payload; CRC32 big-endian; erased padding; and the commit word `MFPC` in the final four sector bytes. CRC covers bytes 0 through the last payload byte, not the CRC or commit word. Fields are encoded explicitly; C structure padding is never persisted.

To update, the store selects the inactive/older copy, erases only it, writes header+payload+CRC, reads and validates it without accepting it as committed, writes `MFPC` last, then reads and validates it again. The prior valid generation is untouched. Boot validates both independently and chooses the newest committed valid record using uint32 serial-number arithmetic (`candidate-reference` in `1..0x7fffffff`). The exactly half-range ambiguous case deterministically does not declare the candidate newer. One valid copy is sufficient; two invalid copies produce `UNPROVISIONED`.

Power loss before erase changes nothing; after erase, during data writing, or before the commit word leaves the previous copy selected; after the commit word, either the previous or fully verified newer record is valid. Flash failure can still destroy data, but the algorithm never erases both copies in one update.

## Serial provisioning and recovery

Existing keypad PCBs physically connect NeoPixel data to ESP8266 GPIO3/RXD0. Normal provisioned operation therefore uses NeoPixelBus's ESP8266 `Neo800KbpsMethod` DMA/I²S backend on GPIO3 and cannot receive UART data at the same time. This is a hardware constraint; production firmware must not redirect the LEDs to another pin.

Serial provisioning is a distinct boot mode. An unprovisioned boot never constructs or initializes the NeoPixelBus object, leaving GPIO3 as UART RX. A provisioned device enters the same mode after two reset pulses within a 1.5-second window. The first boot writes a magic-plus-inverse marker at RTC user-memory block 32 (byte 128, above the RTC words reserved for eboot OTA); the second boot consumes and clears it, skips NeoPixel construction, skips Wi-Fi, and waits for one MFP1 envelope. A normal single reset clears the marker after the window and only then constructs the DMA object and calls `Begin()`.

NeoPixelBus 2.8.4 was inspected for this lifecycle: the ESP8266 DMA method constructor allocates its DMA buffers and records its instance, while `Begin()` reaches `InitializeI2s()` and changes fixed GPIO3 to I²S function 1. Firmware defers both construction and initialization until recovery mode has been ruled out. It never attempts concurrent UART RX and NeoPixel DMA.

Generate a bundle from the server's stored device secret and certificate public key, then send it to the reported stable serial path:

```sh
MINDFLAYER_DATA_DIR=data \
MINDFLAYER_WIFI_SSID='temporary-ap' \
MINDFLAYER_WIFI_PASSWORD='temporary-password' \
MINDFLAYER_SERVER_HOST='10.42.0.1' \
npm run device:bundle -- controller1 provisioning/controller1.provisioning.bin
npm run device:serial-provision -- provisioning/controller1.provisioning.bin /dev/serial/by-path/...
```

Bundles are mode 0600 and ignored. The sender requires a stable `/dev/serial/by-path` path, releases GPIO0, pulses reset twice through FTDI RTS to select recovery mode, waits for the application, sends the already validated bounded envelope, and waits for the device acknowledgement. The same sequence also works on an unprovisioned device. No person needs to press reset. The device bounds and validates the envelope in RAM, writes only through `ProvisioningStore`, verifies persisted data, reports success, and reboots. Reprovisioning uses the alternate sector and increments generation. Physical serial access and raw flash access are trusted: CRC detects accidental corruption, not tampering. A physical attacker can extract the Wi-Fi password and HMAC secret; this is accepted for the ESP8266 threat model.

rBoot is integrated without changing this store. Slot writes end at `0x300000`, so neither OTA nor promotion touches provisioning. The one-time installer hashes both sectors before and after migration. Normal A/B OTA requires them to remain byte-identical.
