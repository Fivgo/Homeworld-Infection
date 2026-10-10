#!/usr/bin/env python3
"""Compare two checksum-oracle streams (.hwo) and report the first divergent tick and which sub-checksum moved.

Usage:
  hwo_compare.py REF.hwo CAND.hwo [--from-tick N] [--to-tick M] [--fields f1,f2,...] [--max-draws K] [--json OUT]
  hwo_compare.py --dump FILE.hwo [--from-tick N] [--to-tick M]
  hwo_compare.py --perturb-copy IN.hwo OUT.hwo --tick T --field FIELD [--record T|P]

Compare (docs/m2-checksum-oracle.md 5.3, docs/m2-r2-oracle.md "compare"):
  1. both headers: format version, ticks per second and option bits must match; a mismatch is reported first (and
     the comparison continues: a perturbed self-test, for example, carries its own option bit);
  2. 'T' records are joined on the tick; ticks present in only one file are reported;
  3. per tick, fields are compared in causal order (RNG streams first, then ships, other objects, the engine's own
     checksums, players, clock). The first tick with a difference is the first divergent tick; every differing field
     at that tick is printed and the first in causal order is flagged as the likely origin. For an RNG stream the tool
     steps a copy of the reference stream forward (and backward, by stepping the candidate) up to --max-draws draws
     to tell "candidate is N draws ahead" from "unrelated";
  4. 'P' records are joined on (packetnum, occurrence); the first packet whose randcheck/univcheck/blobcheck differ is
     reported, and any non-zero sync-error counter in either file;
  5. a field is compared only when both files fill it (the 'T' valid bits; rng_all only over the same stream count;
     fpcw only between files of the same producer).
Exit code: 0 identical over the compared range, 1 diverged, 2 unusable input.

Only standard-library Python; reads .hwo files, never game data. The layout mirrors tools/oracle/hwo_format.h.
"""
import argparse
import json
import struct
import sys

MAGIC = b"HWIORCL1"
HEADER_SIZE = 64
REC_T, REC_P, REC_E = 0x54, 0x50, 0x45
TICK_PAYLOAD, PACKET_PAYLOAD = 242, 52

PRODUCERS = {1: "GOG DLL", 2: "hwi-engine"}
MODES = {1: "playback", 2: "save-run", 3: "record", 4: "observe", 5: "self-test"}
OPTIONS = {0x1: "clock=virtual", 0x2: "rng0=split", 0x4: "determCompPlayer", 0x100: "perturbed"}
EVENTS = {1: "game-new", 2: "game-load", 3: "record-start", 4: "record-stop", 5: "playback-start", 6: "playback-end",
          7: "quit", 8: "selftest-start", 9: "selftest-end", 10: "hooks", 11: "offset-check", 12: "note", 13: "perturb"}

TV_REAL, TV_SHIPMISC, TV_HEALTH, TV_LISTS, TV_PLAYERS, TV_CHECKSUMS = 0x1, 0x2, 0x4, 0x8, 0x10, 0x20

# 'T' payload layout: (name, struct code). Mirrors hwoPackTick.
T_LAYOUT = [
    ("tick", "I"), ("dt", "I"), ("total", "I"), ("real", "I"), ("fpcw", "H"), ("valid", "H"),
    ("univcheck", "I"), ("nships", "H"), ("statcheck", "I"), ("rng_streams", "H"), ("rng_all", "I"),
    ("rng10", "5I"), ("rng6", "5I"), ("rng0", "5I"),
    ("ships_n", "H"), ("ships_pos", "Q"), ("ships_vel", "Q"), ("ships_rot", "Q"), ("ships_misc", "Q"),
    ("ships_health", "Q"),
    ("missiles_n", "H"), ("missiles_h", "Q"), ("bullets_n", "H"), ("bullets_h", "Q"),
    ("derelicts_n", "H"), ("derelicts_h", "Q"), ("resources_n", "H"), ("resources_h", "Q"),
    ("players", "16I"),
]
P_LAYOUT = [
    ("tick", "I"), ("packetnum", "H"), ("randcheck", "H"), ("univcheck", "I"), ("blobcheck", "I"), ("extra", "I"),
    ("ncommands", "H"), ("flags", "H"), ("randSyncErr", "I"), ("univSyncErr", "I"), ("blobSyncErr", "I"),
    ("pktSyncErr", "I"), ("randSyncErrFrame", "I"), ("univSyncErrFrame", "I"), ("blobSyncErrFrame", "I"),
]
T_FMT = "<" + "".join(c for _, c in T_LAYOUT)
P_FMT = "<" + "".join(c for _, c in P_LAYOUT)
assert struct.calcsize(T_FMT) == TICK_PAYLOAD, struct.calcsize(T_FMT)
assert struct.calcsize(P_FMT) == PACKET_PAYLOAD, struct.calcsize(P_FMT)

