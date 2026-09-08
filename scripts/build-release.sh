#!/usr/bin/env bash
set -euo pipefail

if [[ $# -ne 1 ]] || [[ ! "$1" =~ ^[0-9]+\.[0-9]+\.[0-9]+(-[0-9A-Za-z.-]+)?$ ]]; then
  echo "Usage: $0 <semantic-version>" >&2
  exit 2
fi

version="$1"
repo_dir="$(cd "$(dirname "$0")/.." && pwd)"
private_key="${FIRMWARE_SIGNING_PRIVATE_KEY_FILE:-}"
temporary_key=""

if [[ -z "$private_key" ]]; then
  [[ -n "${FIRMWARE_SIGNING_PRIVATE_KEY:-}" ]] || {
    echo "FIRMWARE_SIGNING_PRIVATE_KEY is not configured" >&2
    exit 1
  }
  temporary_key="$(mktemp)"
  chmod 600 "$temporary_key"
  printf '%s\n' "$FIRMWARE_SIGNING_PRIVATE_KEY" > "$temporary_key"
  private_key="$temporary_key"
fi

cleanup() {
  if [[ -n "$temporary_key" ]]; then rm -f -- "$temporary_key"; fi
}
trap cleanup EXIT

cd "$repo_dir"
if command -v pio >/dev/null 2>&1; then
  pio="$(command -v pio)"
elif [[ -x "$repo_dir/.venv/bin/pio" ]]; then
  pio="$repo_dir/.venv/bin/pio"
else
  echo "PlatformIO (pio) is not installed" >&2
  exit 1
fi
scripts/build-rboot.sh
PLATFORMIO_BUILD_FLAGS="-DFIRMWARE_VERSION=\\\"$version\\\"" "$pio" run -e keypad_rboot
python3 scripts/verify-rboot-image.py \
  --elf .pio/build/keypad_rboot/firmware.elf \
  --image .pio/build/keypad_rboot/rboot-app.bin \
  --esptool2 .pio/rboot-upstream/esptool2/esptool2
test "$(wc -c < .pio/rboot-artifacts/rboot.bin)" -le 4096
scripts/sign-firmware.sh \
  .pio/build/keypad_rboot/rboot-app.bin \
  "$private_key" mindflayer-keypad-v1 "$version" dist/firmware
tar -C dist/firmware -czf "dist/mindflayer-keypad-$version-server-firmware.tar.gz" .
