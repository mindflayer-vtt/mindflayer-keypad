import hashlib
import importlib.util
import io
import pathlib
import re
import struct
import subprocess
import sys
import tempfile
import unittest
import zlib
from unittest.mock import patch

SCRIPT = pathlib.Path(__file__).parents[1] / "scripts" / "install-rboot.py"
SPEC = importlib.util.spec_from_file_location("install_rboot", SCRIPT)
install_rboot = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(install_rboot)


def checksum(data, initial=0xEF):
    for byte in data:
        initial ^= byte
    return initial


def ram_image(payload=b"\x01\x02\x03\x04", initial=0xEF):
    image = (
        struct.pack("<BBBBIII", 0xE9, 1, 2, 0x40, 0x40100000, 0x40100000, len(payload))
        + payload
    )
    return (
        image + b"\x00" * (15 - len(image) % 16) + bytes([checksum(payload, initial)])
    )


def boot2_image(irom=b"\x05" * 16):
    return (
        struct.pack("<BBBBIII", 0xEA, 4, 2, 0x40, 0x40100000, 0, len(irom))
        + irom
        + ram_image(initial=checksum(irom))
    )


def replace_u32(data, offset, value):
    result = bytearray(data)
    struct.pack_into("<I", result, offset, value)
    return bytes(result)


class InstallRbootIdentityTest(unittest.TestCase):
    def test_accepts_any_port_when_target_is_esp8266(self):
        with patch.object(
            install_rboot, "run", return_value="Chip is ESP8266EX"
        ) as run:
            install_rboot.assert_esp8266("python", "esptool", "/dev/ttyUSB-arbitrary")

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


