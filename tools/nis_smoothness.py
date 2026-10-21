#!/usr/bin/env python3
"""nis_smoothness.py - per-frame screen-space smoothness of a motion / nis dump (HWI-FEATURE(motion-interp)).

Reads the per-frame dumps of tools/m2_harness.sh modes `motion` and `nis` (src/Game/MotionTest.c, one "F" line per
rendered frame: the camera pose given to the main view's renderer, then NAME=x,y,depth,onscreen per tracked object, then
the frame's CRC-32) and reports, per object, the per-frame screen-space displacement in pixels (max, 95th percentile,
mean) and the repeated frames (a frame whose displacement from the previous frame is below --eps pixels: the object
did not move on screen), plus the camera's per-frame eye movement and repeated eye poses. With two dumps the numbers
are printed side by side (e.g. interpolation off | on) and the dumps are compared byte for byte.

M3-R1 motion-jitter adds the scene dumps (--scene; src/Game/MotionTest.c "the scene mode"): their frame lines carry
alpha= / raw= / calls= / mi= (and ms= / ft= / tt= / sub= on the real clock) and <name>m= per object, read here, and
three options and two figures:
  jerk          per object the second difference of the screen position |p(k+1) - 2 p(k) + p(k-1)| (max, p95): 0 for
                a constant screen velocity, about one tick's step at each tick for 16 Hz stepping; a hitch in the
                interpolation shows as a spike
  clock         with alpha= in the dump: alpha's range, its per-frame steps (min / median / max, the wraps), the frames
                drawn without a blend (alpha -1), raw above 1 (a universe tick due later in the same pass), the frames
                in which the universe ran more than one tick; with ft= the frame-time distribution

Usage:
  tools/nis_smoothness.py DUMP [DUMP2] [--eps PX] [--object NAME] [--all] [--offscreen] [--span] [--objects A,B]
    --eps PX      a frame counts as repeated when the object moved less than PX pixels on screen (default by the
                  dump's mode: motion 0.05, since the main view's camera float moves even a held frame's objects by up
                  to about 0.05 px; nis 0.01, since an NIS has no camera float and a held frame repeats exactly)
    --object NAME nis dumps: the tracked object (default: the NIS object on screen in the most frame pairs of the first
                  dump)
    --all         nis dumps: every object, not only the tracked one
    --offscreen   count frame pairs with the object off screen too (its projection in front of the camera)
    --span        per object only the frames of its own motion on screen: from the first to the last frame whose
                  position differs by more than 1 px from the one a tick (framesPerTick frames) later (an object at rest
                  on a still screen has no span); the same frame range is used for both dumps
    --objects L   only these objects (comma list)
    --jerk        print the jerk figures (objects and camera) for any dump
    --world       scene dumps: the objects' drawn WORLD positions (<name>w=, units) instead of their screen positions:
                  each object's own motion as drawn, without the camera (eps and the span's 1 then in world units)

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
                fr = {"frame": int(toks[1]), "objs": {}, "eye": None, "crc": None, "tick": None, "clock": {}}
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
                    elif k in ("alpha", "raw", "ms", "ft", "sub", "rsub"):
                        fr["clock"][k] = float(v)
                    elif k in ("calls", "tt"):
                        fr["clock"][k] = int(v)
                    elif k == "mi":
                        fr["clock"][k] = tuple(int(x) for x in v.split("/"))
                    elif v.count(",") == 3:
                        x, y, d, vis = v.split(",")
                        fr["objs"][k] = (float(x), float(y), float(d), int(vis))
                    else:
                        fr["clock"][k] = v
                frames.append(fr)
    return header, roles, order, frames


def pct(values, p):
    if not values:
        return float("nan")
    s = sorted(values)
    k = max(0, min(len(s) - 1, int(math.ceil(p / 100.0 * len(s))) - 1))
    return s[k]


OPTS = {"offscreen": False, "world": False}


def usable(p):
    if OPTS["world"]:
        return p is not None
    if p is None or any(math.isnan(v) for v in p[:2]):
        return False
    return bool(p[3]) or (OPTS["offscreen"] and p[2] > 0.0)


def coords(p):
    return p[:3] if OPTS["world"] else p[:2]


def worldify(frames):
    """--world: the objects' drawn world positions (<name>w=x,y,z) in place of their screen positions"""
    for f in frames:
        objs = {}
        for k, v in f["clock"].items():
            if k.endswith("w") and isinstance(v, str) and v.count(",") == 2:
                x, y, z = (float(t) for t in v.split(","))
                objs[k[:-1]] = (x, y, z, 1)
        f["objs"] = objs


