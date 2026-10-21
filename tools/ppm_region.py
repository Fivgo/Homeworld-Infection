#!/usr/bin/env python3
"""Pixel statistics, probes, diffs and a 2x box downscale for binary PPM (P6) captures.

M3-R1 letterbox-front: the shared reader of the harness's frame captures (tools/m2_harness.sh --shot, /catShot;
/catMotionShot's <dump>.<frame>.ppm). Standard library only; run with python3 -I. Rows count from the TOP (row 0 is the
first row of the file, as the captures are written top row first). A REGION is x0,y0,x1,y1 with x1 / y1 EXCLUSIVE
(default: the whole image). A colour is r,g,b.

  python3 -I tools/ppm_region.py info FILE
  python3 -I tools/ppm_region.py stats FILE [--region x0,y0,x1,y1] [--hist N] [--colour r,g,b [--tol T]]
        mean / min / max per channel, the count of distinct colours, the N most common colours (--hist, default 5);
        with --colour: how many pixels lie within T (default 0, the largest channel difference) of that colour and
        how many do not ("match=" / "other=")
  python3 -I tools/ppm_region.py probe FILE x,y [x,y ...]
        the colour of each pixel
  python3 -I tools/ppm_region.py diff A B [--region x0,y0,x1,y1] [--tol T] [--out DIFF.ppm]
        pixels whose largest channel difference exceeds T (default 0): count, bounding box, largest difference;
        exit 1 when any differ (0: identical within T); --out writes a mask (white = differs)
  python3 -I tools/ppm_region.py down2 IN OUT
        a 2x box downscale (each output pixel the rounded mean of a 2x2 block; an odd last row / column is dropped):
        a 3840x2160 capture compared with a 1920x1080 one
  python3 -I tools/ppm_region.py bands FILE [--colour r,g,b] [--tol T]
        the rows from the top and from the bottom that are entirely that colour (default 0,0,0): the letterbox bars

Every command prints one "key=value ..." line per result, so a report can quote it.
"""
import argparse
import sys
from collections import Counter


def read_ppm(path):
    with open(path, "rb") as f:
        data = f.read()
    tokens = []
    pos = 0
    while len(tokens) < 4:
        while data[pos:pos + 1].isspace():
            pos += 1
        if data[pos:pos + 1] == b"#":
            while data[pos:pos + 1] not in (b"\n", b""):
                pos += 1
            continue
        start = pos
        while not data[pos:pos + 1].isspace():
            pos += 1
        tokens.append(data[start:pos])
    if tokens[0] != b"P6":
        sys.exit("ppm_region: %s is not a binary PPM (P6)" % path)
    w, h, maxval = int(tokens[1]), int(tokens[2]), int(tokens[3])
    if maxval != 255:
        sys.exit("ppm_region: %s: maxval %d (only 255)" % (path, maxval))
    pos += 1
    pix = data[pos:pos + w * h * 3]
    if len(pix) != w * h * 3:
        sys.exit("ppm_region: %s is short (%d of %d bytes)" % (path, len(pix), w * h * 3))
    return w, h, pix


def write_ppm(path, w, h, pix):
    with open(path, "wb") as f:
        f.write(b"P6\n%d %d\n255\n" % (w, h))
        f.write(bytes(pix))


def region(arg, w, h):
    if not arg:
        return 0, 0, w, h
    x0, y0, x1, y1 = (int(v) for v in arg.split(","))
    return max(0, x0), max(0, y0), min(w, x1), min(h, y1)


def colour(arg):
    return tuple(int(v) for v in arg.split(","))


def pixels(w, pix, reg):
    x0, y0, x1, y1 = reg
    for y in range(y0, y1):
        row = pix[(y * w + x0) * 3:(y * w + x1) * 3]
        for i in range(0, len(row), 3):
            yield row[i], row[i + 1], row[i + 2]


def near(c, ref, tol):
    return max(abs(c[0] - ref[0]), abs(c[1] - ref[1]), abs(c[2] - ref[2])) <= tol


def cmd_info(a):
    w, h, _ = read_ppm(a.file)
    print("file=%s width=%d height=%d" % (a.file, w, h))


def cmd_stats(a):
    w, h, pix = read_ppm(a.file)
    reg = region(a.region, w, h)
    n = 0
    s = [0, 0, 0]
    lo = [255, 255, 255]
    hi = [0, 0, 0]
    hist = Counter()
    match = 0
    ref = colour(a.colour) if a.colour else None
    for c in pixels(w, pix, reg):
        n += 1
        hist[c] += 1
        for k in range(3):
            s[k] += c[k]
            lo[k] = min(lo[k], c[k])
            hi[k] = max(hi[k], c[k])
        if ref is not None and near(c, ref, a.tol):
            match += 1
    if n == 0:
        print("region=%d,%d,%d,%d pixels=0" % reg)
        return
    print("region=%d,%d,%d,%d pixels=%d mean=%.2f,%.2f,%.2f min=%d,%d,%d max=%d,%d,%d colours=%d" %
          (reg + (n, s[0] / n, s[1] / n, s[2] / n) + tuple(lo) + tuple(hi) + (len(hist),)))
    for c, k in hist.most_common(a.hist):
        print("hist colour=%d,%d,%d count=%d" % (c + (k,)))
    if ref is not None:
        print("colour=%d,%d,%d tol=%d match=%d other=%d" % (ref + (a.tol, match, n - match)))


