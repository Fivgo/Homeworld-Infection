#!/usr/bin/env python3
"""Build the curated 32-bit (MSVC6-style, /Zp4) C header set that ghidra/scripts/ImportHeaders.py
loads into the cataclysm.exe program's data type manager.

    tools/prep_ghidra_headers.py [--out DIR] [--ref-root REPO]
        DIR      default <repo>/ghidra/exports/headers_r2b (generated, never committed)
        REPO     checkout that holds reference/ (default: this checkout, else the main worktree)

Inputs (read-only):
  reference/CataclysmEngineTweaks/game/Game/*.h|*.H and game/Win32/*.h  -- CET's reconstructed
      Cataclysm layouts (preferred; ~300 offset/size static_asserts checked against the GOG exe)
  reference/HomeworldSDL/src/Game/{Memory,Switches,Chatting}.h  -- HW1 headers for the three
      includes CET references but does not ship
Outputs:
  DIR/stage/inc/*.h      staged + fixed headers (lower-case names)
  DIR/stage/sys/*.h      minimal stubs for system headers (windows.h, wchar.h, string.h, main.h)
  DIR/preprocessed.c     clang -E output of the whole set (line markers kept)
  DIR/decls.json         {"headers":[{"header","origin","decls":[...]}], "asserts":[...],
                          "fixups":[...]}: top-level C declarations in parse order, grouped by
                          the header they came from
  DIR/prep_report.json   clang verification result and per-header declaration counts

Why a preprocessing step: CET compiles these headers as C++ (MSVC); Ghidra's C parser takes C.
Every change is listed in decls.json "fixups". clang (target i686-pc-windows-msvc, C2x,
-fpack-struct=4) must compile the fixed set with every static_assert holding (exit 1 otherwise);
the asserts are stripped from the text Ghidra parses and passed separately so ImportHeaders.py can
re-check the layouts Ghidra produced.
"""
import argparse
import json
import os
import re
import shutil
import subprocess
import sys

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))

HW1_FILL_INS = {"memory.h": "Memory.h", "switches.h": "Switches.h", "chatting.h": "Chatting.h"}

STUBS = {
    "windows.h": """#pragma once
/* minimal Win32 stand-ins for the handful of types the CET headers use */
typedef unsigned long DWORD; typedef int BOOL; typedef unsigned char BYTE; typedef unsigned short WORD;
typedef void *HANDLE; typedef HANDLE HWND; typedef HANDLE HDC; typedef HANDLE HINSTANCE; typedef HANDLE HGLRC;
typedef long LONG; typedef unsigned int UINT; typedef long LPARAM; typedef unsigned int WPARAM; typedef long LRESULT;
typedef char *LPSTR; typedef const char *LPCSTR; typedef void *LPVOID; typedef unsigned short WCHAR;
typedef struct tagRECT { LONG left, top, right, bottom; } RECT;
typedef struct tagPOINT { LONG x, y; } POINT;
typedef struct tagPIXELFORMATDESCRIPTOR PIXELFORMATDESCRIPTOR, *LPPIXELFORMATDESCRIPTOR; /* opaque: only used by pointer */
typedef struct _RTL_CRITICAL_SECTION { void *DebugInfo; LONG LockCount; LONG RecursionCount; HANDLE OwningThread; HANDLE LockSemaphore; DWORD SpinCount; } CRITICAL_SECTION;
#define WINAPI __stdcall
#define CALLBACK __stdcall
#define APIENTRY __stdcall
""",
    "wchar.h": "#pragma once\ntypedef unsigned short wchar_t;\n",
    "string.h": "#pragma once\n",
    "main.h": "#pragma once\n/* the port's SDL main.h is not Relic's Win32 main.h; prim2d.h needs nothing from it */\n",
}

# prelude for every compile: retail build level, CET's ASSERT_OFFSET (from CET pch.h), Relic's
# 4-byte bool (original TYPES.H: typedef sdword bool) and HW1's memsize
PRELUDE_CHECK = """#pragma once
#include <windows.h>
#define HW_Release 1
#define offsetof(s,m) __builtin_offsetof(s,m)
#define ASSERT_OFFSET(_struct, field, offset) _Static_assert(offsetof(_struct, field) == offset, #_struct "." #field)
#define countof(array) (sizeof(array) / sizeof(array[0]))
#define bool sdword
typedef unsigned long memsize; typedef long smemsize;
"""
PRELUDE_EMIT = """#pragma once
#include <windows.h>
#define HW_Release 1
#define ASSERT_OFFSET(_struct, field, offset)
#define static_assert(...)
#define countof(array) (sizeof(array) / sizeof(array[0]))
#define bool sdword
typedef unsigned long memsize; typedef long smemsize;
"""