def motion_span(frames, name, fpt):
    """the frame range of the object's own motion on screen (--span): first .. last frame whose position differs by
    more than 1 px from the one fpt frames later"""
    fpt = max(1, fpt)
    moving = [i for i in range(len(frames) - fpt)
              if usable(frames[i]["objs"].get(name)) and usable(frames[i + fpt]["objs"].get(name)) and
              math.dist(coords(frames[i + fpt]["objs"][name]), coords(frames[i]["objs"][name])) > 1.0]
    if not moving:
        return None
    return (moving[0], min(len(frames) - 1, moving[-1] + fpt))


def object_stats(frames, name, eps, span=None):
    disp = []
    jerk = []
    lo, hi = span if span is not None else (0, len(frames) - 1)
    sub = frames[lo:hi + 1]
    for a, b in zip(sub, sub[1:]):
        pa, pb = a["objs"].get(name), b["objs"].get(name)
        if not usable(pa) or not usable(pb):
            continue
        disp.append(math.dist(coords(pb), coords(pa)))
    for a, b, c in zip(sub, sub[1:], sub[2:]):
        pa, pb, pc = a["objs"].get(name), b["objs"].get(name), c["objs"].get(name)
        if not usable(pa) or not usable(pb) or not usable(pc):
            continue
        jerk.append(math.sqrt(sum((c3 - 2.0 * b3 + a3) ** 2 for a3, b3, c3 in zip(coords(pa), coords(pb), coords(pc)))))
    rep = sum(1 for d in disp if d < eps)
    return {
        "pairs": len(disp),
        "max": max(disp) if disp else float("nan"),
        "p95": pct(disp, 95),
        "mean": (sum(disp) / len(disp)) if disp else float("nan"),
        "repeated": rep,
        "jmax": max(jerk) if jerk else float("nan"),
        "jp95": pct(jerk, 95),
    }


def clock_stats(frames):
    al = [f["clock"]["alpha"] for f in frames if "alpha" in f["clock"]]
    if not al:
        return None
    raw = [f["clock"].get("raw", float("nan")) for f in frames if "alpha" in f["clock"]]
    blended = [a for a in al if a >= 0.0]
    steps = []
    wraps = 0
    for a, b in zip(al, al[1:]):
        if a < 0.0 or b < 0.0:
            continue
        if b < a:
            wraps += 1
        else:
            steps.append(b - a)
    calls = [f["clock"].get("calls", 0) for f in frames if "alpha" in f["clock"]]
    out = {
        "frames": len(al), "unblended": sum(1 for a in al if a < 0.0),
        "amin": min(blended) if blended else float("nan"), "amax": max(blended) if blended else float("nan"),
        "smin": min(steps) if steps else float("nan"), "smed": pct(steps, 50), "smax": max(steps) if steps else float("nan"),
        "wraps": wraps, "raw_ge1": sum(1 for r in raw if r >= 1.0), "rawmax": max(raw) if raw else float("nan"),
        "calls0": sum(1 for c in calls if c == 0), "calls1": sum(1 for c in calls if c == 1),
        "callsN": sum(1 for c in calls if c > 1),
    }
    ft = [f["clock"]["ft"] for f in frames if "ft" in f["clock"]][1:]
    if ft:
        out["ft"] = (min(ft), pct(ft, 50), pct(ft, 95), pct(ft, 99), max(ft), sum(ft) / len(ft))
        tt = [f["clock"].get("tt", 0) for f in frames if "ft" in f["clock"]][1:]
        out["tt"] = {v: tt.count(v) for v in sorted(set(tt))}
    return out


def camera_stats(frames):
    disp = []
    jerk = []
    for a, b, c in zip(frames, frames[1:], frames[2:]):
        if a.get("eye") is None or b.get("eye") is None or c.get("eye") is None:
            continue
        jerk.append(sum(math.sqrt(sum((z - 2.0 * y + x) ** 2 for x, y, z in zip(a[k], b[k], c[k]))) for k in ("eye", "look")
                        if a.get(k) is not None and b.get(k) is not None and c.get(k) is not None))
    for a, b in zip(frames, frames[1:]):
        if a.get("eye") is None or b.get("eye") is None:
            continue
        disp.append(math.dist(a["eye"], b["eye"]) + math.dist(a.get("look", a["eye"]), b.get("look", b["eye"])))
    rep = sum(1 for d in disp if d == 0.0)
    crc_rep = sum(1 for a, b in zip(frames, frames[1:]) if a["crc"] is not None and a["crc"] == b["crc"])
    return {"pairs": len(disp), "max": max(disp) if disp else float("nan"), "p95": pct(disp, 95), "repeated": rep,
            "crc_repeated": crc_rep, "jmax": max(jerk) if jerk else float("nan"), "jp95": pct(jerk, 95)}


