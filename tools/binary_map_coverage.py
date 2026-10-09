#!/usr/bin/env python3
"""Coverage numbers for docs/binary-map.md, computed the way round 1 did (docs/binary-map.md s.2).

    python3 -I tools/binary_map_coverage.py --export ghidra/exports/cataclysm_gog_r2 \
        --before ghidra/exports/cataclysm_gog_r1 [--mid ghidra/exports/cataclysm_gog_r2a] --out /tmp/bmap_r2

Round 3 (labels and extra history columns are options; defaults reproduce round 2):

    python3 -I tools/binary_map_coverage.py --export ghidra/exports/cataclysm_gog_r3 \
        --history "round 1=ghidra/exports/cataclysm_gog_r1" --before ghidra/exports/cataclysm_gog_r2 \
        --before-label "round 2" --mid ghidra/exports/cataclysm_gog_r3a --mid-label "after B1" \
        --label "round 3" --new-tag "r3" \
        --symbols ghidra/symbols_round1.json ghidra/symbols_round2.json ghidra/symbols_round3.json \
        --props ghidra/proposals/r2/*.verified.json ghidra/proposals/r3/*.verified.json --out /tmp/bmap_r3

Relative paths resolve against the main checkout (the one holding ghidra/exports and reference/,
also when this runs from a linked git worktree).

Tiers: real name = not FUN_* and not class-only; class-only = lib_<class>_<addr>,
lib_<class>_Unwind_<addr>, Ghidra Unwind@/Catch@/thunk_FUN_/switchD_ names; unnamed = FUN_*.
Class per function: symbols_roundN.json `class=` notes (latest round wins), then the audited
proposal sets (ghidra/proposals/r2/*.verified.json, only where their name is the current name or
they are class-only), then CET (hw1_equivalent -> "CET (hw1)"), else "other named"/"unnamed".
HW1 `defs` = round 1's rough count: column-0 lines that look like `type name(` with a `{` before
the next `;` in reference/HomeworldSDL/src (approximate; #if branches and macros skew it).
Writes <out>/coverage.json and <out>/tables.md. Read-only on everything else.
"""
import argparse
import collections
import csv
import glob
import json
import os
import re
import subprocess

HERE = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))


def main_root():
    try:
        common = subprocess.check_output(["git", "-C", HERE, "rev-parse", "--path-format=absolute",
                                          "--git-common-dir"], text=True).strip()
        return os.path.dirname(common)
    except Exception:
        return HERE


ROOT = main_root()
CLASS_ONLY = re.compile(r"^(lib_|Unwind@|Catch@|thunk_FUN_|switchD)")
REGIONS = [
    ("game C/C++", 0x401000, 0x5ca0ff),
    ("IJG libjpeg", 0x5ca100, 0x5d6fdf),
    ("kas2c mission code", 0x5d6fe0, 0x670a0f),
    ("WSOCK32 thunks + static CRT", 0x670a10, 0x69412f),
    ("WON/Titan + AdApi + Magick", 0x694130, 0x705c9f),
    ("Dinkumware C++ std lib", 0x705ca0, 0x71cccf),
    ("late CRT", 0x71ccd0, 0x71f62f),
    ("text$x EH funclets", 0x71f630, 0x729fff),
    (".cms_t stub + CRT", 0xc55000, 0xc68fff),
]
MISSION = (0x5d6fe0, 0x670a0f)
GAME = (0x401000, 0x5c9fff)


def rp(p):
    return p if os.path.isabs(p) else os.path.join(ROOT, p)


def tier(name):
    if name.startswith("FUN_"):
        return "unnamed"
    if CLASS_ONLY.match(name):
        return "class-only"
    return "real"


def load_funcs(exp):
    out = {}
    with open(os.path.join(exp, "functions.csv"), newline="") as fh:
        for r in csv.DictReader(fh):
            out[int(r["address"], 16)] = {"name": r["name"], "size": int(r["size"] or 0),
                                          "section": r["section"], "calls_in": int(r["calls_in"] or 0)}
    return out


