import importlib.util
import pathlib
import unittest
from unittest.mock import patch


SCRIPT = pathlib.Path(__file__).parents[1] / "scripts" / "install-rboot.py"
SPEC = importlib.util.spec_from_file_location("install_rboot", SCRIPT)
install_rboot = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(install_rboot)


class InstallRbootIdentityTest(unittest.TestCase):
    def test_accepts_any_port_when_target_is_esp8266(self):
        with patch.object(
            install_rboot, "run", return_value="Chip is ESP8266EX"
        ) as run:
            install_rboot.assert_esp8266(
                "python", "esptool", "/dev/ttyUSB-arbitrary"
            )

        run.assert_called_once_with(
            "python",
            "esptool",
            "/dev/ttyUSB-arbitrary",
            "chip_id",
            capture=True,
        )

    def test_rejects_a_different_chip_family(self):
        with patch.object(install_rboot, "run", return_value="Chip is ESP32-S3"):
            with self.assertRaisesRegex(SystemExit, "did not identify as an ESP8266"):
                install_rboot.assert_esp8266(
                    "python", "esptool", "/dev/ttyACM-arbitrary"
                )


if __name__ == "__main__":
    unittest.main()