# causal comparison order: (display name, record fields, valid bit needed or None, kind)
T_ORDER = [
    ("rng10", ["rng10"], None, "rng"),
    ("rng6", ["rng6"], None, "rng"),
    ("rng0", ["rng0"], None, "rng"),
    ("rng_all", ["rng_all"], None, "rng_all"),
    ("ships_n", ["ships_n"], None, "int"),
    ("ships_misc", ["ships_misc"], TV_SHIPMISC, "hash"),
    ("ships_pos", ["ships_pos"], None, "hash"),
    ("ships_vel", ["ships_vel"], None, "hash"),
    ("ships_rot", ["ships_rot"], None, "hash"),
    ("ships_health", ["ships_health"], TV_HEALTH, "hash"),
    ("missiles", ["missiles_n", "missiles_h"], TV_LISTS, "hash"),
    ("bullets", ["bullets_n", "bullets_h"], TV_LISTS, "hash"),
    ("derelicts", ["derelicts_n", "derelicts_h"], TV_LISTS, "hash"),
    ("resources", ["resources_n", "resources_h"], TV_LISTS, "hash"),
    ("statcheck", ["statcheck"], TV_CHECKSUMS, "int"),
    ("univcheck", ["univcheck", "nships"], TV_CHECKSUMS, "float"),
    ("players", ["players"], TV_PLAYERS, "int"),
    ("dt", ["dt"], None, "float"),
    ("total", ["total"], None, "float"),
    ("real", ["real"], TV_REAL, "float"),
    ("fpcw", ["fpcw"], None, "fpcw"),
]
FIELD_NAMES = [n for n, _, _, _ in T_ORDER]


class HwoError(Exception):
    pass


def f32(bits):
    return struct.unpack("<f", struct.pack("<I", bits))[0]


def parse_header(b):
    if len(b) < HEADER_SIZE or b[:8] != MAGIC:
        raise HwoError("not a .hwo file (bad magic)")
    version, producer, mode, options, ident1, ident2, fpcw, tps = struct.unpack_from("<HBBIIIHH", b, 8)
    tag = b[28:64].split(b"\0", 1)[0].decode("latin-1")
    return {"version": version, "producer": producer, "mode": mode, "options": options, "ident1": ident1,
            "ident2": ident2, "fpcw": fpcw, "ticks_per_second": tps, "tag": tag}


def unpack(layout, fmt, payload):
    vals = struct.unpack(fmt, payload)
    out, i = {}, 0
    for name, code in layout:
        n = int(code[:-1]) if len(code) > 1 else 1
        out[name] = tuple(vals[i:i + n]) if n > 1 else vals[i]
        i += n
    return out


def read_hwo(path):
    with open(path, "rb") as f:
        b = f.read()
    header = parse_header(b)
    if header["version"] != 1:
        raise HwoError("%s: format version %d, this tool reads version 1" % (path, header["version"]))
    ticks, packets, events, other = [], [], [], 0
    pos, truncated = HEADER_SIZE, False
    while pos < len(b):
        if pos + 4 > len(b):
            truncated = True
            break
        rtype, _res, length = struct.unpack_from("<BBH", b, pos)
        payload = b[pos + 4:pos + 4 + length]
        if len(payload) < length:
            truncated = True
            break
        if rtype == REC_T:
            if length != TICK_PAYLOAD:
                raise HwoError("%s: 'T' record of %d bytes at offset %d (expected %d)" % (path, length, pos, TICK_PAYLOAD))
            t = unpack(T_LAYOUT, T_FMT, payload)
            t["_offset"] = pos
            ticks.append(t)
        elif rtype == REC_P:
            if length != PACKET_PAYLOAD:
                raise HwoError("%s: 'P' record of %d bytes at offset %d" % (path, length, pos))
            p = unpack(P_LAYOUT, P_FMT, payload)
            p["_offset"] = pos
            packets.append(p)
        elif rtype == REC_E:
            tick, code, n = struct.unpack_from("<IHH", payload, 0)
            events.append({"tick": tick, "code": code, "text": payload[8:8 + n].decode("latin-1"), "_offset": pos})
        else:
            other += 1
        pos += 4 + length
    return {"path": path, "header": header, "ticks": ticks, "packets": packets, "events": events,
            "other": other, "truncated": truncated, "size": len(b)}


