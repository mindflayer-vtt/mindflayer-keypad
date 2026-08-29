#!/usr/bin/env bash
set -euo pipefail

RBOOT_COMMIT=614f33685d0dd990fc4202f2409b0d2365eeaef3
ESPTOOL2_COMMIT=91759a7c4faec1b7bc8410166d22ed92eb90556c
PROJECT_DIR=$(cd "$(dirname "$0")/.." && pwd)
CACHE_DIR="$PROJECT_DIR/.pio/rboot-upstream"
OUTPUT_DIR="$PROJECT_DIR/.pio/rboot-artifacts"
TOOLCHAIN_DIR=${PLATFORMIO_PACKAGES_DIR:-$HOME/.platformio/packages}/toolchain-xtensa/bin

mkdir -p "$CACHE_DIR" "$OUTPUT_DIR"
if [[ ! -d "$CACHE_DIR/rboot/.git" ]]; then
  git clone https://github.com/raburton/rboot.git "$CACHE_DIR/rboot"
fi
if [[ ! -d "$CACHE_DIR/esptool2/.git" ]]; then
  git clone https://github.com/raburton/esptool2.git "$CACHE_DIR/esptool2"
fi
git -C "$CACHE_DIR/rboot" checkout --detach "$RBOOT_COMMIT"
git -C "$CACHE_DIR/rboot" reset --hard "$RBOOT_COMMIT"
git -C "$CACHE_DIR/rboot" apply "$PROJECT_DIR/scripts/rboot-metadata.patch"
git -C "$CACHE_DIR/esptool2" checkout --detach "$ESPTOOL2_COMMIT"
make -C "$CACHE_DIR/esptool2"
make -C "$CACHE_DIR/rboot" clean all \
  XTENSA_BINDIR="$TOOLCHAIN_DIR" \
  ESPTOOL2="$CACHE_DIR/esptool2/esptool2" \
  RBOOT_BIG_FLASH=1 RBOOT_RTC_ENABLED=1 RBOOT_CONFIG_CHKSUM=1 RBOOT_IROM_CHKSUM=1 \
  RBOOT_INTEGRATION=1 RBOOT_EXTRA_INCDIR="$PROJECT_DIR/scripts" \
  SPI_SIZE=4M SPI_MODE=dio SPI_SPEED=40
cp "$CACHE_DIR/rboot/firmware/rboot.bin" "$OUTPUT_DIR/rboot.bin"
size=$(wc -c < "$OUTPUT_DIR/rboot.bin")
if (( size > 4096 )); then
  echo "rBoot binary exceeds its 4 KiB sector: $size bytes" >&2
  exit 1
fi
sha256sum "$OUTPUT_DIR/rboot.bin"
