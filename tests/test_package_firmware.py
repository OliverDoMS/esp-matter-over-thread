"""Exercise layout rejection and packaging without a connected device or SDK."""
import importlib.util
import json
import struct
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
        (self.build / "partition_table").mkdir()
        entries = [("nvs", 1, 2, 0x10000, 0xC000),
                   ("otadata", 1, 0, 0x1D000, 0x2000),
                   ("ota_0", 0, 0x10, 0x20000, 0x1E0000),
                   ("ota_1", 0, 0x11, 0x200000, 0x1E0000)]
        table = b"".join(struct.pack("<HBBII16sI", 0x50AA, kind, subtype, address, size,
                                    name.encode(), 0) for name, kind, subtype, address, size in entries)
        (self.build / "partition_table" / "partition-table.bin").write_bytes(table)
        # Test-only header fixture; the SDK creates real TLV headers during builds.
        self.ota = self.build / "esp_matter_base-ota.bin"
        self.ota.write_bytes(struct.pack("<IQI", 0x1BEEF11E, 25, 1) + b"x" + b"test-app")
        self.layout = {
            "extra_esptool_args": {"chip": "esp32h2"},
            "flash_files": {"0x0": "bootloader.bin", "0x20000": "esp_matter_base.bin"},
            "app": {"offset": "0x20000", "file": "esp_matter_base.bin"},
            "flash_settings": {"flash_mode": "dio", "flash_freq": "48m", "flash_size": "4MB"},
        }
        (self.build / "config").mkdir()
        self.use_variant("soil3")
        self.save_layout()

    def use_variant(self, variant, diagnostic=False):
        """Write the resolved config and version a real build of this variant would have."""
        config = {name.removeprefix("CONFIG_"): pkg.resolved_value(value)
                  for name, value in pkg.required_config(variant, diagnostic).items()}
        (self.build / "config" / "sdkconfig.json").write_text(json.dumps(config))
        version = pkg.load_variant(variant)["PROJECT_VER"]
        (self.build / "project_description.json").write_text(json.dumps({"project_version": version}))

    def package(self, variant="soil3", diagnostic=False):
        output = Path(self.temp.name) / "dist"

        def fake_merge(command, **kwargs):
            # Only a temporary unit-test fixture; never a deliverable firmware.
            Path(command[command.index("-o") + 1]).write_bytes(b"test-only-merged-image")
            self.assertIn("0x20000", command)
            self.assertEqual(kwargs["cwd"], self.build.resolve())

        with patch.object(pkg.subprocess, "run", side_effect=fake_merge):
            pkg.package(self.build, output, variant, diagnostic)
        return output, json.loads((output / "manifest.json").read_text())

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

    def test_ota_wrong_payload_rejected(self):
        self.ota.write_bytes(self.ota.read_bytes()[:-1] + b"!")
        with self.assertRaisesRegex(ValueError, "payload mismatch"):
            pkg.validate_ota_outputs(self.build)

    def test_ota_truncated_header_rejected(self):
        self.ota.write_bytes(b"bad")
        with self.assertRaisesRegex(ValueError, "truncated"):
            pkg.validate_ota_outputs(self.build)

    def test_ota_partition_change_rejected(self):
        table = self.build / "partition_table" / "partition-table.bin"
        table.write_bytes(table.read_bytes()[:96])  # Missing second slot.
        with self.assertRaisesRegex(ValueError, "partition layout"):
            pkg.validate_ota_outputs(self.build)

    def test_ota_slot_overflow_rejected(self):
        (self.build / "esp_matter_base.bin").write_bytes(b"x" * (pkg.OTA_SLOT_BYTES + 1))
        with self.assertRaisesRegex(ValueError, "exceeds OTA slot"):
            pkg.validate_ota_outputs(self.build)

    def test_real_package_manifest_and_checksums(self):
        output, manifest = self.package()
        self.assertEqual(manifest["chip"], "esp32h2")
        self.assertEqual(manifest["variant"], "soil3")
        self.assertEqual(manifest["app_offset"], "0x20000")
        self.assertEqual(manifest["files"]["factory.bin"], pkg.describe(output / "factory.bin"))
        self.assertEqual((output / "esp_matter_base.bin").read_bytes(), b"test-app")
        self.assertTrue((output / "flash.ps1").is_file())
        self.assertTrue((output / "FLASHEN.md").is_file())
        self.assertTrue((output / "BODENFEUCHTE-3.md").is_file())
        self.assertTrue(manifest["ota_supported"])
        self.assertEqual((output / "MoistureSensor3.ota").read_bytes(), self.ota.read_bytes())
        self.assertEqual(manifest["files"]["MoistureSensor3.ota"], pkg.describe(output / "MoistureSensor3.ota"))

    def test_rain_variant_package(self):
        self.use_variant("rain")
        output, manifest = self.package("rain")
        self.assertEqual(manifest["product"], "RainSensor")
        self.assertTrue((output / "RainSensor.ota").is_file())
        self.assertFalse((output / "BODENFEUCHTE.md").exists())

    def test_variants_have_distinct_product_ids_and_one_sensor(self):
        seen = {}
        for variant_dir in sorted((pkg.ROOT / "variants").glob("*/variant.env")):
            name = variant_dir.parent.name
            overlay = pkg.parse_defaults(variant_dir.parent / "sdkconfig.defaults")
            pid = int(overlay["CONFIG_DEVICE_PRODUCT_ID"], 0)
            self.assertNotIn(pid, seen, f"{name} reuses the product ID of {seen.get(pid)}")
            seen[pid] = name
            sensors = [k for k in ("CONFIG_BASE_RAIN_SENSOR", "CONFIG_BASE_SOIL_MOISTURE") if overlay.get(k) == "y"]
            self.assertEqual(len(sensors), 1, f"{name} must select exactly one sensor")
            self.assertTrue(pkg.load_variant(name)["PROJECT_VER_NUMBER"].isdigit())

    def test_ci_matrix_builds_every_variant(self):
        workflow = (pkg.ROOT / ".github" / "workflows" / "firmware.yml").read_text()
        line = next(l for l in workflow.splitlines() if l.strip().startswith("variant: ["))
        matrix = sorted(v.strip() for v in line.split("[", 1)[1].rstrip("]").split(","))
        variants = sorted(p.parent.name for p in (pkg.ROOT / "variants").glob("*/variant.env"))
        self.assertEqual(matrix, variants)

    def test_wrong_variant_for_build_rejected(self):
        # A soil build directory packaged as rain must fail before esptool runs.
        with patch.object(pkg.subprocess, "run") as merge:
            with self.assertRaisesRegex(ValueError, "mismatch|expects"):
                pkg.package(self.build, Path(self.temp.name) / "dist", "rain")
        merge.assert_not_called()

    def test_unknown_variant_rejected(self):
        with self.assertRaisesRegex(ValueError, "Unknown variant"):
            pkg.load_variant("does-not-exist")

    def test_diagnostic_package_requires_sleep_off(self):
        self.use_variant("rain", diagnostic=True)
        _, manifest = self.package("rain", diagnostic=True)
        self.assertTrue(manifest["diagnostic"])
        with patch.object(pkg.subprocess, "run") as merge:
            with self.assertRaisesRegex(ValueError, "BASE_DIAGNOSTIC_NO_SLEEP"):
                pkg.package(self.build, Path(self.temp.name) / "dist2", "rain")
        merge.assert_not_called()

    def test_sleep_disabled_blocks_packaging_before_esptool(self):
        config_file = self.build / "config" / "sdkconfig.json"
        config = json.loads(config_file.read_text())
        config["PM_ENABLE"] = False
        config_file.write_text(json.dumps(config))
        with patch.object(pkg.subprocess, "run") as merge:
            with self.assertRaisesRegex(ValueError, "PM_ENABLE"):
                pkg.package(self.build, Path(self.temp.name) / "dist", "soil3")
        merge.assert_not_called()

    def test_no_sleep_diagnostic_rejected_by_low_power_packaging(self):
        config_file = self.build / "config" / "sdkconfig.json"
        config = json.loads(config_file.read_text())
        config["BASE_DIAGNOSTIC_NO_SLEEP"] = True
        config_file.write_text(json.dumps(config))
        with patch.object(pkg.subprocess, "run") as merge:
            with self.assertRaisesRegex(ValueError, "BASE_DIAGNOSTIC_NO_SLEEP"):
                pkg.package(self.build, Path(self.temp.name) / "dist", "soil3")
        merge.assert_not_called()

    def test_peripheral_power_down_rejected_for_gpio_wakeup(self):
        config_file = self.build / "config" / "sdkconfig.json"
        config = json.loads(config_file.read_text())
        config["PM_POWER_DOWN_PERIPHERAL_IN_LIGHT_SLEEP"] = True
        config_file.write_text(json.dumps(config))
        with patch.object(pkg.subprocess, "run") as merge:
            with self.assertRaisesRegex(ValueError, "PM_POWER_DOWN_PERIPHERAL_IN_LIGHT_SLEEP"):
                pkg.package(self.build, Path(self.temp.name) / "dist", "soil3")
        merge.assert_not_called()

    def test_text_config_requires_low_power_soil_profile(self):
        config_file = Path(self.temp.name) / "sdkconfig"
        required = pkg.required_config("soil3")
        lines = [f"{name}={value}" if value != "n" else f"# {name} is not set"
                 for name, value in required.items()]
        config_file.write_text("\n".join(lines))
        pkg.validate_config(config_file, required)
        config_file.write_text("\n".join(lines) + "\nCONFIG_BASE_DIAGNOSTIC_NO_SLEEP=y\n")
        with self.assertRaisesRegex(ValueError, "BASE_DIAGNOSTIC_NO_SLEEP"):
            pkg.validate_config(config_file, required)


if __name__ == "__main__":
    unittest.main()
