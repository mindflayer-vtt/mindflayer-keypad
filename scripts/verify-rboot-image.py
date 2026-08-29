#!/usr/bin/env python3
"""Compare the production boot2 image byte-for-byte with pinned esptool2."""
import argparse
import os
import subprocess
import tempfile

def main():
    parser=argparse.ArgumentParser()
    parser.add_argument('--elf',required=True); parser.add_argument('--image',required=True)
    parser.add_argument('--esptool2',required=True)
    args=parser.parse_args()
    with tempfile.TemporaryDirectory() as directory:
        reference=os.path.join(directory,'reference.bin')
        subprocess.check_call([args.esptool2,'-quiet','-bin','-boot2','-iromchksum','-dio','-40','-4096',
                               args.elf,reference,'.text','.text1','.data','.rodata'])
        with open(args.image,'rb') as actual, open(reference,'rb') as expected:
            if actual.read()!=expected.read(): raise SystemExit('boot2 image differs from pinned esptool2')
    print('verified boot2 image byte-for-byte against pinned esptool2')

if __name__=='__main__': main()