class InstallRbootTest(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory()
        self.addCleanup(self.temp.cleanup)
        self.root = pathlib.Path(self.temp.name)
        self.backup = self.root / "backup"
        self.paths = {
            name: self.root / (name + ".bin")
            for name in ("rboot", "metadata-a", "metadata-b", "slot-a")
        }
        self.paths["rboot"].write_bytes(ram_image())
        self.paths["slot-a"].write_bytes(boot2_image())
        self.generate_metadata("metadata-a")
        self.generate_metadata("metadata-b", "--state", "erased")
        self.argv = ["--port", "/dev/never-opened", "--backup-dir", str(self.backup)]
        for name, path in self.paths.items():
            self.argv.extend(["--" + name, str(path)])
        self.flash_sizes = iter(["4MB", "4MB"])
        self.chips = iter(["ESP8266EX", "ESP8266EX"])
        self.short_backup = False
        self.changed_provisioning = False
        self.failed_write = False
        self.mutate_source = False
        self.writes = []
        self.runner = patch.object(
            install_rboot, "run", side_effect=self.fake_run
        ).start()
        self.addCleanup(patch.stopall)
        patch("sys.stdout", new=io.StringIO()).start()
        patch("sys.stderr", new=io.StringIO()).start()

    def generate_metadata(self, name, *args):
        subprocess.run(
            [
                sys.executable,
                str(SCRIPT.with_name("make-rboot-config.py")),
                str(self.paths[name]),
                *args,
            ],
            check=True,
            capture_output=True,
        )

    def fake_run(self, python, esptool, port, command, *args, capture=False):
        if command == "chip_id":
            return "Chip is " + next(self.chips)
        if command == "flash_id":
            return (
                "Manufacturer: ef\nDevice: 4016\nDetected flash size: "
                + next(self.flash_sizes)
                + "\n"
            )
        if command == "read_flash":
            address, size, path = args
            self.assertIn(int(address, 16), (0x3F9000, 0x3FA000))
            self.assertEqual(size, "0x1000")
            self.assertEqual(path.stat().st_mode & 0o777, 0o600)
            data = bytes([int(address, 16) // 0x1000 & 0xFF]) * 0x1000
            if self.short_backup:
                data = data[:-1]
            if self.changed_provisioning and path.name.startswith("after-"):
                data = b"\x00" + data[1:]
            path.write_bytes(data)
            if self.mutate_source:
                self.paths["slot-a"].write_bytes(b"changed after validation")
            return ""
        self.assertEqual(command, "write_flash")
        self.assertEqual(
            args[:6],
            ("--flash_mode", "dio", "--flash_freq", "40m", "--flash_size", "4MB"),
        )
        self.assertEqual(args[6::2], ("0x000000", "0x001000", "0x100000", "0x002000"))
        self.assertEqual(self.backup.stat().st_mode & 0o777, 0o700)
        self.assertEqual((self.backup / "sha256.txt").stat().st_mode & 0o777, 0o600)
        expected_manifest = "".join(
            f"{hashlib.sha256((self.backup / name).read_bytes()).hexdigest()}  {name}\n"
            for _, name in install_rboot.PROVISIONING
        )
        self.assertEqual((self.backup / "sha256.txt").read_text(), expected_manifest)
        staged = args[7::2]
        self.assertTrue(all(path not in self.paths.values() for path in staged))
        self.writes.append([path.read_bytes() for path in staged])
        if self.failed_write:
            raise subprocess.CalledProcessError(2, "esptool")
        return ""

    def assert_no_serial(self):
        with self.assertRaises((SystemExit, OSError)):
            install_rboot.main(self.argv)
        self.runner.assert_not_called()
        self.assertFalse(self.backup.exists())

    def test_valid_install_preserves_provisioning(self):
        expected = [path.read_bytes() for path in self.paths.values()]
        install_rboot.main(self.argv)
        self.assertEqual(self.writes, [expected])
        self.assertEqual(
            [call.args[3] for call in self.runner.call_args_list],
            [
                "chip_id",
                "flash_id",
                "read_flash",
                "read_flash",
                "chip_id",
                "flash_id",
                "write_flash",
                "read_flash",
                "read_flash",
            ],
        )
        for _, name in install_rboot.PROVISIONING:
            self.assertEqual(
                (self.backup / name).read_bytes(),
                (self.backup / ("after-" + name)).read_bytes(),
            )

    def test_committed_redundant_metadata_selecting_a_is_accepted(self):
        self.generate_metadata("metadata-b", "--slot", "a", "--generation", "42")
        install_rboot.main(self.argv)
        self.assertEqual(len(self.writes), 1)

    def test_maximum_extent_images_are_accepted(self):
        self.paths["rboot"].write_bytes(ram_image(b"\x01" * (0x1000 - 32)))
        self.paths["slot-a"].write_bytes(boot2_image(b"\x02" * (0xFE000 - 48)))
        install_rboot.main(self.argv)
        self.assertEqual(len(self.writes[0][0]), 0x1000)
        self.assertEqual(len(self.writes[0][3]), 0xFE000)

    def test_oversized_inputs_fail_before_serial(self):
        for name, limit in (
            ("rboot", 0x1000),
            ("metadata-a", 0x1000),
            ("metadata-b", 0x1000),
            ("slot-a", 0xFE000),
        ):
            with self.subTest(name=name):
                original = self.paths[name].read_bytes()
                self.paths[name].write_bytes(b"\xff" * (limit + 1))
                self.assert_no_serial()
                self.paths[name].write_bytes(original)

    def test_empty_and_missing_inputs_fail_before_serial(self):
        for name, path in self.paths.items():
            with self.subTest(name=name):
                original = path.read_bytes()
                path.write_bytes(b"")
                self.assert_no_serial()
                path.unlink()
                self.assert_no_serial()
                path.mkdir()
                self.assert_no_serial()
                path.rmdir()
                path.write_bytes(original)

    def test_short_metadata_fails_before_serial(self):
        for name in ("metadata-a", "metadata-b"):
            original = self.paths[name].read_bytes()
            self.paths[name].write_bytes(original[:-1])
            self.assert_no_serial()
            self.paths[name].write_bytes(original)

    def test_bad_metadata_states_fail_before_serial(self):
        for name in ("metadata-a", "metadata-b"):
            original = self.paths[name].read_bytes()
            for state in (
                "partial-header",
                "partial-payload",
                "uncommitted",
                "bad-crc",
            ):
                with self.subTest(name=name, state=state):
                    self.generate_metadata(name, "--state", state)
                    self.assert_no_serial()
            self.paths[name].write_bytes(original)

    def test_erased_metadata_a_fails_before_serial(self):
        self.generate_metadata("metadata-a", "--state", "erased")
        self.assert_no_serial()

    def test_slot_b_selection_fails_even_with_lower_generation(self):
        for name in ("metadata-a", "metadata-b"):
            original = self.paths[name].read_bytes()
            self.generate_metadata(name, "--slot", "b", "--generation", "0")
            self.assert_no_serial()
            self.paths[name].write_bytes(original)

    def test_metadata_config_and_reserved_space_are_checked(self):
        original = self.paths["metadata-a"].read_bytes()
        # Recompute both checksums: reject semantically unsafe, otherwise valid records.
        for offset in (0, 4, 12, 13, 14, 16, 17, 18, 19, 20, 24, 28, 32, 50, 4092):
            with self.subTest(offset=offset):
                data = bytearray(original)
                data[offset] ^= 1
                data[36] = checksum(data[12:36])
                struct.pack_into("<I", data, 37, zlib.crc32(data[:37]))
                self.paths["metadata-a"].write_bytes(data)
                self.assert_no_serial()

    def test_config_checksum_is_checked_separately(self):
        data = bytearray(self.paths["metadata-a"].read_bytes())
        data[36] ^= 1
        struct.pack_into("<I", data, 37, zlib.crc32(data[:37]))
        self.paths["metadata-a"].write_bytes(data)
        self.assert_no_serial()

    def test_invalid_images_fail_before_serial(self):
        for name in ("rboot", "slot-a"):
            original = self.paths[name].read_bytes()
            ram_offset = 32 if name == "slot-a" else 0
            mutations = [
                b"\xff" * 32,
                original[:-1],
                original + b"\x00" * 260,
                original[:-1] + bytes([original[-1] ^ 1]),
                replace_u32(original, ram_offset + 4, 0x40200000),
                replace_u32(original, ram_offset + 8, 0x40000000),
                replace_u32(original, ram_offset + 12, 0),
                replace_u32(original, ram_offset + 12, 0xFFFFFFFF),
                original[: ram_offset + 12],
            ]
            for offset in (ram_offset, ram_offset + 2, ram_offset + 3):
                mutated = bytearray(original)
                mutated[offset] ^= 1
                mutations.append(mutated)
            for count in (0, 17):
                mutated = bytearray(original)
                mutated[ram_offset + 1] = count
                mutations.append(mutated)
            for index, data in enumerate(mutations):
                with self.subTest(name=name, mutation=index):
                    self.paths[name].write_bytes(data)
                    self.assert_no_serial()
            self.paths[name].write_bytes(original)

    def test_boot2_wrapper_and_irom_are_checked(self):
        original = self.paths["slot-a"].read_bytes()
        for data in (
            ram_image(),
            replace_u32(original, 8, 1),
            replace_u32(original, 12, 0),
            replace_u32(original, 12, 0xFFFFFFFF),
            replace_u32(original, 4, 0x40100001),
            replace_u32(original, 16, 123),
        ):
            self.paths["slot-a"].write_bytes(data)
            self.assert_no_serial()

    def test_every_truncated_image_fails_before_serial(self):
        for name in ("rboot", "slot-a"):
            original = self.paths[name].read_bytes()
            for size in range(len(original)):
                with self.subTest(name=name, size=size):
                    self.paths[name].write_bytes(original[:size])
                    self.assert_no_serial()
            self.paths[name].write_bytes(original)

    def test_ram_segment_must_not_cross_memory_boundary(self):
        self.paths["rboot"].write_bytes(replace_u32(ram_image(), 8, 0x4010FFFE))
        self.assert_no_serial()

    def test_overlapping_ram_segments_are_rejected(self):
        image = bytearray(ram_image()[:20])
        image[1] = 2
        image += image[8:20]
        image += b"\x00" * 15 + b"\xef"
        self.paths["rboot"].write_bytes(image)
        with self.assertRaisesRegex(SystemExit, "overlapping"):
            install_rboot.main(self.argv)
        self.runner.assert_not_called()

    def test_entry_point_must_be_loaded(self):
        self.paths["rboot"].write_bytes(replace_u32(ram_image(), 4, 0x40100004))
        with self.assertRaisesRegex(SystemExit, "not in a loaded segment"):
            install_rboot.main(self.argv)
        self.runner.assert_not_called()

    def test_flash_size_must_be_exactly_4mb(self):
        for size in (
            "1MB",
            "2MB",
            "8MB",
            "Unknown",
            "",
            "4MB\nDetected flash size: 8MB",
        ):
            with self.subTest(size=size):
                self.chips = iter(["ESP8266EX"])
                self.flash_sizes = iter([size])
                with self.assertRaisesRegex(SystemExit, "exactly 4 MiB"):
                    install_rboot.main(self.argv)
                self.assertFalse(self.writes)
                self.assertFalse(self.backup.exists())
                self.assertNotIn(
                    "read_flash", [call.args[3] for call in self.runner.call_args_list]
                )

    def test_flash_size_is_rechecked_before_write(self):
        self.flash_sizes = iter(["4MB", "2MB"])
        with self.assertRaisesRegex(SystemExit, "exactly 4 MiB"):
            install_rboot.main(self.argv)
        self.assertFalse(self.writes)
        self.assertTrue((self.backup / "sha256.txt").exists())

    def test_chip_is_rechecked_before_write(self):
        self.chips = iter(["ESP8266EX", "ESP32"])
        with self.assertRaisesRegex(SystemExit, "ESP8266"):
            install_rboot.main(self.argv)
        self.assertFalse(self.writes)

    def test_existing_backup_is_not_overwritten(self):
        self.backup.mkdir()
        sentinel = self.backup / "keep"
        sentinel.write_bytes(b"keep")
        with self.assertRaisesRegex(SystemExit, "already exists"):
            install_rboot.main(self.argv)
        self.runner.assert_not_called()
        self.assertEqual(sentinel.read_bytes(), b"keep")

    def test_short_backup_prevents_write(self):
        self.short_backup = True
        with self.assertRaisesRegex(SystemExit, "invalid size"):
            install_rboot.main(self.argv)
        self.assertFalse(self.writes)

    def test_serial_probe_or_read_failure_prevents_write(self):
        for command in ("chip_id", "flash_id", "read_flash"):
            with self.subTest(command=command):
                self.chips = iter(["ESP8266EX"])
                self.flash_sizes = iter(["4MB"])

                def fail(python, esptool, port, operation, *args, **kwargs):
                    if operation == command:
                        raise subprocess.CalledProcessError(2, "esptool")
                    return self.fake_run(
                        python, esptool, port, operation, *args, **kwargs
                    )

                self.runner.side_effect = fail
                with self.assertRaises(subprocess.CalledProcessError):
                    install_rboot.main(self.argv)
                self.assertFalse(self.writes)

    def test_changed_provisioning_is_reported(self):
        self.changed_provisioning = True
        with self.assertRaisesRegex(SystemExit, "PROVISIONING CHANGED"):
            install_rboot.main(self.argv)
        self.assertTrue((self.backup / "sha256.txt").exists())

    def test_failed_write_keeps_backups_and_manifest(self):
        self.failed_write = True
        with self.assertRaises(subprocess.CalledProcessError):
            install_rboot.main(self.argv)
        self.assertTrue((self.backup / "sha256.txt").exists())
        for _, name in install_rboot.PROVISIONING:
            self.assertEqual((self.backup / name).stat().st_size, 0x1000)

    def test_source_changes_do_not_change_flashed_snapshot(self):
        original = self.paths["slot-a"].read_bytes()
        self.mutate_source = True
        install_rboot.main(self.argv)
        self.assertEqual(self.writes[0][3], original)
        self.assertNotEqual(self.paths["slot-a"].read_bytes(), original)

    def test_layout_matches_firmware_header(self):
        header = (
            SCRIPT.parents[1] / "lib/Provisioning/FlashLayoutValues.h"
        ).read_text()
        layout = {
            key: int(value, 16)
            for key, value in re.findall(r"#define MF_(\w+) (0x[0-9a-fA-F]+)U", header)
        }
        self.assertEqual(layout["FLASH_SIZE"], 4 * 1024 * 1024)
        self.assertEqual(layout["SECTOR_SIZE"], install_rboot.SECTOR_SIZE)
        self.assertEqual(layout["RBOOT_START"], 0)
        self.assertEqual(layout["BOOT_METADATA_A"], install_rboot.SECTOR_SIZE)
        self.assertEqual(layout["BOOT_METADATA_B"], install_rboot.SLOT_A_END)
        self.assertEqual(layout["SLOT_A_START"], install_rboot.SLOT_A_START)
        self.assertEqual(layout["SLOT_A_END"], install_rboot.SLOT_A_END)
        self.assertEqual(layout["SLOT_B_START"], install_rboot.SLOT_B_START)
        self.assertEqual(
            (layout["PROVISIONING_A"], layout["PROVISIONING_B"]),
            tuple(address for address, _ in install_rboot.PROVISIONING),
        )


if __name__ == "__main__":
    unittest.main()