FIXUPS = []


def fix(header, what, why):
    FIXUPS.append({"header": header, "change": what, "reason": why})


def sub_once(text, pattern, repl, header, what, why, flags=0):
    new, n = re.subn(pattern, repl, text, flags=flags)
    if n:
        fix(header, what + (" (x%d)" % n if n > 1 else ""), why)
    return new


def ref_root(arg):
    if arg:
        return arg
    if os.path.isdir(os.path.join(ROOT, "reference")):
        return ROOT
    try:
        common = subprocess.run(["git", "rev-parse", "--path-format=absolute", "--git-common-dir"],
                                cwd=ROOT, capture_output=True, text=True, check=True).stdout.strip()
        return os.path.dirname(common)
    except Exception:
        return ROOT


def stage(out, rr):
    cet = os.path.join(rr, "reference/CataclysmEngineTweaks/game")
    hw1 = os.path.join(rr, "reference/HomeworldSDL/src/Game")
    st = os.path.join(out, "stage")
    if os.path.isdir(st):
        shutil.rmtree(st)
    inc, sysd = os.path.join(st, "inc"), os.path.join(st, "sys")
    os.makedirs(inc)
    os.makedirs(sysd)
    sources = {}
    for sub in ("Game", "Win32"):
        d = os.path.join(cet, sub)
        for f in sorted(os.listdir(d)):
            if f.lower().endswith(".h"):
                sources[f.lower()] = ("cet", os.path.join(d, f))
    for low, hw in HW1_FILL_INS.items():
        if low not in sources:
            sources[low] = ("hw1", os.path.join(hw1, hw))
    order = []
    for low, (origin, path) in sorted(sources.items()):
        with open(path, encoding="latin-1") as fh:
            t = fh.read()
        t = t.replace("\r\n", "\n")
        # 1. the Windows file system is case-insensitive: lower-case every quoted include
        t = re.sub(r'^(\s*#\s*include\s*")([^"]+)"', lambda m: m.group(1) + m.group(2).lower() + '"',
                   t, flags=re.M)
        # 2. include guard (gamestats.h has none and is included twice)
        if "#pragma once" not in t:
            t = "#pragma once\n" + t
        # 3. C++ constexpr constants -> macros
        t = sub_once(t, r"^\s*constexpr\s+\w+\s+(\w+)\s*=\s*([^;]+);",
                     r"#define \1 (\2)", low, "constexpr constant -> #define", "C++-only keyword",
                     flags=re.M)
        # 4. C++ lets a bare struct tag name a type; C needs a typedef
        for m in list(re.finditer(r"^struct\s+(\w+)\s*\n?\s*\{", t, flags=re.M)):
            name = m.group(1)
            if not re.search(r"typedef\s+struct\s+%s\s+%s\s*;" % (name, name), t):
                t = t.replace(m.group(0), "typedef struct %s %s;\n%s" % (name, name, m.group(0)), 1)
                fix(low, "typedef struct %s %s; added" % (name, name),
                    "C++ uses the bare tag as a type name")
        if low == "trails.h":
            # CET's bool is C++ bool (1 byte) and CET inserted 'sdword unknown;' so grainCounter
            # lands at 0x3C. With Relic's 4-byte bool (HW1 Trails.h: bool32 wobbly; Relic
            # TYPES.H: typedef sdword bool) the original field list reaches 0x3C without it.
            t = sub_once(t, r"\n\s*sdword unknown;\s*\n(\s*sdword grainCounter;)", r"\n\1", low,
                         "shiptrail: removed CET pad 'sdword unknown;'",
                         "it compensates CET's 1-byte C++ bool; with the 4-byte bool it double-counts")
        if low == "universe.h":
            # exe 12948: players[] stride 0x6530, AllianceProposals at +0x64F6, its timeout at
            # +0x64F8 (ghidra/cet_symbols.json universe_players notes). CET's Player is 0x2F8
            # bytes, asserted only up to classtotals (0x274)
            t = sub_once(t, r"(PlayerResearchInfo researchinfo;[^\n]*\n)",
                         r"\1    ubyte cat_unknown_02EC[0x6208];   /* A2: exe stride 0x6530, Allies at 0x64F4 */\n",
                         low, "Player: pad cat_unknown_02EC[0x6208] after researchinfo",
                         "exe 12948 players[] stride 0x6530, AllianceProposals @+0x64F6")
            t = sub_once(t, r"(sdword initialShipCost;[^\n]*\n)(\} Player;)",
                         r"\1    ubyte cat_unknown_6500[0x30];\n\2", low,
                         "Player: tail pad cat_unknown_6500[0x30] (sizeof 0x6530)",
                         "exe 12948 players[] stride 0x6530")
            t = t.replace("// static_assert(sizeof(Player) == 0x6530u);",
                          "static_assert(sizeof(Player) == 0x6530u);\n"
                          "ASSERT_OFFSET(Player, AllianceProposals, 0x64F6);\n"
                          "ASSERT_OFFSET(Player, AllianceProposalsTimeout, 0x64F8);")
        with open(os.path.join(inc, low), "w", encoding="latin-1") as fh:
            fh.write(t)
        order.append({"header": low, "origin": origin, "source": os.path.relpath(path, rr)})
    for name, body in STUBS.items():
        with open(os.path.join(sysd, name), "w") as fh:
            fh.write(body)
        fix("stub:" + name, "minimal stub written", "system/SDL header not available for the target")
    fix("*", "bool -> sdword (macro)", "Relic TYPES.H: typedef sdword bool; CET relies on C++'s 1-byte bool")
    fix("*", "static_assert/ASSERT_OFFSET stripped from the parsed text",
        "C++/C11-only; checked by clang here and re-checked in Ghidra from decls.json asserts")
    for name, body in (("prelude_check.h", PRELUDE_CHECK), ("prelude_emit.h", PRELUDE_EMIT)):
        with open(os.path.join(st, name), "w") as fh:
            fh.write(body)
    with open(os.path.join(st, "all.c"), "w") as fh:
        for o in order:
            fh.write('#include "%s"\n' % o["header"])
    return st, order