# ---- RNG: the game's generator (Randy.c ranRandomFn, cataclysm.exe 0x004ef860), integer only ----
M32 = 0xFFFFFFFF


def ran_step(st):
    x, y, z, c, n = st
    if y > ((x + c) & M32):
        s = (y - (x + c)) & M32
        c = 0
    else:
        s = ((x + c) - y - 18) & M32
        c = 1
    x, y, z = y, z, s
    n = (69069 * n + 1013904243) & M32
    return (x, y, z, c, n)


def ran_distance(ref, cand, max_draws):
    """+k: cand is ref advanced by k draws; -k: ref is cand advanced by k; None: unrelated within max_draws."""
    a = tuple(ref)
    for k in range(1, max_draws + 1):
        a = ran_step(a)
        if a == tuple(cand):
            return k
    a = tuple(cand)
    for k in range(1, max_draws + 1):
        a = ran_step(a)
        if a == tuple(ref):
            return -k
    return None


def fmt_value(name, kind, rec, fields):
    if kind == "rng":
        x, y, z, c, n = rec[fields[0]]
        return "x=%08x y=%08x z=%08x c=%d n=%08x" % (x, y, z, c, n)
    if kind == "float":
        parts = []
        for fname in fields:
            v = rec[fname]
            if fname in ("univcheck", "dt", "total", "real"):
                parts.append("%s=%08x (%.9g)" % (fname, v, f32(v)))
            else:
                parts.append("%s=%d" % (fname, v))
        return " ".join(parts)
    if kind == "hash":
        return " ".join(("%s=%d" % (f, rec[f])) if f.endswith("_n") else ("%s=%016x" % (f, rec[f])) for f in fields)
    if name == "players":
        p = rec["players"]
        return "ru/ships " + " ".join("%d/%d" % (p[2 * i], p[2 * i + 1]) for i in range(8))
    return " ".join("%s=%s" % (f, rec[f]) for f in fields)


def comparable(name, needed, kind, a, b, ha, hb):
    if needed is not None and not (a["valid"] & needed and b["valid"] & needed):
        return False
    if kind == "rng_all" and a["rng_streams"] != b["rng_streams"]:
        return False
    if kind == "fpcw" and ha["producer"] != hb["producer"]:
        return False
    return True


