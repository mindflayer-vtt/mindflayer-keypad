#!/usr/bin/env python3
"""Reject a bootloader built without the required transactional metadata patch."""

import pathlib
import sys


def verify(data):
    if not 0 < len(data) <= 4096:
        raise ValueError("rBoot binary must fit its 4 KiB sector")
    if b"Using safe default boot config." not in data:
        raise ValueError("rBoot transactional metadata patch is missing")
    if b"Writing default boot config." in data:
        raise ValueError("rBoot contains the legacy destructive config fallback")


if __name__ == "__main__":
    if len(sys.argv) != 2:
        raise SystemExit("usage: verify-rboot-bootloader.py <rboot.bin>")
    try:
        verify(pathlib.Path(sys.argv[1]).read_bytes())
    except (OSError, ValueError) as error:
        raise SystemExit(str(error))
    print("verified rBoot size and transactional metadata build marker")
