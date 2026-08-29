#!/usr/bin/env bash
set -euo pipefail
repo_dir="$(cd "$(dirname "$0")/.." && pwd)"
key_dir="${1:-$repo_dir/keys}"
mkdir -p "$key_dir"
chmod 700 "$key_dir"
if [[ -e "$key_dir/signing-private.pem" || -e "$key_dir/signing-public.pem" ]]; then
  echo "Refusing to overwrite an existing test signing key" >&2
  exit 1
fi
openssl genrsa -out "$key_dir/signing-private.pem" 2048
chmod 600 "$key_dir/signing-private.pem"
openssl rsa -in "$key_dir/signing-private.pem" -pubout -out "$key_dir/signing-public.pem"
echo "Generated local development signing keypair in $key_dir"