ASSERT_RE = [
    (re.compile(r"ASSERT_OFFSET\(\s*(\w+)\s*,\s*([\w.\[\]]+)\s*,\s*(0x[0-9A-Fa-f]+|\d+)u?\s*\)"), "offset"),
    (re.compile(r"static_assert\(\s*offsetof\(\s*(\w+)\s*,\s*([\w.\[\]]+)\s*\)\s*==\s*(0x[0-9A-Fa-f]+|\d+)u?"), "offset"),
    (re.compile(r"static_assert\(\s*sizeof\(\s*(\w+)\s*\)\s*==\s*(0x[0-9A-Fa-f]+|\d+)u?\s*[,)]"), "size"),
]


def collect_asserts(st):
    out = []
    inc = os.path.join(st, "inc")
    for f in sorted(os.listdir(inc)):
        for ln, line in enumerate(open(os.path.join(inc, f), encoding="latin-1"), 1):
            if line.lstrip().startswith("//"):
                continue
            for rx, kind in ASSERT_RE:
                for m in rx.finditer(line):
                    if kind == "offset":
                        out.append({"kind": "offset", "struct": m.group(1), "field": m.group(2),
                                    "value": int(m.group(3), 0), "where": "%s:%d" % (f, ln)})
                    else:
                        out.append({"kind": "size", "struct": m.group(1), "field": "",
                                    "value": int(m.group(2), 0), "where": "%s:%d" % (f, ln)})
    return out


CLANG_BASE = ["-x", "c", "-std=c2x", "-target", "i686-pc-windows-msvc", "-fms-extensions",
              "-fpack-struct=4", "-nostdinc", "-Wno-everything"]


def clang_check(st):
    cmd = ["clang", "-fsyntax-only"] + CLANG_BASE + ["-Iinc", "-Isys", "-include", "prelude_check.h",
                                                    "-ferror-limit=0", "all.c"]
    p = subprocess.run(cmd, cwd=st, capture_output=True, text=True)
    errs = [l for l in p.stderr.splitlines() if " error: " in l]
    return p.returncode, errs, " ".join(cmd)


