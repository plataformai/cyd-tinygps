#!/usr/bin/env python3
"""Create the standalone Arduino download with its required image header."""

from pathlib import Path
import zipfile

ROOT = Path(__file__).resolve().parents[1]
FIRMWARE = ROOT / "artifacts/beeline-navigator/firmware"
OUTPUT_DIR = ROOT / "generated-artifacts"
FILES = (
    "beeline-esp32.ino",
    "beeline_background.h",
    "beeline_background_renderer.h",
    "beeline_color_test.h",
    "README.md",
    "assets/background.png",
    "assets/README.md",
    "tools/generate-background.py",
)


def main():
    for name in FILES:
        if not (FIRMWARE / name).is_file():
            raise SystemExit(f"Required firmware file is missing: {name}")

    OUTPUT_DIR.mkdir(exist_ok=True)
    target = OUTPUT_DIR / "beeline-firmware.zip"
    with zipfile.ZipFile(target, "w", zipfile.ZIP_DEFLATED) as archive:
        for name in FILES:
            info = zipfile.ZipInfo("beeline-esp32/" + name,
                                   (2026, 1, 1, 0, 0, 0))
            info.compress_type = zipfile.ZIP_DEFLATED
            info.external_attr = 0o100644 << 16
            archive.writestr(info, (FIRMWARE / name).read_bytes())
    print(f"Created {target.relative_to(ROOT)} ({target.stat().st_size:,} bytes)")


if __name__ == "__main__":
    main()
