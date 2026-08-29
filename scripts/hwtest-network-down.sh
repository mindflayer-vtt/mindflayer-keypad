#!/usr/bin/env bash
set -euo pipefail
repo_dir="$(cd "$(dirname "$0")/.." && pwd)"; state="$repo_dir/.hwtest/network.env"
[[ -f "$state" ]] || { echo "No hardware-test network state found"; exit 0; }
# shellcheck disable=SC1090
source "$state"
[[ "$PROFILE" == mindflayer-hwtest-* ]] || { echo "Refusing to remove non-test profile" >&2; exit 1; }
nmcli connection down "$PROFILE" >/dev/null 2>&1 || true
nmcli connection delete "$PROFILE"
echo "Removed temporary AP profile $PROFILE; normal routes:"
ip route show default
