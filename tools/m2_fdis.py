#!/usr/bin/env python3
"""Print one exe function's disassembly (Intel syntax), located by name or address in the Ghidra export.

M2-R4 command port helper. Reads functions.csv of the export and a full `objdump -d -M intel` listing of
original/game/cataclysm.exe (the R2 copy, original/scratch-m2/m2r2-mesh-tools/exe.dis, by default).

  python3 -I tools/m2_fdis.py NAME|0xADDR [REGEX] [--export DIR] [--dis FILE]
"""
import argparse
import csv
import os
import re
import subprocess

HERE = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))


def main_root():
    try:
        common = subprocess.check_output(["git", "-C", HERE, "rev-parse", "--path-format=absolute",
                                          "--git-common-dir"], text=True, stderr=subprocess.DEVNULL).strip()
        return os.path.dirname(common)
    except Exception:
        return HERE


def main():
    root = main_root()
    ap = argparse.ArgumentParser()
    ap.add_argument("func")
    ap.add_argument("regex", nargs="?")
    ap.add_argument("--export", default=os.path.join(root, "ghidra/exports/cataclysm_gog_r6"))
    ap.add_argument("--dis", default=os.path.join(root, "original/scratch-m2/m2r2-mesh-tools/exe.dis"))
    a = ap.parse_args()
    with open(os.path.join(a.export, "functions.csv"), newline="") as f:
        rows = [(int(r["address"], 16), int(r["size"]), r["name"]) for r in csv.DictReader(f)]
    if a.func.startswith("0x"):
        addr = int(a.func, 16)
        r = [x for x in rows if x[0] <= addr < x[0] + max(x[1], 1)][0]
    else:
        r = [x for x in rows if x[2] == a.func][0]
    print("# %s 0x%08x size %d" % (r[2], r[0], r[1]))
    lo, hi = r[0], r[0] + r[1]
    pat = re.compile(a.regex) if a.regex else None
    on = False
    with open(a.dis, errors="replace") as f:
        for ln in f:
            m = re.match(r"^\s*([0-9a-f]+):", ln)
            if not m:
                continue
            addr = int(m.group(1), 16)
            if addr >= hi:
                if on:
                    break
                continue
            if addr >= lo:
                on = True
                s = re.sub(r"^\s*([0-9a-f]+):\s*(?:[0-9a-f]{2} )+\s*", "\\1  ", ln.rstrip())
                if not pat or pat.search(s):
                    print(s)


if __name__ == "__main__":
    main()