def clang_emit(st, out):
    cmd = ["clang", "-E"] + CLANG_BASE + ["-Iinc", "-Isys", "-include", "prelude_emit.h", "all.c"]
    p = subprocess.run(cmd, cwd=st, capture_output=True, text=True)
    if p.returncode:
        sys.exit("clang -E failed:\n" + p.stderr[-3000:])
    with open(os.path.join(out, "preprocessed.c"), "w") as fh:
        fh.write(p.stdout)
    return p.stdout


def probe_one(st, e):
    path = os.path.join(st, "dimprobe1.c")
    with open(path, "w") as fh:
        fh.write('#include "all.c"\nstruct __p { char a[(%s) + 1]; };\nint __u = sizeof(struct __p);\n' % e)
    cmd = ["clang", "-fsyntax-only"] + CLANG_BASE + ["-Iinc", "-Isys", "-include", "prelude_emit.h",
                                                    "-Xclang", "-fdump-record-layouts-simple", path]
    p = subprocess.run(cmd, cwd=st, capture_output=True, text=True)
    m = re.search(r"Type: struct __p\s+Layout: <ASTRecordLayout\s+Size:(\d+)", p.stdout)
    return int(m.group(1)) // 8 - 1 if m and p.returncode == 0 else None


GLOBALS = {}

MARK_RE = re.compile(r'^#\s+(\d+)\s+"([^"]*)"')


def split_decls(pre):
    """Top-level declarations in order, grouped by the header each one starts in."""
    groups = []
    cur_file = None
    buf, depth, par, start_file = [], 0, 0, None

    def emit(text, owner):
        text = text.strip()
        if not text or text == ";":
            return
        if groups and groups[-1][0] == owner:
            groups[-1][1].append(text)
        else:
            groups.append((owner, [text]))

    for line in pre.splitlines():
        m = MARK_RE.match(line)
        if m:
            f = m.group(2)
            cur_file = None if f.startswith("<") else os.path.basename(f)
            if f.endswith("prelude_emit.h"):
                cur_file = "prelude"
            continue
        if line.startswith("#pragma"):
            if depth == 0 and par == 0 and not "".join(buf).strip():
                emit(line, cur_file or "prelude")
            continue
        if not line.strip():
            continue
        if not "".join(buf).strip():
            start_file = cur_file
        i, n = 0, len(line)
        while i < n:
            c = line[i]
            if c in "\"'":
                j = i + 1
                while j < n and line[j] != c:
                    j += 2 if line[j] == "\\" else 1
                buf.append(line[i:j + 1])
                i = j + 1
                continue
            buf.append(c)
            if c == "{":
                depth += 1
            elif c == "}":
                depth -= 1
            elif c == "(":
                par += 1
            elif c == ")":
                par -= 1
            elif c == ";" and depth == 0 and par == 0:
                emit("".join(buf), start_file or cur_file)
                buf = []
                start_file = cur_file
            i += 1
        buf.append("\n")
    if "".join(buf).strip():
        emit("".join(buf), start_file or cur_file)
    return groups


BRACKET_RE = re.compile(r"\[([^\[\]]+)\]")
PLAIN_NUM_RE = re.compile(r"^\s*(0[xX][0-9A-Fa-f]+|\d+)[uUlL]*\s*$")