def note_field(notes, key):
    m = re.search(r"(?:^|;\s*)%s=([^;]*)" % key, notes or "")
    return m.group(1).strip() if m else ""


def load_symbols(path):
    if not os.path.isfile(path):
        return {}
    out = {}
    for s in json.load(open(path)).get("symbols", []):
        try:
            out[int(s["address"], 16)] = s
        except Exception:
            pass
    return out


def class_label(cls, lib):
    return "unknown, lib=%s" % lib if cls == "unknown" and lib else cls


def build_classes(funcs, symfiles, propfiles, cet):
    """address -> (class label, confidence or '')"""
    rounds = [load_symbols(p) for p in symfiles]  # oldest first
    props = collections.defaultdict(list)
    for pf in propfiles:
        try:
            for p in json.load(open(pf)).get("proposals", []):
                props[int(p["address"], 16)].append(p)
        except Exception:
            pass
    out = {}
    for a, f in funcs.items():
        name = f["name"]
        got = None
        for rnd in reversed(rounds):
            s = rnd.get(a)
            if s and s.get("kind", "function") == "function":
                c = note_field(s.get("notes"), "class")
                if c:
                    got = (class_label(c, note_field(s.get("notes"), "lib")), note_field(s.get("notes"), "confidence"))
                    break
        if got is None:
            for p in props.get(a, []):
                if p.get("kind", "function") == "function" and p.get("name") in ("", None, name) and p.get("class"):
                    got = (p["class"], p.get("confidence", ""))
                    break
        if got is None and a in cet:
            got = ("CET (hw1)" if cet[a].get("hw1_equivalent") else "CET (other)", "")
        if got is None:
            got = ("unnamed FUN_*", "") if tier(name) == "unnamed" else ("other named", "")
        out[a] = got
    return out


def coverage(funcs, lo=None, hi=None):
    c, b = collections.Counter(), collections.Counter()
    for a, f in funcs.items():
        if lo is not None and not (lo <= a <= hi):
            continue
        t = tier(f["name"])
        c[t] += 1
        b[t] += f["size"]
    return c, b


def body_union(funcs, lo, hi):
    """Bytes of [lo, hi] inside some function's [address, address+size) (round-3 B1 metric)."""
    iv = sorted((a, a + f["size"]) for a, f in funcs.items() if f["size"] and a <= hi and a + f["size"] > lo)
    tot, cur_s, cur_e = 0, None, None
    for s, e in iv:
        s, e = max(s, lo), min(e, hi + 1)
        if cur_e is None or s > cur_e:
            if cur_e is not None:
                tot += cur_e - cur_s
            cur_s, cur_e = s, e
        else:
            cur_e = max(cur_e, e)
    if cur_e is not None:
        tot += cur_e - cur_s
    return tot


def hw1_defs():
    """basename -> (relpath under src/, set(definition names))."""
    src = os.path.join(ROOT, "reference", "HomeworldSDL", "src")
    out = {}
    head = re.compile(r"^(?!(?:if|while|for|switch|return|else|do|typedef|struct|enum|union|sizeof)\b)"
                      r"[A-Za-z_][\w \t\*:<>,]*?\b((?:\w+::)*~?[A-Za-z_]\w*)\s*\(")
    rank = lambda r: (0 if r.startswith("Game/") else 1 if r.startswith("SDL/") else 2)
    for path in sorted(glob.glob(os.path.join(src, "**", "*.c*"), recursive=True)):
        if not re.search(r"\.(c|cpp)$", path):
            continue
        rel = os.path.relpath(path, src)
        text = open(path, encoding="latin-1").read()
        names, count = set(), set()
        pos = 0
        for ln in text.split("\n"):
            start = pos
            pos += len(ln) + 1
            if not ln or ln[0] in " \t#/{}*":
                continue
            m = head.match(ln)
            if not m:
                continue
            rest = text[start + m.end():start + m.end() + 4000]
            br, sc = rest.find("{"), rest.find(";")
            if br >= 0 and (sc < 0 or br < sc):
                full = m.group(1)
                count.add(full)
                names.add(full.split("::")[-1])
                if "::" in full:
                    names.add(full.replace("::", "__"))
        base = os.path.basename(rel)
        if base not in out or rank(rel) < rank(out[base][0]):
            out[base] = (rel, names, len(count))
    return out


