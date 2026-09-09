import importlib.util
import pathlib
import unittest

SCRIPT = pathlib.Path(__file__).parents[1] / "scripts/verify-rboot-bootloader.py"
SPEC = importlib.util.spec_from_file_location("verify_rboot_bootloader", SCRIPT)
verifier = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(verifier)


class RbootBootloaderTest(unittest.TestCase):
    def test_requires_transactional_patch_marker(self):
        verifier.verify(b"image: Using safe default boot config.")
        for image in (b"unpatched", b"Writing default boot config."):
            with self.subTest(image=image):
                with self.assertRaisesRegex(ValueError, "patch is missing"):
                    verifier.verify(image)

    def test_rejects_legacy_fallback_even_with_new_marker(self):
        with self.assertRaisesRegex(ValueError, "legacy destructive"):
            verifier.verify(
                b"Using safe default boot config. Writing default boot config."
            )

    def test_rejects_empty_or_oversized_bootloader(self):
        for image in (b"", b"Using safe default boot config." + b"x" * 4096):
            with self.assertRaisesRegex(ValueError, "4 KiB"):
                verifier.verify(image)


if __name__ == "__main__":
    unittest.main()
