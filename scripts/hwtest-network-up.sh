#!/usr/bin/env bash
set -euo pipefail
repo_dir="$(cd "$(dirname "$0")/.." && pwd)"; state_dir="$repo_dir/.hwtest"; mkdir -p "$state_dir"; chmod 700 "$state_dir"
wifi_device="${1:-$(nmcli -t -f DEVICE,TYPE device | awk -F: '$2=="wifi" {print $1; exit}')}"
[[ -n "$wifi_device" ]] || { echo "No NetworkManager Wi-Fi device found" >&2; exit 1; }
nmcli -t -f DEVICE,TYPE device | grep -qx "$wifi_device:wifi" || { echo "$wifi_device is not a NetworkManager Wi-Fi device" >&2; exit 1; }
profile="mindflayer-hwtest-$(openssl rand -hex 4)"; ssid="$profile"; password="$(openssl rand -base64 24 | tr -d '/+=' | head -c 24)"
printf 'DEVICE=%q\nPROFILE=%q\nSSID=%q\nPASSWORD=%q\n' "$wifi_device" "$profile" "$ssid" "$password" > "$state_dir/network.env"; chmod 600 "$state_dir/network.env"
nmcli radio wifi on
nmcli connection add type wifi ifname "$wifi_device" con-name "$profile" autoconnect no ssid "$ssid"
trap 'nmcli connection delete "$profile" >/dev/null 2>&1 || true' ERR
nmcli connection modify "$profile" 802-11-wireless.mode ap 802-11-wireless.band bg 802-11-wireless.channel 6 wifi-sec.key-mgmt wpa-psk wifi-sec.psk "$password" ipv4.method shared ipv6.method disabled
nmcli connection up "$profile"
address="$(ip -4 -o addr show dev "$wifi_device" scope global | awk '{print $4; exit}')"; [[ -n "$address" ]] || { echo "AP has no IPv4 address" >&2; exit 1; }
printf 'AP_CIDR=%q\n' "$address" >> "$state_dir/network.env"
echo "Temporary AP ready: profile=$profile device=$wifi_device address=$address"
echo "Credentials are stored only in $state_dir/network.env"
