#!/usr/bin/env bash
set -euo pipefail

RBOOT_COMMIT=614f33685d0dd990fc4202f2409b0d2365eeaef3
ESPTOOL2_COMMIT=91759a7c4faec1b7bc8410166d22ed92eb90556c
PROJECT_DIR=$(cd "$(dirname "$0")/.." && pwd)
CACHE_DIR="$PROJECT_DIR/.pio/rboot-upstream"
OUTPUT_DIR="$PROJECT_DIR/.pio/rboot-artifacts"
TOOLCHAIN_DIR=${PLATFORMIO_PACKAGES_DIR:-${PLATFORMIO_CORE_DIR:-$HOME/.platformio}/packages}/toolchain-xtensa/bin

if [[ -x "$PROJECT_DIR/.venv/bin/pio" ]]; then
  pio="$PROJECT_DIR/.venv/bin/pio"
else
  pio="$(command -v pio)"
fi
# Installing Core alone does not install the ESP8266 compiler. This entry point
# must also work before the first application build, including on release CI.
"$pio" pkg install --project-dir "$PROJECT_DIR" --environment keypad_rboot
[[ -x "$TOOLCHAIN_DIR/xtensa-lx106-elf-gcc" ]] || {
  echo "Xtensa compiler missing after package installation: $TOOLCHAIN_DIR" >&2
  exit 1
}

mkdir -p "$CACHE_DIR" "$OUTPUT_DIR"
if [[ ! -d "$CACHE_DIR/rboot/.git" ]]; then
  git clone https://github.com/raburton/rboot.git "$CACHE_DIR/rboot"
fi
if [[ ! -d "$CACHE_DIR/esptool2/.git" ]]; then
  git clone https://github.com/raburton/esptool2.git "$CACHE_DIR/esptool2"
fi
build_dir="$(mktemp -d "$CACHE_DIR/build.XXXXXX")"
trap 'rm -rf -- "$build_dir"' EXIT
mkdir "$build_dir/rboot" "$build_dir/esptool2"
# Build exact source snapshots without resetting a cached checkout or reusing
# objects from a different compiler. Cached source edits are left untouched.
git -C "$CACHE_DIR/rboot" archive "$RBOOT_COMMIT" | tar -x -C "$build_dir/rboot"
git -C "$CACHE_DIR/esptool2" archive "$ESPTOOL2_COMMIT" | tar -x -C "$build_dir/esptool2"
# This exported tree lives inside the parent keypad Git worktree. git apply
# would silently skip rboot.c because its patch path is outside that subdirectory.
# Apply directly to the exported filesystem tree and reject fuzzy matches.
patch --batch --fuzz=0 -p1 --directory="$build_dir/rboot" < "$PROJECT_DIR/scripts/rboot-metadata.patch"
make -C "$build_dir/esptool2"
make -C "$build_dir/rboot" all \
  XTENSA_BINDIR="$TOOLCHAIN_DIR" \
  ESPTOOL2="$build_dir/esptool2/esptool2" \
  RBOOT_BIG_FLASH=1 RBOOT_RTC_ENABLED=1 RBOOT_CONFIG_CHKSUM=1 RBOOT_IROM_CHKSUM=1 \
  RBOOT_INTEGRATION=1 RBOOT_EXTRA_INCDIR="$PROJECT_DIR/scripts" \
  SPI_SIZE=4M SPI_MODE=dio SPI_SPEED=40
python3 "$PROJECT_DIR/scripts/verify-rboot-bootloader.py" "$build_dir/rboot/firmware/rboot.bin"
cp "$build_dir/rboot/firmware/rboot.bin" "$OUTPUT_DIR/rboot.bin"
cp "$build_dir/esptool2/esptool2" "$CACHE_DIR/esptool2/esptool2"
size=$(wc -c < "$OUTPUT_DIR/rboot.bin")
if (( size > 4096 )); then
  echo "rBoot binary exceeds its 4 KiB sector: $size bytes" >&2
  exit 1
fi
sha256sum "$OUTPUT_DIR/rboot.bin"
