#!/usr/bin/env python3
"""Print exe functions' Ghidra decompilation from the export, by name or address, optionally only matching lines.

M2-R4 command port helper (comments of the export are stripped).

  python3 -I tools/m2_fdec.py NAME|0xADDR [NAME|0xADDR ...] [--grep REGEX] [--export DIR] [--max N]
"""
import argparse
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
    ap = argparse.ArgumentParser()
    ap.add_argument("funcs", nargs="+")
    ap.add_argument("--grep")
    ap.add_argument("--max", type=int, default=100000)
    ap.add_argument("--export", default=os.path.join(main_root(), "ghidra/exports/cataclysm_gog_r6"))
    a = ap.parse_args()
    text = open(os.path.join(a.export, "decompiled.c"), encoding="utf-8", errors="replace").read()
    want = set(a.funcs)
    pat = re.compile(a.grep) if a.grep else None
    for m in re.finditer(r"^//==== FUNCTION (0x[0-9a-f]+) (\S+) size=(\d+).*$", text, re.M):
        if m.group(2) not in want and m.group(1) not in want:
            continue
        end = text.find("//==== FUNCTION", m.end())
        body = re.sub(r"/\*.*?\*/", "", text[m.end():end if end > 0 else len(text)], flags=re.S)
        print(m.group(0))
        lines = [l for l in body.split("\n") if l.strip()]
        if pat:
            lines = [l for l in lines if pat.search(l)]
        for l in lines[:a.max]:
            print(l)


if __name__ == "__main__":
    main()
