#!/usr/bin/env bash
set -euo pipefail

repo_dir="$(cd "$(dirname "$0")/.." && pwd)"
if [[ -x "$repo_dir/.venv/bin/pio" ]]; then
  pio="$repo_dir/.venv/bin/pio"
else
  pio="$(command -v pio)"
fi

tmp="$(mktemp -d)"
trap 'rm -rf "$tmp"' EXIT

cd "$repo_dir"
rg -n '#define (WIFI_SSID|WIFI_PASS|DEVICE_SECRET_HEX|SERVER_PUBLIC_KEY_PEM|ESP_NAME)|-D(ESP_NAME|WIFI_|DEVICE_SECRET|SERVER_PUBLIC)|#include "config.h"' src lib include platformio.ini && { echo 'Installation-specific build input found' >&2; exit 1; } || true
rg -q '#define NEOPIXEL_DATA_PIN 3' include/HardwareConfig.h
rg -q 'NeoPixelBus<NeoGrbFeature, Neo800KbpsMethod>' src/LedController.cpp
if rg -n 'NeoEsp8266BitBang|NEOPIXEL_DATA_PIN 2' src lib include README.md docs/PROVISIONING.md docs/DEVICE_PROTOCOL.md; then
  echo 'Production NeoPixel configuration no longer matches physical GPIO3 DMA hardware' >&2
  exit 1
fi

MINDFLAYER_WIFI_SSID=provisioning-A MINDFLAYER_DEVICE_ID=device-A "$pio" run -e keypad -t clean >/dev/null
MINDFLAYER_WIFI_SSID=provisioning-A MINDFLAYER_DEVICE_ID=device-A "$pio" run -e keypad >/dev/null
cp .pio/build/keypad/firmware.bin "$tmp/firmware-a.bin"

MINDFLAYER_WIFI_SSID=provisioning-B MINDFLAYER_DEVICE_ID=device-B "$pio" run -e keypad -t clean >/dev/null
MINDFLAYER_WIFI_SSID=provisioning-B MINDFLAYER_DEVICE_ID=device-B "$pio" run -e keypad >/dev/null
cmp "$tmp/firmware-a.bin" .pio/build/keypad/firmware.bin

sha256sum .pio/build/keypad/firmware.bin

echo 'Generic firmware is byte-identical across distinct provisioning inputs.'
