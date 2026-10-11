#!/usr/bin/env python3
"""Ship.specialFlags / specialFlags2 bit scan: the exe's bits against HW1's SPECIAL_* names.

M2-R4 command port, subsystem `flags`. The exe keeps Ship.specialFlags at +0x1C8 and specialFlags2 at +0x1CC
(docs/m2-r3-selection.md: `makeShipsControllable` reads `dockingship` at +0x1C4). This tool reads the exe's
disassembly (`objdump -d -M intel --no-show-raw-insn` of original/game/cataclysm.exe, or the R2 copy with raw bytes)
and lists, per exe function (functions.csv of the Ghidra export), every masked access to those words:
  - `test/or/and/xor <ptr> [r+0x1c8..0x1cb], imm` directly, byte offsets shifted to dword bits;
  - `mov reg, [r+0x1c8..]` followed (within 8 instructions, until reg is reloaded) by test/and/or/xor on reg, al/ah/ax
    or the matching sub-register with an immediate.
`and` with a mask whose complement has fewer bits is a clear, `or` a set, everything else a test.
Per HW1 function (GoK at the bootstrap commit) it lists every SPECIAL_* / SPECIAL_2_* name used, and joined by
function name it prints, per HW1 flag, the exe bits its functions touch. The offset is a heuristic (other structs
have members at +0x1C8), so every mapping in src/Game/SpaceObj.h cites functions that were read by hand.

  python3 -I tools/m2_specialflags_scan.py --dis EXE.dis --export DIR --hw1 GOK_SRC [--flags2]
         [--func NAME | --bit 0xNN | --summary]
"""
import argparse
import bisect
import collections
import csv
import os
import re

REGFAM = {
    "eax": ("eax", "ax", "al", "ah"), "ebx": ("ebx", "bx", "bl", "bh"), "ecx": ("ecx", "cx", "cl", "ch"),
    "edx": ("edx", "dx", "dl", "dh"), "esi": ("esi", "si"), "edi": ("edi", "di"), "ebp": ("ebp", "bp"),
}
SUBSHIFT = {"ah": 8, "bh": 8, "ch": 8, "dh": 8}
INSN = re.compile(r"^\s*([0-9a-f]+):\s*(?:(?:[0-9a-f]{2} )+\s*)?\t?(\S+)\s*(.*)$")


def load_functions(export):
    rows = []
    with open(os.path.join(export, "functions.csv"), newline="") as f:
        for r in csv.DictReader(f):
            rows.append((int(r["address"], 16), int(r["size"]), r["name"]))
    rows.sort()
    return rows


def func_of(rows, starts, addr):
    i = bisect.bisect_right(starts, addr) - 1
    if i >= 0 and addr < rows[i][0] + max(rows[i][1], 1):
        return rows[i]
    return None


def classify(op, imm, width):
    full = (1 << width) - 1
    imm &= full
    if op == "or":
        return "set", imm
    if op == "and":
        inv = (~imm) & full
        if bin(inv).count("1") < bin(imm).count("1"):
            return "clear", inv
        return "test", imm
    if op == "xor":
        return "toggle", imm
    return "test", imm


def scan_dis(path, base):
    """Yield (addr, kind, mask32, text)."""
    lines = []
    with open(path, errors="replace") as f:
        for ln in f:
            m = INSN.match(ln.rstrip("\n"))
            if m and m.group(2) not in ("(bad)",):
                lines.append((int(m.group(1), 16), m.group(2), m.group(3)))
    offre = re.compile(r"(BYTE|WORD|DWORD) PTR \[([a-z]+)\+0x(1c[89a-f])\]")
    out = []
    for i, (addr, mn, ops) in enumerate(lines):
        m = offre.search(ops)
        if not m:
            continue
        off = int(m.group(3), 16) - base
        if off < 0 or off > 3:
            continue
        width = {"BYTE": 8, "WORD": 16, "DWORD": 32}[m.group(1)]
        shift = 8 * off
        imm = re.search(r",(0x[0-9a-f]+|\d+)$", ops)
        if mn in ("test", "or", "and", "xor") and imm and ops.startswith(m.group(1)):
            kind, mask = classify(mn, int(imm.group(1), 0), width)
            out.append((addr, kind, (mask << shift) & 0xFFFFFFFF, mn + " " + ops))
            continue
        rsrc = re.search(r",([a-z]{2,3})$", ops)
        if mn in ("test", "or", "and", "xor") and rsrc and ops.startswith(m.group(1)):
            # register operand: find `mov reg, imm` in the 16 instructions before
            r = rsrc.group(1)
            fam = next((k for k, v in REGFAM.items() if r in v), None)
            for addr0, mn0, ops0 in reversed(lines[max(0, i - 16):i]):
                d0 = ops0.split(",")[0]
                if fam and d0 in REGFAM[fam]:
                    im0 = re.match(r"^[a-z]+,(0x[0-9a-f]+|\d+)$", ops0)
                    if mn0 == "mov" and im0:
                        kind, mask = classify(mn, int(im0.group(1), 0), width)
                        out.append((addr, kind, (mask << shift) & 0xFFFFFFFF, "%s %s ; %s=%s" % (mn, ops, r, im0.group(1))))
                    else:
                        out.append((addr, "dyn", 0, mn + " " + ops))
                    break
            continue
        if mn in ("mov", "movzx", "movsx") and not ops.startswith(m.group(1)):
            reg = ops.split(",")[0]
            fam = next((k for k, v in REGFAM.items() if reg in v), None)
            if not fam:
                continue
            held = {fam: shift}          # register family -> bit shift of the value it holds
            for addr2, mn2, ops2 in lines[i + 1:i + 12]:
                parts = [x.strip() for x in ops2.split(",")]
                dst = parts[0]
                dfam = next((k for k, v in REGFAM.items() if dst in v), None)
                src = parts[1] if len(parts) > 1 else ""
                sfam = next((k for k, v in REGFAM.items() if src in v), None)
                if mn2 == "mov" and dfam and sfam in held and src in ("eax", "ebx", "ecx", "edx", "esi", "edi"):
                    held[dfam] = held[sfam]
                    continue
                if dfam in held:
                    sh = held[dfam] + SUBSHIFT.get(dst, 0)
                    imm2 = re.match(r"^(0x[0-9a-f]+|\d+)$", src)
                    if mn2 in ("shr", "sar") and imm2:      # reg bit k = word bit k + n
                        held[dfam] += int(src, 0)
                        continue
                    if mn2 == "shl" and imm2:
                        held[dfam] -= int(src, 0)
                        continue
                    if mn2 == "not":
                        continue
                    if mn2 in ("test", "and", "or", "xor") and imm2:
                        w2 = 8 if len(dst) == 2 and dst[1] in "lh" else (16 if len(dst) == 2 else 32)
                        kind, mask = classify(mn2, int(src, 0), w2)
                        mask = (mask << sh) & 0xFFFFFFFF if sh >= 0 else mask >> -sh
                        out.append((addr2, kind, mask, "%s %s ; after %x" % (mn2, ops2, addr)))
                        if mn2 == "test":
                            continue
                        continue
                    if mn2 in ("and", "test") and sfam:
                        out.append((addr2, "dyn", 0, "%s %s ; after %x" % (mn2, ops2, addr)))
                        continue
                    if mn2 in ("mov", "movzx", "movsx", "lea", "pop", "xor", "sub", "add", "imul", "sete", "fnstsw",
                               "setne", "cmp") and mn2 != "cmp":
                        del held[dfam]
                        if not held:
                            break
                        continue
                if mn2 == "call" or mn2.startswith("ret"):
                    break
    return out


