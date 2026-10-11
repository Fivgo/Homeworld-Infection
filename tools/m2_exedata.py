#!/usr/bin/env python3
"""Read cataclysm.exe data by virtual address: dwords, C strings, or a NumStrXlate table.

M2-R4 command port helper (no Ghidra needed): parses the PE section table of original/game/cataclysm.exe.

  python3 -I tools/m2_exedata.py dwords 0xVA N
  python3 -I tools/m2_exedata.py str 0xVA
  python3 -I tools/m2_exedata.py numstr 0xVA      # {udword num; char *str} pairs until str == NULL
"""
import os
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


class PE:
    def __init__(self, path):
        self.d = open(path, "rb").read()
        pe = struct.unpack_from("<I", self.d, 0x3C)[0]
        nsec = struct.unpack_from("<H", self.d, pe + 6)[0]
        optsz = struct.unpack_from("<H", self.d, pe + 20)[0]
        self.base = struct.unpack_from("<I", self.d, pe + 24 + 28)[0]
        self.secs = []
        off = pe + 24 + optsz
        for i in range(nsec):
            vsize, va, rsize, raw = struct.unpack_from("<IIII", self.d, off + 8)
            self.secs.append((self.base + va, max(vsize, rsize), raw, rsize))
            off += 40

    def off(self, va):
        for sva, size, raw, rsize in self.secs:
            if sva <= va < sva + size:
                if va - sva >= rsize:
                    return None
                return raw + va - sva
        return None

    def dword(self, va):
        o = self.off(va)
        return None if o is None else struct.unpack_from("<I", self.d, o)[0]

    def cstr(self, va):
        o = self.off(va)
        if o is None:
            return None
        e = self.d.index(b"\0", o)
        return self.d[o:e].decode("latin-1")


def main():
    pe = PE(os.path.join(main_root(), "original/game/cataclysm.exe"))
    cmd, va = sys.argv[1], int(sys.argv[2], 16)
    if cmd == "dwords":
        for i in range(int(sys.argv[3])):
            print("0x%08x: 0x%08x" % (va + 4 * i, pe.dword(va + 4 * i)))
    elif cmd == "str":
        print(pe.cstr(va))
    elif cmd == "numstr":
        while True:
            n, s = pe.dword(va), pe.dword(va + 4)
            if not s:
                break
            print("%d\t%s" % (n, pe.cstr(s)))
            va += 8


if __name__ == "__main__":
    main()