def resolve_dims(st, groups, asserts, out):
    """Ghidra's C parser cannot size an array whose dimension is an enum constant or a sizeof
    expression (it silently makes it zero-length). Let clang evaluate every non-literal [expr]
    in the declarations and in the indexed assert paths, then substitute the numbers."""
    exprs = set()
    for _, decls in groups:
        for d in decls:
            for m in BRACKET_RE.finditer(d):
                if not PLAIN_NUM_RE.match(m.group(1)):
                    exprs.add(m.group(1).strip())
    for a in asserts:
        for m in BRACKET_RE.finditer(a["field"]):
            if not PLAIN_NUM_RE.match(m.group(1)):
                exprs.add(m.group(1).strip())
    exprs = sorted(exprs)
    gexprs = sorted({d.strip() for g in GLOBALS.values() for d in g["dims"]
                     if not PLAIN_NUM_RE.match(d) and d != "__UNSIZED__"} - set(exprs))
    probe = ['#include "all.c"\n']
    # global dims come from HW1 .c files whose macros may be missing: probe them one per file
    # chunk so an unknown macro only loses its own value
    for i, e in enumerate(exprs):
        # +1 keeps a zero-valued index legal as an array size; subtracted below
        probe.append("struct __dimprobe_%d { char a[(%s) + 1]; };\n"
                     "int __dimuse_%d = sizeof(struct __dimprobe_%d);\n" % (i, e, i, i))
    path = os.path.join(st, "dimprobe.c")
    with open(path, "w") as fh:
        fh.write("".join(probe))
    cmd = ["clang", "-fsyntax-only"] + CLANG_BASE + ["-Iinc", "-Isys", "-include", "prelude_emit.h",
                                                    "-Xclang", "-fdump-record-layouts-simple", path]
    p = subprocess.run(cmd, cwd=st, capture_output=True, text=True)
    if p.returncode:
        print("dimension probe failed:\n" + p.stderr[-2000:])
    vals = {}
    cur = None
    for line in p.stdout.splitlines():
        m = re.match(r"Type: struct __dimprobe_(\d+)$", line.strip())
        if m:
            cur = int(m.group(1))
            continue
        m = re.match(r"Size:(\d+)", line.strip())
        if m and cur is not None:
            vals[exprs[cur]] = int(m.group(1)) // 8 - 1
            cur = None
    unresolved = [e for e in exprs if e not in vals]
    for e in gexprs:
        v = probe_one(st, e)
        if v is not None:
            vals[e] = v
    for g in GLOBALS.values():
        res = []
        for d in g["dims"]:
            d = d.strip()
            if d == "__UNSIZED__":
                res = None
                g["unresolved_dim"] = "[] (unsized array: length unknown)"
                break
            if PLAIN_NUM_RE.match(d):
                res.append(int(re.sub(r"[uUlL]+$", "", d.strip()), 0))
            elif d in vals:
                res.append(vals[d])
            else:
                res = None
                g["unresolved_dim"] = d
                break
        g["dims"] = res

    def sub(text):
        return BRACKET_RE.sub(lambda m: "[%d]" % vals[m.group(1).strip()]
                              if m.group(1).strip() in vals else m.group(0), text)
    n = 0
    new_groups = []
    for h, decls in groups:
        nd = []
        for d in decls:
            d2 = sub(d)
            n += d2 != d
            nd.append(d2)
        new_groups.append((h, nd))
    for a in asserts:
        a["field"] = sub(a["field"])
    fix("*", "%d array dimensions / indices evaluated by clang and replaced with numbers (%d decls)"
        % (len(vals), n), "Ghidra's C parser makes T x[ENUM_CONST] zero-length")
    return new_groups, unresolved


SYMBOL_FILES = ("ghidra/cet_symbols.json", "ghidra/symbols_round1.json", "ghidra/symbols_round1_data.json")
NOT_TYPES = {"return", "else", "goto", "case", "typedef", "extern", "static", "if", "while", "for"}


