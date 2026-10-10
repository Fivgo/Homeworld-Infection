#!/usr/bin/env python3
"""M2-R1 function-level delta map: Cataclysm (decompiled) vs Homeworld 1 source (ORIG@ad1bb82).

For every function the binary map matched to Homeworld 1 (class hw1:<File> in the
symbols_round*.json notes, the audited proposal sets, and CET's hw1_equivalent entries; the same
rule docs/binary-map.md uses for its "4,426 HW1 matches"), extract

  * the HW1 body from Relic's 2003 source (GoK root commit ad1bb82), preprocessed with
    `cpp -DHW_Release -DWIN32 ...` so that debug-only code (dbgAssert, *_VERBOSE_LEVEL,
    *_ERROR_CHECKING ...) is compiled out the way a retail build compiles it out, and macros
    (constants, function-like wrappers) are expanded the way the compiler saw them;
  * the Cataclysm body from the Ghidra export (decompiled.c + decompiled_index.csv) and its
    callees from call_graph.csv;

computes comparable features (callee set, string literals, distinctive numeric constants,
switch/case, loop and branch counts, parameter count, body size against a per-file size ratio),
scores them and proposes IDENTICAL / MODIFIED / REWRITTEN with a confidence and a one-line
automatic change description. Hand review results are merged from a JSON file (--review).

Stages (each resumable; outputs are overwritten whole after every finished group):
  prep      download-free: needs the ad1bb82 src tree (default: original/scratch-m2/delta-orig/...)
            builds a case-insensitive include dir + stubs, runs cpp per .c/.cpp file, indexes
            function definitions -> <work>/hw1_index.json
  classify  [--groups ...] features + proposed classes -> ghidra/delta_map.json (incremental)
  show      ADDR [ADDR...]: print the HW1 source and the Cataclysm decompiled body side by side
            (for hand review)
  report    write docs/m2-r1-function-deltas.md from ghidra/delta_map.json

All game data stays outside the repo: the work dir is original/scratch-m2/delta-work (gitignored).
"""
import argparse
import collections
import csv
import glob
import importlib.util
import json
import math
import os
import re
import struct
import subprocess
import sys

HERE = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))


def main_root():
    try:
        common = subprocess.check_output(["git", "-C", HERE, "rev-parse", "--path-format=absolute",
                                          "--git-common-dir"], text=True, stderr=subprocess.DEVNULL).strip()
        return os.path.dirname(common)
    except Exception:
        return HERE


ROOT = main_root()
EXPORT = os.path.join(ROOT, "ghidra/exports/cataclysm_gog_r5")
ORIG = os.path.join(ROOT, "original/scratch-m2/delta-orig/gardens-of-kadesh-ad1bb82c-src/src")
WORK = os.path.join(ROOT, "original/scratch-m2/delta-work")
OUT_JSON = os.path.join(HERE, "ghidra/delta_map.json")
OUT_MD = os.path.join(HERE, "docs/m2-r1-function-deltas.md")
REVIEW = os.path.join(HERE, "ghidra/delta_map_review.json")
SRC_DIRS = ["Game", "Win32", "Ships", "SinglePlayer"]
GOK = os.path.join(ROOT, "reference", "gardens-of-kadesh", "src")
GROUPS = ["Game a-f", "Game g-m", "Game n-s", "Game t-z", "Ships", "other"]

CPP_DEFS = ["-DHW_Release", "-DWIN32", "-D_WIN32", "-D_X86_", "-D_MSC_VER=1100", "-D_M_IX86=600",
            "-D__MSC__=yes", "-DHW_English", "-D_MT"]

KEYWORDS = set("""if else while for do switch case default return break continue goto sizeof typedef
struct union enum static extern const volatile register auto signed unsigned int char short long float
double void inline __inline _inline __forceinline __asm _asm asm __declspec __cdecl __stdcall __fastcall
__thiscall""".split())

# compiler intrinsics / inlinable libc: excluded from callee sets on both sides
SOFT_CALLS = set("""memset memcpy memmove memcmp strcpy strncpy strlen strcmp strcat abs labs fabs sqrt
sin cos tan atan atan2 acos asin exp log log10 pow fmod floor ceil _ftol ftol chkstk alloca_probe
allmul alldiv allrem aulldiv aullrem allshl allshr aullshr _CIsin _CIcos _CIsqrt _CItan _CIatan
_CIatan2 _CIacos _CIasin _CIexp _CIlog _CIpow _CIfmod fsqrt fsin fcos ftol
""".split())
CI_MAP = {"_CIsin": "sin", "_CIcos": "cos", "_CIsqrt": "sqrt", "_CItan": "tan", "_CIatan": "atan",
          "_CIatan2": "atan2", "_CIacos": "acos", "_CIasin": "asin", "_CIexp": "exp", "_CIlog": "log",
          "_CIpow": "pow", "_CIfmod": "fmod"}

TOKEN_RE = re.compile(r'''
   (?P<str>L?"(?:\\.|[^"\\\n])*")
 | (?P<chr>L?'(?:\\.|[^'\\\n])+')
 | (?P<num>(?:\d|\.\d)(?:[eEpP][+-]|[\w.])*)
 | (?P<id>[A-Za-z_$][\w$]*)
 | (?P<op>::|->|\+\+|--|<<=|>>=|<<|>>|<=|>=|==|!=|&&|\|\||[-+*/%&|^]=|\.\.\.|[{}()\[\];,.<>=!~?:+\-*/%&|^#@\\])
 | (?P<ws>\s+)
 | (?P<other>.)
''', re.X | re.S)

COMMENT_RE = re.compile(r'''("(?:\\.|[^"\\\n])*"|'(?:\\.|[^'\\\n])+')|/\*.*?\*/|//[^\n]*''', re.S)


def strip_comments(text):
    return COMMENT_RE.sub(lambda m: m.group(1) if m.group(1) else " ", text)


def tokenize(text):
    out = []
    for m in TOKEN_RE.finditer(text):
        k = m.lastgroup
        if k == "ws":
            continue
        out.append((k, m.group(), m.start()))
    return out


def unescape(lit):
    s = lit[1:-1] if not lit.startswith("L") else lit[2:-1]
    try:
        return bytes(s, "latin-1").decode("unicode_escape")
    except Exception:
        return s


def parse_num(tok):
    t = tok.lower()
    try:
        if t.startswith("0x"):
            return ("i", int(re.sub(r"[ul]+$", "", t), 16) & 0xffffffff)
        if re.fullmatch(r"\d+[ul]*", t):
            s = re.sub(r"[ul]+$", "", t)
            v = int(s, 8) if (len(s) > 1 and s.startswith("0")) else int(s)
            return ("i", v & 0xffffffff)
        v = float(re.sub(r"[fl]+$", "", t))
        if v == int(v) and abs(v) < 1e9 and "e" not in t:
            pass
        return ("f", float_key(v))
    except Exception:
        return None


def float_key(v):
    try:
        v32 = struct.unpack("<f", struct.pack("<f", v))[0]
    except OverflowError:
        v32 = v
    return "%.5g" % v32


def char_val(tok):
    s = unescape(tok if not tok.startswith("L") else tok[1:])
    return ord(s[0]) if s else 0


def distinctive_int(v):
    sv = v - (1 << 32) if v >= 0x80000000 else v
    return not (-2 <= sv <= 16)


def distinctive_float(k):
    return k not in ("0", "1", "-1", "0.5", "2", "-0.5")


# ----------------------------------------------------------------------------------- prep stage
def build_include_dir(orig, inc):
    os.makedirs(inc, exist_ok=True)
    by_lower = {}
    for path in glob.glob(os.path.join(orig, "**", "*"), recursive=True):
        if os.path.isfile(path) and re.search(r"\.(h|hpp|inc)$|/STRING$", path, re.I):
            by_lower.setdefault(os.path.basename(path).lower(), path)
    spell = set()
    for path in glob.glob(os.path.join(orig, "**", "*"), recursive=True):
        if not os.path.isfile(path) or not re.search(r"\.(c|cpp|h|hpp)$", path, re.I):
            continue
        for m in re.finditer(r'^\s*#\s*include\s*[<"]([^>"]+)[>"]', open(path, encoding="latin-1").read(), re.M):
            spell.add(m.group(1))
    made = {"link": 0, "stub": 0}
    for sp in sorted(spell):
        dst = os.path.join(inc, sp)
        if os.path.lexists(dst):
            continue
        os.makedirs(os.path.dirname(dst), exist_ok=True)
        base = re.split(r"[\\/]", sp)[-1].lower()
        src = by_lower.get(base)
        if src:
            os.symlink(src, dst)
            made["link"] += 1
        else:
            open(dst, "w").close()
            made["stub"] += 1
    return made


LINEMARK = re.compile(r'^#\s+(\d+)\s+"([^"]*)"')