def compare(ref, cand, args):
    out = {"ref": ref["path"], "cand": cand["path"], "header_notes": [], "first_divergent_tick": None,
           "fields": [], "likely_origin": None, "last_equal_tick": None, "ticks_compared": 0,
           "packets_compared": 0, "first_divergent_packet": None, "sync_errors": {}, "not_comparable": [],
           "only_in_ref": 0, "only_in_cand": 0, "rng_draws": None, "events_differ": False}
    ha, hb = ref["header"], cand["header"]
    lines = []
    lines.append("REF  %s: %s, %s, tag '%s', %d ticks/s, options %s, %d T / %d P / %d E records%s" % (
        ref["path"], PRODUCERS.get(ha["producer"], ha["producer"]), MODES.get(ha["mode"], ha["mode"]), ha["tag"],
        ha["ticks_per_second"], opt_str(ha["options"]), len(ref["ticks"]), len(ref["packets"]), len(ref["events"]),
        " (TRUNCATED)" if ref["truncated"] else ""))
    lines.append("CAND %s: %s, %s, tag '%s', %d ticks/s, options %s, %d T / %d P / %d E records%s" % (
        cand["path"], PRODUCERS.get(hb["producer"], hb["producer"]), MODES.get(hb["mode"], hb["mode"]), hb["tag"],
        hb["ticks_per_second"], opt_str(hb["options"]), len(cand["ticks"]), len(cand["packets"]), len(cand["events"]),
        " (TRUNCATED)" if cand["truncated"] else ""))

    # 1. headers
    if ha["ticks_per_second"] != hb["ticks_per_second"]:
        out["header_notes"].append("ticks per second differ: %d vs %d" % (ha["ticks_per_second"], hb["ticks_per_second"]))
    if ha["options"] != hb["options"]:
        out["header_notes"].append("option bits differ: %s vs %s" % (opt_str(ha["options"]), opt_str(hb["options"])))
    if ha["producer"] == hb["producer"] and (ha["ident1"], ha["ident2"]) != (hb["ident1"], hb["ident2"]):
        out["header_notes"].append("same producer, different build identity: %08x/%08x vs %08x/%08x" % (
            ha["ident1"], ha["ident2"], hb["ident1"], hb["ident2"]))
    if ha["producer"] == hb["producer"] and ha["fpcw"] != hb["fpcw"]:
        out["header_notes"].append("floating-point control word differs: %04x vs %04x" % (ha["fpcw"], hb["fpcw"]))
    for n in out["header_notes"]:
        lines.append("HEADER: " + n)

    # 2. join T on tick (a tick written twice keeps its first record: tick 0 of a start and of a reset)
    def bytick(recs):
        d = {}
        for t in recs:
            if args.from_tick is not None and t["tick"] < args.from_tick:
                continue
            if args.to_tick is not None and t["tick"] > args.to_tick:
                continue
            d.setdefault(t["tick"], t)
        return d
    ta, tb = bytick(ref["ticks"]), bytick(cand["ticks"])
    only_a = sorted(set(ta) - set(tb))
    only_b = sorted(set(tb) - set(ta))
    out["only_in_ref"], out["only_in_cand"] = len(only_a), len(only_b)
    if only_a:
        lines.append("ticks only in REF: %d (first %d, last %d)" % (len(only_a), only_a[0], only_a[-1]))
    if only_b:
        lines.append("ticks only in CAND: %d (first %d, last %d)" % (len(only_b), only_b[0], only_b[-1]))
    common = sorted(set(ta) & set(tb))
    fields = [f for f in T_ORDER if args.fields is None or f[0] in args.fields]

    noted = set()
    for tick in common:
        a, b = ta[tick], tb[tick]
        diffs = []
        for name, recfields, needed, kind in fields:
            if not comparable(name, needed, kind, a, b, ha, hb):
                if name not in noted:
                    noted.add(name)
                    out["not_comparable"].append(name)
                continue
            if any(a[f] != b[f] for f in recfields):
                diffs.append((name, recfields, kind))
        out["ticks_compared"] += 1
        if not diffs:
            out["last_equal_tick"] = tick
            continue
        out["first_divergent_tick"] = tick
        out["likely_origin"] = diffs[0][0]
        lines.append("FIRST DIVERGENT TICK %d (last equal tick %s); %d field(s) differ, likely origin: %s" % (
            tick, out["last_equal_tick"], len(diffs), diffs[0][0]))
        for name, recfields, kind in diffs:
            entry = {"field": name, "ref": fmt_value(name, kind, a, recfields), "cand": fmt_value(name, kind, b, recfields)}
            lines.append("  %-13s REF  %s" % (name, entry["ref"]))
            lines.append("  %-13s CAND %s" % ("", entry["cand"]))
            if kind == "rng":
                k = ran_distance(a[recfields[0]], b[recfields[0]], args.max_draws)
                if k is None:
                    entry["draws"] = None
                    lines.append("  %-13s not the same stream shifted by up to %d draws" % ("", args.max_draws))
                else:
                    entry["draws"] = k
                    lines.append("  %-13s CAND is %d draw(s) %s REF" % ("", abs(k), "ahead of" if k > 0 else "behind"))
                if out["rng_draws"] is None and name == diffs[0][0]:
                    out["rng_draws"] = k
            out["fields"].append(entry)
        break
    if out["not_comparable"]:
        lines.append("not compared (a file does not fill it): " + ", ".join(out["not_comparable"]))

    # 4. packets, joined on (packetnum, occurrence)
    def bypacket(recs):
        d, seen = {}, {}
        for p in recs:
            k = p["packetnum"]
            seen[k] = seen.get(k, 0) + 1
            d[(k, seen[k])] = p
        return d
    pa, pb = bypacket(ref["packets"]), bypacket(cand["packets"])
    for key in sorted(set(pa) & set(pb), key=lambda k: (pa[k]["_offset"])):
        a, b = pa[key], pb[key]
        if args.from_tick is not None and a["tick"] < args.from_tick:
            continue
        if args.to_tick is not None and a["tick"] > args.to_tick:
            continue
        out["packets_compared"] += 1
        d = [f for f in ("randcheck", "univcheck", "blobcheck") if a[f] != b[f]]
        if d:
            out["first_divergent_packet"] = {"packetnum": key[0], "tick_ref": a["tick"], "tick_cand": b["tick"], "fields": d}
            lines.append("FIRST DIVERGENT PACKET %d (REF tick %d, CAND tick %d): %s" % (
                key[0], a["tick"], b["tick"], ", ".join("%s %d vs %d" % (f, a[f], b[f]) for f in d)))
            break
    if len(pa) != len(pb):
        lines.append("packet records: REF %d, CAND %d" % (len(pa), len(pb)))
    for label, run in (("REF", ref), ("CAND", cand)):
        if run["packets"]:
            last = run["packets"][-1]
            errs = {f: last[f] for f in ("randSyncErr", "univSyncErr", "blobSyncErr", "pktSyncErr") if last[f]}
            out["sync_errors"][label] = errs
            if errs:
                lines.append("%s: the game's own sync check counted errors: %s (first frames rand %d univ %d blob %d)" % (
                    label, errs, last["randSyncErrFrame"], last["univSyncErrFrame"], last["blobSyncErrFrame"]))

    ea = [(e["code"], e["text"]) for e in ref["events"]]
    eb = [(e["code"], e["text"]) for e in cand["events"]]
    if ea != eb:
        out["events_differ"] = True
        lines.append("event records differ: REF %s / CAND %s" % (
            [EVENTS.get(c, c) for c, _ in ea], [EVENTS.get(c, c) for c, _ in eb]))

    diverged = (out["first_divergent_tick"] is not None or out["first_divergent_packet"] is not None
                or only_a or only_b)
    if not diverged:
        lines.append("IDENTICAL: %d ticks and %d packets compared (ticks %s-%s), no difference" % (
            out["ticks_compared"], out["packets_compared"], common[0] if common else "-", common[-1] if common else "-"))
    out["verdict"] = "diverged" if diverged else "identical"
    return out, lines, (1 if diverged else 0)