SPEC_RE = re.compile(r"\bSPECIAL_(2_)?[A-Za-z_0-9]+\b")


def split_c(path):
    text = open(path, encoding="utf-8", errors="replace").read()
    out = []
    for m in re.finditer(r"^(?:[A-Za-z_][\w\s\*]*?\s\**)?([A-Za-z_]\w*)\s*\(([^;{}]*)\)\s*\n?\{", text, re.M):
        name = m.group(1)
        if name in ("if", "while", "for", "switch", "return", "sizeof"):
            continue
        depth, i = 0, m.end() - 1
        while i < len(text):
            c = text[i]
            if c == "{":
                depth += 1
            elif c == "}":
                depth -= 1
                if depth == 0:
                    break
            i += 1
        out.append((name, text[m.end():i]))
    return out


def bits(mask):
    return [1 << b for b in range(32) if mask & (1 << b)]


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--dis", required=True)
    ap.add_argument("--export", required=True)
    ap.add_argument("--hw1", required=True, help="GoK src/ at the bootstrap commit (git archive 13723dd src)")
    ap.add_argument("--flags2", action="store_true", help="specialFlags2 (+0x1CC) instead of specialFlags (+0x1C8)")
    ap.add_argument("--func")
    ap.add_argument("--bit")
    ap.add_argument("--summary", action="store_true", help="per exe bit: kinds and functions")
    a = ap.parse_args()
    base = 0x1CC if a.flags2 else 0x1C8
    rows = load_functions(a.export)
    starts = [r[0] for r in rows]
    exe = collections.defaultdict(list)
    for addr, kind, mask, text in scan_dis(a.dis, base):
        fn = func_of(rows, starts, addr)
        exe[fn[2] if fn else "?%x" % addr].append((addr, kind, mask, text))
    hw1 = collections.defaultdict(set)
    for root, _, files in os.walk(a.hw1):
        for f in files:
            if f.endswith(".c"):
                for name, body in split_c(os.path.join(root, f)):
                    for m in SPEC_RE.finditer(body):
                        n = m.group(0)
                        if n.startswith("SPECIAL_2_") == a.flags2:
                            hw1[name].add(n)
    if a.func:
        for addr, kind, mask, text in exe.get(a.func, []):
            print("  %x %-6s 0x%08x  %s" % (addr, kind, mask, text))
        print("HW1:", " ".join(sorted(hw1.get(a.func, []))))
        return
    if a.bit or a.summary:
        want = [int(a.bit, 0)] if a.bit else sorted({b for acc in exe.values() for _, _, m, _ in acc for b in bits(m)})
        for bit in want:
            print("bit 0x%x" % bit)
            for name, acc in sorted(exe.items(), key=lambda x: x[1][0][0]):
                kinds = sorted({k for _, k, m, _ in acc if m & bit})
                if kinds:
                    print("  0x%08x %-48s %-14s HW1: %s" % (acc[0][0], name, ",".join(kinds),
                                                             " ".join(sorted(hw1.get(name, [])))))
        return
    flag_bits = collections.defaultdict(collections.Counter)
    flag_funcs = collections.defaultdict(list)
    for name, flags in hw1.items():
        if name not in exe:
            continue
        b = {x for _, _, m, _ in exe[name] for x in bits(m)}
        for fl in flags:
            flag_funcs[fl].append(name)
            for x in b:
                flag_bits[fl][x] += 1
    for fl in sorted(flag_funcs):
        top = ", ".join("0x%x:%d" % (b, c) for b, c in flag_bits[fl].most_common(6))
        print("%-58s funcs %3d  %s" % (fl, len(flag_funcs[fl]), top))
    missing = sorted({f for fs in hw1.values() for f in fs} - set(flag_funcs))
    print("\nHW1 flags with no matched exe function:", " ".join(missing))


if __name__ == "__main__":
    main()
