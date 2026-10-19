#!/usr/bin/env python3
"""Generate the KAS function table (src/Game/KASFuncTable.c) from cataclysm.exe (M2-R8 kasf library,
docs/m2-r8-kasf-library.md "The KAS function table").

The exe carries its own kas2c table, cat_kasFunctionTable 0x00755048: 487 FunctionCall rows of 0x990 bytes (HW1
tools/kas2c/kas2c.h: name[129] at +0, returnsNumber at +132, realName[129] at +136, numParams at +268, then 16
FunctionParam rows of 136 bytes from +272: name[129], type at +132), the last one the "*" sentinel. The dev console's KAS
command (cat_consoleKasCommand 0x0045f270) parses a line by this table and calls cat_kasCallFunctionByName 0x0045b9a0,
which compares cat_kasHashString(name) (0x00491860, the PJW hash of the upper-cased characters) with one constant per
row in an MSVC compare tree (cmp / sub / dec / ja / je / jne) and calls that row's kasf. This script

  1. reads the 486 rows and the sentinel (python, from the PE file),
  2. hashes every KAS name with the exe's hash and
  3. walks the dispatcher's compare tree for every hash (objdump -d -M intel of 0x0045b9a0..0x0045f270, a small
     simulation of eax and the flags) to the kasf it calls: the exe address of every row, with no guessing,

then writes the table with each row's hash and address (or, with --check, compares with the tracked file and exits 1
on a difference). With --dispatch it also writes src/Game/KASFuncDispatch.c: cat_kasCallFunctionByName as a C switch
on the hash, each case converting the console's argument pointers as the exe's branch does (*(T *)args[i]).

Usage: python3 -I tools/m2_kasf_table_gen.py [--exe original/game/cataclysm.exe] [--check] [--list]
"""
import argparse
import os
import re
import struct
import subprocess
import sys

HERE = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
TABLE, ROW, NROWS = 0x00755048, 0x990, 487
DISPATCH, DISPATCH_END, HASHFN = 0x0045b9a0, 0x0045f270, 0x00491860
TYPES = ["SBYTE", "UBYTE", "SWORD", "UWORD", "SDWORD", "UDWORD", "REAL32", "REAL64", "BOOL", "BOOL8", "BOOL16",
         "SHIPPTR", "GROWSELECTIONPTR", "AITEAMPTR", "CHARPTR", "PATHPTR", "VECTORPTR", "VOLUMEPTR"]
CTYPES = {"SBYTE": "sbyte", "UBYTE": "ubyte", "SWORD": "sword", "UWORD": "uword", "SDWORD": "sdword",
          "UDWORD": "udword", "REAL32": "real32", "REAL64": "real64", "BOOL": "bool32", "BOOL8": "bool8",
          "BOOL16": "bool16", "SHIPPTR": "Ship *", "GROWSELECTIONPTR": "GrowSelection *", "AITEAMPTR": "struct AITeam *",
          "CHARPTR": "char *", "PATHPTR": "Path *", "VECTORPTR": "hvector *", "VOLUMEPTR": "Volume *"}


def main_root():
    try:
        common = subprocess.check_output(["git", "-C", HERE, "rev-parse", "--path-format=absolute",
                                          "--git-common-dir"], text=True, stderr=subprocess.DEVNULL).strip()
        return os.path.dirname(common)
    except Exception:
        return HERE


class PE:
    def __init__(self, path):
        self.d = open(path, "rb").read()
        pe = struct.unpack_from("<I", self.d, 0x3C)[0]
        nsec = struct.unpack_from("<H", self.d, pe + 6)[0]
        optsz = struct.unpack_from("<H", self.d, pe + 20)[0]
        self.base = struct.unpack_from("<I", self.d, pe + 24 + 28)[0]
        self.secs = []
        off = pe + 24 + optsz
        for _ in range(nsec):
            vsize, va, rsize, raw = struct.unpack_from("<IIII", self.d, off + 8)
            self.secs.append((self.base + va, rsize, raw))
            off += 40

    def read(self, va, n):
        for sva, rsize, raw in self.secs:
            if sva <= va and va + n <= sva + rsize:
                return self.d[raw + va - sva: raw + va - sva + n]
        raise ValueError("0x%08x not in the file" % va)


def kas_hash(s):
    """cat_kasHashString 0x00491860: h = (c & 0xDF) + (h << 4); g = h & 0xF0000000; if g: h ^= g >> 24, h &= ~g."""
    h = 0
    for c in s.encode("latin-1"):
        h = ((c & 0xDF) + (h << 4)) & 0xFFFFFFFF
        g = h & 0xF0000000
        if g:
            h ^= g >> 24
            h &= ~g & 0xFFFFFFFF
    return h


def cstr(b):
    return b.split(b"\0")[0].decode("latin-1")


