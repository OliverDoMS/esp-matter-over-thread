"""Package only actual ESP-IDF build outputs, using generated flash offsets."""
from __future__ import annotations

import argparse
import hashlib
import json
import os
from pathlib import Path
import shutil
import struct
import subprocess
import sys

FLASH_BYTES = 4 * 1024 * 1024
ROOT = Path(__file__).resolve().parents[1]
# Settings every variant must keep. The variant overlay adds its own requirements.
REQUIRED_CONFIG = {
    "CONFIG_IDF_TARGET": '"esp32h2"',
    "CONFIG_ESPTOOLPY_FLASHSIZE_4MB": "y",
    "CONFIG_OPENTHREAD_MTD": "y",
    "CONFIG_ENABLE_ICD_SERVER": "y",
    "CONFIG_PM_ENABLE": "y",
    "CONFIG_FREERTOS_USE_TICKLESS_IDLE": "y",
    "CONFIG_IEEE802154_SLEEP_ENABLE": "y",
    "CONFIG_USJ_NO_AUTO_LS_ON_CONNECTION": "y",
    "CONFIG_BASE_DIAGNOSTIC_NO_SLEEP": "n",
    "CONFIG_PM_POWER_DOWN_PERIPHERAL_IN_LIGHT_SLEEP": "n",
    "CONFIG_ENABLE_OTA_REQUESTOR": "y",
    "CONFIG_CHIP_OTA_IMAGE_BUILD": "y",
    "CONFIG_OTA_AUTO_REBOOT_ON_APPLY": "y",
    "CONFIG_CUSTOM_COMMISSIONABLE_DATA_PROVIDER": "y",
}


def parse_defaults(path: Path) -> dict:
    """KEY=VALUE lines of an sdkconfig defaults file, comments skipped."""
    return dict(line.split("=", 1) for line in path.read_text().splitlines()
                if line.startswith("CONFIG_") and "=" in line)


def load_variant(name: str, root: Path = ROOT) -> dict:
    folder = root / "variants" / name
    if not (folder / "variant.env").is_file():
        known = sorted(p.parent.name for p in (root / "variants").glob("*/variant.env"))
        raise ValueError(f"Unknown variant {name!r}; known: {', '.join(known)}")
    info = dict(line.split("=", 1) for line in (folder / "variant.env").read_text().splitlines()
                if line and not line.startswith("#") and "=" in line)
    for key in ("PRODUCT_NAME", "PROJECT_VER", "PROJECT_VER_NUMBER"):
        if key not in info:
            raise ValueError(f"variants/{name}/variant.env lacks {key}")
    info["DOCS"] = info.get("DOCS", "").split()
    info["NAME"] = name
    return info


def required_config(variant: str, diagnostic: bool = False, root: Path = ROOT) -> dict:
    """Common rules plus every line of the variant overlay. A diagnostic image must have
    Light-sleep off instead; it is for short-lived USB-log builds only."""
    required = {**REQUIRED_CONFIG, **parse_defaults(root / "variants" / variant / "sdkconfig.defaults")}
    if diagnostic:
        required.update(parse_defaults(root / "variants" / "diagnostic.sdkconfig"))
    return required


def resolved_value(expected: str):
    """How an sdkconfig defaults value appears in build/config/sdkconfig.json."""
    if expected in ("y", "n"):
        return expected == "y"
    if expected.startswith('"'):
        return expected.strip('"')
    try:
        return int(expected, 0)
    except ValueError:
        return expected


OTA_SLOT_BYTES = 0x1E0000


def validate_ota_outputs(build: Path) -> tuple[Path, Path]:
    table = build / "partition_table" / "partition-table.bin"
    entries = {}
    raw = table.read_bytes()
    for offset in range(0, len(raw) - 31, 32):
        magic, kind, subtype, address, size, label, _ = struct.unpack_from("<HBBII16sI", raw, offset)
        if magic != 0x50AA:
            break
        entries[label.rstrip(b"\0").decode()] = (kind, subtype, address, size)
    required = {
        "nvs": (1, 2, 0x10000, 0xC000),
        "otadata": (1, 0, 0x1D000, 0x2000),
        "ota_0": (0, 0x10, 0x20000, OTA_SLOT_BYTES),
        "ota_1": (0, 0x11, 0x200000, OTA_SLOT_BYTES),
    }
    if any(entries.get(name) != value for name, value in required.items()):
        raise ValueError("OTA partition layout mismatch; refusing unsafe package")
    app = (build / "esp_matter_base.bin").read_bytes()
    if len(app) > OTA_SLOT_BYTES:
        raise ValueError("Application exceeds OTA slot size")
    ota = build / "esp_matter_base-ota.bin"
    content = ota.read_bytes()
    if len(content) < 16:
        raise ValueError("Missing or truncated Matter OTA header")
    magic, total_size, header_size = struct.unpack_from("<IQI", content)
    if (magic != 0x1BEEF11E or total_size != len(content)
            or header_size == 0 or content[16 + header_size:] != app):
        raise ValueError("Matter OTA header or payload mismatch")
    return table, ota


def validate_config(path: Path, required: dict) -> None:
    values = parse_defaults(path)
    for key, expected in required.items():
        # Disabled Kconfig booleans appear as comments in sdkconfig.
        if values.get(key, "n") != expected:
            raise ValueError(f"Build configuration mismatch: {key} must be {expected}")


