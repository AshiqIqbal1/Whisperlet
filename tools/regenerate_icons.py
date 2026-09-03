#!/usr/bin/env python3
"""Regenerate every app icon from assets/logo-source.png.

Outputs the PNG sizes, the Windows .ico, and the macOS iconset. Turn the
iconset into the bundle icon with:

    iconutil -c icns assets/whisperlet.iconset -o assets/whisperlet.icns
"""
from PIL import Image, ImageDraw
import os

ROOT = os.path.join(os.path.dirname(__file__), "..", "assets")
src = Image.open(os.path.join(ROOT, "logo-source.png")).convert("RGBA")


def rounded(size: int) -> Image.Image:
    """Square icon with the rounded corners macOS and Windows both expect."""
    im = src.resize((size, size), Image.LANCZOS)
    mask = Image.new("L", (size, size), 0)
    ImageDraw.Draw(mask).rounded_rectangle(
        [0, 0, size - 1, size - 1], radius=int(size * 0.225), fill=255)
    out = Image.new("RGBA", (size, size), (0, 0, 0, 0))
    out.paste(im, (0, 0), mask)
    return out


sizes = [16, 32, 48, 64, 128, 256, 512, 1024]
icons = {s: rounded(s) for s in sizes}

for s, im in icons.items():
    im.save(os.path.join(ROOT, f"icon-{s}.png"))

icons[256].save(os.path.join(ROOT, "whisperlet.ico"),
                sizes=[(16, 16), (32, 32), (48, 48), (64, 64), (128, 128), (256, 256)])

iconset = os.path.join(ROOT, "whisperlet.iconset")
os.makedirs(iconset, exist_ok=True)
for pt in [16, 32, 128, 256, 512]:
    icons[pt].save(os.path.join(iconset, f"icon_{pt}x{pt}.png"))
    icons[pt * 2].save(os.path.join(iconset, f"icon_{pt}x{pt}@2x.png"))

print("regenerated icons from logo-source.png")
