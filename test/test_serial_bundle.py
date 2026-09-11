import hashlib
import importlib.util
import json
from pathlib import Path
import subprocess
import tarfile
import tempfile
import unittest

from test_install_rboot import ram_image, boot2_image

ROOT = Path(__file__).resolve().parents[1]
SPEC = importlib.util.spec_from_file_location("serial_bundle", ROOT / "scripts/build-serial-bundle.py")
bundle = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(bundle)


class SerialBundleTests(unittest.TestCase):
    def test_signed_roundtrip_and_rejection_preserve_previous_artifact(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            private, public = root / "private.pem", root / "public.pem"
            subprocess.run(["openssl", "genpkey", "-algorithm", "RSA", "-pkeyopt", "rsa_keygen_bits:2048", "-out", str(private)], check=True, capture_output=True)
            subprocess.run(["openssl", "pkey", "-in", str(private), "-pubout", "-out", str(public)], check=True, capture_output=True)
            rboot, app, output = root / "rboot.bin", root / "app.bin", root / "bundle.tar.gz"
            rboot.write_bytes(ram_image())
            app.write_bytes(boot2_image())
            bundle.build("1.2.3", rboot, app, private, public, output)
            original = output.read_bytes()
            with tarfile.open(output) as archive:
                self.assertTrue(all(member.isfile() for member in archive))
                data = {member.name: archive.extractfile(member).read() for member in archive}
            manifest = json.loads(data["manifest.json"])
            self.assertEqual(manifest["preserveSectors"], [0x3f9000, 0x3fa000])
            self.assertEqual(manifest["version"], "1.2.3")
            self.assertEqual({entry["path"]: entry["address"] for entry in manifest["files"] if "address" in entry},
                             {"rboot.bin": 0, "metadata-a.bin": 0x1000, "metadata-b.bin": 0x100000, "application.bin": 0x2000})
            for entry in manifest["files"]:
                self.assertEqual(entry["size"], len(data[entry["path"]]))
                self.assertEqual(entry["sha256"], hashlib.sha256(data[entry["path"]]).hexdigest())
            (root / "manifest.json").write_bytes(data["manifest.json"])
            (root / "manifest.sig").write_bytes(data["manifest.sig"])
            subprocess.run(["openssl", "dgst", "-sha256", "-verify", str(public), "-signature", str(root / "manifest.sig"), str(root / "manifest.json")], check=True, capture_output=True)
            with self.assertRaises(subprocess.CalledProcessError):
                bundle.build("1.2.3", rboot, app, private, ROOT / "keys/firmware-signing-public.pem", output)
            self.assertEqual(output.read_bytes(), original)
            app.write_bytes(boot2_image() + b"signed OTA trailer")
            with self.assertRaises(SystemExit):
                bundle.build("1.2.3", rboot, app, private, public, output)
            self.assertEqual(output.read_bytes(), original)


if __name__ == "__main__":
    unittest.main()
