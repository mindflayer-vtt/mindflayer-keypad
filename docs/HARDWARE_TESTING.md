# Interactive keypad/server test

Use `scripts/hwtest-server.cjs` to run the real sibling server with an isolated
credential store and a loopback Foundry WebSocket receiver. This tests physical
keypad events through Wi-Fi, pinned TLS, device authentication, binary CBOR,
server dispatch, and the Foundry-facing WebSocket. The receiver substitutes for
Foundry itself; this is not a Foundry module or signed OTA test.

## Start

Connect a 4 MiB ESP8266 keypad, identify its stable serial path, and install the
production `keypad_rboot` build as described in the root README. Serial installation
overwrites firmware; get the owner's approval first. Do not flash a fault-injection
environment for this test.

If using the temporary hotspot, first record the active Wi-Fi connection UUID and
default routes. Creating the hotspot replaces the connection on that Wi-Fi adapter;
keep another network connection available. Run:

```sh
./scripts/hwtest-network-up.sh wlan0
node scripts/hwtest-server.cjs ../mindflayer-server .hwtest/interactive-test 10.42.0.1
```

Use a new state directory per test. The server repository must have its dependencies
installed and Node.js 24 available. The harness provisions `hwtest-keypad` without
printing its secret, creates a separate server TLS identity, binds the device listener
to the supplied hotspot address on port 10443, and binds the Foundry listener only to
127.0.0.1:8080. Both ports must be free. Credentials and event logs remain in the ignored
state directory; never commit them.

In a second shell, generate and send provisioning using the same state directory:

```sh
source .hwtest/network.env
MINDFLAYER_DATA_DIR="$PWD/.hwtest/interactive-test/data" \
MINDFLAYER_WIFI_SSID="$SSID" \
MINDFLAYER_WIFI_PASSWORD="$PASSWORD" \
MINDFLAYER_SERVER_HOST=10.42.0.1 \
MINDFLAYER_SERIAL_DEBUG=true \
node ../mindflayer-server/scripts/build-provisioning.js \
  hwtest-keypad .hwtest/interactive-test/keypad.provisioning.bin
node ../mindflayer-server/scripts/serial-provision.js \
  .hwtest/interactive-test/keypad.provisioning.bin /dev/serial/by-path/REPLACE_ME
```

Use the actual hotspot address if it differs. Wait for the harness to report the
keypad registration; `status` must show `connected: true`.

On firmware with the restored status indication, watch the left LED turn red
before Wi-Fi connects, yellow while Wi-Fi is up but the server is not yet
authenticated, then green. Stages may be brief on a healthy network. Server color
commands override this indication until the connection state next changes;
connection-status updates do not change the right LED. Recovery mode intentionally
leaves the LED driver uninitialized to keep serial RX available.

## LEDs and keys

Type `leds` into the harness terminal. Every 2.5 seconds it sends a configuration
through the Foundry WebSocket: LED 1 red, green, blue; LED 2 red, green, blue; both
white; both off; both dim green. In the individual tests the other LED is off.
Brightness is deliberately modest. A sent command is not proof of light output:
ask the operator to confirm the colors and which physical LED changes.

Once both LEDs are dim green, ask the operator to press and release each key once
in this order, pausing briefly between keys:

```text
Q W E
A S D
Z X C
Shift Space
```

Check that the receiver logs one `down` and one `up` for every key, with the correct
device ID. Shift is reported as `SHI`, and Space as `SPC`. Then check a held key and
Shift plus another key; inspect the event ordering and release events. The 5 ms
matrix scan is not a hardware debounce guarantee, so report duplicate transitions
instead of hiding them. `status` prints cumulative per-key transition counts.

The harness saves timestamped events and LED commands in `events.jsonl`. It does not
automatically mark a test successful. Record operator observations separately and
distinguish them from programmatically observed results.

With firmware containing the restart shortcut, hold Shift + Space + E together
and release when the keypad restarts. Confirm disconnection followed by a fresh
authenticated registration, and confirm provisioning is retained. These keys
still produce ordinary server events. Shift + Space + Q must not restart it.
The shortcut uses normal scanning while connected; it does not replace serial
recovery or reset a keypad stuck waiting for Wi-Fi during startup.

## Stop

Type `quit` (or press Ctrl-C) to send both LEDs off and stop the test listeners.
Stop any serial monitor, then remove only the generated hotspot and restore the
original Wi-Fi connection:

```sh
./scripts/hwtest-network-down.sh
nmcli connection up uuid ORIGINAL_WIFI_UUID
ip route show default
```

The keypad retains the newly installed firmware and test provisioning. It will not
connect after the hotspot is removed; provision it again for its intended deployment.
Do not claim the old firmware or provisioning can be recovered when installation
was explicitly performed without a backup.

## Server-delivered OTA test

Place a signed artifact and its manifest under the test state's `firmware/`
directory, then restart the harness with an explicit target version:

```sh
node scripts/hwtest-server.cjs ../mindflayer-server \
  .hwtest/interactive-test 10.42.0.1 hwtest-keypad 0.0.1-hwtest.1
```

The harness validates the manifest/artifact through the real server repository
and selects that target for this test process only. On reconnection, the real
server offers the update, serves an authorized HTTPS download, and acknowledges
the registered candidate version. The log records the firmware response status
and declared byte count, without logging the bearer token. A completed server
response alone is not proof that the device installed or promoted the image:
also observe device validation, temporary boot, server acceptance, and a subsequent
permanent boot via serial diagnostics and fresh server registrations.

The signature must match the public key embedded in the currently running device,
regardless of where either build was compiled. Never disable verification to run
this test. A local-key intermediate requires explicit authorization and a serial
installation. For a temporary local-key test, the locally signed OTA payload can
contain the production public key, restoring production trust after promotion.
Such an artifact is test-only and must not be published as a production release;
it does not validate GitHub's production signing workflow. The temporary source
key substitution must be reverted and never committed.
