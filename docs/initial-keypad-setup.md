# Initial keypad setup over USB

For a new keypad: install the generic rBoot firmware, create a unique device
credential on the intended server, provision the Wi-Fi and server settings over
USB, then check both LEDs and all keys. Firmware is identical across keypads;
their identities and network settings are provisioned separately.

These commands target Linux with Bash, a supported ESP8266 keypad with **4 MiB
flash**, and an FTDI-style USB reset connection. The current serial provisioning
tool requires a `/dev/serial/by-path/...` port and Linux serial-control APIs.

## 1. Prepare the computer and identify the board

Install Python 3 with virtual-environment support, Git, Make, patch, a host C
compiler, OpenSSL, and Node.js 24 or newer. From the `mindflayer-keypad` checkout:

```sh
python3 -m venv .venv
.venv/bin/python -m pip install platformio==6.1.19
ls -l /dev/serial/by-path/
```

Connect only the intended board while identifying its port. Set its actual path
and choose a unique logical ID (the examples use `keypad7`):

```sh
KEYPAD_PORT='/dev/serial/by-path/REPLACE_WITH_ACTUAL_PORT'
KEYPAD_ID='keypad7'
```

Close serial monitors and ensure your user can access the serial device (your
distribution may require membership in `dialout` or `uucp` and a new login).
Use a reliable data cable and USB connector; stop if power cuts out when the
connector moves. Keep the same board connected throughout flashing/provisioning.

## 2. Build and install the initial firmware

Use a trusted checkout. For a released version, use its source tag and build
version, not an unrelated development tree labelled as that release. The example
below labels a local setup build `0.0.0-setup.1`; replace it as appropriate.

```sh
.venv/bin/python scripts/verify-signing-key.py
./scripts/build-rboot.sh
PLATFORMIO_BUILD_FLAGS='-DFIRMWARE_VERSION=\"0.0.0-setup.1\"' \
  .venv/bin/pio run -e keypad_rboot
.venv/bin/python scripts/verify-rboot-bootloader.py .pio/rboot-artifacts/rboot.bin
.venv/bin/python scripts/make-rboot-config.py \
  .pio/rboot-artifacts/metadata-a.bin --slot a --generation 1
.venv/bin/python scripts/make-rboot-config.py \
  .pio/rboot-artifacts/metadata-b.bin --state erased
```

The bootloader script installs the pinned Xtensa toolchain before building rBoot.
The firmware retains the committed production signing public key. **No private
signing key is needed for USB installation**, and no new signing key should be
generated per keypad. Later OTA updates must be signed by the matching private
key. See [repository setup](repository-setup.md) for release signing.

Install the four artifacts with the validated installer:

```sh
.venv/bin/python scripts/install-rboot.py \
  --port "$KEYPAD_PORT" \
  --rboot .pio/rboot-artifacts/rboot.bin \
  --metadata-a .pio/rboot-artifacts/metadata-a.bin \
  --metadata-b .pio/rboot-artifacts/metadata-b.bin \
  --slot-a .pio/build/keypad_rboot/rboot-app.bin \
  --backup-dir ".hwtest/initial-$KEYPAD_ID-backup"
```

The backup directory must not already exist; choose a fresh name for a retry.
The installer validates the images and requires an ESP8266 with exactly 4 MiB
detected flash. It overwrites the bootloader, boot metadata, and slot-A
application, but backs up and verifies preservation of both provisioning
sectors. This is **not a backup of the old firmware**. Keep the private backups
after failures as well as success; they can contain Wi-Fi and device secrets.

Do not substitute `firmware.bin`, a `.signed` OTA file, a fault-injection build,
or the static-row diagnostic. Do not use a whole-chip erase. USB installation
is not power-loss safe; an interrupted flash may require repeating this step.
Serial structural checks do not authenticate the artifacts.

After successful installation, a truly unprovisioned keypad pulses both LEDs red
once every three seconds (one second fading, two seconds dark). A previously
provisioned module may instead reconnect using its preserved settings.

## 3. Register the keypad on the intended server

Keep using the same Bash shell so `KEYPAD_PORT` and `KEYPAD_ID` remain set:

```sh
cd ../mindflayer-server
npm ci
export MINDFLAYER_DATA_DIR='/absolute/path/to/the/deployment/data'
```

Use the **actual deployment's persistent data directory**, not a new directory
for each keypad. It must contain `devices.json` and `tls/device-cert.pem` from
the intended server. For a container deployment, use the host path of its data
mount. The bundle builder reads these specific files beneath
`MINDFLAYER_DATA_DIR`; setting `MINDFLAYER_DEVICES_FILE` alone is not sufficient.

For a completely new server, start it once with this data directory to generate
its TLS identity, then stop it before creating credentials. See the
[server README](../../mindflayer-server/README.md) for deployment/startup. On an
existing deployment, schedule the server restart required below; other keypads
will briefly disconnect. Do not run a second server on occupied ports.

Create the new ID once. The CLI normally prints its secret, so suppress stdout:

```sh
MINDFLAYER_DEVICES_FILE="$MINDFLAYER_DATA_DIR/devices.json" \
  npm run device:provision -- "$KEYPAD_ID" > /dev/null
```

