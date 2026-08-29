#!/usr/bin/env python3
"""Generate the checked rBoot configuration sector for the experiment."""

import argparse
import os
import struct
import zlib


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("output")
    parser.add_argument("--slot", choices=("a", "b"), default="a")
    parser.add_argument("--generation", type=int, default=1)
    parser.add_argument("--state", choices=("committed", "erased", "partial-header",
                                            "partial-payload", "uncommitted", "bad-crc"),
                        default="committed")
    args = parser.parse_args()

    selected = 0 if args.slot == "a" else 1
    config = bytearray(struct.pack("<BBBBBBBBIIII", 0xE1, 0x01, 0, selected, 0, 2, 0, 0,
                                   0x002000, 0x202000, 0, 0))
    checksum = 0xEF
    for byte in config:
        checksum ^= byte
    config.append(checksum)
    record = bytearray(struct.pack("<III", 0x4D465242, 1, args.generation)) + config
    crc = zlib.crc32(record)
    if args.state == "bad-crc":
        crc ^= 1
    record.extend(struct.pack("<I", crc))
    record.extend(b"\xff" * (0x1000 - len(record)))
    if args.state == "committed" or args.state == "bad-crc":
        record[-4:] = struct.pack("<I", 0x434D4954)
    if args.state == "erased":
        record[:] = b"\xff" * len(record)
    elif args.state == "partial-header":
        record[8:] = b"\xff" * (len(record) - 8)
    elif args.state == "partial-payload":
        record[20:] = b"\xff" * (len(record) - 20)
    os.makedirs(os.path.dirname(os.path.abspath(args.output)), exist_ok=True)
    with open(args.output, "wb") as stream:
        stream.write(record)


if __name__ == "__main__":
    main()
