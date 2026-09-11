#!/usr/bin/env python3
"""Package validated serial images with a detached, trust-anchor-verified signature."""
import argparse
import hashlib
import importlib.util
import io
import json
import os
from pathlib import Path
import re
import subprocess
import sys
import tarfile
import tempfile
from types import SimpleNamespace

ROOT = Path(__file__).resolve().parents[1]


def build(version, rboot, application, private_key, public_key, output):
    if not re.fullmatch(r"[0-9]+\.[0-9]+\.[0-9]+(?:-[0-9A-Za-z.-]+)?", version):
        raise ValueError("Invalid release version")
    spec = importlib.util.spec_from_file_location("serial_installer", ROOT / "scripts/install-rboot.py")
    installer = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(installer)
    output = Path(output)
    output.parent.mkdir(parents=True, exist_ok=True)
    with tempfile.TemporaryDirectory(prefix="serial-bundle-") as directory:
        stage = Path(directory)
        for name, extra in (("metadata-a.bin", []), ("metadata-b.bin", ["--state", "erased"])):
            subprocess.run([sys.executable, str(ROOT / "scripts/make-rboot-config.py"), str(stage / name), *extra], check=True)
        images = installer.preflight(SimpleNamespace(rboot=rboot, slot_a=application,
            metadata_a=stage / "metadata-a.bin", metadata_b=stage / "metadata-b.bin"))
        names = ("rboot.bin", "metadata-a.bin", "metadata-b.bin", "application.bin")
        files = dict(zip(names, images))
        files["install-rboot.py"] = (ROOT / "scripts/install-rboot.py").read_bytes()
        files["LICENSE"] = (ROOT / "LICENSE").read_bytes()
        addresses = dict(zip(names, (0, 0x1000, 0x100000, 0x2000)))
        manifest = {"format": 1, "type": "mindflayer-serial-install", "version": version,
            "hardware": "mindflayer-keypad-v1", "chip": "esp8266", "flashSize": 0x400000, "deviceProtocol": 3,
            "preserveSectors": [0x3f9000, 0x3fa000],
            "files": [{"path": name, "size": len(data), "sha256": hashlib.sha256(data).hexdigest(),
                       **({"address": addresses[name]} if name in addresses else {})}
                      for name, data in files.items()]}
        encoded = (json.dumps(manifest, sort_keys=True, separators=(",", ":")) + "\n").encode()
        (stage / "manifest.json").write_bytes(encoded)
        signature = stage / "manifest.sig"
        subprocess.run(["openssl", "dgst", "-sha256", "-sign", str(private_key), "-out", str(signature), str(stage / "manifest.json")], check=True, capture_output=True)
        # Fail closed if CI supplied a different private key. Consumers must use
        # their pinned public key, never a key embedded in the downloaded archive.
        subprocess.run(["openssl", "dgst", "-sha256", "-verify", str(public_key), "-signature", str(signature), str(stage / "manifest.json")], check=True, capture_output=True)
        if signature.stat().st_size != 256:
            raise ValueError("Serial bundle format 1 requires an RSA-2048 signature")
        files.update({"manifest.json": encoded, "manifest.sig": signature.read_bytes()})
        fd, temporary = tempfile.mkstemp(prefix=".serial-", dir=output.parent)
        try:
            with os.fdopen(fd, "wb") as stream:
                with tarfile.open(fileobj=stream, mode="w:gz") as archive:
                    for name, data in files.items():
                        entry = tarfile.TarInfo(name)
                        entry.size, entry.mode = len(data), 0o644
                        archive.addfile(entry, io.BytesIO(data))
                stream.flush()
                os.fsync(stream.fileno())
            os.replace(temporary, output)
        finally:
            Path(temporary).unlink(missing_ok=True)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("version")
    parser.add_argument("--rboot", required=True)
    parser.add_argument("--application", required=True)
    parser.add_argument("--private-key", required=True)
    parser.add_argument("--public-key", default=ROOT / "keys/firmware-signing-public.pem")
    parser.add_argument("--output", required=True)
    args = parser.parse_args()
    build(args.version, args.rboot, args.application, args.private_key, args.public_key, args.output)


if __name__ == "__main__":
    main()
