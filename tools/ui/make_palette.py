"""Builds the pixel art palette from screenshots of the smooth interface.

    BouquetTest --snapshot plain.png synth 3 1
    BouquetTest --snapshot real.png synth 3 1 real   (and meadow, realmeadow ...)
    python3 tools/ui/make_palette.py plain.png real.png ...

Prints the palette for Source/ui/PixelArt.cpp: the interface colours exactly, then the
colours that best cover the screenshots (k-means, in Pillow), merged where two are
too close to tell apart on the pixel grid.
"""
import sys
from PIL import Image

# Kept exactly: paper, ink and the accents (BouquetLookAndFeel.h).
FIXED = [0xfbf7f0, 0xefe7da, 0xffffff, 0x2b2631, 0xe8735a, 0xf4a77c, 0xe4789b, 0xf2c14e,
         0x6fa8f5, 0xa58cf0, 0x5cc9a7, 0x8a6fe6, 0x6f86f0, 0x4fb8c9]
SIZE = 64          # colours in all
MIN_DISTANCE = 14  # closer than this (RGB) to a colour already chosen: dropped


def rgb(c):
    return (c >> 16) & 255, (c >> 8) & 255, c & 255


def distance(a, b):
    return sum((x - y) ** 2 for x, y in zip(a, b)) ** 0.5


def main(paths):
    # Every screenshot side by side, halved (the pixel grid's resolution).
    images = [Image.open(p).convert("RGB") for p in paths]
    images = [im.resize((im.width // 2, im.height // 2), Image.BOX) for im in images]
    sheet = Image.new("RGB", (sum(im.width for im in images), max(im.height for im in images)))
    x = 0
    for im in images:
        sheet.paste(im, (x, 0))
        x += im.width

    # Twice as many candidates as needed, the most used first.
    quantised = sheet.quantize(colors=SIZE * 2, method=Image.Quantize.MEDIANCUT, kmeans=4)
    found = quantised.getpalette()[: SIZE * 6]
    candidates = [tuple(found[i:i + 3]) for i in range(0, len(found), 3)]
    counts = sorted(quantised.getcolors(), reverse=True)

    chosen = [rgb(c) for c in FIXED]
    for _, index in counts:
        c = candidates[index]
        if len(chosen) >= SIZE:
            break
        if all(distance(c, o) >= MIN_DISTANCE for o in chosen):
            chosen.append(c)

    chosen = chosen[: len(FIXED)] + sorted(chosen[len(FIXED):], key=lambda c: (0.3 * c[0] + 0.59 * c[1] + 0.11 * c[2]))
    words = ["0xff%02x%02x%02x" % c for c in chosen]
    for i in range(0, len(words), 8):
        print("        " + ", ".join(words[i:i + 8]) + ",")


if __name__ == "__main__":
    main(sys.argv[1:])
