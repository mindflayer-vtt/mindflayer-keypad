#!/usr/bin/env python3
"""Fail an rBoot build if its large-flash mapping contract drifts."""

import argparse
import re
import subprocess


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--elf", required=True)
    parser.add_argument("--map", required=True)
    parser.add_argument("--toolchain-bin", required=True)
    args = parser.parse_args()
    prefix = args.toolchain_bin + "/xtensa-lx106-elf-"
    symbols = subprocess.check_output([prefix + "nm", "-A", args.elf], text=True)
    definitions = [line for line in symbols.splitlines()
                   if re.search(r"\b[0-9a-fA-F]+\s+[Tt]\s+Cache_Read_Enable_New$", line)]
    if len(definitions) != 1:
        raise SystemExit(f"expected one strong Cache_Read_Enable_New, found {len(definitions)}")
    sections = subprocess.check_output([prefix + "objdump", "-h", args.elf], text=True)
    irom = next((line.split() for line in sections.splitlines() if ".irom0.text" in line), None)
    if not irom or int(irom[3], 16) != 0x40202010:
        raise SystemExit(".irom0.text VMA is not 0x40202010")
    with open(args.map, encoding="utf-8") as stream:
        link_map = stream.read()
    if not re.search(r"libBootControl\.a\(rboot_bigflash\.cpp\.o\).*Cache_Read_Enable_New|"
                     r"Cache_Read_Enable_New.*libBootControl\.a\(rboot_bigflash\.cpp\.o\)",
                     link_map, re.S):
        raise SystemExit("Cache_Read_Enable_New is not owned by rboot_bigflash.cpp.o")
    print("verified rBoot ELF mapping symbol and 0x40202010 IROM VMA")


if __name__ == "__main__":
    main()