def read_table(pe):
    rows = []
    for i in range(NROWS):
        b = pe.read(TABLE + i * ROW, ROW)
        n = struct.unpack_from("<i", b, 268)[0]
        params = []
        for k in range(max(0, n)):
            p = b[272 + k * 136: 272 + (k + 1) * 136]
            params.append((cstr(p[:129]), TYPES[struct.unpack_from("<i", p, 132)[0]]))
        rows.append(dict(name=cstr(b[:129]), returns=struct.unpack_from("<i", b, 132)[0], real=cstr(b[136:265]),
                         nparams=n, params=params))
    return rows


def disassemble(exe):
    out = subprocess.check_output(["objdump", "-d", "-M", "intel", "--no-show-raw-insn",
                                   "--start-address=0x%x" % DISPATCH, "--stop-address=0x%x" % DISPATCH_END, exe],
                                  text=True)
    ins = {}
    order = []
    for line in out.splitlines():
        m = re.match(r"\s+([0-9a-f]+):\s+(.*)$", line)
        if m:
            a = int(m.group(1), 16)
            ins[a] = m.group(2).strip()
            order.append(a)
    return ins, order


def walk(ins, order, h):
    """Simulates the compare tree for eax = h; returns the address of the first call after it (the kasf)."""
    nxt = {a: order[i + 1] for i, a in enumerate(order[:-1])}
    pc = order[0]
    eax, last = None, None     # last = (unsigned left, unsigned right) of the last cmp / sub / dec
    for _ in range(100000):
        s = ins[pc]
        m = re.match(r"call\s+0x([0-9a-f]+)", s)
        if m:
            t = int(m.group(1), 16)
            if t == HASHFN:
                eax = h
                pc = nxt[pc]
                continue
            return t
        m = re.match(r"(cmp|sub)\s+eax,0x([0-9a-f]+)$", s)
        if m:
            k = int(m.group(2), 16)
            last = (eax, k)
            if m.group(1) == "sub":
                eax = (eax - k) & 0xFFFFFFFF
            pc = nxt[pc]
            continue
        if s == "dec    eax" or re.match(r"dec\s+eax$", s):
            last = (eax, 1)
            eax = (eax - 1) & 0xFFFFFFFF
            pc = nxt[pc]
            continue
        m = re.match(r"(ja|jae|jb|jbe|je|jne|jmp)\s+0x([0-9a-f]+)", s)
        if m:
            op, t = m.group(1), int(m.group(2), 16)
            a, k = last if last else (0, 0)
            take = {"ja": a > k, "jae": a >= k, "jb": a < k, "jbe": a <= k, "je": a == k, "jne": a != k,
                    "jmp": True}[op]
            if take and not (DISPATCH <= t < DISPATCH_END):
                return t            # a tail jump to the kasf
            pc = t if take else nxt[pc]
            continue
        if s.startswith("ret"):
            return None
        pc = nxt[pc]                # argument loads before the call
    return None


def c_str(s):
    return '"' + s.replace("\\", "\\\\").replace('"', '\\"') + '"'


def gen_table(rows, addrs, exe_rel):
    L = []
    L.append("// =============================================================================")
    L.append("//  KASFuncTable.c")
    L.append("//  - Cataclysm's kas2c function table (cat_kasFunctionTable 0x00755048), with each row's name hash")
    L.append("//    (cat_kasHashString 0x00491860) and the exe address of the kasf its dispatcher calls")
    L.append("//    (cat_kasCallFunctionByName 0x0045b9a0)")
    L.append("//  GENERATED by tools/m2_kasf_table_gen.py from %s: do not edit; rerun the generator" % exe_rel)
    L.append("// =============================================================================")
    L.append("//  Copyright Relic Entertainment, Inc. All rights reserved.")
    L.append("// =============================================================================")
    L.append("")
    L.append('#include "KASFuncTable.h"')
    L.append("")
    L.append("const CatKasFunction cat_kasFunctionTable[CAT_KAS_FUNCTION_COUNT + 1] =")
    L.append("{")
    for r, a in zip(rows, addrs):
        ps = ", ".join("{%s, KAS_PARAM_%s}" % (c_str(n), t) for n, t in r["params"]) or "{NULL, 0}"
        L.append("    {%s, 0x%08x, %d, %s, %d, {%s}, 0x%08x}," % (
            c_str(r["name"]), kas_hash(r["name"]), r["returns"], c_str(r["real"]),
            r["nparams"], ps, a or 0))
    L.append("};")
    L.append("")
    return "\n".join(L)


