"""Exercise the same downloaded transport sources compiled into production."""

import os
import configparser
import pathlib
import subprocess
import tempfile
import unittest

ROOT = pathlib.Path(__file__).resolve().parents[1]


class WebsocketTransportTest(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.directory = tempfile.TemporaryDirectory()
        cls.addClassCleanup(cls.directory.cleanup)
        cls.binary = str(pathlib.Path(cls.directory.name) / "websocket-transport")
        library = pathlib.Path(os.environ.get(
            "WEBSOCKETS_TEST_SOURCE", ROOT / ".pio/libdeps/keypad/ArduinoWebsockets/src"))
        if not library.is_dir():
            raise RuntimeError("Build keypad first to install and patch the pinned WebSocket library")
        config = configparser.ConfigParser()
        config.read(ROOT / "platformio.ini")
        limit_flags = [flag for flag in config["esp8266"]["build_flags"].split()
                       if flag.startswith("-D_WS_CONFIG_MAX_MESSAGE_SIZE=")]
        if not limit_flags:
            raise RuntimeError("Production WebSocket allocation limit is not configured")
        subprocess.run([
            "g++", "-std=c++17", "-D_WS_CONFIG_NO_SSL", "-DWSDefaultTcpClient=FakeTcpClient",
            "-fsanitize=address,undefined", "-fno-omit-frame-pointer",
            *limit_flags,
            f"-I{ROOT / 'test/host'}", f"-I{library}",
            "-include", str(ROOT / "test/host/FakeTcpClient.h"),
            str(ROOT / "test/host/websocket_transport.cpp"),
            *(str(library / name) for name in (
                "websockets_endpoint.cpp", "websockets_client.cpp", "message.cpp", "ws_common.cpp", "crypto.cpp")),
            "-o", cls.binary,
        ], check=True)

    def test_bounded_transport(self):
        for scenario in ("exact-limit", "oversized", "oversized-64", "fragments-at-limit",
                         "fragments-oversized", "batched", "partial-reads", "stalled-body", "stalled-header"):
            with self.subTest(scenario=scenario):
                subprocess.run([self.binary, scenario], check=True, timeout=5)


if __name__ == "__main__":
    unittest.main()
