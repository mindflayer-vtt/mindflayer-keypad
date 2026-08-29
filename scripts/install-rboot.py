#!/usr/bin/env python3
"""One-time serial rBoot migration, hard-locked to the Phase-1 USB topology/MAC."""
import argparse, hashlib, os, pathlib, re, subprocess, sys

EXPECTED_MAC='3c:61:05:cf:d1:54'
EXPECTED_TOPOLOGY='4.1'
PROVISIONING=((0x3f9000,'provisioning-a.bin'),(0x3fa000,'provisioning-b.bin'))

def run(python,port,*args,capture=False):
    command=[python,'-m','esptool','--chip','esp8266','--port',port,*map(str,args)]
    return subprocess.run(command,check=True,text=True,stdout=subprocess.PIPE if capture else None).stdout or ''

def assert_identity(python,port):
    path=pathlib.Path(port)
    if '/dev/serial/by-path/' not in str(path) or f'usb-0:{EXPECTED_TOPOLOGY}:' not in path.name:
        raise SystemExit(f'refusing non-{EXPECTED_TOPOLOGY} stable by-path port: {port}')
    tty=path.resolve().name; device=pathlib.Path('/sys/class/tty')/tty/'device'
    resolved=str(device.resolve())
    if f'-{EXPECTED_TOPOLOGY}/' not in resolved and f'-{EXPECTED_TOPOLOGY}:' not in resolved:
        raise SystemExit(f'sysfs topology mismatch: {resolved}')
    output=run(python,port,'read_mac',capture=True)
    match=re.search(r'MAC:\s*([0-9a-f:]{17})',output,re.I)
    if not match or match.group(1).lower()!=EXPECTED_MAC: raise SystemExit('Phase-1 MAC verification failed')

def digest(path): return hashlib.sha256(pathlib.Path(path).read_bytes()).hexdigest()

def main():
    p=argparse.ArgumentParser(); p.add_argument('--port',required=True);p.add_argument('--rboot',required=True)
    p.add_argument('--metadata-a',required=True);p.add_argument('--metadata-b',required=True);p.add_argument('--slot-a',required=True)
    p.add_argument('--backup-dir',required=True);p.add_argument('--python',default=sys.executable);a=p.parse_args()
    assert_identity(a.python,a.port); backup=pathlib.Path(a.backup_dir);backup.mkdir(parents=True,exist_ok=False)
    before=[]
    for address,name in PROVISIONING:
        target=backup/name;run(a.python,a.port,'read_flash',hex(address),'0x1000',target);before.append(digest(target))
    assert_identity(a.python,a.port)
    run(a.python,a.port,'write_flash','--flash_mode','dio','--flash_freq','40m','--flash_size','4MB',
        '0x000000',a.rboot,'0x001000',a.metadata_a,'0x100000',a.metadata_b,'0x002000',a.slot_a)
    after=[]
    for address,name in PROVISIONING:
        target=backup/('after-'+name);run(a.python,a.port,'read_flash',hex(address),'0x1000',target);after.append(digest(target))
    if before!=after: raise SystemExit('PROVISIONING CHANGED: stop and restore from validated backup')
    (backup/'sha256.txt').write_text('\n'.join(f'{value}  {name}' for value,(_,name) in zip(before,PROVISIONING))+'\n')
    print(f'rBoot migration complete; provisioning unchanged: {before[0]} {before[1]}')

if __name__=='__main__': main()
