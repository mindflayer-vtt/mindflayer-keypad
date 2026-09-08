#!/usr/bin/env python3
"""Install rBoot on an ESP8266 while preserving its provisioning sectors."""

import argparse
import hashlib
import os
import pathlib
import re
import struct
import subprocess
import sys
import tempfile
import zlib

SECTOR_SIZE = 0x1000
SLOT_A_START = 0x002000
SLOT_A_END = 0x100000
SLOT_B_START = 0x202000
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


def assert_flash_size(python, esptool, port):
    output = run(python, esptool, port, "flash_id", capture=True)
    sizes = re.findall(r"^Detected flash size:\s*(\S+)\s*$", output, re.MULTILINE)
    if sizes != ["4MB"]:
        raise SystemExit("connected target must have exactly 4 MiB of detected flash")


def require(condition, message):
    if not condition:
        raise SystemExit(message)


def read_bounded(path, maximum, exact=False):
    path = pathlib.Path(path)
    require(path.is_file(), f"{path}: expected a regular file")
    with path.open("rb") as stream:
        data = stream.read(maximum + 1)
    require(
        len(data) == maximum if exact else 0 < len(data) <= maximum,
        f"{path}: invalid size (expected {'exactly' if exact else '1..'} {maximum} bytes)",
    )
    return data


def xor_checksum(data, initial=0xEF):
    for byte in data:
        initial ^= byte
    return initial


def validate_image(data, *, boot2):
    """Validate the pinned ESP8266 E9/boot2 formats, including IROM checksum."""
    offset = 0
    checksum = 0xEF
    outer_entry = None
    if boot2:
        require(
            len(data) >= 16 and data[:4] == b"\xea\x04\x02\x40",
            "slot A: expected unsigned DIO/40 MHz/4 MiB boot2 image",
        )
        outer_entry = struct.unpack_from("<I", data, 4)[0]
        address, length = struct.unpack_from("<II", data, 8)
        require(
            address == 0 and 0 < length <= len(data) - 16,
            "slot A: invalid IROM segment",
        )
        checksum = xor_checksum(data[16 : 16 + length], checksum)
        offset = (16 + length + 15) & ~15

    require(offset + 8 <= len(data), "image: truncated RAM header")
    magic, count, mode, size_freq, entry = struct.unpack_from("<BBBBI", data, offset)
    require(
        magic == 0xE9 and 1 <= count <= 16 and mode == 2 and size_freq == 0x40,
        "image: invalid ESP8266 RAM header or flash settings",
    )
    require(
        0x40100000 <= entry < 0x40110000
        and (outer_entry is None or outer_entry == entry),
        "image: invalid entry point",
    )
    offset += 8
    segments = []
    for _ in range(count):
        require(offset + 8 <= len(data), "image: truncated segment header")
        address, length = struct.unpack_from("<II", data, offset)
        offset += 8
        require(0 < length <= len(data) - offset, "image: invalid segment length")
        end = address + length
        require(
            (0x40100000 <= address < end <= 0x40110000)
            or (0x3FFE8000 <= address < end <= 0x40000000),
            "image: segment outside ESP8266 RAM",
        )
        require(
            all(end <= start or address >= stop for start, stop in segments),
            "image: overlapping RAM segments",
        )
        segments.append((address, end))
        checksum = xor_checksum(data[offset : offset + length], checksum)
        offset += length
    require(
        any(start <= entry < end for start, end in segments),
        "image: entry point is not in a loaded segment",
    )
    checksum_offset = ((offset + 16) & ~15) - 1
    require(
        checksum_offset == len(data) - 1,
        "image: truncated checksum, trailing data, or signed OTA artifact",
    )
    require(data[checksum_offset] == checksum, "image: checksum mismatch")


