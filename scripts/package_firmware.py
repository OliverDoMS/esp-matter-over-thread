"""Package only actual ESP-IDF build outputs, using generated flash offsets."""
from __future__ import annotations

import argparse
import hashlib
import json
from pathlib import Path
import shutil
import subprocess
import sys

FLASH_BYTES = 4 * 1024 * 1024
REQUIRED_CONFIG = {
    "CONFIG_IDF_TARGET": '"esp32h2"',
    "CONFIG_ESPTOOLPY_FLASHSIZE_4MB": "y",
    "CONFIG_OPENTHREAD_MTD": "y",
    "CONFIG_ENABLE_ICD_SERVER": "y",
    "CONFIG_PM_ENABLE": "y",
    "CONFIG_FREERTOS_USE_TICKLESS_IDLE": "y",
    "CONFIG_IEEE802154_SLEEP_ENABLE": "y",
    "CONFIG_USJ_NO_AUTO_LS_ON_CONNECTION": "y",
    "CONFIG_BASE_DIAGNOSTIC_NO_SLEEP": "y",
    "CONFIG_BASE_SOIL_MOISTURE": "y",
    "CONFIG_BASE_BUTTON_LED_FEEDBACK": "y",
    "CONFIG_BASE_TEST_CONTACT": "n",
    "CONFIG_PM_POWER_DOWN_PERIPHERAL_IN_LIGHT_SLEEP": "n",
}


def validate_config(path: Path) -> None:
    values = dict(line.split("=", 1) for line in path.read_text().splitlines()
                  if line.startswith("CONFIG_") and "=" in line)
    for key, expected in REQUIRED_CONFIG.items():
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


def package(build: Path, output: Path) -> None:
    build = build.resolve()
    output = output.resolve()
    # ESP-IDF copies the resolved project configuration into the build directory.
    config = build / "config" / "sdkconfig.json"
    if config.exists():
        config_values = json.loads(config.read_text())
        for name, expected in REQUIRED_CONFIG.items():
            value = config_values.get(name.removeprefix("CONFIG_"), False if expected == "n" else None)
            wanted = {"y": True, "n": False}.get(expected, expected.strip('"'))
            if value != wanted:
                raise ValueError(f"Resolved sdkconfig mismatch: {name}={value!r}")
    else:
        validate_config(build.parent / "sdkconfig")
    data, files = load_flash_layout(build)
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
    metadata = json.loads((build / "project_description.json").read_text())
    manifest = {
        "chip": "esp32h2", "flash_bytes": FLASH_BYTES,
        "version": metadata["project_version"],
        "factory_offset": "0x0", "app_offset": data["app"]["offset"],
        "flash_settings": settings,
        "files": {factory.name: describe(factory), app.name: describe(app)},
    }
    (output / "manifest.json").write_text(json.dumps(manifest, indent=2) + "\n")
    root = Path(__file__).resolve().parents[1]
    shutil.copyfile(root / "scripts" / "flash.ps1", output / "flash.ps1")
    shutil.copyfile(root / "docs" / "FLASHEN.md", output / "FLASHEN.md")
    shutil.copyfile(root / "docs" / "BODENFEUCHTE.md", output / "BODENFEUCHTE.md")
    shutil.copyfile(root / "versions.env", output / "versions.env")
    for optional in [root / "dependencies.lock", root / "sdkconfig"]:
        if optional.is_file():
            shutil.copyfile(optional, output / optional.name)
    print(f"Packaged real build outputs in {output}")


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--build-dir", type=Path, default=Path("build"))
    parser.add_argument("--output-dir", type=Path, default=Path("dist"))
    args = parser.parse_args()
    package(args.build_dir, args.output_dir)
