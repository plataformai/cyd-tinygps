#!/usr/bin/env python3
"""Regenerate beeline_background.h from assets/background.png.

Development-time tool only. Needs ImageMagick; the Arduino sketch does not.
"""

from pathlib import Path
import shutil
import subprocess

ROOT = Path(__file__).resolve().parents[1]
SOURCE = ROOT / "assets/background.png"
DESTINATION = ROOT / "beeline_background.h"
WIDTH, HEIGHT = 320, 240
BRIGHTNESS_PERCENT = 75


def main():
    converter = shutil.which("magick") or shutil.which("convert")
    if not converter:
        raise SystemExit("Install ImageMagick to regenerate the RGB565 background.")
    command = [converter]
    if Path(converter).name == "magick":
        command.append("convert")
    command += [str(SOURCE), "-alpha", "remove", "-background", "#101a12",
                "-resize", f"{WIDTH}x{HEIGHT}!", "-depth", "8", "RGB:-"]
    raw = subprocess.run(command, check=True, stdout=subprocess.PIPE).stdout
    if len(raw) != WIDTH * HEIGHT * 3:
        raise SystemExit(f"Unexpected RGB image size: {len(raw)} bytes")

    words = []
    for offset in range(0, len(raw), 3):
        red, green, blue = (
            (channel * BRIGHTNESS_PERCENT + 50) // 100
            for channel in raw[offset:offset + 3]
        )
        words.append(((red >> 3) << 11) | ((green >> 2) << 5) | (blue >> 3))

    lines = [
        "#pragma once",
        "#include <pgmspace.h>",
        "#include <stdint.h>",
        "",
        f"static constexpr uint16_t BEE_LINE_BACKGROUND_WIDTH = {WIDTH};",
        f"static constexpr uint16_t BEE_LINE_BACKGROUND_HEIGHT = {HEIGHT};",
        f"static const uint16_t BEE_LINE_BACKGROUND_PIXELS[{WIDTH * HEIGHT}] PROGMEM = {{",
    ]
    for row in range(HEIGHT):
        start = row * WIDTH
        lines.append("  " + ", ".join(f"0x{value:04X}" for value in words[start:start + WIDTH]) + ",")
    lines += ["};", ""]
    DESTINATION.write_text("\n".join(lines))
    print(f"Wrote {DESTINATION} ({DESTINATION.stat().st_size:,} bytes of source, "
          f"{WIDTH * HEIGHT * 2:,} bytes of RGB565 flash data).")


if __name__ == "__main__":
    main()