def validate_metadata(data, *, allow_erased):
    require(len(data) == SECTOR_SIZE, "metadata: expected exactly one sector")
    if allow_erased and data == b"\xff" * SECTOR_SIZE:
        return
    magic, version, _generation = struct.unpack_from("<III", data)
    require(
        magic == 0x4D465242
        and version == 1
        and data[-4:] == struct.pack("<I", 0x434D4954),
        "metadata: expected a committed version-1 record",
    )
    require(
        struct.unpack_from("<I", data, 37)[0] == zlib.crc32(data[:37]),
        "metadata: record CRC mismatch",
    )
    config = data[12:36]
    require(data[36] == xor_checksum(config), "metadata: config checksum mismatch")
    expected = struct.pack(
        "<BBBBBBBBIIII", 0xE1, 1, 0, 0, 0, 2, 0, 0, SLOT_A_START, SLOT_B_START, 0, 0
    )
    require(
        config == expected, "metadata: must select slot A with the production layout"
    )
    require(
        data[41:-4] == b"\xff" * (SECTOR_SIZE - 45),
        "metadata: unexpected data in reserved space",
    )


def preflight(args):
    images = [
        read_bounded(args.rboot, SECTOR_SIZE),
        read_bounded(args.metadata_a, SECTOR_SIZE, exact=True),
        read_bounded(args.metadata_b, SECTOR_SIZE, exact=True),
        read_bounded(args.slot_a, SLOT_A_END - SLOT_A_START),
    ]
    validate_image(images[0], boot2=False)
    validate_metadata(images[1], allow_erased=False)
    validate_metadata(images[2], allow_erased=True)
    validate_image(images[3], boot2=True)
    return images


def backup_sector(args, address, target):
    # Provisioning contains credentials: create privately before esptool opens it.
    target.touch(mode=0o600, exist_ok=False)
    run(
        args.python,
        args.esptool,
        args.port,
        "read_flash",
        hex(address),
        "0x1000",
        target,
    )
    data = read_bounded(target, SECTOR_SIZE, exact=True)
    with target.open("rb") as stream:
        os.fsync(stream.fileno())
    return hashlib.sha256(data).hexdigest()


def install(args, images, backup):
    # Flash the validated snapshot, not paths that could change during backup.
    with tempfile.TemporaryDirectory(prefix="mindflayer-rboot-") as staging:
        staged = []
        for name, data in zip(
            ("rboot.bin", "metadata-a.bin", "metadata-b.bin", "slot-a.bin"), images
        ):
            path = pathlib.Path(staging) / name
            path.write_bytes(data)
            staged.append(path)

        assert_esp8266(args.python, args.esptool, args.port)
        assert_flash_size(args.python, args.esptool, args.port)
        backup.mkdir(mode=0o700, parents=True, exist_ok=False)
        before = [
            backup_sector(args, address, backup / name)
            for address, name in PROVISIONING
        ]
        manifest = backup / "sha256.txt"
        manifest.touch(mode=0o600, exist_ok=False)
        with manifest.open("w") as stream:
            stream.write(
                "".join(
                    f"{value}  {name}\n"
                    for value, (_, name) in zip(before, PROVISIONING)
                )
            )
            stream.flush()
            os.fsync(stream.fileno())

        assert_esp8266(args.python, args.esptool, args.port)
        assert_flash_size(args.python, args.esptool, args.port)
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
            staged[0],
            "0x001000",
            staged[1],
            "0x100000",
            staged[2],
            "0x002000",
            staged[3],
        )

        after = [
            backup_sector(args, address, backup / ("after-" + name))
            for address, name in PROVISIONING
        ]
        require(
            before == after,
            "PROVISIONING CHANGED: stop and restore from the validated backup",
        )
        print(
            f"rBoot installation complete; provisioning unchanged: {before[0]} {before[1]}"
        )


def main(argv=None):
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
    args = parser.parse_args(argv)
    images = preflight(args)
    backup = pathlib.Path(args.backup_dir)
    require(not backup.exists(), f"backup directory already exists: {backup}")
    try:
        install(args, images, backup)
    except (OSError, subprocess.CalledProcessError):
        print(
            f"Installation failed; retain any provisioning backups in {backup}",
            file=sys.stderr,
        )
        raise


if __name__ == "__main__":
    main()
