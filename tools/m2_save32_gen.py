#!/usr/bin/env python3
"""Generate src/Game/SaveCat32Layouts.c: field-by-field ILP32 -> LP64 conversion tables for the Cataclysm save loader.

M2-R6 oracle-mode-a. cataclysm.exe saves are raw dumps of its ILP32 structs (docs/formats/savegame.md). This tree's
structs, compiled for i686 with tools/m2_layout32.sh's flags, have the exe's layouts (pinned by the static asserts that
script evaluates), so the 32-bit offsets of every field come from clang's record-layout dump of an -m32 compile, and
the 64-bit offsets from a native dump of the same declarations (both configs of the build, debug and release, must
agree). The two dumps are walked in lockstep, field by field:

  * a pointer (data or function) becomes a PTR op: the exe's 32-bit value (an object ID, a player index, a list
    position, a stale pointer, or NULL) is sign-extended into the 64-bit slot, where the loader's Fix* code reads it
    back as an sdword, as GoK's 64-bit loader does with its own saves;
  * a scalar of the same size in both becomes a COPY (adjacent copies are merged); a scalar whose size differs (none in
    the roots below; the generator stops if it meets one) would need an extension rule;
  * a record recurses; an array of records becomes an ARRAY op over a sub-layout (element stride per ABI);
  * a union contributes the ops of all its members when they agree on every 32-bit offset (the tree's object
    "inheritance" unions), else the ops of its largest member (listed in the generated file as a conflict);
  * a bit-field copies the bytes its bits occupy.

  python3 -I tools/m2_save32_gen.py            # (re)write src/Game/SaveCat32Layouts.c
  python3 -I tools/m2_save32_gen.py --check    # exit 1 if the file on disk differs from a fresh generation

Needs clang (the -m32 compile needs no 32-bit libc: an empty gnu/stubs-32.h shim is used, as in m2_layout32.sh).
"""
import hashlib
import os
import re
import subprocess
import sys
import tempfile

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
OUT = os.path.join(ROOT, "src", "Game", "SaveCat32Layouts.c")

# the structs the loader converts, in the common translation unit (headers below)
COMMON_HEADERS = ["SpaceObj.h", "Universe.h", "TitanInterfaceC.h", "CommandLayer.h", "CameraCommand.h", "Formation.h",
                  "Tactics.h", "SinglePlayer.h", "Blobs.h", "MeshAnim.h", "AIPlayer.h", "AITeam.h", "AIVar.h",
                  "ConsMgr.h", "ResearchAPI.h", "Nebulae.h", "FOW.h", "Randy.h", "B-Spline.h", "Clouds.h"]
COMMON_ROOTS = ["CaptainGameInfo", "Ship", "ClampInfo", "DamageLightStatic", "Gun", "DockInfo", "ShipsInsideMe",
                "InsideShip", "ShipSinglePlayerGameInfo", "Asteroid", "Bullet", "Missile", "Derelict", "DustCloud",
                "Nebula", "blob", "Universe", "CommandToDo", "MilitaryParadeCommand", "MilitarySlot", "CameraCommand",
                "CameraStackEntry", "RetreatAtom", "ResourceVolume", "MineFormationInfo", "ShipStaticInfo",
                "AsteroidStaticInfo", "SinglePlayerGameInfo", "madanim", "splinecurve", "PlayerResearchInfo",
                "shipsinprogress", "shipinprogress", "CatFOWGrid", "ResearchTopic", "AttackAtom", "DustCloudStaticInfo",
                "cloudSystem", "Camera"]
OPTIONAL_ROOTS = {"shipsinprogress", "shipinprogress", "CatFOWGrid", "splinecurve"}

SHIP_DIR = os.path.join(ROOT, "src", "Ships")

FLAGS32 = ["clang", "-m32", "-fsyntax-only", "-std=gnu11", "-D_X86", "-malign-double", "-DHW_GAME_HOMEWORLD",
           "-DHW_BUILD_FOR_DISTRIBUTION", "-D_LINUX_FIX_ME", "-D_GNU_SOURCE", "-fshort-wchar", "-w", "-Wno-int-conversion"]
