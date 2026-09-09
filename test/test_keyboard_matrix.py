"""Compile the real key scanner with deterministic GPIO and time doubles."""

import pathlib
import subprocess
import tempfile
import unittest

ROOT = pathlib.Path(__file__).resolve().parents[1]


class KeyboardMatrixTest(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.directory = tempfile.TemporaryDirectory()
        cls.addClassCleanup(cls.directory.cleanup)
        cls.binary = str(pathlib.Path(cls.directory.name) / "keyboard-matrix")
        subprocess.run([
            "g++", "-std=c++17", "-fsanitize=address,undefined",
            "-fno-omit-frame-pointer", f"-I{ROOT / 'test/host'}",
            f"-I{ROOT / 'lib/KeyboardMatrix'}",
            str(ROOT / "lib/KeyboardMatrix/KeyboardMatrix.cpp"),
            str(ROOT / "test/host/keyboard_matrix.cpp"), "-o", cls.binary,
        ], check=True)

    def test_debounced_gpio_scanning(self):
        for scenario in ("all-keys", "bounce", "glitches", "independent",
                         "snapshot", "wrap", "reinitialize"):
            with self.subTest(scenario=scenario):
                subprocess.run([self.binary, scenario], check=True, timeout=5)


if __name__ == "__main__":
    unittest.main()
