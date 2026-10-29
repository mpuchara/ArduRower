#!/usr/bin/env python3
"""Build a GitHub Pages ESP Web Tools bundle without erasing NVS.

IMPORTANT: do not flash a merged image at address 0. The padding between
partition table 0x8000 and boot_app0 0xe000 includes NVS at 0x9000. Flashing
that padding on every update would erase the user's saved Ghost Race records.
Instead flash only the four actual binary parts at their respective offsets.
"""
import json
import os
from pathlib import Path
import shutil

ROOT = Path(__file__).resolve().parents[1]
SITE = ROOT / "site"
ENVIRONMENTS = {
    "st7789-gpio21": "esp32-2432s028-st7789-gpio21",
}
REVISION = os.environ.get("GITHUB_SHA", "local")[:12]
VERSION = "0.09.3-hr-link-" + REVISION


def require(path):
    if not path.is_file() or path.stat().st_size == 0:
        raise FileNotFoundError(f"Missing firmware component: {path}")
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
    version_info = {"version": VERSION, "revision": REVISION}
    (SITE / "version.json").write_text(json.dumps(version_info, indent=2) + "\n", encoding="utf-8")

    for slug, env in ENVIRONMENTS.items():
        build = ROOT / "cyd_rower/.pio/build" / env
        parts = [
            (0x1000, "bootloader", require(build / "bootloader.bin")),
            (0x8000, "partitions", require(build / "partitions.bin")),
            (0xE000, "boot_app0", boot_app0),
            (0x10000, "firmware", require(build / "firmware.bin")),
        ]
        manifest_parts = []
        for offset, name, source in parts:
            destination = SITE / "firmware" / f"{slug}-{name}-{REVISION}.bin"
            shutil.copyfile(source, destination)
            if offset + destination.stat().st_size > 0x400000:
                raise ValueError(f"Flash component exceeds 4 MB: {source}")
            manifest_parts.append({"path": f"../firmware/{destination.name}", "offset": offset})
        assert any(REVISION in p["path"] for p in manifest_parts)
        # NVS lives between 0x9000 and 0xDFFF. Neither the component ranges
        # nor full-flash erasure should touch that region in regular upgrades.
        assert all((part["offset"] >= 0xE000 or
                    part["offset"] + parts[i][2].stat().st_size <= 0x9000)
                   for i, part in enumerate(manifest_parts))
        manifest = {
            "name": "ArduRower CYD " + VERSION + " " + slug,
            "version": VERSION,
            "new_install_prompt_erase": False,
            "builds": [{
                "chipFamily": "ESP32",
                "improv": False,
                "parts": manifest_parts,
            }],
        }
        (SITE / "manifests" / f"{slug}.json").write_text(
            json.dumps(manifest, indent=2, ensure_ascii=False) + "\n", encoding="utf-8"
        )
        print(f"{env}: {len(manifest_parts)} separate parts, preserving NVS; {VERSION}")
    print("Pages bundle ready:", SITE)


if __name__ == "__main__":
    main()
