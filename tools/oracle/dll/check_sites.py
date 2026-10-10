#!/usr/bin/env python3
"""Check tools/oracle/dll/hook_sites.h against cataclysm.exe offline (read-only; nothing is run or patched).

Usage: check_sites.py [--exe PATH] [--functions PATH/functions.csv] [--header PATH]
  --exe        default: original/game/cataclysm.exe of the main checkout
  --functions  the round-5 Ghidra export's functions.csv (default: ghidra/exports/cataclysm_gog_r5/functions.csv of
               the main checkout), used to check the H4 split ranges; skipped if missing

Checks, the same the DLL makes at attach plus the ones it cannot make itself:
  1. PE identity: ImageBase, TimeDateStamp, SizeOfImage equal the CAT_* constants;
  2. every HWI_HOOK_SITES entry: the expected bytes are in the file at that VA (a DATA site in .bss: the VA lies past
     the section's raw data, so the loader zero-fills it, which is what the table expects);
  3. ENTRY and CALLSITE sites (objdump -d): the patch length ends on an instruction boundary, and for ENTRY sites the
     copied instructions contain no relative branch (a plain copy into the trampoline must stay correct);
  4. HWI_RNG0_SPLIT_RANGES: each (start, size, name) matches functions.csv.
Exit code 0 when everything matches, 1 otherwise.
"""
import argparse
import csv
import os
import re
import struct
import subprocess
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
REPO = os.path.abspath(os.path.join(HERE, "..", "..", ".."))
MAIN = REPO if os.path.isdir(os.path.join(REPO, "original", "game")) else os.path.abspath(os.path.join(REPO, "..", "..", ".."))


def pe_info(b):
    pe = struct.unpack_from("<I", b, 0x3C)[0]
    nsec = struct.unpack_from("<H", b, pe + 6)[0]
    tds = struct.unpack_from("<I", b, pe + 8)[0]
    optsize = struct.unpack_from("<H", b, pe + 20)[0]
    opt = pe + 24
    base = struct.unpack_from("<I", b, opt + 28)[0]
    soi = struct.unpack_from("<I", b, opt + 56)[0]
    secs = []
    for i in range(nsec):
        name, vsize, va, rsize, rptr = struct.unpack_from("<8sIIII", b, opt + optsize + 40 * i)
        secs.append((name.rstrip(b"\0").decode(), va, vsize, rptr, rsize))
    return base, tds, soi, secs


def locate(va, base, secs):
    rva = va - base
    for name, sva, vsize, rptr, rsize in secs:
        if sva <= rva < sva + max(vsize, rsize):
            off = rva - sva
            return name, (rptr + off) if off < rsize else None
    return None, None


def main(argv):
    ap = argparse.ArgumentParser(description=__doc__.split("\n")[0])
    ap.add_argument("--exe", default=os.path.join(MAIN, "original", "game", "cataclysm.exe"))
    ap.add_argument("--functions", default=os.path.join(MAIN, "ghidra", "exports", "cataclysm_gog_r5", "functions.csv"))
    ap.add_argument("--header", default=os.path.join(HERE, "hook_sites.h"))
    args = ap.parse_args(argv)

    h = open(args.header).read()
    consts = {m.group(1): int(m.group(2), 16) for m in re.finditer(r"#define\s+(CAT_\w+)\s+0x([0-9A-Fa-f]+)u", h)}
    sites = re.findall(r'X\((H\d), "([^"]+)",\s*0x([0-9A-Fa-f]+)u, (HOOK_\w+),\s*(\d+), "([0-9A-Fa-f ]+)"\)', h)
    ranges = re.findall(r'X\(0x([0-9A-Fa-f]+)u,\s*(\d+), "([^"]+)"\)', h)
    b = open(args.exe, "rb").read()
    base, tds, soi, secs = pe_info(b)
    fails = 0

    def report(ok, text):
        nonlocal fails
        print(("OK    " if ok else "FAIL  ") + text)
        fails += 0 if ok else 1

    report(base == consts["CAT_IMAGE_BASE"], "ImageBase %#x" % base)
    report(tds == consts["CAT_TIMEDATESTAMP"], "TimeDateStamp %#010x" % tds)
    report(soi == consts["CAT_SIZEOFIMAGE"], "SizeOfImage %#x" % soi)
    if len(sites) != 7:
        report(False, "expected 7 hook sites in %s, parsed %d" % (args.header, len(sites)))

    for sid, name, va, kind, plen, expected in sites:
        va, plen = int(va, 16), int(plen)
        exp = bytes.fromhex(expected)
        sec, off = locate(va, base, secs)
        if kind == "HOOK_DATA":
            ok = sec is not None and off is None and exp == bytes(len(exp))
            report(ok, "%s %-22s %#010x DATA in %s past its raw data (zero-filled at load)" % (sid, name, va, sec))
            continue
        got = b[off:off + len(exp)] if off is not None else b""
        report(got == exp, "%s %-22s %#010x %s: %s" % (sid, name, va, sec, got.hex(" ").upper()))
        dis = subprocess.run(["objdump", "-d", "-M", "intel", "--start-address=%#x" % va,
                              "--stop-address=%#x" % (va + plen + 16), args.exe],
                             capture_output=True, text=True).stdout
        insns = []
        for line in dis.splitlines():
            m = re.match(r"\s+([0-9a-f]+):\s+((?:[0-9a-f]{2} )+)\s*(.*)", line)
            if m:
                insns.append((int(m.group(1), 16), m.group(3).strip()))
        starts = [a for a, _ in insns]
        report((va + plen) in starts, "%s patch of %d bytes ends on an instruction boundary (next at %#x)" % (sid, plen, va + plen))
        if kind == "HOOK_ENTRY":
            copied = [t for a, t in insns if a < va + plen]
            rel = [t for t in copied if re.match(r"(call|jmp|j[a-z]+|loop\w*)\s", t)]
            report(not rel, "%s copied instructions: %s" % (sid, "; ".join(copied)))

    if os.path.exists(args.functions):
        fn = {}
        with open(args.functions, newline="") as f:
            for row in csv.DictReader(f):
                fn[row["name"]] = (int(row["address"], 16), int(row["size"]))
        for start, size, name in ranges:
            got = fn.get(name)
            report(got == (int(start, 16), int(size)), "H4 range %-28s %s" % (name, "%#010x+%d" % got if got else "not in functions.csv"))
    else:
        print("SKIP  H4 ranges: %s not found" % args.functions)

    print("%d check(s) failed" % fails if fails else "all checks passed")
    return 1 if fails else 0


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
