#!/usr/bin/env python3
"""Convert an ESP8266 Arduino ELF to one rBoot-compatible V1 image."""

import argparse
import os
import struct
import subprocess


SECTIONS = (".irom0.text", ".text", ".text1", ".data", ".rodata")


def tool(path, name):
    return os.path.join(path, "xtensa-lx106-elf-" + name)


def section_info(elf, section, toolchain_bin):
    output = subprocess.check_output(
        [tool(toolchain_bin, "objdump"), "-h", elf], text=True
    )
    for line in output.splitlines():
        fields = line.split()
        if len(fields) >= 7 and fields[1] == section:
            return int(fields[2], 16), int(fields[3], 16)
    return None


def entry_point(elf, toolchain_bin):
    output = subprocess.check_output(
        [tool(toolchain_bin, "readelf"), "-h", elf], text=True
    )
    for line in output.splitlines():
        if "Entry point address:" in line:
            return int(line.split(":", 1)[1].strip(), 16)
    raise RuntimeError("ELF entry point not found")


def extract(elf, section, toolchain_bin, destination):
    subprocess.check_call(
        [tool(toolchain_bin, "objcopy"), "-O", "binary", "-j", section, elf, destination]
    )
    with open(destination, "rb") as stream:
        return stream.read()


def convert(elf, output, toolchain_bin):
    segments = []
    for section in SECTIONS:
        info = section_info(elf, section, toolchain_bin)
        if not info or info[0] == 0:
            continue
        temp = output + "." + section.strip(".")
        data = extract(elf, section, toolchain_bin, temp)
        os.unlink(temp)
        segments.append((info[1], data))

    irom = next((segment for segment in segments if segment[0] == 0x40202010), None)
    ram_segments = [segment for segment in segments if segment is not irom]
    if irom is None:
        raise RuntimeError(".irom0.text at 0x40202010 was not found")

    # SDK bootloader v1.2+ (rBoot "boot2") format: an EA/04 outer header and
    # zero-addressed IROM section precede the normal E9 RAM-segment image.
    checksum = 0xEF
    with open(output, "wb") as stream:
        entry = entry_point(elf, toolchain_bin)
        stream.write(struct.pack("<BBBBI", 0xEA, 0x04, 2, 0x40, entry))
        irom_data = irom[1]
        irom_padding = (-len(irom_data)) % 16
        stream.write(struct.pack("<II", 0, len(irom_data) + irom_padding))
        stream.write(irom_data)
        stream.write(b"\0" * irom_padding)
        for byte in irom_data:
            checksum ^= byte

        stream.write(struct.pack("<BBBBI", 0xE9, len(ram_segments), 2, 0x40, entry))
        for address, data in ram_segments:
            section_padding = (-len(data)) % 4
            stream.write(struct.pack("<II", address, len(data) + section_padding))
            stream.write(data)
            stream.write(b"\0" * section_padding)
            for byte in data:
                checksum ^= byte
        while (stream.tell() + 1) % 16:
            stream.write(b"\0")
        stream.write(bytes([checksum]))


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--elf", required=True)
    parser.add_argument("--output", required=True)
    parser.add_argument("--toolchain-bin", required=True)
    args = parser.parse_args()
    convert(args.elf, args.output, args.toolchain_bin)


if __name__ == "__main__":
    main()

