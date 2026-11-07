#!/usr/bin/env python3
"""Render a captured trace of production TFT calls. Text fonts are representative.

ARDUINOJSON_DIR=/path/to/ArduinoJson python scripts/render-beeline-preview.py
Requires Pillow; see scripts/beeline-font-requirements.txt.
"""
import os
from pathlib import Path
import subprocess
import tempfile
from PIL import Image, ImageDraw, ImageFont

ROOT = Path(__file__).resolve().parents[1]
TESTS = ROOT / "artifacts/beeline-navigator/firmware/tests"
OUT = ROOT / "generated-artifacts"
FONTS = Path("/usr/share/fonts/truetype/dejavu")


def rgb565(value):
    color = int(value)
    return ((color >> 11) * 255 // 31,
            ((color >> 5) & 63) * 255 // 63, (color & 31) * 255 // 31)


def main():
    library = Path(os.environ["ARDUINOJSON_DIR"])
    with tempfile.TemporaryDirectory() as temp:
        binary = Path(temp) / "render"
        subprocess.run([
            os.environ.get("CXX", "g++"), "-std=c++17", "-Wall", "-Wextra", "-Werror",
            "-pedantic", "-DARDUINOJSON_ENABLE_ARDUINO_STRING=0",
            "-I" + str(TESTS / "stubs"), "-I" + str(library / "src"),
            str(TESTS / "render-preview.cpp"), "-o", str(binary),
        ], check=True)
        trace = subprocess.check_output([str(binary)], text=True)
    OUT.mkdir(exist_ok=True)
    (OUT / "beeline-preview-trace.tsv").write_text(trace)
    frames = []
    for line in trace.splitlines():
        op, *parts = line.split("\t")
        if op == "frame":
            image = Image.new("RGB", (320, 240))
            draw = ImageDraw.Draw(image)
            frames.append((parts[0], image))
        elif op == "screen":
            draw.rectangle((0, 0, 319, 239), fill=rgb565(parts[0]))
        elif op == "rect":
            x, y, w, h, color = map(int, parts)
            draw.rectangle((x, y, x + w - 1, y + h - 1), fill=rgb565(color))
        elif op == "circle":
            x, y, r, color = map(int, parts)
            draw.ellipse((x-r, y-r, x+r, y+r), fill=rgb565(color))
        elif op == "triangle":
            x0, y0, x1, y1, x2, y2, color = map(int, parts)
            draw.polygon([(x0, y0), (x1, y1), (x2, y2)], fill=rgb565(color))
        elif op == "line":
            x0, y0, x1, y1, color = map(int, parts)
            draw.line(((x0, y0), (x1, y1)), fill=rgb565(color))
        elif op in ("vline", "hline"):
            x, y, length, color = map(int, parts)
            end = (x, y + length - 1) if op == "vline" else (x + length - 1, y)
            draw.line(((x, y), end), fill=rgb565(color))
        elif op == "text":
            x, y, number, color, datum = map(int, parts[:5])
            text = bytes.fromhex(parts[5]).decode("utf-8")
            # TFT built-ins are not desktop TTFs. Keep the same boxes/datums
            # while making the small labels legible in a validation preview.
            size = {1: 8, 2: 12, 4: 24}[number]
            font = ImageFont.truetype(str(FONTS / "DejaVuSans.ttf"), size)
            anchor = {0: "lt", 1: "mt", 2: "mm"}[datum]
            draw.text((x, y), text, font=font, fill=rgb565(color), anchor=anchor)
    day_frame = next((frame for name, frame in frames if name.startswith("MODO DIA")), None)
    if day_frame is None:
        raise RuntimeError("The firmware trace did not include the daytime preview.")
    day_frame.save(OUT / "beeline-dia-preview.png")
    sheet = Image.new("RGB", (1360, 1150), "#101318")
    draw = ImageDraw.Draw(sheet)
    title = ImageFont.truetype(str(FONTS / "DejaVuSans.ttf"), 24)
    caption = ImageFont.truetype(str(FONTS / "DejaVuSans.ttf"), 18)
    draw.text((24, 16), "ESP32 - proposta de exibicao (320 x 240)", font=title, fill="white")
    for i, (name, frame) in enumerate(frames):
        if name.startswith("MODO DIA"):
            continue
        x, y = 24 + (i % 2) * 668, 65 + (i // 2) * 530
        draw.text((x, y), name, font=caption, fill="#f6d742")
        sheet.paste(frame.resize((640, 480), Image.Resampling.NEAREST), (x, y + 30))
        frame.save(OUT / f"beeline-preview-{i + 1}.png")
    draw.text((24, 1130), "Simulacao dos comandos reais do firmware; fontes representativas, nao foto do painel.",
              font=caption, fill="#a8aeb5")
    sheet.save(OUT / "beeline-validacao-visual.png")
    print("Created generated-artifacts/beeline-validacao-visual.png")
    print("Created generated-artifacts/beeline-dia-preview.png")


if __name__ == "__main__":
    main()
