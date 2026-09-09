"""Host regression tests compile the real application, not a copy of its logic."""

import pathlib
import subprocess
import tempfile
import unittest

ROOT = pathlib.Path(__file__).resolve().parents[1]


class ApplicationDeadlineTest(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.directory = tempfile.TemporaryDirectory()
        cls.addClassCleanup(cls.directory.cleanup)
        cls.binary = str(pathlib.Path(cls.directory.name) / "application-deadline")
        includes = [ROOT / "test/host/application", ROOT / "test/host", ROOT / "src", ROOT / "include"]
        includes.extend(path for path in (ROOT / "lib").iterdir() if path.is_dir())
        subprocess.run([
            "g++", "-std=c++17", "-DARDUINO", "-DRBOOT_INTEGRATION",
            "-fsanitize=address,undefined", "-fno-omit-frame-pointer",
            *(f"-I{path}" for path in includes),
            str(ROOT / "src/Application.cpp"),
            str(ROOT / "src/LedController.cpp"),
            str(ROOT / "test/host/application_deadline.cpp"), "-o", cls.binary,
        ], check=True)

    def test_temporary_deadline_covers_every_startup_and_poll_failure(self):
        for scenario in ("wifi-unavailable", "provisioning-failure", "invalid-public-key",
                         "blocked-connect", "blocked-poll", "permanent"):
            with self.subTest(scenario=scenario):
                subprocess.run([self.binary, scenario], check=True, timeout=5)

    def test_restart_shortcut_requires_shift_space_e(self):
        subprocess.run([self.binary, "restart-shortcut"], check=True, timeout=5)

    def test_connection_status_leds(self):
        subprocess.run([self.binary, "status-leds"], check=True, timeout=5)


if __name__ == "__main__":
    unittest.main()
