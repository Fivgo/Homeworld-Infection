#!/usr/bin/env python3
"""make_icon.py - draw the Homeworld Infection icon (original art) as a 1024x1024 PNG.

usage: make_icon.py OUT.png [--ico OUT.ico] [--icns OUT.icns] [--sizes OUTDIR]
A dark rounded tile with a starfield, a green spiral (the infection) and a white arrowhead ship.
Deterministic: the starfield comes from a fixed seed. Needs Pillow.
"""
import math, random, sys
from PIL import Image, ImageDraw, ImageFilter

S = 1024

def draw():
    img = Image.new("RGBA", (S, S), (0, 0, 0, 0))
    # tile: rounded square, deep blue to black
    tile = Image.new("RGBA", (S, S), (0, 0, 0, 0))
    grad = Image.new("RGBA", (S, S))
    gp = grad.load()
    for y in range(S):
        for x in range(S):
            d = math.hypot(x - S * 0.42, y - S * 0.38) / (S * 0.85)
            t = min(1.0, d)
            gp[x, y] = (int(18 * (1 - t) + 4 * t), int(30 * (1 - t) + 6 * t), int(58 * (1 - t) + 14 * t), 255)
    mask = Image.new("L", (S, S), 0)
    ImageDraw.Draw(mask).rounded_rectangle((40, 40, S - 40, S - 40), radius=200, fill=255)
    tile.paste(grad, (0, 0), mask)
    # stars
    rnd = random.Random(1999)
    sd = ImageDraw.Draw(tile)
    for _ in range(170):
        x, y = rnd.uniform(90, S - 90), rnd.uniform(90, S - 90)
        r = rnd.choice((1.5, 2, 2, 2.5, 3.5))
        a = rnd.randint(110, 230)
        sd.ellipse((x - r, y - r, x + r, y + r), fill=(220, 230, 255, a))
    tile.putalpha(Image.composite(tile.getchannel("A"), Image.new("L", (S, S), 0), mask))
    img.alpha_composite(tile)
    # the spiral: a logarithmic arm, drawn as overlapping dots, with a glow
    arm = Image.new("RGBA", (S, S), (0, 0, 0, 0))
    ad = ImageDraw.Draw(arm)
    cx, cy = S * 0.5, S * 0.52
    for arm_i, phase in enumerate((0.0, math.pi)):
        for i in range(900):
            t = i / 900
            ang = phase + t * 3.2 * math.pi
            rad = 30 + 330 * t ** 1.15
            x, y = cx + rad * math.cos(ang), cy + rad * math.sin(ang) * 0.82
            w = (1 - t) * 34 + 6 if arm_i == 0 else (1 - t) * 22 + 4
            g = int(255 * (1 - 0.45 * t))
            ad.ellipse((x - w, y - w, x + w, y + w), fill=(60, g, 110, 235 if arm_i == 0 else 170))
    glow = arm.filter(ImageFilter.GaussianBlur(28))
    img.alpha_composite(glow)
    img.alpha_composite(arm)
    core = Image.new("RGBA", (S, S), (0, 0, 0, 0))
    ImageDraw.Draw(core).ellipse((cx - 70, cy - 60, cx + 70, cy + 60), fill=(190, 255, 200, 255))
    img.alpha_composite(core.filter(ImageFilter.GaussianBlur(22)))
    # the ship: a white arrowhead with a notch, crossing the spiral, and a short trail
    ship = Image.new("RGBA", (S, S), (0, 0, 0, 0))
    sdw = ImageDraw.Draw(ship)
    tip, wing_l, notch, wing_r = (770, 250), (470, 470), (560, 445), (610, 560)
    for k in range(16):  # trail
        f = k / 16
        x, y = 560 - 260 * f, 470 + 190 * f
        r = 26 * (1 - f) + 4
        sdw.ellipse((x - r, y - r, x + r, y + r), fill=(200, 230, 255, int(150 * (1 - f))))
    sdw.polygon([tip, wing_l, notch, wing_r], fill=(245, 250, 255, 255))
    sdw.polygon([tip, notch, wing_r], fill=(196, 214, 236, 255))
    img.alpha_composite(ship.filter(ImageFilter.GaussianBlur(10)))
    img.alpha_composite(ship)
    return img

def main(argv):
    if len(argv) < 2:
        print(__doc__); return 2
    img = draw()
    img.save(argv[1])
    args = argv[2:]
    for flag, path in zip(args[::2], args[1::2]):
        if flag == "--ico":
            img.save(path, sizes=[(16, 16), (24, 24), (32, 32), (48, 48), (64, 64), (128, 128), (256, 256)])
        elif flag == "--icns":
            img.save(path)
        elif flag == "--sizes":
            import os
            os.makedirs(path, exist_ok=True)
            for n in (16, 32, 48, 64, 128, 256, 512):
                img.resize((n, n), Image.LANCZOS).save(os.path.join(path, f"homeworld-infection-{n}.png"))
    return 0

if __name__ == "__main__":
    sys.exit(main(sys.argv))
