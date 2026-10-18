#!/usr/bin/env python3
"""nis_smoothness.py - per-frame screen-space smoothness of a motion / nis dump (HWI-FEATURE(motion-interp)).

Reads the per-frame dumps of tools/m2_harness.sh modes `motion` and `nis` (src/Game/MotionTest.c, one "F" line per
rendered frame: the camera pose given to the main view's renderer, then NAME=x,y,depth,onscreen per tracked object, then
the frame's CRC-32) and reports, per object, the per-frame screen-space displacement in pixels (max, 95th percentile,
mean) and the repeated frames (a frame whose displacement from the previous frame is below --eps pixels: the object
did not move on screen), plus the camera's per-frame eye movement and repeated eye poses. With two dumps the numbers
are printed side by side (e.g. interpolation off | on) and the dumps are compared byte for byte.

Usage:
  tools/nis_smoothness.py DUMP [DUMP2] [--eps PX] [--object NAME] [--all]
    --eps PX      a frame counts as repeated when the object moved less than PX pixels on screen (default by the
                  dump's mode: motion 0.05, since the main view's camera float moves even a held frame's objects by up
                  to about 0.05 px; nis 0.01, since an NIS has no camera float and a held frame repeats exactly)
    --object NAME nis dumps: the tracked object (default: the NIS object on screen in the most frame pairs of the first
                  dump)
    --all         nis dumps: every object, not only the tracked one

Standard library only (like tools/oracle/hwo_compare.py). docs/m2-r6-motion-interp.md has the measured numbers.
"""
import hashlib
import math
import sys


def parse(path):
    header = {}
    roles = {}
    order = []
    frames = []
    with open(path, encoding="ascii") as f:
        for line in f:
            line = line.rstrip("\n")
            if line.startswith("# motion-interp dump"):
                for tok in line.split()[3:]:
                    if "=" in tok:
                        k, v = tok.split("=", 1)
                        header[k] = v
            elif line.startswith("# object "):
                parts = line.split()
                name, role = parts[2], parts[3]
                roles[name] = (role, " ".join(parts[4:]))
                order.append(name)
            elif line.startswith("F "):
                toks = line.split()
                fr = {"frame": int(toks[1]), "objs": {}, "eye": None, "crc": None, "tick": None}
                for tok in toks[2:]:
                    if "=" not in tok:
                        continue
                    k, v = tok.split("=", 1)
                    if k == "tick":
                        fr["tick"] = int(v)
                    elif k == "crc":
                        fr["crc"] = v
                    elif k in ("eye", "look", "up"):
                        fr[k] = tuple(float(x) for x in v.split(","))
                    else:
                        x, y, d, vis = v.split(",")
                        fr["objs"][k] = (float(x), float(y), float(d), int(vis))
                frames.append(fr)
    return header, roles, order, frames


def pct(values, p):
    if not values:
        return float("nan")
    s = sorted(values)
    k = max(0, min(len(s) - 1, int(math.ceil(p / 100.0 * len(s))) - 1))
    return s[k]


def object_stats(frames, name, eps):
    disp = []
    for a, b in zip(frames, frames[1:]):
        pa, pb = a["objs"].get(name), b["objs"].get(name)
        if pa is None or pb is None or not pa[3] or not pb[3]:
            continue
        if any(math.isnan(v) for v in pa[:2] + pb[:2]):
            continue
        disp.append(math.hypot(pb[0] - pa[0], pb[1] - pa[1]))
    rep = sum(1 for d in disp if d < eps)
    return {
        "pairs": len(disp),
        "max": max(disp) if disp else float("nan"),
        "p95": pct(disp, 95),
        "mean": (sum(disp) / len(disp)) if disp else float("nan"),
        "repeated": rep,
    }


def camera_stats(frames):
    disp = []
    for a, b in zip(frames, frames[1:]):
        if a.get("eye") is None or b.get("eye") is None:
            continue
        disp.append(math.dist(a["eye"], b["eye"]) + math.dist(a.get("look", a["eye"]), b.get("look", b["eye"])))
    rep = sum(1 for d in disp if d == 0.0)
    crc_rep = sum(1 for a, b in zip(frames, frames[1:]) if a["crc"] is not None and a["crc"] == b["crc"])
    return {"pairs": len(disp), "max": max(disp) if disp else float("nan"), "p95": pct(disp, 95), "repeated": rep,
            "crc_repeated": crc_rep}


def fmt(st, fpt):
    if st["pairs"] == 0:
        return "no visible frame pairs"
    per9 = st["repeated"] / st["pairs"] * fpt if fpt else 0.0
    return ("max %8.3f px  p95 %8.3f px  mean %7.3f px  repeated %5d / %5d (%.2f of %d a tick)"
            % (st["max"], st["p95"], st["mean"], st["repeated"], st["pairs"], per9, fpt))


def main(argv):
    eps = None
    obj = None
    show_all = False
    paths = []
    i = 0
    while i < len(argv):
        a = argv[i]
        if a == "--eps":
            eps = float(argv[i + 1]); i += 1
        elif a == "--object":
            obj = argv[i + 1]; i += 1
        elif a == "--all":
            show_all = True
        elif a in ("-h", "--help"):
            print(__doc__); return 0
        else:
            paths.append(a)
        i += 1
    if not 1 <= len(paths) <= 2:
        print(__doc__, file=sys.stderr)
        return 2
    dumps = [parse(p) for p in paths]
    for p, (h, roles, order, frames) in zip(paths, dumps):
        with open(p, "rb") as f:
            sha = hashlib.sha256(f.read()).hexdigest()[:16]
        print("dump %s: mode=%s fps=%s framesPerTick=%s frames=%d sha256 %s" %
              (p, h.get("mode"), h.get("fps"), h.get("framesPerTick"), len(frames), sha))
    if len(paths) == 2:
        same = open(paths[0], "rb").read() == open(paths[1], "rb").read()
        print("byte-identical: %s" % ("YES" if same else "NO"))
    h0, roles0, order0, frames0 = dumps[0]
    fpt = int(h0.get("framesPerTick", "0") or 0)
    if eps is None:
        eps = 0.01 if h0.get("mode") == "nis" else 0.05
    names = list(order0)
    tracked = None
    if h0.get("mode") == "nis":
        if obj is None:
            counts = {n: object_stats(frames0, n, eps)["pairs"] for n in names}
            obj = max(names, key=lambda n: (counts[n], -names.index(n))) if names else None
        tracked = obj
        if not show_all and obj is not None:
            names = [obj]
    print("eps %.3f px; a repeated frame = the object moved less than eps on screen" % eps)
    for n in names:
        role, desc = roles0.get(n, ("?", ""))
        label = "%s (%s%s%s)" % (n, role, ", " + desc if desc else "", ", tracked" if n == tracked else "")
        print(label)
        for p, (h, roles, order, frames) in zip(paths, dumps):
            print("  %-40s %s" % (p.split("/")[-2] if "/" in p else p, fmt(object_stats(frames, n, eps), fpt)))
    print("camera (eye + look movement per frame, world units)")
    for p, (h, roles, order, frames) in zip(paths, dumps):
        cs = camera_stats(frames)
        print("  %-40s max %9.3f  p95 %9.3f  repeated poses %5d / %5d  repeated frame CRCs %d" %
              (p.split("/")[-2] if "/" in p else p, cs["max"], cs["p95"], cs["repeated"], cs["pairs"],
               cs["crc_repeated"]))
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
