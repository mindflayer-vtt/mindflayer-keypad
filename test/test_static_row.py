"""Check the separate electrical diagnostic's actual GPIO setup."""

import pathlib
import subprocess
import tempfile
import unittest

ROOT = pathlib.Path(__file__).resolve().parents[1]


class StaticRowTest(unittest.TestCase):
    def test_only_zxc_row_is_low_and_columns_keep_pullups(self):
        with tempfile.TemporaryDirectory() as directory:
            binary = str(pathlib.Path(directory) / "static-row")
            subprocess.run([
                "g++", "-std=c++17", "-fsanitize=address,undefined",
                "-fno-omit-frame-pointer", f"-I{ROOT / 'test/host'}",
                str(ROOT / "test/host/static_row.cpp"), "-o", binary,
            ], check=True)
            subprocess.run([binary], check=True, timeout=5)


if __name__ == "__main__":
    unittest.main()

