#!/usr/bin/env python3
"""Install rBoot on an ESP8266 while preserving its provisioning sectors."""

import argparse
import hashlib
import os
import pathlib
import re
import subprocess
import sys


PROVISIONING = (
    (0x3F9000, "provisioning-a.bin"),
    (0x3FA000, "provisioning-b.bin"),
)


def run(python, esptool, port, *args, capture=False):
    command = [python, esptool, "--chip", "esp8266", "--port", port, *map(str, args)]
    result = subprocess.run(
        command,
        check=True,
        text=True,
        stdout=subprocess.PIPE if capture else None,
    )
    return result.stdout or ""


def assert_esp8266(python, esptool, port):
    output = run(python, esptool, port, "chip_id", capture=True)
    if not re.search(r"\bESP8266(?:EX)?\b", output, re.IGNORECASE):
        raise SystemExit("connected target did not identify as an ESP8266")


def digest(path):
    return hashlib.sha256(pathlib.Path(path).read_bytes()).hexdigest()


def main():
    parser = argparse.ArgumentParser(
        description="One-time rBoot installation for any serial-connected ESP8266"
    )
    parser.add_argument("--port", required=True)
    parser.add_argument("--rboot", required=True)
    parser.add_argument("--metadata-a", required=True)
    parser.add_argument("--metadata-b", required=True)
    parser.add_argument("--slot-a", required=True)
    parser.add_argument("--backup-dir", required=True)
    parser.add_argument("--python", default=sys.executable)
    parser.add_argument(
        "--esptool",
        default=os.path.expanduser("~/.platformio/packages/tool-esptoolpy/esptool.py"),
    )
    args = parser.parse_args()

    assert_esp8266(args.python, args.esptool, args.port)
    backup = pathlib.Path(args.backup_dir)
    backup.mkdir(parents=True, exist_ok=False)

    before = []
    for address, name in PROVISIONING:
        target = backup / name
        run(
            args.python,
            args.esptool,
            args.port,
            "read_flash",
            hex(address),
            "0x1000",
            target,
        )
        before.append(digest(target))

    assert_esp8266(args.python, args.esptool, args.port)
    run(
        args.python,
        args.esptool,
        args.port,
        "write_flash",
        "--flash_mode",
        "dio",
        "--flash_freq",
        "40m",
        "--flash_size",
        "4MB",
        "0x000000",
        args.rboot,
        "0x001000",
        args.metadata_a,
        "0x100000",
        args.metadata_b,
        "0x002000",
        args.slot_a,
    )

    after = []
    for address, name in PROVISIONING:
        target = backup / ("after-" + name)
        run(
            args.python,
            args.esptool,
            args.port,
            "read_flash",
            hex(address),
            "0x1000",
            target,
        )
        after.append(digest(target))

    if before != after:
        raise SystemExit(
            "PROVISIONING CHANGED: stop and restore from the validated backup"
        )

    (backup / "sha256.txt").write_text(
        "\n".join(
            f"{value}  {name}"
            for value, (_, name) in zip(before, PROVISIONING)
        )
        + "\n"
    )
    print(
        "rBoot installation complete; provisioning unchanged: "
        f"{before[0]} {before[1]}"
    )


if __name__ == "__main__":
    main()