def collect_globals(st, rr):
    """Declared types of the named data globals: CET header externs first (read from the raw
    staged headers, including blocks CET disabled with #if 0), else the definition in the HW1
    .c file named by the symbol's hw1_equivalent. Dimensions are evaluated later by clang."""
    syms = {}
    for f in SYMBOL_FILES:
        p = os.path.join(ROOT, f)
        if not os.path.isfile(p):
            continue
        for s in json.load(open(p, encoding="utf-8")).get("symbols", []):
            if str(s.get("kind", "")).lower() == "data" and s.get("name"):
                syms.setdefault(s["name"], s)
    out = {}
    ext_re = re.compile(r"^extern\s+(?:const\s+)?(?:struct\s+)?([A-Za-z_]\w*)\s*(\**)\s*([A-Za-z_]\w*)\s*((?:\[[^\]]*\])*)\s*;")
    inc = os.path.join(st, "inc")
    for f in sorted(os.listdir(inc)):
        for ln, line in enumerate(open(os.path.join(inc, f), encoding="latin-1"), 1):
            m = ext_re.match(line)
            if m and m.group(3) in syms and m.group(3) not in out and m.group(1) not in NOT_TYPES:
                out[m.group(3)] = {"base": m.group(1), "stars": len(m.group(2)),
                                   "dims": BRACKET_RE.findall(m.group(4)),
                                   "unsized": "[]" in m.group(4).replace(" ", ""),
                                   "source": "%s-header:%s:%d" % ("hw1" if f in HW1_FILL_INS else "cet", f, ln)}
    for name, s in syms.items():
        if name in out:
            continue
        eq = s.get("hw1_equivalent") or ""
        m = re.match(r"^(HomeworldSDL/src/[\w/]+\.c):(\w+)$", eq)
        if not m or m.group(2) != name:
            continue
        path = os.path.join(rr, "reference", m.group(1))
        if not os.path.isfile(path):
            continue
        rx = re.compile(r"^(?:static\s+)?(?:const\s+)?(?:struct\s+)?([A-Za-z_]\w*)\s+(\**)\s*%s\s*((?:\[[^\]]*\])*)\s*(?:=|;)" % re.escape(name))
        for ln, line in enumerate(open(path, encoding="latin-1"), 1):
            mm = rx.match(line)
            if mm and mm.group(1) not in NOT_TYPES:
                out[name] = {"base": mm.group(1), "stars": len(mm.group(2)),
                             "dims": BRACKET_RE.findall(mm.group(3)),
                             "unsized": "[]" in mm.group(3).replace(" ", ""),
                             "source": "hw1:%s:%d" % (m.group(1), ln)}
                break
    for g in out.values():
        if g.pop("unsized", False):
            g["dims"] = ["__UNSIZED__"]
    return out


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--out", default=os.path.join(ROOT, "ghidra/exports/headers_r2b"))
    ap.add_argument("--ref-root", default="")
    a = ap.parse_args()
    rr = ref_root(a.ref_root)
    os.makedirs(a.out, exist_ok=True)
    if not shutil.which("clang"):
        sys.exit("clang not found (needed for the layout check and the preprocessor)")
    st, order = stage(a.out, rr)
    asserts = collect_asserts(st)
    GLOBALS.update(collect_globals(st, rr))
    rc, errs, cmd = clang_check(st)
    pre = clang_emit(st, a.out)
    groups = split_decls(pre)
    groups, unresolved_dims = resolve_dims(st, groups, asserts, a.out)
    origin = {o["header"]: o for o in order}
    headers = [{"header": h, "origin": origin.get(h, {}).get("origin", "stub" if h in STUBS else "prelude"),
                "decls": d} for h, d in groups]
    doc = {"generated_by": "tools/prep_ghidra_headers.py", "pack": 4, "headers": headers,
           "globals": GLOBALS,
           "asserts": asserts, "fixups": FIXUPS, "staged": order}
    with open(os.path.join(a.out, "decls.json"), "w") as fh:
        json.dump(doc, fh, indent=1)
    rep = {"clang_check_cmd": cmd, "clang_check_rc": rc, "clang_errors": errs,
           "asserts": len(asserts), "headers": len(order), "unresolved_dims": unresolved_dims,
           "globals": len(GLOBALS),
           "globals_unresolved": sorted("%s[%s]" % (n, g.get("unresolved_dim")) for n, g in GLOBALS.items()
                                        if g["dims"] is None),
           "decls_per_header": {h["header"]: len(h["decls"]) for h in headers}}
    with open(os.path.join(a.out, "prep_report.json"), "w") as fh:
        json.dump(rep, fh, indent=1)
    print("globals with a declared type: %d (cet-header %d, hw1 %d, dims unresolved %d)" % (
        len(GLOBALS), sum(1 for g in GLOBALS.values() if "-header:" in g["source"]),
        sum(1 for g in GLOBALS.values() if g["source"].startswith("hw1:")),
        sum(1 for g in GLOBALS.values() if g["dims"] is None)))
    print("staged %d headers, %d asserts, clang check rc=%d errors=%d, %d decl groups, %d decls"
          % (len(order), len(asserts), rc, len(errs), len(headers),
             sum(len(h["decls"]) for h in headers)))
    for e in errs[:20]:
        print("  " + e)
    if unresolved_dims:
        print("  unresolved array dimensions: %s" % unresolved_dims[:20])
    return 0 if rc == 0 else 1


if __name__ == "__main__":
    sys.exit(main())