If the ID already exists, stop and check whether it belongs to this physical
keypad. Reprovisioning the same keypad uses its existing credential; do not
create a duplicate identity or copy another keypad's bundle. When replacing an
ESP module and reusing its ID, keep the old module offline until it is erased
or reprovisioned with a different identity.

Start/restart the deployment server with the same persistent data directory
after registering new IDs: the current server loads credentials at startup,
not automatically when the file changes. Back up this data directory securely,
including its durable TLS private key. Do not regenerate the server TLS key
when adding another keypad.

## 4. Provision Wi-Fi and server settings over USB

Use the intended 2.4 GHz Wi-Fi network and a server IP address or hostname
reachable from it. The server host must not contain `https://` or a URL path;
`localhost` would refer to the keypad itself. TCP port 10443 must be reachable
unless the deployment uses another device port.

In Bash, prompt for Wi-Fi credentials so the password is not typed into shell
history. Do not enable shell tracing (`set -x`) or record secret-bearing output:

```bash
read -r -p 'Wi-Fi SSID: ' MINDFLAYER_WIFI_SSID
read -r -s -p 'Wi-Fi password: ' MINDFLAYER_WIFI_PASSWORD
printf '\n'
export MINDFLAYER_WIFI_SSID MINDFLAYER_WIFI_PASSWORD
export MINDFLAYER_SERVER_HOST='192.168.1.10'
export MINDFLAYER_SERVER_PORT=10443
export MINDFLAYER_SERIAL_DEBUG=false

npm run device:bundle -- "$KEYPAD_ID" "provisioning/$KEYPAD_ID.provisioning.bin"
unset MINDFLAYER_WIFI_SSID MINDFLAYER_WIFI_PASSWORD
npm run device:serial-provision -- \
  "provisioning/$KEYPAD_ID.provisioning.bin" "$KEYPAD_PORT"
```

Replace the example host/port before running. The bundle includes **SSID and
Wi-Fi password**, device ID, its unique HMAC secret, server host/port, the pinned
server public key, and the serial-debug setting. It is private (mode 0600) and
ignored by Git; do not upload or share it. See [PROVISIONING.md](PROVISIONING.md)
for the format and storage details.

The sender automatically performs a double reset, enters serial recovery,
sends the validated bundle, and waits for `PROVISIONING OK` before reporting
success. Recovery disables LED DMA because the LED data pin shares UART RX.
An LED color alone is not proof of successful provisioning.

**Known timing limitation:** the sender's fixed 0.5-second interval between
reset pulses timed out on some tested modules. A controlled retry with a
1.0-second interval and waiting for `SERIAL PROVISIONING MODE` before sending
the validated bundle succeeded. That banner-aware retry is not yet an option
in the standard CLI. If retrying the normal command still times out, stop and
diagnose recovery/reset timing rather than erasing flash or assuming success;
see the [replacement-module test record](hardware-test-2026-09-10.md#keypad-1-replacement-esp).

## 5. Verify operation

The production server automatically installs newer stable verified releases by
default. A newly provisioned keypad may therefore download an update and reboot
as soon as it connects. Wait for it to reconnect before checking LEDs and keys,
and record the final reported firmware version. To hold a specific version during
commissioning, set `"autoUpdate": false` in that device's server `devices.json`
entry (with no `targetVersion`), then restart the server before connecting it.
Remove the opt-out afterward to follow stable releases. The physical test harness
below disables automatic discovery and only installs explicitly selected targets.

- After reboot, the left LED should progress **red → yellow → green**:
  booting/no Wi-Fi, Wi-Fi connected, then server authenticated. Early stages
  can be brief. Server color commands may override the connection color.
- Confirm the server logs the intended device ID and expected firmware version.
  Persistent red suggests Wi-Fi/power trouble; yellow suggests server reachability,
  TLS-pin, or device-credential trouble; red pulses indicate no valid provisioning.
- Through the connected Foundry receiver, exercise each LED separately in red,
  green, and blue, then both together and off. Confirm the physical result.
- Press and release **Q W E A S D Z X C Shift Space**, one at a time. Require
  exactly one down/up pair per key for this device, no missing keys, no duplicate
  transitions, and no disconnects. Shift/Space appear as `SHI`/`SPC`. Hold each
  test press comfortably longer than the 30 ms debounce interval.
- Hold **Shift + Space + E**, release when it restarts, and confirm it reconnects
  with provisioning retained. This is a restart, not a factory reset; these keys
  also produce ordinary events, so test outside an active game.

For the interactive LED sequencer and per-key event counts used during our bench
tests, follow [HARDWARE_TESTING.md](HARDWARE_TESTING.md). Its isolated server is
a separate deployment: provisioning for it replaces the intended server/Wi-Fi
settings, so restore deployment provisioning afterward. Do not start its hotspot
or overwrite an existing deployment merely to check a production keypad.

Record the keypad label, ESP MAC, firmware version, device ID, and test results
without secrets. Repeat with a different ID and bundle for each new keypad.
Also test keypads powered simultaneously; individual passes do not establish
multi-keypad behavior. Ordinary signed OTA preserves provisioning; changing
Wi-Fi settings or the pinned server key requires serial reprovisioning, not a
firmware rebuild.