FLAGS64 = ["clang", "-fsyntax-only", "-std=gnu11", "-DHW_PTR_64", "-DHW_PTR_64_FIX_ME", "-DHW_GAME_HOMEWORLD",
           "-D_LINUX_FIX_ME", "-D_GNU_SOURCE", "-DHWI_ORACLE=1", "-w", "-Wno-int-conversion"]
CONFIGS64 = [["-DHW_BUILD_FOR_DEBUGGING"], ["-DHW_BUILD_FOR_DISTRIBUTION"]]


def incs(shim):
    r = ROOT
    return ["-I" + os.path.join(r, "src", "Game"), "-I" + os.path.join(r, "src", "SDL"),
            "-I" + os.path.join(r, "src", "Ships"), "-I" + os.path.join(r, "src", "ThirdParty", "CRC"),
            "-I" + os.path.join(r, "src", "ThirdParty", "LZSS"), "-I" + os.path.join(r, "src", "ThirdParty", "Titan"),
            "-I/usr/include/SDL2", "-idirafter", "/usr/include/x86_64-linux-gnu", "-I" + shim]


# ---------------------------------------------------------------- dump parsing

class Field:
    __slots__ = ("off", "bits", "type", "name", "children")

    def __init__(self, off, bits, typ, name):
        self.off, self.bits, self.type, self.name, self.children = off, bits, typ, name, []


LINE = re.compile(r"^\s*(\d+)(?::(\d+)-(\d+))?\s\|( *)(.*?)\s*$")


def parse_dump(text):
    """{record header text: (sizeof, [top Field])}"""
    out = {}
    for block in text.split("*** Dumping AST Record Layout")[1:]:
        lines = [ln for ln in block.splitlines() if "|" in ln]
        if not lines:
            continue
        m = LINE.match(lines[0])
        if not m:
            continue
        head = m.group(5)
        size = None
        stack = []          # (depth, Field)
        top = []
        for ln in lines[1:]:
            ms = re.search(r"\[sizeof=(\d+)", ln)
            if ms:
                size = int(ms.group(1))
                break
            m = LINE.match(ln)
            if not m:
                continue
            depth = len(m.group(4)) // 2
            content = m.group(5)
            bits = (int(m.group(2)), int(m.group(3))) if m.group(2) is not None else None
            if content.endswith(")") and ("(anonymous" in content or "(unnamed" in content):
                typ, name = content, ""
            else:
                typ, _, name = content.rpartition(" ")
                typ = typ.rstrip()
            f = Field(int(m.group(1)), bits, typ, name)
            while stack and stack[-1][0] >= depth:
                stack.pop()
            if stack:
                stack[-1][1].children.append(f)
            else:
                top.append(f)
            stack.append((depth, f))
        out[head] = (size, top)
    return out


def run_dump(flags, src, shim):
    with tempfile.NamedTemporaryFile("w", suffix=".c", delete=False) as fh:
        fh.write(src)
        path = fh.name
    try:
        p = subprocess.run(flags + incs(shim) + ["-Xclang", "-fdump-record-layouts-complete", path],
                           capture_output=True, text=True)
    finally:
        os.unlink(path)
    if p.returncode != 0:
        sys.stderr.write(p.stderr[-4000:])
        raise SystemExit("m2_save32_gen: clang failed")
    return parse_dump(p.stdout)


# ---------------------------------------------------------------- walking

DIMS = re.compile(r"((?:\[\d+\])+)$")


def split_array(typ):
    m = DIMS.search(typ)
    if not m:
        return typ, []
    dims = [int(x) for x in re.findall(r"\[(\d+)\]", m.group(1))]
    return typ[:m.start()].rstrip(), dims


def is_pointer(base):
    return base.endswith("*") or "(*)" in base


SCALAR = {"ubyte": 1, "sbyte": 1, "char": 1, "signed char": 1, "unsigned char": 1, "bool8": 1, "bitflag8": 1,
          "uword": 2, "sword": 2, "short": 2, "unsigned short": 2, "bool16": 2, "bitflag16": 2, "crc16": 2,
          "udword": 4, "sdword": 4, "real32": 4, "bool32": 4, "bitflag32": 4, "int": 4, "unsigned int": 4,
          "float": 4, "crc32": 4, "Uint8": 1, "Sint8": 1, "Uint16": 2, "Sint16": 2, "Uint32": 4, "Sint32": 4,
          "real64": 8, "double": 8, "sqword": 8, "uqword": 8, "Uint64": 8, "Sint64": 8, "bitflag64": 8}


