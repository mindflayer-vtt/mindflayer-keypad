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
public="${FIRMWARE_SIGNING_PUBLIC_KEY:-$repo_dir/keys/firmware-signing-public.pem}"

[[ -f "$unsigned" ]] || { echo "Unsigned firmware not found: $unsigned" >&2; exit 1; }
[[ -f "$private" ]] || { echo "Signing key not found: $private" >&2; exit 1; }
[[ -f "$signer" ]] || { echo "ESP8266 signing tool not found: $signer" >&2; exit 1; }
[[ -f "$public" ]] || { echo "Firmware signing public key not found: $public" >&2; exit 1; }

tmp="$(mktemp -d)"
trap 'rm -rf "$tmp"' EXIT

openssl pkey -in "$private" -pubout -out "$tmp/derived-public.pem" >/dev/null 2>&1
openssl pkey -pubin -in "$public" -outform DER -out "$tmp/expected.der"
openssl pkey -pubin -in "$tmp/derived-public.pem" -outform DER -out "$tmp/derived.der"
cmp -s "$tmp/expected.der" "$tmp/derived.der" || {
  echo "Signing private key does not match the public key compiled into the firmware" >&2
  exit 1
}

mkdir -p "$output_dir/$hardware/$version"

artifact="$output_dir/$hardware/$version/firmware.bin.signed"
python3 "$signer" --mode sign --privatekey "$private" --bin "$unsigned" --out "$artifact"

[[ -s "$artifact" ]] || { echo "Signing tool did not create an artifact" >&2; exit 1; }

python3 - "$artifact" "$unsigned" "$tmp/signature.bin" <<'PY'
import pathlib, struct, sys
signed, unsigned, signature = map(pathlib.Path, sys.argv[1:])
data = signed.read_bytes()
source = unsigned.read_bytes()
if len(data) < len(source) + 5 or data[:len(source)] != source:
    raise SystemExit("Signed artifact does not contain the exact unsigned image")
size = struct.unpack("<I", data[-4:])[0]
if size <= 0 or len(data) != len(source) + size + 4:
    raise SystemExit("Signed artifact has an invalid signature trailer")
signature.write_bytes(data[len(source):-4])
PY

openssl dgst -sha256 -verify "$public" -signature "$tmp/signature.bin" "$unsigned" >/dev/null

size="$(stat -c %s "$artifact")"; digest="$(sha256sum "$artifact" | cut -d' ' -f1)"

printf '{\n  "version": 1,\n  "releases": [{\n    "hardware": "%s",\n    "version": "%s",\n    "artifact": "%s/%s/firmware.bin.signed",\n    "size": %s,\n    "sha256": "%s"\n  }]\n}\n' "$hardware" "$version" "$hardware" "$version" "$size" "$digest" > "$output_dir/manifest.json"

echo "Signed artifact and manifest written under $output_dir"