def gen_dispatch(rows, addrs, exe_rel):
    L = []
    L.append("// =============================================================================")
    L.append("//  KASFuncDispatch.c")
    L.append("//  - cat_kasCallFunctionByName 0x0045b9a0: the dev console's KAS call, a switch on the name's hash")
    L.append("//    (cat_kasHashString 0x00491860) over the 486 rows of cat_kasFunctionTable 0x00755048; each branch")
    L.append("//    reads its arguments through the console's pointers (*(T *)args[i]) and returns the kasf's number,")
    L.append("//    0 for a kasf without one; an unknown name returns 0 (the exe's 0x0045f103)")
    L.append("//  GENERATED by tools/m2_kasf_table_gen.py --dispatch from %s: do not edit; rerun the generator" % exe_rel)
    L.append("// =============================================================================")
    L.append("//  Copyright Relic Entertainment, Inc. All rights reserved.")
    L.append("// =============================================================================")
    L.append("")
    L.append('#include "AITeam.h"')
    L.append('#include "KASFunc.h"')
    L.append('#include "KASFuncTable.h"')
    L.append('#include "KASVar.h"')
    L.append("")
    L.append("sdword cat_kasCallFunctionByName(char *name, void **args)")
    L.append("{")
    L.append("    (void)args;")
    L.append("    switch (cat_kasHashString(name))")
    L.append("    {")
    seen = set()
    for r, a in zip(rows, addrs):
        if r["name"] == "*":
            continue
        h = kas_hash(r["name"])
        if h in seen:
            continue
        seen.add(h)
        argl = ", ".join("*(%s*)args[%d]" % (CTYPES[t] + ("" if CTYPES[t].endswith("*") else " "), i)
                         for i, (n, t) in enumerate(r["params"]))
        call = "%s(%s)" % (r["real"], argl)
        if r["returns"]:
            L.append("        case 0x%08x: return (sdword)%s;  // %s 0x%08x" % (h, call, r["name"], a))
        else:
            L.append("        case 0x%08x: %s; return 0;  // %s 0x%08x" % (h, call, r["name"], a))
    L.append("        default: return 0;")
    L.append("    }")
    L.append("}")
    L.append("")
    return "\n".join(L)


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--exe", default=os.path.join(main_root(), "original/game/cataclysm.exe"))
    ap.add_argument("--out", default=os.path.join(HERE, "src/Game/KASFuncTable.c"))
    ap.add_argument("--dispatch-out", default=os.path.join(HERE, "src/Game/KASFuncDispatch.c"))
    ap.add_argument("--dispatch", action="store_true", help="also write the dispatcher")
    ap.add_argument("--check", action="store_true", help="compare with the tracked files, exit 1 on a difference")
    ap.add_argument("--list", action="store_true", help="print row, KAS name, hash, address, C name, params")
    a = ap.parse_args()

    pe = PE(a.exe)
    rows = read_table(pe)
    if rows[-1]["name"] != "*" or any(r["name"] == "*" for r in rows[:-1]):
        sys.exit("m2_kasf_table_gen: the table does not end with the '*' sentinel")
    ins, order = disassemble(a.exe)
    addrs = []
    for r in rows:
        addrs.append(walk(ins, order, kas_hash(r["name"])) if r["name"] != "*" else 0)
    missing = [r["name"] for r, ad in zip(rows[:-1], addrs[:-1]) if not ad]
    if missing:
        sys.exit("m2_kasf_table_gen: no dispatcher branch for %s" % ", ".join(missing))
    # one kasf per C name: the dispatcher must call the same address for rows sharing a C name
    byreal = {}
    for r, ad in zip(rows[:-1], addrs[:-1]):
        if byreal.setdefault(r["real"], ad) != ad:
            sys.exit("m2_kasf_table_gen: %s dispatched to two addresses" % r["real"])
    if a.list:
        for i, (r, ad) in enumerate(zip(rows, addrs)):
            print("%d\t%s\t0x%08x\t0x%08x\t%s\t%d\t%s" % (i, r["name"], kas_hash(r["name"]), ad or 0, r["real"],
                  r["returns"], ", ".join("%s:%s" % p for p in r["params"])))
        return
    exe_rel = "cataclysm.exe (GOG 1.01)"
    outs = [(a.out, gen_table(rows, addrs, exe_rel))]
    if a.dispatch:
        outs.append((a.dispatch_out, gen_dispatch(rows, addrs, exe_rel)))
    rc = 0
    for path, text in outs:
        if a.check:
            old = open(path).read() if os.path.exists(path) else ""
            if old != text:
                print("m2_kasf_table_gen: %s differs from the exe's table" % os.path.relpath(path, HERE))
                rc = 1
            else:
                print("m2_kasf_table_gen: %s current" % os.path.relpath(path, HERE))
        else:
            open(path, "w").write(text)
            print("m2_kasf_table_gen: wrote %s (%d rows + sentinel, %d distinct C names, %d distinct addresses)" % (
                os.path.relpath(path, HERE), len(rows) - 1, len(byreal), len(set(byreal.values()))))
    sys.exit(rc)


if __name__ == "__main__":
    main()
