#!/usr/bin/env python3
"""Compare a UI-scaled capture with a reference capture at its logical size, region by region.

M3-R1 ui-scale (HWI-FEATURE(ui-scale), docs/m3-r1-ui-scale.md): a frame drawn at a physical size with a UI scale (e.g.
3840x2160 at 200%: logical 1920x1080) is box-downscaled to the reference frame's size (a capture at the logical size
with the scale 100%, e.g. 1920x1080) and compared over named UI regions. The downscale is an area average: each
reference pixel (x, y) takes the mean of the scaled frame's pixels over [x * fx, (x + 1) * fx) x [y * fy, (y + 1) * fy),
fx = Ws / Wr, fy = Hs / Hr, partial pixels weighted by their covered area (fx = fy = 2: the 2x2 block mean of
tools/ppm_region.py down2; 2560x1440 against 1707x960: fx 1.49971, fy 1.5). --sample centre takes instead the scaled
frame's pixel at each reference pixel's centre ((x + 0.5) * fx, (y + 0.5) * fy): what a nearest-filtered scale draws there
(the measure of a non-integer scale, where an area average mixes a glyph's edge with its neighbour). Standard library
only; run with python3 -I.

  python3 -I tools/ui_scale_compare.py SCALED REFERENCE [--preset fe|hud] [--region NAME=x0,y0,x1,y1 ...]
          [--tol T] [--min-match F] [--sample area|centre] [--out DIR]

REGIONS are in the REFERENCE frame's pixels (rows from the top, x1 / y1 exclusive). --preset fe: Main_game_screen's
640x480 art centred on the reference size (the exe's centring, feAllScreensReposition 0x00474ae0): the menu buttons with
their text, the title, the version label, the whole 640x480 panel, the cursor (0,0); --preset hud: the hud self-test's
frame (the info panel, the selected / selecting lists, the support-units box, the cursor), anchored as the exe anchors them
(left / right / bottom of the logical screen). Per region one line: the pixel count, match= the share of pixels whose
largest channel difference is within T (default 24 of 255), mean= the mean of that difference, p99=, max=, and
PASS / FAIL against --min-match (default 0.98). Exit 1 when a region FAILs. --out writes, per region, the downscaled crop
and the reference crop side by side as a PPM (game-derived: keep it under original/scratch-m2).
"""
import argparse
import importlib.util
import os
import sys

_spec = importlib.util.spec_from_file_location("ppm_region", os.path.join(os.path.dirname(os.path.abspath(__file__)),
                                                                          "ppm_region.py"))
ppm_region = importlib.util.module_from_spec(_spec)
_spec.loader.exec_module(ppm_region)


def fe_regions(w, h):
    ox, oy = (w - 640) // 2, (h - 480) // 2
    def r(x0, y0, x1, y1):
        return (x0 + ox, y0 + oy, x1 + ox, y1 + oy)
    return [("fe-buttons", r(217, 168, 425, 397)),        # the six buttons and their labels (front_end.fib)
            ("fe-title", r(35, 29, 608, 164)),
            ("fe-version-label", r(37, -2, 175, 29)),     # "Version Emergence 1.01" (FE_DrawVersionNum)
            ("fe-panel", r(0, 0, 640, 480)),
            ("cursor", (0, 0, 24, 24))]


def hud_regions(w, h):
    # the hud self-test's frame (tools/m2_harness.sh --mode hud): the ship info panel at the left, the selected /
    # selecting lists at the top right, the support-units box at the bottom right, the cursor at the origin; the
    # sizes are the 1024x768 frame's, anchored as the exe anchors them
    return [("hud-info-panel", (19, 189, 229, 335)),
            ("hud-lists", (w - 152, 10, w, 382)),
            ("hud-support", (w - 102, h - 58, w, h)),
            ("cursor", (0, 0, 24, 24))]