def preprocess(path, inc, orig, extra=()):
    args = ["cpp", "-nostdinc", "-undef", "-w", "-I", inc, "-isystem", inc] + CPP_DEFS + list(extra) + [
        "-I", os.path.dirname(path), path]
    for _ in range(60):
        p = subprocess.run(args, capture_output=True, text=True, encoding="latin-1")
        m = re.search(r"fatal error: (.+?): No such file or directory", p.stderr)
        if not m:
            break
        stub = os.path.join(inc, m.group(1))
        os.makedirs(os.path.dirname(stub), exist_ok=True)
        open(stub, "w").close()
    return p.stdout, p.returncode, p.stderr[-2000:]


def index_defs(pp_text, srcfile):
    """Yield definitions found in preprocessed text: name, params text, body tokens, origin file+lines."""
    lines = pp_text.split("\n")
    # flatten with origin info
    buf, origin = [], []
    cur_file, cur_line = srcfile, 1
    pos = 0
    starts = []
    for ln in lines:
        m = LINEMARK.match(ln)
        if m:
            cur_line, cur_file = int(m.group(1)), m.group(2)
            continue
        starts.append(pos)
        origin.append((cur_file, cur_line))
        buf.append(ln)
        pos += len(ln) + 1
        cur_line += 1
    text = "\n".join(buf)
    toks = tokenize(text)
    import bisect

    def where(off):
        i = bisect.bisect_right(starts, off) - 1
        return origin[max(0, i)]

    defs = []
    depth = 0
    stack = []
    i = 0
    n = len(toks)
    while i < n:
        k, v, off = toks[i]
        if v == "{" and depth == 0 and i > 0 and (toks[i - 1][1] == '"C"' or toks[i - 1][1] == "namespace" or (
                i > 1 and toks[i - 2][1] == "namespace")):
            stack.append("t")
            i += 1
            continue
        if v == "}" and depth == 0 and stack:
            stack.pop()
            i += 1
            continue
        if v == "{" and depth == 0:
            # find a function head ending at i-1
            j = i - 1
            while j >= 0 and toks[j][1] in ("const",):
                j -= 1
            if j >= 0 and toks[j][1] == ")":
                pd, jj = 0, j
                while jj >= 0:
                    if toks[jj][1] == ")":
                        pd += 1
                    elif toks[jj][1] == "(":
                        pd -= 1
                        if pd == 0:
                            break
                    jj -= 1
                if jj > 0 and toks[jj - 1][0] == "id" and toks[jj - 1][1] not in KEYWORDS:
                    name = toks[jj - 1][1]
                    if jj > 2 and toks[jj - 2][1] == "::":
                        name = toks[jj - 3][1] + "::" + name
                    # body
                    bd, e = 0, i
                    while e < n:
                        if toks[e][1] == "{":
                            bd += 1
                        elif toks[e][1] == "}":
                            bd -= 1
                            if bd == 0:
                                break
                        e += 1
                    params = [t[1] for t in toks[jj + 1:j]]
                    f0, l0 = where(toks[jj - 1][2])
                    f1, l1 = where(toks[min(e, n - 1)][2])
                    defs.append({"name": name, "params": params, "body": [(t[0], t[1]) for t in toks[i:e + 1]],
                                 "file": f0, "line": l0, "end_line": l1 if f1 == f0 else None})
                    i = e + 1
                    continue
        if v == "{":
            depth += 1
        elif v == "}":
            depth = max(0, depth - 1)
        i += 1
    return defs


def count_params(ptoks):
    if not ptoks or ptoks == ["void"]:
        return 0
    n, d = 1, 0
    for t in ptoks:
        if t in "([":
            d += 1
        elif t in ")]":
            d -= 1
        elif t == "," and d == 0:
            n += 1
    return n


def hw1_features(body):
    calls, strs, ints, flts = set(), [], set(), set()
    fields, ids, mcalls = set(), set(), set()
    ctl = collections.Counter()
    pend = []
    for idx, (k, v) in enumerate(body):
        nxt = body[idx + 1][1] if idx + 1 < len(body) else ""
        prv = body[idx - 1][1] if idx else ""
        if k == "str":
            pend.append(unescape(v))
            continue
        if pend:
            strs.append("".join(pend))
            pend = []
        if k == "id":
            if v in ("if", "for", "while", "do", "switch", "case", "goto", "return", "else", "default"):
                ctl[v] += 1
            elif nxt == "(" and prv not in (".", "->") and v not in KEYWORDS:
                calls.add(v)
            elif prv in (".", "->"):
                fields.add(v)
                if nxt == "(":
                    mcalls.add(v)   # C++ method / function-pointer member call
            elif v not in KEYWORDS:
                ids.add(v)
        elif k == "num":
            r = parse_num(v)
            if r:
                (ints if r[0] == "i" else flts).add(r[1])
        elif k == "chr":
            ints.add(char_val(v))
    if pend:
        strs.append("".join(pend))
    return {"calls": sorted(calls), "strings": sorted(set(strs)), "ints": sorted(ints), "floats": sorted(flts),
            "ctl": dict(ctl), "ntok": len(body), "fields": sorted(fields), "ids": sorted(ids),
            "mcalls": sorted(mcalls)}


MACRO_CALL = re.compile(r"^\s*#\s*define\s+([A-Za-z_]\w*)\s*\(([^)]*)\)\s*(?:\(\s*)*(?:\([A-Za-z_][\w\s\*]*\)\s*)?([A-Za-z_]\w*)\s*\(", re.M)


STR_GLOBAL = re.compile(r'^\s*(?:static\s+)?(?:unsigned\s+)?char\s+([A-Za-z_]\w*)\s*\[[^\]]*\]\s*=\s*"((?:\\.|[^"\\])*)"', re.M)


def string_globals(orig):
    """char NAME[] = "..." initialisers: Ghidra shows such arrays as string literals."""
    out = {}
    for path in glob.glob(os.path.join(orig, "**", "*.c"), recursive=True):
        for m in STR_GLOBAL.finditer(open(path, encoding="latin-1").read()):
            out.setdefault(m.group(1), unescape('"%s"' % m.group(2)))
    return out


def macro_aliases(orig):
    """function-like macros whose body is a call of another function: NAME -> CALLEE."""
    out = {}
    for path in glob.glob(os.path.join(orig, "**", "*"), recursive=True):
        if not os.path.isfile(path) or not re.search(r"\.(c|cpp|h)$", path, re.I):
            continue
        for m in MACRO_CALL.finditer(open(path, encoding="latin-1").read()):
            name, callee = m.group(1), m.group(3)
            if callee not in KEYWORDS and callee != name:
                out.setdefault(name, callee)
    return out


def index_tree(root, dirs, inc, extra=()):
    index, fails = {}, []
    for d in dirs:
        for path in sorted(glob.glob(os.path.join(root, d, "*"))):
            if not re.search(r"\.(c|cpp)$", path, re.I):
                continue
            rel = os.path.relpath(path, root)
            pp, rc, err = preprocess(path, inc, root, extra)
            if rc != 0:
                fails.append({"file": rel, "rc": rc, "stderr": err.strip().splitlines()[-3:]})
            for df in index_defs(pp, path):
                own = os.path.relpath(df["file"], root) if df["file"].startswith(root) else df["file"]
                feat = hw1_features(df["body"])
                feat["nparams"] = count_params(df["params"])
                index.setdefault(df["name"], []).append({
                    "unit": rel, "file": own, "line": df["line"], "end_line": df["end_line"], "features": feat})
    return index, fails


def stage_prep(a):
    os.makedirs(a.work, exist_ok=True)
    inc = os.path.join(a.work, "inc")
    made = build_include_dir(a.orig, inc)
    print("include dir:", made)
    index, fails = index_tree(a.orig, SRC_DIRS, inc)
    aliases = macro_aliases(a.orig)
    # fallback for names Relic's tree does not define (HomeworldSDL renames, the .obj-only audio
    # codec): the GoK working tree (read-only reference clone); rows using it say hw1_source=GoK-HEAD
    ginc = os.path.join(a.work, "inc-gok")
    build_include_dir(GOK, ginc)
    gindex, gfails = index_tree(GOK, ["Game", "SDL", "Ships"], ginc, ["-D_LINUX_FIX_ME"])
    gindex = {k: v for k, v in gindex.items() if k not in index}
    json.dump({"orig": a.orig, "cpp_defs": CPP_DEFS, "cpp_failures": fails, "defs": index, "aliases": aliases,
               "string_globals": string_globals(a.orig),
               "gok_root": GOK, "gok_defs": gindex, "gok_cpp_failures": len(gfails)},
              open(os.path.join(a.work, "hw1_index.json"), "w"))
    print("defs:", sum(len(v) for v in index.values()), "names:", len(index), "cpp rc!=0:", len(fails),
          "aliases:", len(aliases), "gok-only names:", len(gindex))


# ------------------------------------------------------------------------------ classify stage
def load_cov():
    spec = importlib.util.spec_from_file_location("bmc", os.path.join(HERE, "tools/binary_map_coverage.py"))
    m = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(m)
    return m


