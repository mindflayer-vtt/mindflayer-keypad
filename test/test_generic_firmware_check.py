import pathlib
import shutil
import subprocess
import tempfile
import unittest


SCRIPT = pathlib.Path(__file__).parents[1] / "scripts/prove-generic-firmware.sh"


class GenericFirmwareCheckTest(unittest.TestCase):
    def setUp(self):
        self.directory = tempfile.TemporaryDirectory()
        self.addCleanup(self.directory.cleanup)
        self.root = pathlib.Path(self.directory.name)
        for name in ("scripts", "src", "lib", "include", "docs", ".venv/bin"):
            (self.root / name).mkdir(parents=True, exist_ok=True)
        shutil.copyfile(SCRIPT, self.root / "scripts/prove-generic-firmware.sh")
        (self.root / "include/HardwareConfig.h").write_text(
            "#define NEOPIXEL_DATA_PIN 3\n"
        )
        (self.root / "src/LedController.cpp").write_text(
            "NeoPixelBus<NeoGrbFeature, Neo800KbpsMethod>\n"
        )
        for name in (
            "platformio.ini",
            "README.md",
            "docs/PROVISIONING.md",
            "docs/DEVICE_PROTOCOL.md",
        ):
            (self.root / name).touch()
        # Reaching this stub proves the source guards allowed compilation.
        pio = self.root / ".venv/bin/pio"
        pio.write_text("#!/bin/sh\nexit 77\n")
        pio.chmod(0o700)

    def run_check(self):
        return subprocess.run(
            ["bash", str(self.root / "scripts/prove-generic-firmware.sh")],
            capture_output=True,
            text=True,
            timeout=10,
        )

    def test_generic_sources_reach_build(self):
        self.assertEqual(self.run_check().returncode, 77)

    def test_installation_specific_sources_stop_before_build(self):
        for source in (
            '#define WIFI_SSID "example"\n',
            '#define DEVICE_SECRET_HEX "example"\n',
            '#include "config.h"\n',
            '-DDEVICE_SECRET=example\n',
        ):
            with self.subTest(source=source):
                (self.root / "src/Installation.cpp").write_text(source)
                result = self.run_check()
                self.assertEqual(result.returncode, 1)
                self.assertIn("Installation-specific build input found", result.stderr)

    def test_scan_failure_stops_before_build(self):
        (self.root / "platformio.ini").unlink()
        result = self.run_check()
        self.assertEqual(result.returncode, 2)
        self.assertIn("Unable to scan for installation-specific build inputs", result.stderr)


if __name__ == "__main__":
    unittest.main()
