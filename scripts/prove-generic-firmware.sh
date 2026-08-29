#!/usr/bin/env bash
set -euo pipefail
repo_dir="$(cd "$(dirname "$0")/.." && pwd)"
pio="$repo_dir/.venv/bin/pio"
tmp="$(mktemp -d)"
trap 'rm -rf "$tmp"' EXIT
cd "$repo_dir"
rg -n '#define (WIFI_SSID|WIFI_PASS|DEVICE_SECRET_HEX|SERVER_PUBLIC_KEY_PEM|ESP_NAME)|-D(ESP_NAME|WIFI_|DEVICE_SECRET|SERVER_PUBLIC)|#include "config.h"' src lib include platformio.ini && { echo 'Installation-specific build input found' >&2; exit 1; } || true
MINDFLAYER_WIFI_SSID=provisioning-A MINDFLAYER_DEVICE_ID=controller-A "$pio" run -e controller_1 -t clean >/dev/null
MINDFLAYER_WIFI_SSID=provisioning-A MINDFLAYER_DEVICE_ID=controller-A "$pio" run -e controller_1 >/dev/null
cp .pio/build/controller_1/firmware.bin "$tmp/firmware-a.bin"
MINDFLAYER_WIFI_SSID=provisioning-B MINDFLAYER_DEVICE_ID=controller-B "$pio" run -e controller_1 -t clean >/dev/null
MINDFLAYER_WIFI_SSID=provisioning-B MINDFLAYER_DEVICE_ID=controller-B "$pio" run -e controller_1 >/dev/null
cmp "$tmp/firmware-a.bin" .pio/build/controller_1/firmware.bin
sha256sum .pio/build/controller_1/firmware.bin
echo 'Generic firmware is byte-identical across distinct provisioning inputs.'