def hw1_targets():
    """address -> {class, hw1_file, hw1_func, source} using the binary map's class rule."""
    cov = load_cov()
    funcs = cov.load_funcs(EXPORT)
    syms = [os.path.join(HERE, "ghidra/symbols_round%d.json" % r) for r in range(1, 6)]
    props = []
    for r in ("r2", "r3", "r4", "r5"):
        props += sorted(glob.glob(os.path.join(HERE, "ghidra/proposals", r, "*.verified.json")))
    cet = {int(s["address"], 16): s for s in json.load(open(os.path.join(HERE, "ghidra/cet_symbols.json")))["symbols"]
           if s.get("kind") == "function"}
    classes = cov.build_classes(funcs, syms, props, cet)
    rounds = [cov.load_symbols(p) for p in syms]
    eqs = {}
    for rnd in rounds:
        for ad, s in rnd.items():
            if s.get("hw1_equivalent"):
                eqs[ad] = s["hw1_equivalent"]
    for pf in props:
        for p in json.load(open(pf)).get("proposals", []):
            try:
                ad = int(p["address"], 16)
            except Exception:
                continue
            if p.get("hw1_equivalent") and ad not in eqs:
                eqs[ad] = p["hw1_equivalent"]
    out = {}
    for ad, (cl, conf) in classes.items():
        src = None
        if cl.startswith("hw1:"):
            src, hfile = "class", cl[4:]
        elif ad in cet and cet[ad].get("hw1_equivalent") and funcs[ad]["name"] == cet[ad]["name"]:
            src, hfile = "cet", os.path.basename(cet[ad]["hw1_equivalent"].split(":")[0])
            eqs.setdefault(ad, cet[ad]["hw1_equivalent"])
        if not src:
            continue
        name = funcs[ad]["name"]
        hfunc = cov.strip_suffix(name)
        eq = eqs.get(ad, "")
        m = re.match(r"^(.*?\.(?:c|cpp|h))\s*:\s*([A-Za-z_~][\w~]*(?:::[A-Za-z_~][\w~]*)?)", eq, re.I)
        if m:
            hfunc = m.group(2)
            if src == "class" and not hfile:
                hfile = os.path.basename(m.group(1))
        out[ad] = {"class": cl, "hw1_file": hfile, "hw1_func": hfunc, "hw1_equivalent": eq, "source": src,
                   "map_confidence": conf, "name": name, "size": funcs[ad]["size"]}
    return out, funcs, classes


def load_cat_bodies(addrs):
    idx = {}
    with open(os.path.join(EXPORT, "decompiled_index.csv"), newline="") as fh:
        for r in csv.DictReader(fh):
            idx[int(r["address"], 16)] = (int(r["line_start"]), int(r["line_end"]), r["status"])
    want = sorted((idx[a][0], idx[a][1], a) for a in addrs if a in idx)
    out = {}
    with open(os.path.join(EXPORT, "decompiled.c"), encoding="latin-1") as fh:
        lines = fh.read().split("\n")
    for s, e, a in want:
        out[a] = "\n".join(lines[s - 1:e])
    return out, idx


def cat_features(text, callees, xstrings):
    body = strip_comments(text)
    # drop everything before the first top-level '{' (signature)
    toks = tokenize(body)
    try:
        b0 = next(i for i, t in enumerate(toks) if t[1] == "{")
    except StopIteration:
        b0 = 0
    sig = toks[:b0]
    toks = toks[b0:]
    strs, ints, flts = [], set(), set()
    fields, ids = set(), set()
    named_calls = set()
    ctl = collections.Counter()
    unresolved = 0
    pend = []
    for ti, (k, v, _) in enumerate(toks):
        if k == "str":
            pend.append(unescape(v))
            continue
        if pend:
            strs.append("".join(pend))
            pend = []
        if k == "id":
            if v in ("if", "for", "while", "do", "switch", "case", "goto", "return", "else", "default"):
                ctl[v] += 1
            elif re.match(r"^_?(DAT|FLOAT|DOUBLE|flt|dbl)_[0-9a-fA-F]+", v) or v.startswith("_DAT_"):
                unresolved += 1
            elif ti + 1 < len(toks) and toks[ti + 1][1] == "(" and not (ti and toks[ti - 1][1] in (".", "->")):
                ids.add(v)
                named_calls.add(v)
            elif ti and toks[ti - 1][1] in (".", "->"):
                if not re.match(r"^(field\d*_0x|_+\d|unk|pad|undefined)", v):
                    fields.add(v)
            else:
                ids.add(v)
        elif k == "num":
            r = parse_num(v)
            if r:
                (ints if r[0] == "i" else flts).add(r[1])
        elif k == "chr":
            ints.add(char_val(v))
    if pend:
        strs.append("".join(pend))
    return {"calls": sorted(callees), "strings": sorted(set(strs) | set(xstrings)), "ints": sorted(ints),
            "floats": sorted(flts), "ctl": dict(ctl), "unresolved_data": unresolved, "ntok": len(toks),
            "fields": sorted(fields), "ids": sorted(ids), "named_calls": sorted(named_calls),
            "raw_offsets": len(re.findall(r"\+ 0x[0-9a-f]+\)", body))}


def jacc(a, b):
    a, b = set(a), set(b)
    if not a and not b:
        return None
    return len(a & b) / len(a | b)


def group_of(unit):
    if not unit:
        return "other"
    d, base = unit.split("/")[0], os.path.basename(unit)
    if d == "Ships":
        return "Ships"
    if d != "Game":
        return "other"
    c = base[0].lower()
    if c <= "f":
        return "Game a-f"
    if c <= "m":
        return "Game g-m"
    if c <= "s":
        return "Game n-s"
    return "Game t-z"


def shorten(s, n=40):
    s = s.replace("\n", "\\n")
    return s if len(s) <= n else s[:n - 3] + "..."


HSDL_RENAMES = {"HWEnqueue": "Enqueue", "HWDequeue": "Dequeue"}
LIB_CLASSES = ("crt", "lib", "won", "bink", "rgl", "unknown")


def mask_like(v):
    if v & (v - 1) == 0:
        return True
    if (v + 1) & v == 0:
        return True
    for sh in (0, 8, 16, 24):
        if v in ((0xff << sh) & 0xffffffff, (0xffff << sh) & 0xffffffff):
            return True
    return False


def good_string(s):
    return len(s) >= 4 and re.search(r"[A-Za-z]{2}", s) is not None


def load_inputs(a):
    hidx = json.load(open(os.path.join(a.work, "hw1_index.json")))
    targets, funcs, classes = hw1_targets()
    calls_of = collections.defaultdict(set)
    with open(os.path.join(EXPORT, "call_graph.csv"), newline="") as fh:
        for r in csv.DictReader(fh):
            try:
                calls_of[int(r["caller_address"], 16)].add(int(r["callee_address"], 16))
            except Exception:
                pass
    xstr = collections.defaultdict(set)
    with open(os.path.join(EXPORT, "strings.csv"), newline="") as fh:
        for r in csv.DictReader(fh):
            xstr["all"].add(norm_s(r["string"]))
            for x in (r.get("xref_functions") or "").split("|"):
                if "@" in x:
                    try:
                        xstr[int(x.rsplit("@", 1)[1], 16)].add(r["string"])
                    except Exception:
                        pass
    fcsv = {}
    with open(os.path.join(EXPORT, "functions.csv"), newline="") as fh:
        for r in csv.DictReader(fh):
            fcsv[int(r["address"], 16)] = r
    return hidx, targets, funcs, classes, calls_of, xstr, fcsv


def norm_s(s):
    # strings.csv holds Ghidra-escaped text; normalise to the raw string
    return s.replace("\\n", "\n").replace("\\t", "\t").replace('\\"', '"').replace("\\\\", "\\")


def write_compact(out, path):
    """One JSON object per function per line: small, still diffable."""
    with open(path, "w") as fh:
        fh.write("{\n")
        keys = [k for k in out if k not in ("functions", "unresolved")]
        for k in keys:
            fh.write(" %s: %s,\n" % (json.dumps(k), json.dumps(out[k], separators=(",", ":"))))
        for k in ("unresolved", "functions"):
            fh.write(" %s: [\n" % json.dumps(k))
            fh.write(",\n".join("  " + json.dumps(r, separators=(",", ":")) for r in out[k]))
            fh.write("\n ]%s\n" % ("," if k == "unresolved" else ""))
        fh.write("}\n")