def cmd_probe(a):
    w, h, pix = read_ppm(a.file)
    for p in a.points:
        x, y = (int(v) for v in p.split(","))
        if not (0 <= x < w and 0 <= y < h):
            print("x=%d y=%d outside=%dx%d" % (x, y, w, h))
            continue
        i = (y * w + x) * 3
        print("x=%d y=%d colour=%d,%d,%d" % (x, y, pix[i], pix[i + 1], pix[i + 2]))


def cmd_diff(a):
    wa, ha, pa = read_ppm(a.a)
    wb, hb, pb = read_ppm(a.b)
    if (wa, ha) != (wb, hb):
        print("size_a=%dx%d size_b=%dx%d differ=size" % (wa, ha, wb, hb))
        sys.exit(1)
    reg = region(a.region, wa, ha)
    x0, y0, x1, y1 = reg
    count = 0
    worst = 0
    box = [x1, y1, -1, -1]
    mask = bytearray(wa * ha * 3) if a.out else None
    for y in range(y0, y1):
        base = y * wa
        for x in range(x0, x1):
            i = (base + x) * 3
            d = max(abs(pa[i] - pb[i]), abs(pa[i + 1] - pb[i + 1]), abs(pa[i + 2] - pb[i + 2]))
            if d > a.tol:
                count += 1
                worst = max(worst, d)
                box = [min(box[0], x), min(box[1], y), max(box[2], x + 1), max(box[3], y + 1)]
                if mask is not None:
                    mask[i:i + 3] = b"\xff\xff\xff"
    if mask is not None:
        write_ppm(a.out, wa, ha, mask)
    if count:
        print("region=%d,%d,%d,%d tol=%d differ=%d bbox=%d,%d,%d,%d maxdiff=%d" % (reg + (a.tol, count) + tuple(box) +
                                                                                    (worst,)))
        sys.exit(1)
    print("region=%d,%d,%d,%d tol=%d differ=0" % (reg + (a.tol,)))


def cmd_down2(a):
    w, h, pix = read_ppm(a.inp)
    ow, oh = w // 2, h // 2
    out = bytearray(ow * oh * 3)
    for y in range(oh):
        r0 = 2 * y * w * 3
        r1 = (2 * y + 1) * w * 3
        for x in range(ow):
            i0 = r0 + 2 * x * 3
            i1 = r1 + 2 * x * 3
            o = (y * ow + x) * 3
            for k in range(3):
                out[o + k] = (pix[i0 + k] + pix[i0 + 3 + k] + pix[i1 + k] + pix[i1 + 3 + k] + 2) // 4
    write_ppm(a.out, ow, oh, out)
    print("in=%s %dx%d out=%s %dx%d" % (a.inp, w, h, a.out, ow, oh))


def cmd_bands(a):
    w, h, pix = read_ppm(a.file)
    ref = colour(a.colour)

    def full(y):
        return all(near(c, ref, a.tol) for c in pixels(w, pix, (0, y, w, y + 1)))

    top = 0
    while top < h and full(top):
        top += 1
    bottom = 0
    while bottom < h - top and full(h - 1 - bottom):
        bottom += 1
    print("file=%s size=%dx%d colour=%d,%d,%d tol=%d top_rows=%d bottom_rows=%d" % ((a.file, w, h) + ref +
                                                                                       (a.tol, top, bottom)))


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    sub = ap.add_subparsers(dest="cmd", required=True)
    p = sub.add_parser("info")
    p.add_argument("file")
    p.set_defaults(fn=cmd_info)
    p = sub.add_parser("stats")
    p.add_argument("file")
    p.add_argument("--region")
    p.add_argument("--hist", type=int, default=5)
    p.add_argument("--colour")
    p.add_argument("--tol", type=int, default=0)
    p.set_defaults(fn=cmd_stats)
    p = sub.add_parser("probe")
    p.add_argument("file")
    p.add_argument("points", nargs="+")
    p.set_defaults(fn=cmd_probe)
    p = sub.add_parser("diff")
    p.add_argument("a")
    p.add_argument("b")
    p.add_argument("--region")
    p.add_argument("--tol", type=int, default=0)
    p.add_argument("--out")
    p.set_defaults(fn=cmd_diff)
    p = sub.add_parser("down2")
    p.add_argument("inp")
    p.add_argument("out")
    p.set_defaults(fn=cmd_down2)
    p = sub.add_parser("bands")
    p.add_argument("file")
    p.add_argument("--colour", default="0,0,0")
    p.add_argument("--tol", type=int, default=0)
    p.set_defaults(fn=cmd_bands)
    a = ap.parse_args()
    a.fn(a)


if __name__ == "__main__":
    main()
