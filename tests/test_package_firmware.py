"""Exercise layout rejection and packaging without a connected device or SDK."""
import importlib.util
import json
from pathlib import Path
import tempfile
import unittest
from unittest.mock import patch

ROOT = Path(__file__).resolve().parents[1]
SPEC = importlib.util.spec_from_file_location("package_firmware", ROOT / "scripts" / "package_firmware.py")
pkg = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(pkg)


class PackageTests(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory()
        self.addCleanup(self.temp.cleanup)
        self.build = Path(self.temp.name) / "build"
        self.build.mkdir()
        (self.build / "bootloader.bin").write_bytes(b"test-bootloader")
        (self.build / "esp_matter_base.bin").write_bytes(b"test-app")
        self.layout = {
            "extra_esptool_args": {"chip": "esp32h2"},
            "flash_files": {"0x0": "bootloader.bin", "0x20000": "esp_matter_base.bin"},
            "app": {"offset": "0x20000", "file": "esp_matter_base.bin"},
            "flash_settings": {"flash_mode": "dio", "flash_freq": "48m", "flash_size": "4MB"},
        }
        (self.build / "config").mkdir()
        config = {name.removeprefix("CONFIG_"): {"y": True, "n": False}.get(value, value.strip('"'))
                  for name, value in pkg.REQUIRED_CONFIG.items()}
        (self.build / "config" / "sdkconfig.json").write_text(json.dumps(config))
        (self.build / "project_description.json").write_text('{"project_version":"0.1.0"}')
        self.save_layout()

    def save_layout(self):
        (self.build / "flasher_args.json").write_text(json.dumps(self.layout))

    def rejected(self, message):
        self.save_layout()
        with self.assertRaisesRegex(ValueError, message):
            pkg.load_flash_layout(self.build)

    def test_offsets_come_from_idf(self):
        _, files = pkg.load_flash_layout(self.build)
        self.assertEqual([offset for offset, _ in files], [0, 0x20000])

    def test_wrong_chip_rejected(self):
        self.layout["extra_esptool_args"]["chip"] = "esp32c3"
        self.rejected("different chip")

    def test_missing_binary_rejected(self):
        (self.build / "esp_matter_base.bin").unlink()
        self.rejected("Missing or empty")

    def test_empty_binary_rejected(self):
        (self.build / "esp_matter_base.bin").write_bytes(b"")
        self.rejected("Missing or empty")

    def test_path_escape_rejected(self):
        self.layout["flash_files"]["0x0"] = "../outside.bin"
        self.rejected("escapes")

    def test_overlapping_images_rejected(self):
        (self.build / "bootloader.bin").write_bytes(b"x" * (0x20000 + 1))
        self.rejected("overlap")

    def test_flash_overflow_rejected(self):
        self.layout["flash_files"]["0x3fffff"] = "bootloader.bin"
        self.rejected("exceed")

    def test_wrong_app_offset_rejected(self):
        self.layout["app"]["offset"] = "0x10000"
        self.rejected("Unexpected application offset")

    def test_real_package_manifest_and_checksums(self):
        output = Path(self.temp.name) / "dist"

        def fake_merge(command, **kwargs):
            # Only a temporary unit-test fixture; never a deliverable firmware.
            Path(command[command.index("-o") + 1]).write_bytes(b"test-only-merged-image")
            self.assertIn("0x20000", command)
            self.assertEqual(kwargs["cwd"], self.build.resolve())

        with patch.object(pkg.subprocess, "run", side_effect=fake_merge):
            pkg.package(self.build, output)
        manifest = json.loads((output / "manifest.json").read_text())
        self.assertEqual(manifest["chip"], "esp32h2")
        self.assertEqual(manifest["app_offset"], "0x20000")
        self.assertEqual(manifest["files"]["factory.bin"], pkg.describe(output / "factory.bin"))
        self.assertEqual((output / "esp_matter_base.bin").read_bytes(), b"test-app")
        self.assertTrue((output / "flash.ps1").is_file())
        self.assertTrue((output / "FLASHEN.md").is_file())

    def test_sleep_disabled_blocks_packaging_before_esptool(self):
        config_file = self.build / "config" / "sdkconfig.json"
        config = json.loads(config_file.read_text())
        config["PM_ENABLE"] = False
        config_file.write_text(json.dumps(config))
        with patch.object(pkg.subprocess, "run") as merge:
            with self.assertRaisesRegex(ValueError, "PM_ENABLE"):
                pkg.package(self.build, Path(self.temp.name) / "dist")
        merge.assert_not_called()

    def test_no_sleep_diagnostic_rejected_by_low_power_packaging(self):
        config_file = self.build / "config" / "sdkconfig.json"
        config = json.loads(config_file.read_text())
        config["BASE_DIAGNOSTIC_NO_SLEEP"] = True
        config_file.write_text(json.dumps(config))
        with patch.object(pkg.subprocess, "run") as merge:
            with self.assertRaisesRegex(ValueError, "BASE_DIAGNOSTIC_NO_SLEEP"):
                pkg.package(self.build, Path(self.temp.name) / "dist")
        merge.assert_not_called()

    def test_peripheral_power_down_rejected_for_gpio_wakeup(self):
        config_file = self.build / "config" / "sdkconfig.json"
        config = json.loads(config_file.read_text())
        config["PM_POWER_DOWN_PERIPHERAL_IN_LIGHT_SLEEP"] = True
        config_file.write_text(json.dumps(config))
        with patch.object(pkg.subprocess, "run") as merge:
            with self.assertRaisesRegex(ValueError, "PM_POWER_DOWN_PERIPHERAL_IN_LIGHT_SLEEP"):
                pkg.package(self.build, Path(self.temp.name) / "dist")
        merge.assert_not_called()

    def test_text_config_requires_low_power_soil_profile(self):
        config_file = Path(self.temp.name) / "sdkconfig"
        lines = [f"{name}={value}" if value != "n" else f"# {name} is not set"
                 for name, value in pkg.REQUIRED_CONFIG.items()]
        config_file.write_text("\n".join(lines))
        pkg.validate_config(config_file)
        config_file.write_text("\n".join(lines) + "\nCONFIG_BASE_DIAGNOSTIC_NO_SLEEP=y\n")
        with self.assertRaisesRegex(ValueError, "BASE_DIAGNOSTIC_NO_SLEEP"):
            pkg.validate_config(config_file)


if __name__ == "__main__":
    unittest.main()