def stage_classify(a):
    hidx, targets, funcs, classes, calls_of, xstr, fcsv = load_inputs(a)
    defs = hidx["defs"]
    gdefs = hidx.get("gok_defs", {})
    aliases = hidx.get("aliases", {})
    strglob = hidx.get("string_globals", {})
    defs_lower = collections.defaultdict(list)
    for nm, lst in defs.items():
        defs_lower[nm.lower()].extend(lst)

    def resolve(nm):
        for _ in range(4):
            if nm in defs:
                return nm
            if nm in HSDL_RENAMES:
                nm = HSDL_RENAMES[nm]
            elif nm in aliases:
                nm = aliases[nm]
            else:
                return nm
        return nm

    canon, libish = {}, {}
    for ad, f in funcs.items():
        nm = f["name"]
        cl = classes.get(ad, ("", ""))[0]
        lib = cl.startswith(LIB_CLASSES) or nm.startswith(("lib_", "nullsub", "Unwind@", "Catch@"))
        if ad in targets:
            c = resolve(targets[ad]["hw1_func"])
        elif re.match(r"^(lib_|cat_new_|hw1_[A-Za-z0-9]+_[0-9a-fA-F]{8}$|nullsub)", nm):
            c = nm
        else:
            c = re.sub(r"_\d+$", "", nm)
            if lib or c.startswith("_"):
                b2 = c.lstrip("_")
                c = CI_MAP.get("_" + b2, b2)
            c = resolve(c)
        if "::" in c and not lib:
            c = c.split("::")[-1]
        if c.startswith("TitanInterface_") and ad not in targets:
            c = c[len("TitanInterface_"):]   # binary-map spelling of TitanInterface:: methods
        if ".DLL::" in nm.upper() or nm.startswith(("Ordinal_", "WSOCK32")):
            lib = True
        canon[ad] = c
        libish[ad] = lib
    binnames = set(canon.values())
    # cat_<hw1name>: the map's cat_ prefix on a function whose HW1 name is otherwise unbound
    # (cat_glcActive, cat_ddActivate...) is that HW1 function
    for ad_, c_ in list(canon.items()):
        if c_.startswith("cat_") and c_[4:] not in binnames and (c_[4:] in defs or c_[4:] in gdefs):
            canon[ad_] = c_[4:]
    binnames = set(canon.values())
    name2ad = {}
    for ad_, f_ in funcs.items():
        name2ad.setdefault(f_["name"], ad_)
    newcls = {canon[ad] for ad, (cl, _) in classes.items() if cl.startswith("cat-new")}
    data = set()
    with open(os.path.join(EXPORT, "symbols.csv"), newline="") as fh:
        for r in csv.DictReader(fh):
            if r["type"] == "Label" and r["namespace"] == "Global" and not re.match(
                    r"^(DAT|s|u|LAB|PTR|switchD|caseD|FLOAT|DOUBLE|BYTE|WORD|DWORD|QWORD|UNK|Elf|IMAGE|dw|lp|h)_", r["name"]):
                data.add(r["name"].lstrip("_"))
    hw1_field_universe = set()
    # every identifier anywhere in Relic's tree (headers included): a field named there is not new
    for path in glob.glob(os.path.join(hidx["orig"], "**", "*"), recursive=True):
        if os.path.isfile(path) and re.search(r"\.(c|cpp|h)$", path, re.I):
            hw1_field_universe.update(re.findall(r"[A-Za-z_]\w*", open(path, encoding="latin-1").read()))
    for lst_ in list(defs.values()) + list(gdefs.values()):
        for dd in lst_:
            hw1_field_universe.update(dd["features"].get("fields", []))

    short_defs = {k.split("::")[-1] for k in list(defs) + list(gdefs) if "::" in k}

    def hdef(nm):
        return (defs.get(nm) or gdefs.get(nm) or [None])[0]

    done = []
    if os.path.isfile(a.out) and not a.fresh:
        try:
            done = json.load(open(a.out)).get("done_subsystems", [])
        except Exception:
            done = []
    review = json.load(open(a.review)) if os.path.isfile(a.review) else {}
    bodies, _ = load_cat_bodies(targets.keys())

    rows, unresolved = [], []
    for ad, t in sorted(targets.items()):
        fn = resolve(t["hw1_func"])
        src = "ORIG"
        cand = defs.get(fn) or defs_lower.get(fn.lower()) or []
        if not cand:
            nm2 = resolve(re.sub(r"_\d+$", "", t["name"]))
            cand = defs.get(nm2) or []
        if not cand:
            cand = gdefs.get(t["hw1_func"]) or gdefs.get(fn) or []
            src = "GoK-HEAD"
        hf = (t["hw1_file"] or "").lower()
        pick = [c for c in cand if os.path.basename(c["file"]).lower() == hf or os.path.basename(c["unit"]).lower() == hf]
        pick = pick or cand
        if not pick:
            unresolved.append({"address": "0x%08x" % ad, "name": t["name"], "hw1_file": t["hw1_file"],
                               "hw1_func": t["hw1_func"], "bytes": t["size"], "source": t["source"],
                               "reason": "no definition of %s in ORIG@ad1bb82 src/{%s} (cpp -DHW_Release) nor in "
                                         "the GoK working tree" % (t["hw1_func"], ",".join(SRC_DIRS))})
            continue
        rows.append((ad, t, pick[0], src))

    ratios = collections.defaultdict(list)
    for ad, t, d, _ in rows:
        if t["size"] > 0 and d["features"]["ntok"] > 0:
            ratios[d["unit"]].append(math.log(t["size"] / d["features"]["ntok"]))
    allr = sorted(x for v in ratios.values() for x in v)
    gmed = allr[len(allr) // 2] if allr else 0.0
    umed = {u: sorted(v)[len(v) // 2] if len(v) >= 5 else gmed for u, v in ratios.items()}

    results = {}
    for ad, t, d, hsrc in rows:
        hf = d["features"]
        unit = d["unit"]
        cg, cl = set(), set()
        for cad in calls_of.get(ad, ()):
            nm = canon.get(cad)
            if not nm or nm in SOFT_CALLS or nm.startswith(("Unwind@", "nullsub")):
                continue
            (cl if libish.get(cad) else cg).add(nm)
        cf = cat_features(bodies.get(ad, ""), cg | cl, {norm_s(s) for s in xstr.get(ad, ())})
        thunk_t = [fcsv[ad]["thunk_target"]] if fcsv.get(ad, {}).get("is_thunk") == "1" and fcsv[ad].get("thunk_target") else []
        for nm in list(cf["named_calls"]) + thunk_t:
            cad = name2ad.get(nm)
            if cad is None or cad == ad:
                continue
            c = canon.get(cad)
            if not c or c in SOFT_CALLS or c.startswith(("Unwind@", "nullsub")):
                continue
            (cl if libish.get(cad) else cg).add(c)
        h_all = {c for c in hf["calls"] if c not in SOFT_CALLS}
        Hg = {c for c in h_all if c in defs or c in gdefs or c in short_defs}
        Hl = {c for c in h_all - Hg if c in binnames}
        missing, added = Hg - cg, cg - Hg
        inlined, unbound = set(), set()
        # a dropped HW1 callee whose own callees appear instead was inlined (2 levels)
        for m in sorted(missing):
            dd = hdef(m)
            if not dd:
                continue
            sub = {c for c in dd["features"]["calls"] if c not in SOFT_CALLS}
            for s2 in list(sub):
                d2 = hdef(s2)
                if d2 and s2 not in binnames:
                    sub |= set(d2["features"]["calls"])
            same_unit = dd["unit"] == unit or not dd["file"].endswith((".c", ".cpp", ".C"))
            if sub & added:
                inlined.add(m)
                added -= sub
            elif same_unit and (dd["features"]["ntok"] <= 120 or m not in binnames):
                inlined.add(m)
            elif m not in binnames:
                unbound.add(m)
        missing -= inlined | unbound
        # an HW1 callee from another unit that Cataclysm does not contain at all was removed with its
        # callers' calls (e.g. aieHandlerSetFuelLow: no fuel in Cataclysm); debug output is noise
        removed = {m for m in unbound if not m.startswith(("dbg", "titanDebug"))}
        callee_sim = jacc(Hg - inlined - unbound, cg - (cg - Hg - added))
        # features of inlined callees (2 levels) are expected in the Cataclysm body too
        ex = {"strings": set(), "fields": set(), "ids": set(), "ints": set(), "floats": set(), "calls": set(),
              "ntok": 0, "loops": 0}
        todo, seen = list(inlined), set()
        while todo:
            m = todo.pop()
            if m in seen or len(seen) > 12:
                continue
            seen.add(m)
            dd = hdef(m)
            if not dd:
                continue
            f2 = dd["features"]
            for k in ("strings", "fields", "ids", "ints", "floats", "calls"):
                ex[k].update(f2.get(k, []))
            ex["ntok"] += f2["ntok"]
            ex["loops"] += f2["ctl"].get("for", 0) + f2["ctl"].get("while", 0)
            for c in f2["calls"]:
                d3 = hdef(c)
                if d3 and c not in cg and (d3["unit"] == unit or c not in binnames) and d3["features"]["ntok"] <= 150:
                    todo.append(c)
        added -= {c for c in ex["calls"]}
        added -= set(hf.get("mcalls", []))   # titanInterface->SendPacketTo(...) etc.
        Hl |= {c for c in ex["calls"] if c in binnames and c not in defs and c not in gdefs}
        lib_drop = sorted(c for c in Hl - cl if not c.startswith("lib_"))
        lib_add = sorted(c for c in cl - Hl if not c.startswith(("lib_", "cat_new_")))
        hs = {s for s in set(hf["strings"]) | ex["strings"] if good_string(s)}
        hs |= {strglob[x] for x in set(hf.get("ids", [])) | ex["ids"] if x in strglob and good_string(strglob[x])}
        cs = {s for s in cf["strings"] if good_string(s)}
        # Cataclysm's AI move set-up macro (AITeam.h, InitNewMove-style) also stores a "\n<processFunction>"
        # name string: one header change, not a per-function one
        macro_strs = {x for x in cs if re.match(r"^\naim[A-Z]\w*$", x)}
        cs -= macro_strs
        # a dropped string Ghidra never typed anywhere may hide behind an unresolved DAT_ reference
        str_excused = {x for x in hs - cs if x not in xstr["all"] and cf["unresolved_data"] > 0}
        hs_eff = hs - str_excused
        str_sim = jacc(hs_eff, cs)
        fh_, fc_ = set(hf.get("fields", [])) | ex["fields"], set(cf["fields"])
        fld_sim = jacc(fh_, fc_)
        fld_new = sorted(x for x in fc_ - fh_ if x not in hw1_field_universe
                         and not re.search(r"Pad|^(HighPart|LowPart|QuadPart|u)$|^tm_|^(c|dw|n|b|i|w|lp|h|sz|by)[A-Z]", x))
        fld_add = sorted(fc_ - fh_ - set(fld_new))
        fld_miss = sorted(fh_ - fc_)
        gh = {x.lstrip("_") for x in set(hf.get("ids", [])) | ex["ids"] if x.lstrip("_") in data}
        gc = {x.lstrip("_") for x in cf["ids"] if x.lstrip("_") in data
              and not re.search(r"(_[0-9a-fA-F]{8}$|^Ordinal_|^ExceptionList$)", x)}
        glob_sim = jacc(gh, gc)
        glob_add = sorted(gc - gh)
        glob_miss = sorted(gh - gc)
        hi = {v for v in set(hf["ints"]) | ex["ints"] if distinctive_int(v) and not mask_like(v)}
        ci = set(cf["ints"])
        ints_missing = sorted(hi - ci)
        int_rec = (len(hi) - len(ints_missing)) / len(hi) if hi else None
        hfl = {v for v in set(hf["floats"]) | ex["floats"] if distinctive_float(v)}
        fl_missing = sorted(hfl - set(cf["floats"]))
        fl_unexcused = max(0, len(fl_missing) - cf["unresolved_data"])
        fl_rec = (len(hfl) - fl_unexcused) / len(hfl) if hfl else None
        hl_ = hf["ctl"].get("for", 0) + hf["ctl"].get("while", 0) + ex["loops"]
        cl_ = cf["ctl"].get("for", 0) + cf["ctl"].get("while", 0)
        loop_diff = cl_ - hl_
        hcase, ccase = hf["ctl"].get("case", 0), cf["ctl"].get("case", 0)
        hsw, csw = hf["ctl"].get("switch", 0), cf["ctl"].get("switch", 0)
        hif, cif = hf["ctl"].get("if", 0), cf["ctl"].get("if", 0)
        np_h = hf["nparams"]
        try:
            np_c = int(fcsv[ad]["param_count"])
        except Exception:
            np_c = None
        size_dev = None
        if t["size"] > 0 and hf["ntok"] > 0:
            size_dev = math.log(t["size"] / (hf["ntok"] + ex["ntok"])) - umed.get(unit, gmed)
        comp = []
        if callee_sim is not None:
            comp.append((3.0, callee_sim))
        if str_sim is not None:
            comp.append((2.0, str_sim))
        if int_rec is not None:
            comp.append((1.0, int_rec))
        if fl_rec is not None:
            comp.append((0.5, fl_rec))
        if max(hl_, cl_) > 0:
            comp.append((0.5, 1 - min(1.0, abs(loop_diff) / max(hl_, cl_, 1))))
        if hcase >= 3 and ccase > 0:
            comp.append((0.5, 1 - min(1.0, abs(ccase - hcase) / max(hcase, ccase))))
        if size_dev is not None:
            comp.append((1.5, math.exp(-abs(size_dev))))
        if np_c is not None:
            comp.append((0.5, 1.0 if np_c == np_h else 0.0))
        if fld_sim is not None:
            # Ghidra prints untyped accesses as raw offsets: excuse that many missing fields
            exc = min(len(fld_miss), cf["raw_offsets"] + cf["unresolved_data"])
            comp.append((1.5, (len(fh_ & fc_) + exc) / max(1, len(fh_ | fc_))))
        if glob_sim is not None:
            exc = min(len(glob_miss), cf["unresolved_data"])
            comp.append((1.0, (len(gh & gc) + exc) / max(1, len(gh | gc))))
        score = sum(w * s for w, s in comp) / sum(w for w, _ in comp) if comp else 0.0
        evidence = (len(Hg | cg) + len(hs | cs) + min(5, len(hi)) + min(3, len(hfl)) + min(6, len(fh_ | fc_)) // 2
                    + min(4, len(gh | gc)))

        def lst(xs, n=6):
            xs = sorted(xs)
            return ", ".join(xs[:n]) + (" (+%d)" % (len(xs) - n) if len(xs) > n else "")

        deltas, hard, soft = [], 0, 0
        newc = sorted(c for c in added if c in newcls)
        if added:
            deltas.append("adds calls " + lst(added))
            hard += 1
        if missing:
            deltas.append("drops calls " + lst(missing))
            hard += 1
        if removed:
            deltas.append("drops calls to %s (absent from Cataclysm)" % lst(removed, 4))
            hard += 1
        sadd, sdrop = sorted(cs - hs), sorted(hs_eff - cs)
        if sadd:
            deltas.append("adds strings " + "; ".join('"%s"' % shorten(s) for s in sadd[:3]) + (" (+%d)" % (len(sadd) - 3) if len(sadd) > 3 else ""))
            hard += 1
        if sdrop:
            deltas.append("drops strings " + "; ".join('"%s"' % shorten(s) for s in sdrop[:3]) + (" (+%d)" % (len(sdrop) - 3) if len(sdrop) > 3 else ""))
            hard += 1
        if np_c is not None and "__return_storage_ptr__" in fcsv.get(ad, {}).get("signature", ""):
            np_c -= 1   # hidden pointer for a struct return value (vector f(...)), an ABI artefact
        if np_c is not None and np_c != np_h:
            # Ghidra's parameter count comes from the binary map's prototypes. Generic ones (param_N,
            # undefined, this/pThis/edx_unused from __thiscall-as-__fastcall, cat_argN, pN) are a hint
            # only; an evidence-checked prototype with real names (timTimerStart(scopeHash, nameHash))
            # is a real interface change
            sig = fcsv.get(ad, {}).get("signature", "")
            generic = re.search(r"\bparam_\d|\bundefined\d?\b|\bthis\b|pThis|edx_unused|cat_arg\d|\bp\d\b|\.\.\.", sig)
            body_txt = bodies.get(ad, "")
            argarray = np_h >= 2 and np_c == 1 and re.search(r"\b(param_1|args)\[\d+\]|\*args\b", body_txt)
            if argarray:
                # Cataclysm's ETG/particle callables read their arguments from one array pointer
                deltas.append("takes one argument-array pointer instead of %d parameters" % np_h)
                hard += 1
            elif generic or not sig:
                deltas.append("params %d -> %d (prototype)" % (np_h, np_c))
                soft += 1
            else:
                deltas.append("parameters %d -> %d: %s" % (np_h, np_c, sig[:90]))
                hard += 1
        if size_dev is not None and size_dev > 1.0 and t["size"] >= 80 and (
                cf["ctl"].get("if", 0) > hf["ctl"].get("if", 0) + 1):
            # much larger with more branches: added logic (a smaller body is usually compiled-out debug code
            # or inlined string helpers, so it stays soft)
            deltas.append("body %.1fx larger than the file's size ratio predicts, with more branches" % math.exp(size_dev))
            hard += 1
        elif size_dev is not None and abs(size_dev) > 0.6:
            deltas.append("body %.1fx %s than predicted" % (math.exp(abs(size_dev)), "larger" if size_dev > 0 else "smaller"))
            soft += 1
        if fld_new:
            deltas.append("uses fields HW1 never uses: " + lst(fld_new, 5))
            hard += 1
        if len(fld_add) >= 2 or len(fld_miss) - cf["raw_offsets"] - cf["unresolved_data"] >= 2:
            deltas.append("fields" + (" +" + lst(fld_add, 4) if fld_add else "") + (" -" + lst(fld_miss, 4) if fld_miss else ""))
            soft += 1
        if glob_add or len(glob_miss) > cf["unresolved_data"]:
            deltas.append("globals" + (" +" + lst(glob_add, 4) if glob_add else "") + (" -" + lst(glob_miss, 4) if glob_miss else ""))
            soft += 1
        if lib_add or lib_drop:
            deltas.append("library calls" + (" +" + lst(lib_add, 4) if lib_add else "") + (" -" + lst(lib_drop, 4) if lib_drop else ""))
            soft += 1
        imiss_allow = 0 if len(hi) < 5 else max(1, len(hi) // 10)
        if len(ints_missing) > imiss_allow:
            deltas.append("constants %s not in body" % ", ".join(hex(v) if v > 255 else str(v) for v in ints_missing[:5]))
            soft += 1
        if fl_unexcused > (0 if len(hfl) < 5 else 1):
            deltas.append("float constants %s not in body" % ", ".join(fl_missing[:4]))
            soft += 1
        if abs(loop_diff) >= 2:
            deltas.append("loops %d -> %d" % (hl_, cl_))
            soft += 1
        if hcase >= 3 and ccase > 0 and ccase != hcase:
            deltas.append("switch cases %d -> %d" % (hcase, ccase))
            soft += 1
        # how much of HW1's own content (game callees + strings) survives in the Cataclysm body
        h_items = (Hg - inlined - unbound) | hs_eff
        h_recall = len(h_items & (cg | cs)) / len(h_items) if h_items else None
        cj = callee_sim if callee_sim is not None else 1.0
        sj = str_sim if str_sim is not None else 1.0
        if hard == 0 and soft == 0:
            cls = "IDENTICAL"
        elif hard and h_recall is not None and h_recall < 0.5 and (
                score < 0.4 or (len(Hg | cg) >= 4 and cj < 0.25 and sj < 0.34)):
            cls = "REWRITTEN"
        elif hard == 0:
            # soft deviations only (fields, size, constants, loops, library calls, prototype): in the
            # calibration sample these were compiler/decompiler noise far more often than source changes
            cls = "IDENTICAL"
        else:
            cls = "MODIFIED"
        if cls == "IDENTICAL":
            if soft == 0:
                conf = "high" if (evidence >= 5 and (size_dev is None or abs(size_dev) <= 0.45)) else (
                    "medium" if evidence >= 2 else "low")
            else:
                conf = "medium" if score >= 0.72 and evidence >= 2 else "low"
        elif cls == "REWRITTEN":
            conf = "high" if (score < 0.25 and evidence >= 6) else ("low" if evidence < 3 else "medium")
        else:
            conf = "high" if (hard >= 1 and evidence >= 5 and score >= 0.45 and (added or missing or removed or sadd or sdrop)) else (
                "low" if hard == 0 or evidence < 3 else "medium")
        if hsrc != "ORIG" and conf == "high":
            conf = "medium"
        change = "" if cls == "IDENTICAL" else "; ".join(deltas)
        feats = {
            "score": round(score, 3), "evidence": evidence,
            "hw1_recall": None if h_recall is None else round(h_recall, 3),
            "callee_jaccard": None if callee_sim is None else round(callee_sim, 3),
            "string_jaccard": None if str_sim is None else round(str_sim, 3),
            "int_recall": None if int_rec is None else round(int_rec, 3),
            "float_recall": None if fl_rec is None else round(fl_rec, 3),
            "hw1_calls": len(Hg), "cat_calls": len(cg), "calls_added": sorted(added), "calls_dropped": sorted(missing),
            "calls_inlined": sorted(inlined), "calls_unbound": sorted(unbound), "calls_new_cataclysm": newc,
            "lib_added": lib_add[:8], "lib_dropped": lib_drop[:8],
            "field_jaccard": None if fld_sim is None else round(fld_sim, 3), "fields_new": fld_new[:8],
            "fields_added": fld_add[:8], "fields_missing": fld_miss[:8], "raw_offsets": cf["raw_offsets"],
            "global_jaccard": None if glob_sim is None else round(glob_sim, 3), "globals_added": glob_add[:8],
            "globals_missing": glob_miss[:8],
            "strings_added": [shorten(s, 60) for s in sadd[:5]], "strings_dropped": [shorten(s, 60) for s in sdrop[:5]],
            "ints_missing": ints_missing[:8], "floats_missing": fl_missing[:6],
            "params": [np_h, np_c], "loops": [hl_, cl_], "switch": [hsw, csw], "cases": [hcase, ccase], "ifs": [hif, cif],
            "hw1_tokens": hf["ntok"], "cat_bytes": t["size"],
            "size_dev": None if size_dev is None else round(size_dev, 3), "hard": hard, "soft": soft,
        }
        r = {"address": "0x%08x" % ad, "name": t["name"], "hw1_file": t["hw1_file"], "hw1_func": t["hw1_func"],
             "hw1_src": "%s:%s" % (d["file"], d["line"]), "hw1_source": hsrc,
             "group": group_of(unit) if hsrc == "ORIG" else "other",
             "map_source": t["source"], "class": cls, "change": change, "confidence": conf, "auto_class": cls,
             "auto_confidence": conf, "features": feats, "reviewed": False}
        rv = review.get(r["address"])
        if rv:
            r["class"] = rv.get("class", cls)
            r["change"] = rv.get("change", "" if r["class"] == "IDENTICAL" else change)
            r["confidence"] = rv.get("confidence", "high")
            r["reviewed"] = True
            if rv.get("note"):
                r["review_note"] = rv["note"]
        results[ad] = r

    groups = a.groups or GROUPS
    if "tool" not in done:
        done.insert(0, "tool")
    for g in GROUPS:
        if g in groups and g not in done:
            done.append(g)
    funcs_out = [r for _, r in sorted(results.items()) if r["group"] in done]
    cnt = collections.Counter(r["class"] for r in funcs_out)
    out = {"done_subsystems": done, "method": METHOD % " ".join(CPP_DEFS), "export": "ghidra/exports/cataclysm_gog_r5",
           "hw1_source": "Relic HW1 source at gardens-of-kadesh root commit ad1bb82ca3c62dcc8be9d35f2e836d0bb7cf7caa (ORIG); "
                         "GoK working tree only where ORIG has no definition (hw1_source=GoK-HEAD)",
           "size_ratio_global_median": round(gmed, 3),
           "counts": {"targets": len(targets), "classified_all_groups": len(results), "in_done_groups": len(funcs_out),
                      "unresolved": len(unresolved), **{k: cnt.get(k, 0) for k in ("IDENTICAL", "MODIFIED", "REWRITTEN")},
                      "reviewed": sum(1 for r in funcs_out if r["reviewed"])},
           "cpp_failures": len(hidx.get("cpp_failures", [])),
           "functions": funcs_out, "unresolved": unresolved}
    tmp = a.out + ".tmp"
    write_compact(out, tmp)
    os.replace(tmp, a.out)
    print(json.dumps(out["counts"]))
    conf = collections.Counter((r["class"], r["confidence"]) for r in funcs_out)
    print(sorted(conf.items()))


METHOD = (
    "tools/delta_map.py. HW1 side: Relic's source at GoK root commit ad1bb82 (gitlab archive of "
    "ad1bb82ca3c62dcc8be9d35f2e836d0bb7cf7caa, src/{Game,Win32,Ships,SinglePlayer}); each unit run through "
    "cpp %s with a case-insensitive include dir (system headers stubbed), so debug-only code is compiled out as in "
    "a retail build and macros are expanded; definitions found by a brace matcher on the token stream. Names ORIG "
    "does not define fall back to the GoK working tree (hw1_source=GoK-HEAD). Cataclysm side: "
    "ghidra/exports/cataclysm_gog_r5 decompiled.c body (comments stripped), call_graph.csv callees, strings.csv "
    "xrefs. Target set: binary-map class hw1:* (symbols_round1-5 notes, then the audited r2-r5 proposal sets) plus "
    "CET entries with an hw1_equivalent whose name is current (the docs/binary-map.md R5 rule, 4,426 functions). "
    "Features: game-callee sets by canonical HW1 name (HomeworldSDL names mapped through ORIG macro aliases, e.g. "
    "clRemoveShipFromSelection -> clRemoveTargetFromSelection); a dropped callee whose own callees appear instead, or "
    "a small one from the same unit, counts as inlined and its strings/fields/constants are expected in the caller; "
    "C++ member calls, thunk targets and tail calls named in the body count; the map's cat_<hw1name> spellings map "
    "back; library/CRT callees compared separately (soft); string literals of >= 4 chars incl. char[] globals "
    "(an untyped string behind a DAT_ reference is excused; AI move-name strings \\naim* are one header change); "
    "struct fields (-> and .) against every identifier in Relic's tree; named globals from symbols.csv; distinctive "
    "integer constants (not -2..16, not masks/powers of two); float constants; for/while loop counts; switch/case "
    "counts; parameter count; body size vs the HW1 file's median log(bytes/tokens). Hard deltas: game callee added "
    "or dropped (not inlined), HW1 callee absent from Cataclysm, string added or dropped, struct field HW1 never "
    "names, parameter count from an evidence-named prototype, argument-array calling convention, body > 2.7x larger "
    "with more branches. IDENTICAL = no hard delta (soft deviations only); REWRITTEN = a hard delta, under half of "
    "HW1's callees+strings survive, and weighted score < 0.4 or (>= 4 game callees, callee Jaccard < 0.25, string "
    "Jaccard < 0.34); else MODIFIED. 'change' is the automatic delta list unless reviewed=true (hand review of both "
    "bodies, recorded in ghidra/delta_map_review.json, which also holds the write-up's findings under _notes).")


# ---------------------------------------------------------------------------------- show stage
def stage_show(a):
    hidx = json.load(open(os.path.join(a.work, "hw1_index.json")))
    dm = {f["address"]: f for f in json.load(open(a.out))["functions"]}
    targets_addr = [int(x, 16) for x in a.addrs]
    byloc = collections.defaultdict(list)
    for dd in list(hidx["defs"].values()) + list(hidx.get("gok_defs", {}).values()):
        for d in dd:
            byloc[(d["file"], d["line"])].append(d)
    bodies, _ = load_cat_bodies(targets_addr)
    for ad in targets_addr:
        key = "0x%08x" % ad
        r = dm.get(key)
        print("=" * 100)
        if not r:
            print(key, "not in delta_map")
            continue
        print("%s %s  <- %s  [%s/%s] score=%s" % (key, r["name"], r["hw1_src"], r["class"], r["confidence"], r["features"]["score"]))
        print("change:", r["change"])
        f, ln = r["hw1_src"].rsplit(":", 1)
        ln = int(ln)
        base = hidx.get("gok_root", GOK) if r.get("hw1_source") == "GoK-HEAD" else a.orig
        fpath = f if os.path.isabs(f) else os.path.join(base, f)
        cand = [d for lst_ in byloc.get((f, ln), []) for d in [lst_]]
        end = cand[0]["end_line"] if cand and cand[0]["end_line"] else ln + 60
        try:
            src = open(fpath, encoding="latin-1").read().split("\n")[ln - 1:end]
        except Exception:
            src = ["(cannot read %s)" % f]
        lim = a.max_lines
        print("--- HW1 %s:%d-%s" % (os.path.relpath(f, ORIG) if f.startswith(ORIG) else f, ln, end))
        src = [x.rstrip() for x in strip_comments("\n".join(src)).split("\n") if x.strip()]
        print("\n".join(src[:lim]) + ("\n... (%d more lines)" % (len(src) - lim) if len(src) > lim else ""))
        cb = strip_comments(bodies.get(ad, "")).split("\n")
        try:
            i0 = next(i for i, x in enumerate(cb) if x.strip() == "{")
            i1 = next(i for i in range(i0 + 1, len(cb)) if not cb[i].strip())
            if all(re.match(r"^\s+[\w\s\*\[\]]+;$", x) for x in cb[i0 + 1:i1]):
                cb = cb[:i0 + 1] + cb[i1:]
        except StopIteration:
            pass
        cb = [x.rstrip() for x in cb if x.strip()]
        ft = r["features"]
        print("--- features: calls +%s -%s inl=%s | str +%s -%s | fields new=%s +%s -%s | glob +%s -%s | ints-miss=%s | lib +%s -%s | size_dev=%s loops=%s cases=%s params=%s" % (
            ft["calls_added"], ft["calls_dropped"], ft["calls_inlined"], ft["strings_added"], ft["strings_dropped"],
            ft.get("fields_new"), ft.get("fields_added"), ft.get("fields_missing"), ft.get("globals_added"), ft.get("globals_missing"),
            ft["ints_missing"], ft["lib_added"], ft["lib_dropped"], ft["size_dev"], ft["loops"], ft["cases"], ft["params"]))
        print("--- Cataclysm decompiled")
        print("\n".join(cb[:lim]) + ("\n... (%d more lines)" % (len(cb) - lim) if len(cb) > lim else ""))


# -------------------------------------------------------------------------------- report stage
def fmt(n):
    return "{:,}".format(int(round(n)))


def pct(a, b):
    return "%.1f%%" % (100.0 * a / b) if b else "-"




def stage_report(a):
    dm = json.load(open(a.out))
    fs = dm["functions"]
    rv = json.load(open(a.review)) if os.path.isfile(a.review) else {}
    K = ("IDENTICAL", "MODIFIED", "REWRITTEN")
    tot_n = len(fs)
    tot_b = sum(r["features"]["cat_bytes"] for r in fs)
    cn = collections.Counter(r["class"] for r in fs)
    cb = collections.Counter()
    for r in fs:
        cb[r["class"]] += r["features"]["cat_bytes"]
    unres = dm.get("unresolved", [])
    # calibration: current automatic class vs hand review
    conf_m = collections.defaultdict(collections.Counter)
    conf_b = collections.defaultdict(collections.Counter)   # per (auto class, auto confidence) bucket
    for r in fs:
        if r["reviewed"]:
            conf_m[r["auto_class"]][r["class"]] += 1
            conf_b[(r["auto_class"], r["auto_confidence"])][r["class"]] += 1
    # estimate: unreviewed functions redistributed by P(human | auto) from the review set
    est_n, est_b = collections.Counter(), collections.Counter()
    for r in fs:
        b = r["features"]["cat_bytes"]
        if r["reviewed"]:
            est_n[r["class"]] += 1
            est_b[r["class"]] += b
            continue
        row = conf_b.get((r["auto_class"], r["auto_confidence"]))
        if not row or sum(row.values()) < 5:
            row = conf_m.get(r["auto_class"])
        tot = sum(row.values()) if row else 0
        if not tot:
            est_n[r["class"]] += 1
            est_b[r["class"]] += b
            continue
        for k in K:
            est_n[k] += row[k] / tot
            est_b[k] += b * row[k] / tot
    per = collections.OrderedDict()
    for r in sorted(fs, key=lambda r: (r["group"], (r["hw1_file"] or "").lower())):
        f = r["hw1_file"] or "?"
        p = per.setdefault(f, {"group": r["group"], "path": r["hw1_src"].rsplit(":", 1)[0], "n": 0, "b": 0,
                                **{k: 0 for k in K}, **{k + "b": 0 for k in K}, "rv": 0, "U": 0})
        p["n"] += 1
        p["b"] += r["features"]["cat_bytes"]
        p[r["class"]] += 1
        p[r["class"] + "b"] += r["features"]["cat_bytes"]
        p["rv"] += r["reviewed"]
    for u in unres:
        f = u["hw1_file"] or "?"
        p = per.setdefault(f, {"group": "other", "path": "-", "n": 0, "b": 0, **{k: 0 for k in K},
                                **{k + "b": 0 for k in K}, "rv": 0, "U": 0})
        p["U"] += 1
    md = []
    md.append("<!-- done: %s -->" % ", ".join(dm["done_subsystems"]))
    md.append("# M2-R1 function deltas: Cataclysm vs Homeworld 1\n")
    md.append("Round M2-R1, item 3 of `cycle/M2_BRIEF.md`. For every function the binary map matched to Homeworld 1 "
              "(class `hw1:<File>`, plus CET's `hw1_equivalent` entries, the rule behind the 4,426 HW1 matches of "
              "docs/binary-map.md round 5), this compares the decompiled Cataclysm body (`ghidra/exports/cataclysm_gog_r5`) "
              "with Relic's original source (gardens-of-kadesh root commit `ad1bb82`) and classifies it IDENTICAL, MODIFIED or "
              "REWRITTEN. Per-function data: `ghidra/delta_map.json`. Tool: `tools/delta_map.py`. Hand-review verdicts: "
              "`ghidra/delta_map_review.json`.\n")
    md.append("## Totals: how much of the engine is free\n")
    md.append("| | functions | share | Cataclysm bytes | share of bytes |")
    md.append("|---|---:|---:|---:|---:|")
    for k in K:
        md.append("| %s | %s | %s | %s | %s |" % (k, fmt(cn[k]), pct(cn[k], tot_n), fmt(cb[k]), pct(cb[k], tot_b)))
    md.append("| **classified** | **%s** | | **%s** | |" % (fmt(tot_n), fmt(tot_b)))
    md.append("| not classified (no HW1 body found) | %s | | %s | |" % (fmt(len(unres)), fmt(sum(u["bytes"] for u in unres))))
    md.append("")
    md.append("Estimated true split, correcting each automatic (class, confidence) bucket by its hand-review agreement "
              "(class-level agreement where a bucket has fewer than 5 reviewed functions); reviewed functions count with their reviewed class: IDENTICAL %s functions / %s bytes (%s of bytes), "
              "MODIFIED %s / %s (%s), REWRITTEN %s / %s (%s).\n" % (
                  fmt(est_n["IDENTICAL"]), fmt(est_b["IDENTICAL"]), pct(est_b["IDENTICAL"], tot_b),
                  fmt(est_n["MODIFIED"]), fmt(est_b["MODIFIED"]), pct(est_b["MODIFIED"], tot_b),
                  fmt(est_n["REWRITTEN"]), fmt(est_b["REWRITTEN"]), pct(est_b["REWRITTEN"], tot_b)))
    if rv.get("_notes"):
        # hand-written findings, kept in the review file so the doc is fully regenerable
        md.append("\n".join(rv["_notes"]).rstrip() + "\n")
    md.append("## Hand review and calibration\n")
    nrev = sum(1 for r in fs if r["reviewed"])
    md.append("%d functions were reviewed by hand (both bodies read; for long bodies sometimes only the head, as the review note says). Rows: the tool's current automatic class; "
              "columns: the reviewed class.\n" % nrev)
    md.append("| automatic \\ reviewed | IDENTICAL | MODIFIED | REWRITTEN | agreement |")
    md.append("|---|---:|---:|---:|---:|")
    for k in K:
        row = conf_m.get(k, collections.Counter())
        t = sum(row.values())
        md.append("| %s | %d | %d | %d | %s |" % (k, row["IDENTICAL"], row["MODIFIED"], row["REWRITTEN"], pct(row[k], t)))
    md.append("")
    ab = collections.Counter((r["auto_class"], r["auto_confidence"]) for r in fs)
    md.append("Per automatic bucket (functions in the bucket, reviewed, reviewed class agreeing):\n")
    md.append("| automatic class | confidence | functions | bytes | reviewed | agree |")
    md.append("|---|---|---:|---:|---:|---:|")
    for k in K:
        for c in ("high", "medium", "low"):
            row = conf_b.get((k, c), collections.Counter())
            nb = sum(r["features"]["cat_bytes"] for r in fs if (r["auto_class"], r["auto_confidence"]) == (k, c))
            md.append("| %s | %s | %s | %s | %d | %s |" % (k, c, fmt(ab[(k, c)]), fmt(nb), sum(row.values()),
                                                       "%d (%s)" % (row[k], pct(row[k], sum(row.values()))) if row else "-"))
    md.append("")
    cc = collections.Counter((r["class"], r["confidence"]) for r in fs)
    md.append("Confidence per class (after review): " + "; ".join(
        "%s %s high / %s medium / %s low" % (k, fmt(cc[(k, "high")]), fmt(cc[(k, "medium")]), fmt(cc[(k, "low")]))
        for k in K) + ".\n")
    md.append("## Per HW1 source file\n")
    md.append("Grouped as in the round plan (Game a-f, g-m, n-s, t-z, Ships, other = Win32/ and GoK-only). "
              "Bytes are Cataclysm function sizes. `rev` = hand-reviewed functions in the file; `unres` = matched "
              "functions with no HW1 body.\n")
    for g in GROUPS:
        rows = [(f, p) for f, p in per.items() if p["group"] == g]
        if not rows:
            continue
        gn = sum(p["n"] for _, p in rows)
        gb = sum(p["b"] for _, p in rows)
        gi = sum(p["IDENTICALb"] for _, p in rows)
        md.append("### %s (%s functions, %s bytes, %s of bytes IDENTICAL)\n" % (g, fmt(gn), fmt(gb), pct(gi, gb)))
        md.append("| file | matched | identical | modified | rewritten | bytes identical | bytes modified | bytes rewritten | rev | unres |")
        md.append("|---|---:|---:|---:|---:|---:|---:|---:|---:|---:|")
        for f, p in sorted(rows, key=lambda x: x[0].lower()):
            md.append("| %s | %d | %d | %d | %d | %s | %s | %s | %d | %d |" % (
                f, p["n"], p["IDENTICAL"], p["MODIFIED"], p["REWRITTEN"], fmt(p["IDENTICALb"]), fmt(p["MODIFIEDb"]),
                fmt(p["REWRITTENb"]), p["rv"], p["U"]))
        md.append("")
    md.append("## Largest MODIFIED functions\n")
    md.append("The 60 largest by Cataclysm bytes; `change` is the hand-written line where reviewed, else the tool's delta list.\n")
    md.append("| address | function | HW1 file | bytes | conf | reviewed | change |")
    md.append("|---|---|---|---:|---|---|---|")
    for r in sorted((r for r in fs if r["class"] == "MODIFIED"), key=lambda r: -r["features"]["cat_bytes"])[:60]:
        md.append("| %s | %s | %s | %s | %s | %s | %s |" % (r["address"], r["name"], r["hw1_file"], fmt(r["features"]["cat_bytes"]),
                                                     r["confidence"], "yes" if r["reviewed"] else "", r["change"].replace("|", "/")[:300]))
    md.append("")
    md.append("## REWRITTEN functions\n")
    md.append("| address | function | HW1 file | bytes | conf | reviewed | change |")
    md.append("|---|---|---|---:|---|---|---|")
    for r in sorted((r for r in fs if r["class"] == "REWRITTEN"), key=lambda r: -r["features"]["cat_bytes"]):
        md.append("| %s | %s | %s | %s | %s | %s | %s |" % (r["address"], r["name"], r["hw1_file"], fmt(r["features"]["cat_bytes"]),
                                                     r["confidence"], "yes" if r["reviewed"] else "", r["change"].replace("|", "/")[:220]))
    md.append("")
    md.append("## Not classified\n")
    md.append("Matched functions for which neither Relic's tree nor the GoK working tree has a definition the brace matcher found:\n")
    for u in unres:
        md.append("- `%s` %s (`%s`, %s bytes): %s" % (u["address"], u["name"], u["hw1_func"], u["bytes"], u["reason"]))
    md.append("")
    md.append("## Method\n")
    md.append(dm["method"] + "\n")
    md.append("Reproduce (the ad1bb82 tree is the gitlab archive `https://gitlab.com/gardens-of-kadesh/gardens-of-kadesh/-/archive/"
              "ad1bb82c/gardens-of-kadesh-ad1bb82c.tar.gz?path=src`, extracted to `original/scratch-m2/delta-orig/`; its pax "
              "header names commit ad1bb82ca3c62dcc8be9d35f2e836d0bb7cf7caa; the reference clones are depth 1 and do not hold "
              "that commit):\n")
    md.append("```\npython3 -I tools/delta_map.py prep       # cpp + index -> original/scratch-m2/delta-work/hw1_index.json\n"
              "python3 -I tools/delta_map.py classify   # -> ghidra/delta_map.json (merges ghidra/delta_map_review.json)\n"
              "python3 -I tools/delta_map.py show 0x00401000   # both bodies side by side, for review\n"
              "python3 -I tools/delta_map.py report     # -> this file\n```\n")
    tmp = a.md + ".tmp"
    open(tmp, "w").write("\n".join(md) + "\n")
    os.replace(tmp, a.md)
    print("wrote", a.md, "functions", tot_n, "bytes", tot_b, dict(cn), "est", {k: round(v) for k, v in est_b.items()})


def stage_mark(a):
    """Record hand-review verdicts. stdin lines: ADDR CLASS CONF [SAMPLE] | change | note
    (SAMPLE = the calibration bucket the function was drawn from, e.g. IDENTICAL/medium)."""
    rv = json.load(open(a.review)) if os.path.isfile(a.review) else {}
    n = 0
    for ln in sys.stdin:
        ln = ln.strip()
        if not ln or ln.startswith("#"):
            continue
        parts = [p.strip() for p in ln.split("|")]
        head = parts[0].split()
        addr = "0x%08x" % int(head[0], 16)
        cls = {"I": "IDENTICAL", "M": "MODIFIED", "R": "REWRITTEN"}.get(head[1], head[1])
        assert cls in ("IDENTICAL", "MODIFIED", "REWRITTEN"), ln
        ent = {"class": cls, "confidence": head[2] if len(head) > 2 else "high",
               "change": parts[1] if len(parts) > 1 else "", "note": parts[2] if len(parts) > 2 else ""}
        if len(head) > 3:
            ent["sample"] = head[3]
        rv[addr] = ent
        n += 1
    tmp = a.review + ".tmp"
    json.dump(dict(sorted(rv.items())), open(tmp, "w"), indent=1)
    os.replace(tmp, a.review)
    print("marked", n, "total", len(rv))


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("stage", choices=["prep", "classify", "show", "report", "mark"])
    ap.add_argument("addrs", nargs="*")
    ap.add_argument("--orig", default=ORIG)
    ap.add_argument("--work", default=WORK)
    ap.add_argument("--out", default=OUT_JSON)
    ap.add_argument("--review", default=REVIEW)
    ap.add_argument("--md", default=OUT_MD)
    ap.add_argument("--groups", nargs="*", default=None)
    ap.add_argument("--fresh", action="store_true")
    ap.add_argument("--max-lines", type=int, default=80)
    a = ap.parse_args()
    {"prep": stage_prep, "classify": stage_classify, "show": stage_show, "report": stage_report,
     "mark": stage_mark}[a.stage](a)


if __name__ == "__main__":
    main()
