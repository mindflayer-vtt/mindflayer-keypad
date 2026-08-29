#!/usr/bin/env bash
set -euo pipefail
if [[ $# -ne 5 ]]; then
  echo "Usage: $0 <unsigned.bin> <private.pem> <hardware> <version> <output-dir>" >&2
  exit 2
fi
unsigned="$1"; private="$2"; hardware="$3"; version="$4"; output_dir="$5"
[[ "$hardware" =~ ^[A-Za-z0-9][A-Za-z0-9._-]{0,63}$ ]] || { echo "Invalid hardware ID" >&2; exit 2; }
[[ "$version" =~ ^[0-9]+\.[0-9]+\.[0-9]+([-+][0-9A-Za-z.-]+)?$ ]] || { echo "Invalid version" >&2; exit 2; }
repo_dir="$(cd "$(dirname "$0")/.." && pwd)"
signer="${ESP8266_SIGNING_SCRIPT:-$HOME/.platformio/packages/framework-arduinoespressif8266/tools/signing.py}"
mkdir -p "$output_dir/$hardware/$version"
artifact="$output_dir/$hardware/$version/firmware.bin.signed"
python3 "$signer" --mode sign --privatekey "$private" --bin "$unsigned" --out "$artifact"
size="$(stat -c %s "$artifact")"; digest="$(sha256sum "$artifact" | cut -d' ' -f1)"
printf '{\n  "version": 1,\n  "releases": [{\n    "hardware": "%s",\n    "version": "%s",\n    "artifact": "%s/%s/firmware.bin.signed",\n    "size": %s,\n    "sha256": "%s"\n  }]\n}\n' "$hardware" "$version" "$hardware" "$version" "$size" "$digest" > "$output_dir/manifest.json"
echo "Signed artifact and manifest written under $output_dir"
