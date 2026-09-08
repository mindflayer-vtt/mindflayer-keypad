#!/usr/bin/env python3
"""Verify that the distributable PEM is exactly the key compiled into firmware."""
import re
import base64
from pathlib import Path

root = Path(__file__).resolve().parent.parent
header = (root / "include" / "HardwareConfig.h").read_text()
match = re.search(
    r'#define FIRMWARE_SIGNING_PUBLIC_KEY_PEM\s+((?:\\?\s*"(?:[^"\\]|\\.)*"\s*)+)',
    header,
)
if not match:
    raise SystemExit("Unable to find FIRMWARE_SIGNING_PUBLIC_KEY_PEM")
parts = re.findall(r'"((?:[^"\\]|\\.)*)"', match.group(1))
compiled = bytes("".join(parts), "utf-8").decode("unicode_escape").encode()
published = (root / "keys" / "firmware-signing-public.pem").read_bytes()
def der(pem: bytes) -> bytes:
    lines = [line for line in pem.splitlines() if not line.startswith(b"-----")]
    return base64.b64decode(b"".join(lines), validate=True)

if der(compiled) != der(published):
    raise SystemExit("Published signing key differs from HardwareConfig.h")
print("verified published signing key matches firmware trust anchor")