def opt_str(o):
    names = [v for k, v in OPTIONS.items() if o & k]
    rest = o & ~sum(OPTIONS)
    if rest:
        names.append("0x%x" % rest)
    return "{" + ",".join(names) + "}"


def dump(run, args):
    h = run["header"]
    print("%s: producer %s, mode %s, options %s, ident %08x/%08x, fpcw %04x, %d ticks/s, tag '%s', %d bytes%s" % (
        run["path"], PRODUCERS.get(h["producer"], h["producer"]), MODES.get(h["mode"], h["mode"]),
        opt_str(h["options"]), h["ident1"], h["ident2"], h["fpcw"], h["ticks_per_second"], h["tag"], run["size"],
        " TRUNCATED" if run["truncated"] else ""))
    recs = [("T", t) for t in run["ticks"]] + [("P", p) for p in run["packets"]] + [("E", e) for e in run["events"]]
    recs.sort(key=lambda r: r[1]["_offset"])
    for kind, r in recs:
        if args.from_tick is not None and r["tick"] < args.from_tick:
            continue
        if args.to_tick is not None and r["tick"] > args.to_tick:
            continue
        if kind == "T":
            p = r["players"]
            print("T %6d dt=%.6g total=%.6g univ=%08x n=%d stat=%08x rngs=%d rng_all=%08x r10.n=%08x r6.n=%08x r0.n=%08x "
                  "ships=%d pos=%016x misc=%016x hp=%016x mis=%d bul=%d der=%d res=%d ru0=%d ru1=%d valid=%02x" % (
                      r["tick"], f32(r["dt"]), f32(r["total"]), r["univcheck"], r["nships"], r["statcheck"],
                      r["rng_streams"], r["rng_all"], r["rng10"][4], r["rng6"][4], r["rng0"][4], r["ships_n"],
                      r["ships_pos"], r["ships_misc"], r["ships_health"], r["missiles_n"], r["bullets_n"],
                      r["derelicts_n"], r["resources_n"], p[0], p[2], r["valid"]))
        elif kind == "P":
            print("P %6d packet=%d rand=%d univ=%08x blob=%08x extra=%08x cmds=%d err=%d/%d/%d/%d" % (
                r["tick"], r["packetnum"], r["randcheck"], r["univcheck"], r["blobcheck"], r["extra"],
                r["ncommands"], r["randSyncErr"], r["univSyncErr"], r["blobSyncErr"], r["pktSyncErr"]))
        else:
            print("E %6d %s %s" % (r["tick"], EVENTS.get(r["code"], r["code"]), r["text"]))


