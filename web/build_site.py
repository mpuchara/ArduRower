#!/usr/bin/env python3
"""Build GitHub Pages bundle with *complete* ESP32 serial-flash images.

Run after building the confirmed ST7789/GPIO21 PlatformIO environment (pio run -d cyd_rower -e ...).
Do not publish app-only firmware.bin at offset 0.
"""
import json
import os
from pathlib import Path
import shutil
import subprocess
import sys

ROOT = Path(__file__).resolve().parents[1]
SITE = ROOT / "site"
ENVIRONMENTS = {
    "st7789-gpio21": "esp32-2432s028-st7789-gpio21",
}
VERSION = "0.08-ghost-" + os.environ.get("GITHUB_SHA", "local")[:7]


def require(path):
    if not path.is_file() or path.stat().st_size == 0:
        raise FileNotFoundError(f"Missing compiled ESP32 flash image: {path}")
    return path


def main():
    framework = Path.home() / ".platformio/packages/framework-arduinoespressif32"
    boot_app0 = require(framework / "tools/partitions/boot_app0.bin")
    require(ROOT / "index.html")
    if SITE.exists():
        shutil.rmtree(SITE)
    (SITE / "firmware").mkdir(parents=True)
    (SITE / "manifests").mkdir(parents=True)
    shutil.copyfile(ROOT / "index.html", SITE / "index.html")
    (SITE / ".nojekyll").write_text("", encoding="utf-8")
    version_info = {"version": VERSION, "revision": os.environ.get("GITHUB_SHA", "local")}
    (SITE / "version.json").write_text(json.dumps(version_info, indent=2) + "\n", encoding="utf-8")

    for slug, env in ENVIRONMENTS.items():
        build = ROOT / "cyd_rower/.pio/build" / env
        bootloader = require(build / "bootloader.bin")
        partitions = require(build / "partitions.bin")
        app = require(build / "firmware.bin")
        output = SITE / "firmware" / f"{slug}.bin"
        # PlatformIO's standard Arduino ESP32 address map; merged image is flashed at 0x0.
        cmd = [
            sys.executable, "-m", "esptool", "--chip", "esp32", "merge_bin",
            "--flash_mode", "dio", "--flash_freq", "40m", "--flash_size", "4MB",
            "-o", str(output),
            "0x1000", str(bootloader),
            "0x8000", str(partitions),
            "0xe000", str(boot_app0),
            "0x10000", str(app),
        ]
        subprocess.run(cmd, check=True)
        require(output)
        if output.stat().st_size > 4 * 1024 * 1024:
            raise RuntimeError(f"{output} exceeds target 4MB flash")
        manifest = {
            "name": "ArduRower CYD " + slug,
            "version": VERSION,
            "new_install_prompt_erase": False,  # preserve Ghost Race records in NVS during updates
            "new_install_improv_wait_time": 0,
            "builds": [{
                "chipFamily": "ESP32",
                "improv": False,
                "parts": [{"path": f"../firmware/{slug}.bin", "offset": 0}],
            }],
        }
        (SITE / "manifests" / f"{slug}.json").write_text(
            json.dumps(manifest, indent=2, ensure_ascii=False) + "\n", encoding="utf-8"
        )
        print(f"{env}: {output.stat().st_size} bytes; ESP32 @ offset 0; {VERSION}")
    print("Pages bundle ready:", SITE)


if __name__ == "__main__":
    main()