def fmt(st, fpt, jerk=False):
    if st["pairs"] == 0:
        return "no visible frame pairs"
    per9 = st["repeated"] / st["pairs"] * fpt if fpt else 0.0
    text = ("max %8.3f px  p95 %8.3f px  mean %7.3f px  repeated %5d / %5d (%.2f of %d a tick)"
            % (st["max"], st["p95"], st["mean"], st["repeated"], st["pairs"], per9, fpt))
    if jerk:
        text += "  jerk max %7.3f p95 %7.3f" % (st["jmax"], st["jp95"])
    return text


def main(argv):
    eps = None
    obj = None
    show_all = False
    use_span = False
    show_jerk = False
    only = None
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
        elif a == "--offscreen":
            OPTS["offscreen"] = True
        elif a == "--world":
            OPTS["world"] = True
        elif a == "--jerk":
            show_jerk = True
        elif a == "--span":
            use_span = True
        elif a == "--objects":
            only = argv[i + 1].split(","); i += 1
        elif a in ("-h", "--help"):
            print(__doc__); return 0
        else:
            paths.append(a)
        i += 1
    if not 1 <= len(paths) <= 2:
        print(__doc__, file=sys.stderr)
        return 2
    dumps = [parse(p) for p in paths]
    if OPTS["world"]:
        for d in dumps:
            worldify(d[3])
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
    if only is not None:
        names = [n for n in names if n in only]
    scene = h0.get("mode") == "motion" and any("alpha" in f["clock"] for f in frames0)
    print("eps %.3f %s; a repeated frame = the object moved less than eps on screen%s%s" %
          (eps, "world units" if OPTS["world"] else "px", "; off-screen frames counted" if OPTS["offscreen"] else "",
           "; each object's own motion span" if use_span else ""))
    for n in names:
        role, desc = roles0.get(n, ("?", ""))
        label = "%s (%s%s%s)" % (n, role, ", " + desc if desc else "", ", tracked" if n == tracked else "")
        span = None
        if use_span:
            span = motion_span(frames0, n, fpt)
            for h, roles, order, frames in dumps[1:]:
                other = motion_span(frames, n, fpt)
                if other is not None:
                    span = other if span is None else (min(span[0], other[0]), max(span[1], other[1]))
            label += (" span frames %d-%d" % span) if span is not None else " at rest on screen (no span)"
        print(label)
        if use_span and span is None:
            continue
        for p, (h, roles, order, frames) in zip(paths, dumps):
            print("  %-40s %s" % (p.split("/")[-2] if "/" in p else p, fmt(object_stats(frames, n, eps, span), fpt,
                                                                          jerk=scene or use_span or show_jerk)))
    for p, (h, roles, order, frames) in zip(paths, dumps):
        cs = clock_stats(frames)
        if cs is None:
            continue
        print("clock %s: frames %d, unblended (alpha -1) %d, alpha %.4f..%.4f, alpha step min %.4f median %.4f max %.4f, "
              "wraps %d, raw >= 1: %d (max %.4f), universe ticks a frame: 0 x%d, 1 x%d, more x%d" %
              (p.split("/")[-2] if "/" in p else p, cs["frames"], cs["unblended"], cs["amin"], cs["amax"], cs["smin"],
               cs["smed"], cs["smax"], cs["wraps"], cs["raw_ge1"], cs["rawmax"], cs["calls0"], cs["calls1"], cs["callsN"]))
        if "ft" in cs:
            print("  frame time ms: min %.3f median %.3f p95 %.3f p99 %.3f max %.3f mean %.3f; task ticks a frame %s" %
                  (cs["ft"] + (cs["tt"],)))
    print("camera (eye + look movement per frame, world units)")
    for p, (h, roles, order, frames) in zip(paths, dumps):
        cs = camera_stats(frames)
        print("  %-40s max %9.3f  p95 %9.3f  repeated poses %5d / %5d  repeated frame CRCs %d%s" %
              (p.split("/")[-2] if "/" in p else p, cs["max"], cs["p95"], cs["repeated"], cs["pairs"],
               cs["crc_repeated"], ("  jerk max %.3f p95 %.3f" % (cs["jmax"], cs["jp95"])) if (show_jerk or use_span) else ""))
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