def area_mean(sw, spix, x0, x1, y0, y1):
    """The area-weighted mean colour of the scaled frame over [x0, x1) x [y0, y1) (float pixel coordinates)."""
    acc = [0.0, 0.0, 0.0]
    total = 0.0
    yy = int(y0)
    while yy < y1:
        wy = min(y1, yy + 1) - max(y0, yy)
        xx = int(x0)
        while xx < x1:
            wx = min(x1, xx + 1) - max(x0, xx)
            wgt = wx * wy
            if wgt > 0:
                o = (yy * sw + xx) * 3
                acc[0] += spix[o] * wgt
                acc[1] += spix[o + 1] * wgt
                acc[2] += spix[o + 2] * wgt
                total += wgt
            xx += 1
        yy += 1
    return [int(c / total + 0.5) for c in acc]


def compare(sw, sh, spix, rw, rh, rpix, reg, tol, centre=False):
    fx, fy = sw / rw, sh / rh
    x0, y0, x1, y1 = max(0, reg[0]), max(0, reg[1]), min(rw, reg[2]), min(rh, reg[3])
    diffs = []
    down = bytearray()
    ref = bytearray()
    for y in range(y0, y1):
        for x in range(x0, x1):
            if centre:
                so = (int((y + 0.5) * fy) * sw + int((x + 0.5) * fx)) * 3
                c = list(spix[so:so + 3])
            else:
                c = area_mean(sw, spix, x * fx, (x + 1) * fx, y * fy, (y + 1) * fy)
            o = (y * rw + x) * 3
            r = rpix[o:o + 3]
            diffs.append(max(abs(c[0] - r[0]), abs(c[1] - r[1]), abs(c[2] - r[2])))
            down += bytes(c)
            ref += r
    n = len(diffs)
    if n == 0:
        return None
    s = sorted(diffs)
    return {"n": n, "match": sum(1 for d in diffs if d <= tol) / n, "mean": sum(diffs) / n, "p99": s[min(n - 1, int(n * 0.99))],
            "max": s[-1], "w": x1 - x0, "h": y1 - y0, "down": down, "ref": ref}


def main():
    ap = argparse.ArgumentParser(description="compare a UI-scaled capture with a logical-size reference")
    ap.add_argument("scaled")
    ap.add_argument("reference")
    ap.add_argument("--preset", choices=["fe", "hud"])
    ap.add_argument("--region", action="append", default=[])
    ap.add_argument("--tol", type=int, default=24)
    ap.add_argument("--min-match", type=float, default=0.98)
    ap.add_argument("--sample", choices=["area", "centre"], default="area")
    ap.add_argument("--out")
    a = ap.parse_args()
    sw, sh, spix = ppm_region.read_ppm(a.scaled)
    rw, rh, rpix = ppm_region.read_ppm(a.reference)
    regions = []
    if a.preset == "fe":
        regions += fe_regions(rw, rh)
    elif a.preset == "hud":
        regions += hud_regions(rw, rh)
    for arg in a.region:
        name, coords = arg.split("=", 1)
        regions.append((name, tuple(int(v) for v in coords.split(","))))
    print("scaled=%dx%d reference=%dx%d factor=%.5fx%.5f sample=%s tol=%d min_match=%.3f" % (
        sw, sh, rw, rh, sw / rw, sh / rh, a.sample, a.tol, a.min_match))
    fails = 0
    for name, reg in regions:
        res = compare(sw, sh, spix, rw, rh, rpix, reg, a.tol, a.sample == "centre")
        if res is None:
            print("region=%s rect=%s empty" % (name, ",".join(map(str, reg))))
            continue
        ok = res["match"] >= a.min_match
        fails += 0 if ok else 1
        print("region=%s rect=%s pixels=%d match=%.4f mean=%.2f p99=%d max=%d %s" % (
            name, ",".join(map(str, reg)), res["n"], res["match"], res["mean"], res["p99"], res["max"],
            "PASS" if ok else "FAIL"))
        if a.out:
            os.makedirs(a.out, exist_ok=True)
            w, h = res["w"], res["h"]
            side = bytearray()
            for y in range(h):
                side += res["down"][y * w * 3:(y + 1) * w * 3] + bytes([255, 0, 255]) * 4 + res["ref"][y * w * 3:(y + 1) * w * 3]
            ppm_region.write_ppm(os.path.join(a.out, "%s.ppm" % name), w * 2 + 4, h, bytes(side))
    print("result=%s regions=%d failed=%d" % ("PASS" if fails == 0 else "FAIL", len(regions), fails))
    return 1 if fails else 0


if __name__ == "__main__":
    sys.exit(main())
