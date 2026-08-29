#!/usr/bin/env bash
set -euo pipefail
if [[ $# -ne 5 ]]; then
  echo "Usage: $0 <ssid> <wifi-password> <wss-url> <device-secret-hex> <server-public.pem>" >&2
  exit 2
fi
repo_dir="$(cd "$(dirname "$0")/.." && pwd)"; output="$repo_dir/include/config.h"
ssid="$1" password="$2" wss_url="$3" secret="$4" server_key="$5" signing_key="$repo_dir/keys/signing-public.pem"
[[ "$secret" =~ ^[0-9a-fA-F]{64}$ ]] || { echo "Device secret must be 64 hexadecimal characters" >&2; exit 2; }
[[ -f "$server_key" && -f "$signing_key" ]] || { echo "Public key file missing" >&2; exit 2; }
escape_c() { sed 's/\\/\\\\/g; s/"/\\"/g; s/$/\\n/' "$1" | tr -d '\n'; }
escape_value() { printf '%s' "$1" | sed 's/\\/\\\\/g; s/"/\\"/g'; }
umask 077
{
  printf '#define WIFI_SSID "%s"\n' "$(escape_value "$ssid")"
  printf '#define WIFI_PASS "%s"\n' "$(escape_value "$password")"
  printf '#define WSS_URL "%s"\n' "$(escape_value "$wss_url")"
  printf '#define DEVICE_SECRET_HEX "%s"\n' "$secret"
  printf '#define SERVER_PUBLIC_KEY_PEM "%s"\n' "$(escape_c "$server_key")"
  printf '#define FIRMWARE_SIGNING_PUBLIC_KEY_PEM "%s"\n' "$(escape_c "$signing_key")"
} > "$output"
echo "Wrote ignored local configuration to $output"