def strip_suffix(n):
    return re.sub(r"_\d+$", "", n)


def fmt(n):
    return "{:,}".format(n)


def pct(a, b):
    return "%.1f%%" % (100.0 * a / b) if b else "-"


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--export", required=True)
    ap.add_argument("--before", required=True)
    ap.add_argument("--baseline", default="ghidra/exports/cataclysm_gog")
    ap.add_argument("--mid", default="", help="optional intermediate export (e.g. after A1)")
    ap.add_argument("--history", nargs="*", default=[],
                    help="extra earlier columns LABEL=EXPORT between baseline and --before (e.g. 'round 1=ghidra/exports/cataclysm_gog_r1')")
    ap.add_argument("--label", default="round 2", help="column label of --export")
    ap.add_argument("--before-label", default="round 1", help="column label of --before")
    ap.add_argument("--mid-label", default="after A1", help="column label of --mid")
    ap.add_argument("--symbols", nargs="+", default=["ghidra/symbols_round1.json", "ghidra/symbols_round2.json"])
    ap.add_argument("--props", nargs="*", default=None)
    ap.add_argument("--cet", default=os.path.join(HERE, "ghidra/cet_symbols.json"))
    ap.add_argument("--out", required=True)
    ap.add_argument("--top", type=int, default=60)
    ap.add_argument("--new-tag", default="A1", help="tag for top-unnamed functions absent from --before")
    a = ap.parse_args()
    os.makedirs(a.out, exist_ok=True)
    symfiles = [p if os.path.isabs(p) else (os.path.join(HERE, p) if os.path.isfile(os.path.join(HERE, p)) else rp(p))
                for p in a.symbols]
    props = a.props if a.props is not None else sorted(glob.glob(os.path.join(HERE, "ghidra/proposals/r2/*.verified.json")))
    L, BL = a.label, a.before_label

    now, before, base = load_funcs(rp(a.export)), load_funcs(rp(a.before)), load_funcs(rp(a.baseline))
    mid = load_funcs(rp(a.mid)) if a.mid else None
    cet = {int(s["address"], 16): s for s in json.load(open(a.cet))["symbols"] if s.get("kind") == "function"}
    classes = build_classes(now, symfiles, props, cet)
    res = {"export": a.export, "before": a.before}
    md = []

    hist = [(h.split("=", 1)[0], load_funcs(rp(h.split("=", 1)[1]))) for h in a.history]
    cols = [("baseline", base)] + hist + [(BL, before)] + ([(a.mid_label, mid)] if mid else []) + [(L, now)]
    tot = {}
    for label, fs in cols:
        c, b = coverage(fs)
        tot[label] = {"functions": len(fs), "bytes": sum(f["size"] for f in fs.values()), "count": dict(c),
                      "bytes_by_tier": dict(b)}
    res["overall"] = tot
    md.append("### Overall\n")
    md.append("| tier | " + " | ".join("%s functions | %s bytes" % (l, l) for l, _ in cols) + " | %s %% of bytes |" % L)
    md.append("|---|" + "---:|---:|" * len(cols) + "---:|")
    for t, lab in (("real", "real name"), ("class-only", "class-only"), ("unnamed", "unnamed `FUN_*`")):
        md.append("| %s | " % lab + " | ".join("%s | %s" % (fmt(tot[l]["count"].get(t, 0)), fmt(tot[l]["bytes_by_tier"].get(t, 0)))
                                             for l, _ in cols) + " | %s |" % pct(tot[L]["bytes_by_tier"].get(t, 0), tot[L]["bytes"]))
    md.append("| **all functions** | " + " | ".join("%s | %s" % (fmt(tot[l]["functions"]), fmt(tot[l]["bytes"])) for l, _ in cols) + " | 100% |")
    md.append("")

    for key, (lo, hi) in (("game", GAME), ("mission", MISSION)):
        res[key] = {}
        for label, fs in cols:
            c, b = coverage(fs, lo, hi)
            allb = sum(b.values())
            res[key][label] = {"functions": sum(c.values()), "bytes": allb, "count": dict(c), "bytes_by_tier": dict(b),
                               "real_pct_bytes": round(100.0 * b.get("real", 0) / allb, 2) if allb else 0.0,
                               "region_bytes": hi - lo + 1,
                               "real_pct_region": round(100.0 * b.get("real", 0) / (hi - lo + 1), 2),
                               "body_bytes_union": body_union(fs, lo, hi),
                               "body_pct_region": round(100.0 * body_union(fs, lo, hi) / (hi - lo + 1), 2)}

    md.append("### Per address region\n")
    md.append("| region | %s functions | %s functions | real | class-only | `FUN_*` | `FUN_*` bytes | real %% of function bytes, %s (%s) |" % (BL, L, L, BL))
    md.append("|---|---:|---:|---:|---:|---:|---:|---|")
    res["regions"] = []
    for name, lo, hi in REGIONS:
        c, b = coverage(now, lo, hi)
        c1, b1 = coverage(before, lo, hi)
        md.append("| %s 0x%x-0x%x | %s | %s | %s | %s | %s | %s | %s (%s) |" % (
            name, lo, hi, fmt(sum(c1.values())), fmt(sum(c.values())), fmt(c.get("real", 0)), fmt(c.get("class-only", 0)),
            fmt(c.get("unnamed", 0)), fmt(b.get("unnamed", 0)), pct(b.get("real", 0), sum(b.values())),
            pct(b1.get("real", 0), sum(b1.values()))))
        res["regions"].append({"region": name, "lo": hex(lo), "hi": hex(hi), "count": dict(c), "bytes": dict(b)})
    md.append("")

    md.append("### Per section\n")
    md.append("| section | functions | bytes | real | class-only | `FUN_*` | `FUN_*` bytes |")
    md.append("|---|---:|---:|---:|---:|---:|---:|")
    secs = collections.defaultdict(lambda: [0, 0, collections.Counter(), collections.Counter()])
    for f in now.values():
        s = secs[f["section"]]
        s[0] += 1
        s[1] += f["size"]
        s[2][tier(f["name"])] += 1
        s[3][tier(f["name"])] += f["size"]
    for k, s in sorted(secs.items(), key=lambda kv: -kv[1][1]):
        md.append("| %s | %s | %s | %s | %s | %s | %s |" % (k, fmt(s[0]), fmt(s[1]), fmt(s[2]["real"]),
                                                       fmt(s[2]["class-only"]), fmt(s[2]["unnamed"]), fmt(s[3]["unnamed"])))
    md.append("")

    pc = collections.defaultdict(lambda: {"n": 0, "bytes": 0, "real": 0, "cls": 0, "conf": collections.Counter(), "lo": None, "hi": None})
    for ad, f in now.items():
        cl, conf = classes[ad]
        p = pc["hw1:* (all files)" if cl.startswith("hw1:") else cl]
        p["n"] += 1
        p["bytes"] += f["size"]
        t = tier(f["name"])
        p["real"] += t == "real"
        p["cls"] += t == "class-only"
        if conf:
            p["conf"][conf] += 1
        p["lo"] = ad if p["lo"] is None else min(p["lo"], ad)
        p["hi"] = ad if p["hi"] is None else max(p["hi"], ad)
    md.append("### Per class\n")
    md.append("| class | functions | bytes | real | class-only | high | medium | low | lowest - highest entry |")
    md.append("|---|---:|---:|---:|---:|---:|---:|---:|---|")
    res["classes"] = {}
    for k, p in sorted(pc.items(), key=lambda kv: (kv[0] == "unnamed FUN_*", kv[0] == "other named", -kv[1]["bytes"])):
        md.append("| %s | %s | %s | %s | %s | %s | %s | %s | 0x%08x - 0x%08x |" % (
            k, fmt(p["n"]), fmt(p["bytes"]), fmt(p["real"]), fmt(p["cls"]), fmt(p["conf"]["high"]),
            fmt(p["conf"]["medium"]), fmt(p["conf"]["low"]), p["lo"], p["hi"]))
        res["classes"][k] = {"functions": p["n"], "bytes": p["bytes"], "real": p["real"]}
    md.append("")

    defs = hw1_defs()
    perfile = collections.defaultdict(lambda: {"matched": set(), "cet": set()})
    for ad in now:
        cl, _ = classes[ad]
        if cl.startswith("hw1:"):
            perfile[cl[4:]]["matched"].add(ad)
    for ad, s in cet.items():
        eq = s.get("hw1_equivalent") or ""
        if ad in now and eq and now[ad]["name"] == s["name"]:
            fn = os.path.basename(eq.split(":")[0])
            if fn:
                perfile[fn]["matched"].add(ad)
                perfile[fn]["cet"].add(ad)
    rows = []
    for fn, p in perfile.items():
        d = defs.get(fn)
        verb = len([ad for ad in p["matched"] if d and strip_suffix(now[ad]["name"]) in d[1]])
        rows.append((fn, d[0] if d else "(not in HomeworldSDL)", len(p["matched"]), len(p["cet"]),
                     d[2] if d else None, verb, sum(now[ad]["size"] for ad in p["matched"])))
    rows.sort(key=lambda r: (-r[2], r[0]))
    tot_m, tot_v = sum(r[2] for r in rows), sum(r[5] for r in rows)
    md.append("### Per HW1 source file\n")
    md.append("Totals: %s matched functions in %d files (verbatim %s). These files define about %s functions; "
              "HomeworldSDL `src/` has %d .c/.cpp basenames with about %s definitions.\n" % (
                  fmt(tot_m), len(rows), fmt(tot_v), fmt(sum(r[4] or 0 for r in rows)), len(defs),
                  fmt(sum(v[2] for v in defs.values()))))
    md.append("| HW1 file | path under src/ | matched | of which CET | defs (approx.) | matched / defs | verbatim | bytes |")
    md.append("|---|---|---:|---:|---:|---:|---:|---:|")
    for r in rows:
        md.append("| %s | %s | %d | %d | %s | %s | %d | %s |" % (r[0], r[1], r[2], r[3], "-" if r[4] is None else r[4],
                                                              "-" if not r[4] else pct(r[2], r[4]), r[5], fmt(r[6])))
    zero = sorted(((v[0], v[2]) for k, v in defs.items() if k not in perfile and v[2]), key=lambda x: (-x[1], x[0]))
    md.append("")
    md.append("**HW1 files with 0 matches** (%d files that define at least one function; defs approx., most first):\n" % len(zero))
    md.append(", ".join("%s (%d)" % z for z in zero))
    md.append("")
    res["hw1_files"] = {"matched": tot_m, "files": len(rows), "verbatim": tot_v, "zero_match_files": len(zero),
                        "rows": [{"file": r[0], "path": r[1], "matched": r[2], "cet": r[3], "defs": r[4], "verbatim": r[5], "bytes": r[6]} for r in rows],
                        "zero": [{"path": z[0], "defs": z[1]} for z in zero]}

    strs = collections.defaultdict(list)
    with open(os.path.join(rp(a.export), "strings.csv"), newline="") as fh:
        for r in csv.DictReader(fh):
            for x in (r.get("xref_functions") or "").split("|"):
                if "@" in x:
                    try:
                        strs[int(x.rsplit("@", 1)[1], 16)].append(r["string"])
                    except ValueError:
                        pass
    callers, callees = collections.defaultdict(set), collections.defaultdict(set)
    with open(os.path.join(rp(a.export), "call_graph.csv"), newline="") as fh:
        for r in csv.DictReader(fh):
            try:
                ca = int(r["caller_address"], 16)
            except ValueError:
                continue
            if tier(r["callee"]) == "real":
                callees[ca].add(r["callee"])
            try:
                ce = int(r["callee_address"], 16)
            except ValueError:
                continue
            if tier(r["caller"]) == "real":
                callers[ce].add(r["caller"])
    un = sorted(((ad, f) for ad, f in now.items() if tier(f["name"]) == "unnamed"), key=lambda kv: (-kv[1]["size"], kv[0]))
    md.append("### Largest still-unnamed functions\n")
    md.append("| # | address | size | calls in | strings | named callers | named callees | in %s |" % BL)
    md.append("|---:|---|---:|---:|---|---|---|---|")
    res["top_unnamed"] = []
    for i, (ad, f) in enumerate(un[:a.top], 1):
        ss = []
        for s in strs.get(ad, []):
            s = s.replace("|", "\\|").replace("`", "'")[:40]
            if s not in ss:
                ss.append(s)
        cr, ce = sorted(callers.get(ad, [])), sorted(callees.get(ad, []))
        was = ("new (%s)" % a.new_tag) if ad not in before else ("yes" if before[ad]["name"] == f["name"] else before[ad]["name"])
        md.append("| %d | 0x%08x | %s | %d | %s | %s | %s | %s |" % (
            i, ad, fmt(f["size"]), f["calls_in"], "; ".join("`%s`" % s for s in ss[:3]) or "-",
            ", ".join(cr[:3]) or "-", ", ".join(ce[:3]) or "-", was))
        res["top_unnamed"].append({"address": "0x%08x" % ad, "size": f["size"], "calls_in": f["calls_in"], "strings": ss[:3],
                                   "callers": cr[:3], "callees": ce[:3], "in_round1": was})
    md.append("")
    bands = collections.defaultdict(lambda: [0, 0])
    for ad, f in un:
        bands[ad >> 16][0] += 1
        bands[ad >> 16][1] += f["size"]
    md.append("Remaining `FUN_*` by 64 KB band (count / bytes): " + ", ".join(
        "0x%04xxxxx %d/%s" % (k, v[0], fmt(v[1])) for k, v in sorted(bands.items())) + ".")
    md.append("")
    res["unnamed_bands"] = {"0x%04x" % k: v for k, v in sorted(bands.items())}

    mv = collections.Counter()
    for ad, f in now.items():
        if ad not in before:
            mv["new function -> " + tier(f["name"])] += 1
        elif before[ad]["name"] != f["name"]:
            mv["%s -> %s" % (tier(before[ad]["name"]), tier(f["name"]))] += 1
    mv["%s functions gone" % BL] = len([ad for ad in before if ad not in now])
    res["movement"] = dict(mv)
    json.dump(res, open(os.path.join(a.out, "coverage.json"), "w"), indent=1)
    open(os.path.join(a.out, "tables.md"), "w").write("\n".join(md) + "\n")
    print(json.dumps({"overall": res["overall"], "game": res["game"], "mission": res["mission"], "movement": mv,
                      "hw1": {k: res["hw1_files"][k] for k in ("matched", "files", "verbatim", "zero_match_files")}}, indent=1))


if __name__ == "__main__":
    main()