def perturb_copy(src, dst, tick, field, record):
    """Write a copy of src with one bit of FIELD flipped in the first 'T' (or 'P') record of TICK."""
    with open(src, "rb") as f:
        b = bytearray(f.read())
    run = read_hwo(src)
    layout = T_LAYOUT if record == "T" else P_LAYOUT
    recs = run["ticks"] if record == "T" else run["packets"]
    target = next((r for r in recs if r["tick"] == tick), None)
    if target is None:
        raise HwoError("no '%s' record at tick %d" % (record, tick))
    off = target["_offset"] + 4
    for name, code in layout:
        if name == field:
            b[off] ^= 0x01           # lowest bit of the field's first byte (little-endian: its least significant bit)
            with open(dst, "wb") as f:
                f.write(b)
            return off
        off += struct.calcsize("<" + code)
    raise HwoError("no field '%s' in a '%s' record (fields: %s)" % (field, record, ", ".join(n for n, _ in layout)))


def main(argv):
    ap = argparse.ArgumentParser(description=__doc__.split("\n")[0])
    ap.add_argument("files", nargs="*")
    ap.add_argument("--dump", metavar="FILE")
    ap.add_argument("--perturb-copy", nargs=2, metavar=("IN", "OUT"))
    ap.add_argument("--tick", type=int)
    ap.add_argument("--field")
    ap.add_argument("--record", choices=("T", "P"), default="T")
    ap.add_argument("--from-tick", type=int)
    ap.add_argument("--to-tick", type=int)
    ap.add_argument("--fields", type=lambda s: s.split(","))
    ap.add_argument("--max-draws", type=int, default=1000)
    ap.add_argument("--json", metavar="OUT")
    args = ap.parse_args(argv)
    try:
        if args.dump:
            dump(read_hwo(args.dump), args)
            return 0
        if args.perturb_copy:
            if args.tick is None or args.field is None:
                ap.error("--perturb-copy needs --tick and --field")
            off = perturb_copy(args.perturb_copy[0], args.perturb_copy[1], args.tick, args.field, args.record)
            print("wrote %s: flipped bit 0 of '%s' in the '%s' record of tick %d (file offset %d)" % (
                args.perturb_copy[1], args.field, args.record, args.tick, off))
            return 0
        if len(args.files) != 2:
            ap.error("give REF.hwo and CAND.hwo")
        if args.fields:
            unknown = [f for f in args.fields if f not in FIELD_NAMES]
            if unknown:
                ap.error("unknown fields %s (known: %s)" % (unknown, ", ".join(FIELD_NAMES)))
        ref, cand = read_hwo(args.files[0]), read_hwo(args.files[1])
    except (HwoError, OSError) as e:
        if isinstance(e, BrokenPipeError):
            raise
        print("hwo_compare: %s" % e, file=sys.stderr)
        return 2
    if not ref["ticks"] or not cand["ticks"]:
        print("hwo_compare: a file has no 'T' records", file=sys.stderr)
        return 2
    out, lines, rc = compare(ref, cand, args)
    print("\n".join(lines))
    if args.json:
        with open(args.json, "w") as f:
            json.dump(out, f, indent=1)
    return rc


if __name__ == "__main__":
    try:
        sys.exit(main(sys.argv[1:]))
    except BrokenPipeError:         # output piped into head & co.
        sys.stderr.close()
        sys.exit(0)