class Gen:
    def __init__(self, tu_src, tu_name, shim):
        self.tu_src, self.tu_name, self.shim = tu_src, tu_name, shim
        self.tu_tag = re.sub(r"[^A-Za-z0-9]+", "_", os.path.splitext(tu_name)[0])
        self.wrappers = {}          # key -> C expression type for __typeof__
        self.d32 = self.d64 = None
        self.dumps64 = []
        self.layouts = {}           # layout name -> dict
        self.order = []
        self.conflicts = []
        self.pending = True

    def wrap_name(self, key):
        return "hwiw_" + hashlib.sha1(key.encode()).hexdigest()[:12]

    def source(self):
        s = [self.tu_src]
        for key, expr in sorted(self.wrappers.items()):
            s.append("struct %s { %s e; };" % (self.wrap_name(key), expr))
        return "\n".join(s) + "\n"

    def dump(self):
        src = self.source()
        self.d32 = run_dump(FLAGS32, src, self.shim)
        self.dumps64 = [run_dump(FLAGS64 + c, src, self.shim) for c in CONFIGS64]
        self.d64 = self.dumps64[0]

    def wrapper(self, key):
        h = "struct " + self.wrap_name(key)
        a, b = self.d32.get(h), self.d64.get(h)
        if a is None or b is None:
            return None
        for other in self.dumps64[1:]:
            if fmt_tree(other.get(h)) != fmt_tree(b):
                raise SystemExit("m2_save32_gen: the debug and release builds lay out %s differently" % key)
        return a, b

    def need(self, key, expr):
        if key not in self.wrappers:
            self.wrappers[key] = expr
            self.pending = True

    def scalar_size(self, base, path_expr):
        """(size32, size64) of a non-pointer leaf type, or None while its wrapper is pending"""
        if base in SCALAR:
            return SCALAR[base], SCALAR[base]
        if base.startswith("enum "):
            return 4, 4
        key = "T:" + path_expr
        self.need(key, "__typeof__(%s)" % path_expr)
        w = self.wrapper(key)
        if w is None:
            return None
        (s32, t32), (s64, t64) = w
        return s32, s64

    def elem(self, path_expr):
        key = "E:" + path_expr
        self.need(key, "__typeof__(%s)" % path_expr)
        return self.wrapper(key)

    # ---- op building: ops are dicts {k, o32, o64, n32, n64, cnt, s32, s64, sub}
    def walk(self, f32, f64, path, base32, base64, ops):
        if f32.type != f64.type or f32.name != f64.name or len(f32.children) != len(f64.children):
            raise SystemExit("m2_save32_gen: dumps disagree at %s (%s %s / %s %s)" % (path, f32.type, f32.name,
                                                                                  f64.type, f64.name))
        o32, o64 = base32 + f32.off, base64 + f64.off
        sub_path = path + ("." + f32.name if f32.name else "")
        if f32.bits is not None:
            first, last = f32.bits
            nbytes = (last // 8) + 1
            if f64.bits != f32.bits:
                raise SystemExit("m2_save32_gen: bit-field %s differs" % sub_path)
            ops.append(dict(k="COPY", o32=o32, o64=o64, n=nbytes))
            return
        if f32.children:
            is_union = f32.type.startswith("union") or (
                len(f32.children) >= 2 and all(c.bits is None and c.off == f32.children[0].off for c in f32.children))
            if is_union:
                members = []
                for c32, c64 in zip(f32.children, f64.children):
                    mops = []
                    self.walk(c32, c64, sub_path, base32, base64, mops)
                    members.append((c32.name or c32.type, mops))
                ops.extend(self.merge_union(sub_path, members))
            else:
                for c32, c64 in zip(f32.children, f64.children):
                    self.walk(c32, c64, sub_path, base32, base64, ops)
            return
        base, dims = split_array(f32.type)
        count = 1
        for d in dims:
            count *= d
        idx = "".join("[0]" for _ in dims)
        if is_pointer(base):
            ops.append(dict(k="PTR", o32=o32, o64=o64, cnt=count))
            return
        if dims:
            w = self.elem(sub_path + idx)
            if w is None:
                return
            (s32, t32), (s64, t64) = w
            e32, e64 = t32[0], t64[0]
            if e32.children:
                name = self.sub_layout(sub_path + idx, e32, e64, s32, s64)
                ops.append(dict(k="ARRAY", o32=o32, o64=o64, cnt=count, s32=s32, s64=s64, sub=name))
                return
            ebase, edims = split_array(e32.type)
            if is_pointer(ebase):
                ops.append(dict(k="PTR", o32=o32, o64=o64, cnt=count))
                return
            if s32 != s64:
                ops.append(ext_op(sub_path, ebase, o32, o64, s32, s64, count))
                return
            ops.append(dict(k="COPY", o32=o32, o64=o64, n=s32 * count, weak=is_pad(f32.name)))
            return
        sz = self.scalar_size(base, sub_path)
        if sz is None:
            return
        if sz[0] != sz[1]:
            ops.append(ext_op(sub_path, base, o32, o64, sz[0], sz[1], 1))
            return
        ops.append(dict(k="COPY", o32=o32, o64=o64, n=sz[0], weak=is_pad(f32.name)))

    def merge_union(self, path, members):
        """all members' ops in declaration order; a pad member's copies (ubyte padXX[]) are dropped where another
        member has a real field; members whose real fields map a 32-bit byte to different 64-bit places are a
        conflict: the later member's write wins at run time, and the conflict is listed"""
        strong = []
        for _, mops in members:
            cov = {}
            for op in mops:
                if op.get("weak"):
                    continue
                for b32, b64 in op_bytes(op):
                    cov[b32] = (b64, op["k"])
            strong.append(cov)
        conflict = False
        allmap = {}
        where = []
        for mi, cov in enumerate(strong):
            for b32, v in sorted(cov.items()):
                if b32 in allmap and allmap[b32][0] != v:
                    conflict = True
                    if len(where) < 4 and (not where or where[-1][0] != allmap[b32][1]):
                        where.append((allmap[b32][1], members[mi][0][:30], b32, allmap[b32][0], v))
                allmap.setdefault(b32, (v, members[mi][0][:30]))
        out, seen = [], set()
        for i, (_, mops) in enumerate(members):
            for op in mops:
                if op.get("weak"):
                    rng = range(op["o32"], op["o32"] + op["n"])
                    if any(b in strong[j] for j in range(len(members)) if j != i for b in rng):
                        continue
                t = norm(op)
                if t in seen:
                    continue
                seen.add(t)
                out.append(op)
        if conflict:
            self.conflicts.append("%s: the members map some 32-bit bytes to different 64-bit offsets (the 64-bit "
                                  "views overlap); converted member by member in declaration order, the later "
                                  "member's write wins: %s; first: %s" % (path, ", ".join(m[0][:40] for m in members),
                                                     "; ".join("32-bit 0x%x: %s says %s, %s says %s" % (b, a, va, c, vc)
                                                               for a, c, b, va, vc in where)))
        return out

    def sub_layout(self, path, e32, e64, s32, s64):
        last = re.sub(r"[^A-Za-z0-9]+", "_", path.split("0)", 1)[-1].split(".")[-1]).strip("_")
        sig = hashlib.sha1((fmt_tree((s32, [e32])) + "|" + fmt_tree((s64, [e64]))).encode()).hexdigest()[:8]
        name = "elem_%s_%s_%s" % (self.tu_tag, last, sig)
        if name in self.layouts:
            return name
        ops = []
        for c32, c64 in zip(e32.children, e64.children):
            self.walk(c32, c64, path, 0, 0, ops)
        self.layouts[name] = dict(name=name, s32=s32, s64=s64, ops=finish(ops), root=None)
        self.order.append(name)
        return name

    def root(self, rname, lname, assert_size):
        key = "R:" + rname
        self.need(key, rname)
        w = self.wrapper(key)
        if w is None:
            return
        (s32, t32), (s64, t64) = w
        e32, e64 = t32[0], t64[0]
        ops = []
        for c32, c64 in zip(e32.children, e64.children):
            self.walk(c32, c64, "(*(%s *)0)" % rname, 0, 0, ops)
        self.layouts[lname] = dict(name=lname, s32=s32, s64=s64, ops=finish(ops),
                                   root=rname if assert_size else None)
        if lname not in self.order:
            self.order.append(lname)


def is_pad(name):
    return re.match(r"^(cat)?[pP]ad", name or "") is not None


def op_bytes(op):
    if op["k"] == "COPY":
        return [(op["o32"] + i, op["o64"] + i) for i in range(op["n"])]
    if op["k"] == "PTR":
        return [(op["o32"] + 4 * j + i, op["o64"] + 8 * j) for j in range(op["cnt"]) for i in range(4)]
    if op["k"] in ("SEXT", "ZEXT"):
        return [(op["o32"] + op["s32"] * j + i, op["o64"] + op["s64"] * j) for j in range(op["cnt"])
                for i in range(op["s32"])]
    return [(op["o32"] + i, ("A", op["o64"], op["sub"])) for i in range(op["s32"] * op["cnt"])]


def ext_op(path, base, o32, o64, n32, n64, count):
    """a scalar whose size differs: Windows' 2-byte wchar_t (-fshort-wchar in the ILP32 dump) and 4-byte long"""
    if (n32, n64) not in ((2, 4), (4, 8)):
        raise SystemExit("m2_save32_gen: scalar %s (%s) is %d bytes in ILP32 and %d in LP64" % (path, base, n32, n64))
    signed = not (base.startswith("unsigned") or base in ("wchar_t", "size_t", "uintptr_t"))
    return dict(k="SEXT" if signed else "ZEXT", o32=o32, o64=o64, cnt=count, s32=n32, s64=n64)


def fmt_tree(rec):
    if rec is None:
        return None
    size, top = rec
    out = [str(size)]

    def rec_(f, d):
        out.append("%d %s %s %s %s" % (d, f.off, f.bits, f.type, f.name))
        for c in f.children:
            rec_(c, d + 1)
    for f in top:
        rec_(f, 0)
    return "\n".join(out)


def expand_ptrs(ops):
    out = []
    for op in ops:
        if op["k"] == "PTR" and op.get("cnt", 1) > 1:
            for i in range(op["cnt"]):
                out.append(dict(k="PTR", o32=op["o32"] + 4 * i, o64=op["o64"] + 8 * i, cnt=1))
        else:
            out.append(op)
    return out


def norm(op):
    return tuple(sorted((k, str(v)) for k, v in op.items()))


def extent(ops):
    e = 0
    for op in ops:
        if op["k"] == "COPY":
            e = max(e, op["o32"] + op["n"])
        elif op["k"] == "PTR":
            e = max(e, op["o32"] + 4 * op.get("cnt", 1))
        else:
            e = max(e, op["o32"] + op["s32"] * op["cnt"])
    return e


def span32(op):
    if op["k"] == "COPY":
        return op["n"]
    if op["k"] == "PTR":
        return 4 * op["cnt"]
    return op["s32"] * op["cnt"]


def finish(ops):
    """drop duplicates (keep the first), merge copies and pointer runs that are adjacent in both layouts; the order
    is kept (it matters only where a union's 64-bit views overlap: the later write wins)"""
    out, seen = [], set()
    for op in ops:
        t = norm(op)
        if t in seen:
            continue
        seen.add(t)
        op = {k: v for k, v in op.items() if k != "weak"}
        if out and op["k"] == "COPY" and out[-1]["k"] == "COPY":
            p = out[-1]
            if op["o32"] == p["o32"] + p["n"] and op["o64"] == p["o64"] + p["n"]:
                p["n"] += op["n"]
                continue
        if out and op["k"] == "PTR" and out[-1]["k"] == "PTR":
            p = out[-1]
            if op["o32"] == p["o32"] + 4 * p["cnt"] and op["o64"] == p["o64"] + 8 * p["cnt"]:
                p["cnt"] += op["cnt"]
                continue
        out.append(dict(op))
    return out


def generate_tu(tu_src, tu_name, roots, shim):
    """roots: [(C type, layout name, assert size, optional)]"""
    g = Gen(tu_src, tu_name, shim)
    for _ in range(12):
        g.pending = False
        g.dump()
        g.layouts, g.order, g.conflicts = {}, [], []
        for rname, lname, assert_size, optional in roots:
            g.root(rname, lname, assert_size)
        if not g.pending:
            break
    else:
        raise SystemExit("m2_save32_gen: wrappers did not converge for %s" % tu_name)
    missing = [r[0] for r in roots if r[1] not in g.layouts and not r[3]]
    if missing:
        raise SystemExit("m2_save32_gen: no layout for %s" % ", ".join(missing))
    return g


def ship_specifics():
    """[(ship .c file, header variable, specifics struct)] from the CustShipHeader initialisers"""
    out = []
    for fn in sorted(os.listdir(SHIP_DIR)):
        if not fn.endswith(".c"):
            continue
        text = open(os.path.join(SHIP_DIR, fn), encoding="latin-1").read()
        for m in re.finditer(r"^CustShipHeader\s+(\w+)\s*=\s*\{(.*?)^\};", text, re.M | re.S):
            ms = re.search(r"\.sizeofShipSpecifics\s*=\s*sizeof\((\w+)\)", m.group(2))
            if ms:
                out.append((fn, m.group(1), ms.group(1)))
    return out


def emit(gens, specs):
    lines = []
    w = lines.append
    w("/*=============================================================================")
    w("    Name    : SaveCat32Layouts.c")
    w("    Purpose : GENERATED by tools/m2_save32_gen.py (do not edit): the ILP32 -> LP64")
    w("              field tables the Cataclysm save loader converts cataclysm.exe's")
    w("              struct dumps with (SaveCat32.c, docs/m2-r6-oracle-mode-a.md).")
    w("              32-bit offsets: clang -m32 record layouts of this tree's headers")
    w("              (the exe's layouts, tools/m2_layout32.sh); 64-bit offsets: the")
    w("              native layouts. Regenerate after any change to a converted struct;")
    w("              the size asserts below catch a stale table at compile time.")
    w("")
    w("    Created 2026-10-18 for Homeworld-Infection (M2-R6 oracle-mode-a). This")
    w("    tree is REULA-licensed, see src/COPYING.REULA.")
    w("=============================================================================*/")
    w("")
    w("#include <stddef.h>")
    w("#include \"SaveCat32.h\"")
    w("")
    w("#if SAVECAT32_LP64")
    w("")
    names = {}
    conf = []
    for g in gens:
        conf += g.conflicts
        for n in g.order:
            names[n] = g.layouts[n]
    over = []
    for n in sorted(names):
        L = names[n]
        spans = []
        for op in L["ops"]:
            if op["k"] == "COPY":
                spans.append((op["o64"], op["o64"] + op["n"]))
            elif op["k"] == "PTR":
                spans.append((op["o64"], op["o64"] + 8 * op["cnt"]))
            elif op["k"] in ("SEXT", "ZEXT"):
                spans.append((op["o64"], op["o64"] + op["s64"] * op["cnt"]))
            else:
                spans.append((op["o64"], op["o64"] + op["s64"] * op["cnt"]))
        spans.sort()
        for (a0, a1), (b0, b1) in zip(spans, spans[1:]):
            if b0 < a1:
                over.append("%s: 64-bit bytes 0x%x-0x%x are written twice (a union whose 64-bit views overlap)" % (
                    n, b0, min(a1, b1) - 1))
                break
    conf += over
    if conf:
        w("// union members that disagree (converted member by member in declaration order, the later write wins):")
        for c in conf:
            w("//   " + c)
        w("")
    for n in sorted(names):
        if n.startswith("elem_"):
            w("static const SaveCat32Layout saveCat32Layout_%s;" % n)
    w("")
    done = set()

    def emit_layout(L):
        if L["name"] in done:
            return
        done.add(L["name"])
        for op in L["ops"]:
            if op["k"] == "ARRAY":
                emit_layout(names[op["sub"]])
        w("static const SaveCat32Op ops_%s[] = {" % L["name"])
        for op in L["ops"]:
            if op["k"] == "COPY":
                w("    { SC32_COPY, 0x%x, 0x%x, 0x%x, 1, 0, 0, NULL }," % (op["o32"], op["o64"], op["n"]))
            elif op["k"] == "PTR":
                w("    { SC32_PTR, 0x%x, 0x%x, 4, %d, 4, 8, NULL }," % (op["o32"], op["o64"], op["cnt"]))
            elif op["k"] in ("SEXT", "ZEXT"):
                w("    { SC32_%s, 0x%x, 0x%x, %d, %d, %d, %d, NULL }," % (op["k"], op["o32"], op["o64"], op["s32"],
                                                                         op["cnt"], op["s32"], op["s64"]))
            else:
                w("    { SC32_ARRAY, 0x%x, 0x%x, 0, %d, 0x%x, 0x%x, &saveCat32Layout_%s }," % (
                    op["o32"], op["o64"], op["cnt"], op["s32"], op["s64"], op["sub"]))
        if not L["ops"]:
            w("    { SC32_COPY, 0, 0, 0, 0, 0, 0, NULL },")
        w("};")
        static = "static " if L["name"].startswith("elem_") else ""
        w("%sconst SaveCat32Layout saveCat32Layout_%s = { \"%s\", 0x%x, 0x%x, %d, ops_%s };" % (
            static, L["name"], L["name"], L["s32"], L["s64"], len(L["ops"]), L["name"]))
        if L["root"]:
            w("_Static_assert(sizeof(%s) == 0x%x, \"%s: regenerate src/Game/SaveCat32Layouts.c (tools/m2_save32_gen.py)\");"
              % (L["root"], L["s64"], L["root"]))
        w("")

    for n in sorted(names):
        if not n.startswith("elem_"):
            emit_layout(names[n])
    w("// ship specifics, by CustShipHeader (the struct each header's sizeofShipSpecifics names)")
    for fn, hdr, spec in specs:
        w("extern CustShipHeader %s;" % hdr)
    w("const SaveCat32SpecLayout saveCat32SpecLayouts[] = {")
    for fn, hdr, spec in specs:
        w("    { &%s, &saveCat32Layout_spec_%s },   // src/Ships/%s" % (hdr, spec, fn))
    w("    { NULL, NULL }")
    w("};")
    w("")
    w("#endif // SAVECAT32_LP64")
    return "\n".join(lines) + "\n"


def main():
    check = "--check" in sys.argv[1:]
    with tempfile.TemporaryDirectory() as tmp:
        shim = os.path.join(tmp, "shim")
        os.makedirs(os.path.join(shim, "gnu"))
        open(os.path.join(shim, "gnu", "stubs-32.h"), "w").close()
        common = "\n".join('#include "%s"' % h for h in COMMON_HEADERS) + "\n"
        gens = [generate_tu(common, "common", [(r, r, True, r in OPTIONAL_ROOTS) for r in COMMON_ROOTS], shim)]
        specs = ship_specifics()
        byfile = {}
        for fn, hdr, spec in specs:
            byfile.setdefault(fn, [])
            if spec not in byfile[fn]:
                byfile[fn].append(spec)
        for fn, spec_list in sorted(byfile.items()):
            src = '#include "%s"\n' % os.path.join(SHIP_DIR, fn)
            gens.append(generate_tu(src, fn, [(sp, "spec_" + sp, False, False) for sp in spec_list], shim))
        text = emit(gens, specs)
    if check:
        old = open(OUT).read() if os.path.exists(OUT) else ""
        if old != text:
            print("m2_save32_gen: %s is STALE (run tools/m2_save32_gen.py)" % os.path.relpath(OUT, ROOT))
            return 1
        print("m2_save32_gen: %s is current" % os.path.relpath(OUT, ROOT))
        return 0
    with open(OUT, "w") as fh:
        fh.write(text)
    nl = sum(len(g.layouts) for g in gens)
    print("m2_save32_gen: wrote %s (%d layouts, %d ship specifics, %d union conflicts)" % (
        os.path.relpath(OUT, ROOT), nl, len(specs), sum(len(g.conflicts) for g in gens)))
    for g in gens:
        for c in g.conflicts:
            print("  conflict: " + c)
    return 0


if __name__ == "__main__":
    sys.exit(main())