def load_flash_layout(build: Path) -> tuple[dict, list[tuple[int, Path]]]:
    data = json.loads((build / "flasher_args.json").read_text())
    if data.get("extra_esptool_args", {}).get("chip") != "esp32h2":
        raise ValueError("Refusing to package firmware for a different chip")
    files = []
    for offset_text, name in data["flash_files"].items():
        offset = int(offset_text, 0)
        file = (build / name).resolve()
        if not file.is_relative_to(build.resolve()):
            raise ValueError("Flash file escapes the build directory")
        if not file.is_file() or file.stat().st_size == 0:
            raise ValueError(f"Missing or empty flash file: {name}")
        files.append((offset, file))
    files.sort()
    if not files or files[0][0] != 0:
        raise ValueError("ESP32-H2 factory image must start with bootloader at 0x0")
    end = 0
    for offset, file in files:
        if offset < end or offset + file.stat().st_size > FLASH_BYTES:
            raise ValueError("Flash files overlap or exceed the 4 MB flash")
        end = offset + file.stat().st_size
    app = data.get("app", {})
    if int(app.get("offset", "-1"), 0) != 0x20000:
        raise ValueError("Unexpected application offset; review partition layout")
    if not any(offset == 0x20000 and file.name == "esp_matter_base.bin" for offset, file in files):
        raise ValueError("Application image is missing from flash_files")
    return data, files


def describe(file: Path) -> dict:
    return {"bytes": file.stat().st_size,
            "sha256": hashlib.sha256(file.read_bytes()).hexdigest()}


def package(build: Path, output: Path, variant: str, diagnostic: bool = False) -> None:
    build = build.resolve()
    output = output.resolve()
    info = load_variant(variant)
    required = required_config(variant, diagnostic)
    # ESP-IDF copies the resolved project configuration into the build directory.
    config = build / "config" / "sdkconfig.json"
    if config.exists():
        config_values = json.loads(config.read_text())
        for name, expected in required.items():
            value = config_values.get(name.removeprefix("CONFIG_"), False if expected == "n" else None)
            if value != resolved_value(expected):
                raise ValueError(f"Resolved sdkconfig mismatch: {name}={value!r}")
    else:
        validate_config(build / "sdkconfig", required)
    metadata = json.loads((build / "project_description.json").read_text())
    if metadata["project_version"] != info["PROJECT_VER"]:
        raise ValueError(f"Build is version {metadata['project_version']!r}, "
                         f"variant {variant} expects {info['PROJECT_VER']!r}; wrong build directory?")
    data, files = load_flash_layout(build)
    table, ota = validate_ota_outputs(build)
    output.mkdir(parents=True, exist_ok=True)
    settings = data["flash_settings"]
    factory = output / "factory.bin"
    command = [sys.executable, "-m", "esptool", "--chip", "esp32h2", "merge_bin",
               "--flash_mode", settings["flash_mode"],
               "--flash_freq", settings["flash_freq"],
               "--flash_size", "4MB", "-o", str(factory)]
    for offset, file in files:
        command.extend([hex(offset), str(file)])
    subprocess.run(command, cwd=build, check=True)
    if not factory.is_file() or factory.stat().st_size == 0:
        raise ValueError("esptool did not produce a factory image")
    app = output / "esp_matter_base.bin"
    shutil.copyfile(build / data["app"]["file"], app)
    shutil.copyfile(table, output / "partition-table.bin")
    ota_name = f"{info['PRODUCT_NAME']}.ota"
    shutil.copyfile(ota, output / ota_name)
    manifest = {
        "chip": "esp32h2", "flash_bytes": FLASH_BYTES,
        "variant": variant, "product": info["PRODUCT_NAME"], "diagnostic": diagnostic,
        "version": metadata["project_version"],
        "version_number": int(info["PROJECT_VER_NUMBER"]),
        "factory_offset": "0x0", "app_offset": data["app"]["offset"],
        "flash_settings": settings,
        "ota_supported": True,
        "partition_table_offset": "0xc000",
        "ota_data_offset": "0x1d000", "ota_data_bytes": 8192,
        "files": {name: describe(output / name) for name in
                  (factory.name, app.name, "partition-table.bin", ota_name)},
    }
    (output / "manifest.json").write_text(json.dumps(manifest, indent=2) + "\n")
    shutil.copyfile(ROOT / "scripts" / "flash.ps1", output / "flash.ps1")
    for doc in info["DOCS"]:
        shutil.copyfile(ROOT / "docs" / doc, output / doc)
    shutil.copyfile(ROOT / "versions.env", output / "versions.env")
    for optional in [ROOT / "dependencies.lock", build / "sdkconfig"]:
        if optional.is_file():
            shutil.copyfile(optional, output / optional.name)
    print(f"Packaged {variant} ({info['PRODUCT_NAME']} {metadata['project_version']}) in {output}")


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--build-dir", type=Path, default=Path("build"))
    parser.add_argument("--output-dir", type=Path, default=Path("dist"))
    parser.add_argument("--variant", required=True, help="directory name below variants/")
    parser.add_argument("--diagnostic", action="store_true",
                        default=os.environ.get("DNIOT_DIAGNOSTIC") == "1",
                        help="expect the diagnostic overlay (Light-sleep off)")
    args = parser.parse_args()
    package(args.build_dir, args.output_dir, args.variant, args.diagnostic)
