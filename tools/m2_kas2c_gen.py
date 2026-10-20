#!/usr/bin/env python3
"""Regenerate Cataclysm's kas2c mission code (the generated C of the KAS missions) from cataclysm.exe.

M2-R8 kas-runtime-mission01 (docs/m2-r8-kas-runtime-mission01.md, docs/mission-code-map.md sections 4, 7 and 8). No
Ghidra run: it reads the named Ghidra export's decompile of the 2,085 mission functions (cataclysm_gog_r6
decompiled.c / functions.csv, the [R2]-created functions named after their FSM / state contexts) for each body's
structure, the exe itself (PE sections) for every string, localised string table and the kas2c function table
(cat_kasFunctionTable 0x00755048 with its dispatcher, through tools/m2_kasf_table_gen.py), and `objdump -d` of the exe
to VERIFY every generated body against the machine code: the multiset of called functions (calls and tail jumps,
thunks resolved to the kasf the exe calls) and the set of 32-bit constants (pushed arguments, compared message hashes,
string and table addresses) must be equal, or the generator stops.

The decompile is read with these rules (the Cataclysm kas2c dialect, mission-code-map 4):
  * the scope prologue's globals get their names (CurrentMissionScope 0x00c3749c, CurrentMissionScopeName 0x00c374a0,
    CurrentTeamP 0x00a43ee8 and its KAS fields: +0x58 kasLabel, +0x1C shipList, +0x20 shipList.selection, +0x50
    msgQueue);
  * every kasf call is rebuilt from the kas2c table's parameter list: a UDWORD is a name hash (a number, whatever
    Ghidra took it for: a string label, &DAT_x, a pointer into aisTeams), a CHARPTR a string read from the exe, a
    localised string (&PTR_x)[strCurLanguage] one of LSTRING_<addr>[3] read from the exe; arguments Ghidra invented
    (this, extraout_ECX) are dropped by the table's arity; a call to a kasf's thunk target is the kasf;
  * locals that hold hashes, strings or counts are uintptr_t (a hash and a string pointer share a register in the
    exe); typed pointers keep their types;
  * names: the functions keep the export's kas2c names (Init_/Watch_<Script>_<FSM>_<State>, docs/mission-code-map.md
    8; a function shared by more than one script (MSVC /OPT:ICF) keeps its home name and goes to KasShared.c).
A body the rules cannot read is fixed by an entry in OVERRIDES below (a text patch of the decompile, applied before
the rules); each one is marked in the output with "HAND-EDIT (tools/m2_kas2c_gen.py OVERRIDES)" and must still pass
the verification.

Outputs (REULA headers, GENERATED): src/Missions/SinglePlayer/<MissionNN>.c, src/Missions/Tutorials/<Tutorial1>.c,
src/Missions/KasShared.c (the shared functions the generated scripts use) and src/Missions/CatMissions.h (prototypes,
the per-script function lists with each function's exe address, used by the saves: kasConvertFuncPtrToOffset).

  python3 -I tools/m2_kas2c_gen.py [--scripts Mission01,Tutorial1,Mission02,..] [--check] [--report] [--exe PATH] [--export DIR]
    (the default --scripts is DEFAULT_SCRIPTS below: every generated script)
    --check   compare with the tracked files, exit 1 on a difference (nothing written)
    --report  print the per-function verification table
"""
import argparse
import collections
import os
import re
import subprocess
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import m2_kasf_table_gen as T  # noqa: E402

HERE = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
ROOT = T.main_root()
MISSION_LO, MISSION_HI = 0x005d6fe0, 0x00670a10
ALL_SCRIPTS = ["Mission%02d" % i for i in range(1, 18)] + ["Tutorial1"]
# the generated scripts (M2-R8: Mission01, Tutorial1; M2-R9 part B: Mission02 .. Mission09 (producer 2), Mission10 ..
# Mission17 (producer 3)): every kas2c script of the exe; a plain run / --check covers every generated file
DEFAULT_SCRIPTS = ["Mission01", "Tutorial1"] + ["Mission%02d" % i for i in range(2, 18)]

# the KAS runtime the generated code calls besides the kasf (docs/mission-code-map.md 3): name -> (address, return
# type, parameter types); a parameter type is a kas2c type name or a C type
RUNTIME = {
    "kasJump": (0x00490640, "void", ["UDWORD", "KASInitFunction", "KASWatchFunction"]),
    "kasFSMCreate": (0x00490660, "void", ["UDWORD", "KASInitFunction", "KASWatchFunction", "AITEAMPTR"]),
    "kasAITeamPtr": (0x004907a0, "AITEAMPTR", ["UDWORD"]),
    "kasAITeamShipsPtr": (0x004907e0, "GROWSELECTIONPTR", ["UDWORD"]),
    "kasShipsVectorPtr": (0x00490820, "VECTORPTR", ["UDWORD"]),
    "kasTeamsVectorPtr": (0x004908c0, "VECTORPTR", ["UDWORD"]),
    "kasVolumeVectorPtr": (0x00490960, "VECTORPTR", ["UDWORD"]),
    "kasThisTeamsVectorPtr": (0x004909f0, "VECTORPTR", []),
    "kasPathPtr": (0x00490aa0, "PATHPTR", ["UDWORD"]),
    "kasVolumePtr": (0x00490af0, "VOLUMEPTR", ["UDWORD"]),
    "kasVectorPtr": (0x00490b30, "VECTORPTR", ["UDWORD"]),
    "kasGetGrowSelectionPtrIfExists": (0x00490b70, "GROWSELECTIONPTR", ["UDWORD"]),
    "kasGrowSelectionPtr": (0x00490bb0, "GROWSELECTIONPTR", ["UDWORD"]),
    "cat_aitMsgQueueRemoveAt": (0x00417ec0, "UDWORD", ["AITEAMPTR", "UDWORD"]),
    "aitMsgSend": (0x00417e10, "void", ["AITEAMPTR", "AITEAMPTR", "UDWORD"]),
}
CT = dict(T.CTYPES)
CT.update({"KASInitFunction": "KASInitFunction", "KASWatchFunction": "KASWatchFunction", "void": "void"})

# Ghidra local types -> (C type, kind); kind "word" = a hash / string / count register
LOCAL_TYPES = {
    "sdword": "sdword", "udword": "word", "uint": "word", "int": "word", "undefined4": "word", "char *": "word",
    "code *": "word", "void *": "word", "bool": "word", "byte": "word", "ubyte *": "word", "ubyte": "word",
    "GrowSelection *": "GrowSelection *", "AITeam *": "AITeam *", "Volume *": "Volume *", "hvector *": "hvector *",
    "Path *": "Path *", "Ship *": "Ship *", "ShipPtr": "Ship *",
}
DROP_LOCAL = re.compile(r"^(this|this_[0-9a-f]+|extraout_\w+|in_\w+|unaff_\w+)$")

# a kasf that is a jmp thunk to a short engine function: Ghidra decompiles the call as the target's body; the rule puts
# the kasf call back (the exe calls the thunk)
INLINED_THUNKS = [
    (r"\*\(undefined4 \*\)\(DAT_00a43ee8 \+ 0x78\) = (\d+);", None),   # placeholder, see FORMATION below
    (r"DAT_00a6c2c4 = 0;", "kasfTutResetGameMessageQueue();"),    # kasfTutResetGameMessageQueue 0x00496760
    (r"speechEventQueue\(\(void \*\)0x0,0x2001028,-1,-1,0,0,-1,5\.0,-1\);\s*univKillPlayer\(0,2,-1\);\s*"
     r"CheckPlayerWin\(\);", "kasfMissionFailed();"),      # kasfMissionFailed 0x00491cb0 -> singlePlayerMissionFailedCB
]

# text patches for bodies the rules cannot read: address -> [(old, new, why)]; each is marked in the output
OVERRIDES = {
    # Watch_Mission05_FleetIntel_WaitMessage: Ghidra took the message hash 0x0b7f8e9 (cmp / mov at 0x005eb...) for an
    # address inside the universe global
    0x005eb0b0: [("msg = universe.players[3].cat_unknown_02EC + 0x516d;", "msg = (ubyte *)0xb7f8e9;",
                  "the message hash 0xb7f8e9 Ghidra printed as universe.players[3].cat_unknown_02EC + 0x516d")],
    # Watch_Mission05_Objectives: the same hash in the message switch's compare (cmp eax,0xb7f8e9 at 0x005ebdab)
    0x005ebd50: [("if ((ubyte *)pcVar1 != universe.players[3].cat_unknown_02EC + 0x516d) goto LAB_005ebf6a;",
                  "if (pcVar1 != (char *)0xb7f8e9) goto LAB_005ebf6a;",
                  "the message hash 0xb7f8e9 (cmp at 0x005ebdab) Ghidra printed as universe.players[3]."
                  "cat_unknown_02EC + 0x516d")],
    # Watch_Mission08_Frigates_Guard: the script's test is always false ((ShipsOrderAttributes == 0) & 2: neg / sbb /
    # inc / test al,2 at 0x00604cb4 .. 0x00604cbb), so Ghidra dropped the kasJump it guards (0x00604cbd .. 0x00604ccc)
    0x00604c90: [("  kasfShipsOrderAttributes((GrowSelection *)(DAT_00a43ee8 + 0x1c));\n",
                  "  if ((kasfShipsOrderAttributes((GrowSelection *)(DAT_00a43ee8 + 0x1c)) == 0) & 2) {\n"
                  "    kasJump((char *)0x4c9664,Init_Mission08_Frigates_Guard,Watch_Mission08_Frigates_Guard);\n"
                  "  }\n",
                  "the always-false test (== 0) & 2 and the kasJump(Guard) it guards, which Ghidra dropped as dead "
                  "code (0x00604cb4 .. 0x00604ccc)")],
    # Watch_Mission09_Resourcers_Nothing: the ICF-folded empty stub 0x0049ae30 called with one argument, the
    # comparison ShipsCount <= 3 (setle dl; push edx; call 0x0049ae30 at 0x006072bf): kasfAssert (dbgAssert) is the only
    # one-argument kasf on the stub that takes a truth value (kasfLog, kasfTeamSkillSet, kasfSetPlayerResourceCap)
    0x006071e0: [("  kasfShipsCount(pGVar1);\n  nullsub();\n", "  kasfAssert(kasfShipsCount(pGVar1) <= 3);\n",
                  "Ghidra's nullsub() is kasfAssert(ShipsCount <= 3) on the folded stub 0x0049ae30 (0x006072b5 .. "
                  "0x006072c3)")],
}


def _assert_stub(indent, expr, cmp, at):
    """An OVERRIDES entry for Ghidra's nullsub() that is kasfAssert(expr cmp) on the folded stub 0x0049ae30 (as
    0x006071e0 above): the asm computes the truth value (cmp eax,K / setle, or sub eax,K / neg / sbb / inc for == K),
    pushes it and calls the stub at `at`."""
    return ("%s%s;\n%snullsub();" % (indent, expr, indent), "%skasfAssert(%s %s);" % (indent, expr, cmp),
            "Ghidra's nullsub() is kasfAssert(%s %s) on the folded stub 0x0049ae30 (call at 0x%08x)" % (expr, cmp, at))


# M2-R9 part B (producer 3): Mission10 .. Mission17. The Sentinels FSMs (Mission11 / 14 / 16 / 17, the same script
# compiled four times) assert their team / build counts against 12 (cmp eax,0xc; setle); Mission13's SupportWave2
# asserts TeamCount == 3 (sub eax,3; neg; sbb; inc)
for _a, _sites in {
        0x006167b0: [("      ", "kasfVarValueGet((char *)0x6a6c3c4)", 0x0061686d), ("      ", "kasfTeamCount()", 0x00616ac8)],
        0x00616130: [("  ", "kasfVarValueGet((char *)0x9379e54)", 0x0061615c)],
        0x006161f0: [("    ", "kasfTeamCount()", 0x0061631c)],
        0x006309e0: [("      ", "kasfVarValueGet((char *)0x6a6c3c4)", 0x00630a9d), ("      ", "kasfTeamCount()", 0x00630cf8)],
        0x00630420: [("    ", "kasfTeamCount()", 0x0063054c)],
        0x0063e920: [("      ", "kasfVarValueGet((char *)0x6a6c3c4)", 0x0063e9dd), ("      ", "kasfTeamCount()", 0x0063ec38)],
        0x0063e360: [("    ", "kasfTeamCount()", 0x0063e48c)],
        0x0064dd60: [("      ", "kasfVarValueGet((char *)0x6a6c3c4)", 0x0064de7e), ("      ", "kasfTeamCount()", 0x0064e0d9)],
        0x0064d710: [("    ", "kasfTeamCount()", 0x0064d83c)]}.items():
    OVERRIDES[_a] = [_assert_stub(i, e, "<= 12", at) for i, e, at in _sites]
OVERRIDES[0x006295b0] = [_assert_stub("  ", "kasfTeamCount()", "== 3", 0x006295d6)]
# Init_Mission14_Wave1_WaitCreate: Ghidra typed the scope prologue's kasLabel load as void * and passed the
# CurrentMissionScopeName register (ECX at 0x0062dff7) as a this-argument of kasfGetBuildTime, which takes one CHARPTR
# (push 0x88b464 / call 0x00499b80 at 0x0062dff2 .. 0x0062dffd)
OVERRIDES[0x0062dfe0] = [
    ("DAT_00c374a0 = *(void **)(DAT_00a43ee8 + 0x58);", "DAT_00c374a0 = *(undefined4 *)(DAT_00a43ee8 + 0x58);",
     "the scope prologue's kasLabel load Ghidra typed void *"),
    ("kasfGetBuildTime(DAT_00c374a0,s_tMultiGunCorvette_0088b464)", "kasfGetBuildTime(s_tMultiGunCorvette_0088b464)",
     "the ECX register (CurrentMissionScopeName) Ghidra passed as a this-argument of the one-parameter "
     "kasfGetBuildTime (0x0062dff2 .. 0x0062dffd)")]

TEAM = "DAT_00a43ee8"
# a message / name hash Ghidra took for a pointer into aisTeams (the skirmish AI's team table, 0x008dfd60: AISTeamEntry
# 0x9D10 bytes = 10 AISTeamSelection of 0xFB4 (MaxSelection 0xFA8 + a vector) + 8; sel.ShipPtr at +8): the hash is the
# element's address plus the byte offset Ghidra wrote (export datatypes.h)
AIS_ELEM = r"aisTeams\[(0x[0-9a-f]+|\d+)\]\.selection\[(0x[0-9a-f]+|\d+)\]\.sel\.ShipPtr"
AIS_ENTRY, AIS_SELECTION, AIS_SHIPPTR = 0x9D10, 0xFB4, 8
# MSVC's signed division by a constant d: the multiplier of the imul (Hacker's Delight 10-1); the C "/ d" stands for it
def div_magic(d):
    two31 = 1 << 31
    t = two31 + (0 if d > 0 else 1)
    anc = t - 1 - t % abs(d)
    p, q1, r1, q2, r2 = 31, two31 // anc, two31 - (two31 // anc) * anc, two31 // abs(d), two31 - (two31 // abs(d)) * abs(d)
    while True:
        p += 1
        q1, r1 = 2 * q1, 2 * r1
        if r1 >= anc:
            q1, r1 = q1 + 1, r1 - anc
        q2, r2 = 2 * q2, 2 * r2
        if r2 >= abs(d):
            q2, r2 = q2 + 1, r2 - abs(d)
        delta = abs(d) - r2
        if not (q1 < delta or (q1 == delta and r1 == 0)):
            break
    return (q2 + 1) & 0xFFFFFFFF


GHIDRA_ESC = {"n": 10, "t": 9, "r": 13, "\\": 92, '"': 34, "'": 39, "0": 0, "a": 7, "b": 8, "f": 12, "v": 11}


def ghidra_bytes(lit):
    """The bytes of a Ghidra string literal's text (without the quotes)."""
    out, i = bytearray(), 0
    while i < len(lit):
        ch = lit[i]
        if ch == "\\":
            nx = lit[i + 1]
            if nx == "x":
                out.append(int(lit[i + 2:i + 4], 16))
                i += 4
                continue
            out.append(GHIDRA_ESC[nx])
            i += 2
            continue
        out += ch.encode("latin-1")
        i += 1
    return bytes(out)


def run(cmd):
    return subprocess.check_output(cmd, text=True)


class Exe:
    def __init__(self, path):
        self.pe = T.PE(path)

    def u32(self, va):
        return int.from_bytes(self.pe.read(va, 4), "little")

    def cstring(self, va):
        out = bytearray()
        while True:
            b = self.pe.read(va + len(out), 1)
            if b == b"\0":
                return bytes(out)
            out += b


def c_literal(b):
    s = []
    for ch in b:
        if ch == 0x5C:
            s.append("\\\\")
        elif ch == 0x22:
            s.append('\\"')
        elif 0x20 <= ch < 0x7F:
            s.append(chr(ch))
        elif ch == 0x0A:
            s.append("\\n")
        elif ch == 0x09:
            s.append("\\t")
        elif ch == 0x0D:
            s.append("\\r")
        else:
            s.append("\\%03o" % ch)
    return '"' + "".join(s) + '"'


def load_export(export):
    funcs = {}
    import csv
    with open(os.path.join(export, "functions.csv"), newline="") as f:
        for r in csv.DictReader(f):
            a = int(r["address"], 16)
            funcs[a] = dict(name=r["name"], size=int(r["size"]), thunk=r["is_thunk"] == "1",
                            target=r["thunk_target"], body=None)
    txt = open(os.path.join(export, "decompiled.c"), encoding="utf-8", errors="replace").read()
    for m in re.finditer(r"^//==== FUNCTION (0x[0-9a-f]+) (\S+) size=(\d+).*$", txt, re.M):
        a = int(m.group(1), 16)
        end = txt.find("//==== FUNCTION", m.end())
        if a in funcs:
            funcs[a]["body"] = txt[m.end():end if end > 0 else len(txt)]
    syms = {}
    with open(os.path.join(export, "symbols.csv"), newline="") as f:
        for r in csv.DictReader(f):
            syms.setdefault(r["name"], int(r["address"], 16))
    return funcs, syms


def disassemble(exe, lo, hi):
    out = run(["objdump", "-d", "-M", "intel", "--no-show-raw-insn", "--start-address=0x%x" % lo,
               "--stop-address=0x%x" % hi, exe])
    ins = []
    for line in out.splitlines():
        m = re.match(r"\s+([0-9a-f]+):\s+(.*)$", line)
        if m:
            ins.append((int(m.group(1), 16), m.group(2).strip()))
    return ins


def split_args(s):
    """Splits a call's argument text at depth-0 commas."""
    out, depth, cur = [], 0, []
    for ch in s:
        if ch in "([":
            depth += 1
        elif ch in ")]":
            depth -= 1
        if ch == "," and depth == 0:
            out.append("".join(cur).strip())
            cur = []
        else:
            cur.append(ch)
    if "".join(cur).strip():
        out.append("".join(cur).strip())
    return out


def match_paren(s, i):
    """s[i] == '(' -> index of the matching ')'."""
    depth = 0
    for j in range(i, len(s)):
        if s[j] == "(":
            depth += 1
        elif s[j] == ")":
            depth -= 1
            if depth == 0:
                return j
    raise ValueError("unbalanced")


CAST = re.compile(r"^\(\s*(?:const\s+)?[A-Za-z_]\w*(?:\s*\*+)?\s*\)\s*")


def strip_casts(e):
    e = e.strip()
    while True:
        while e.startswith("(") and match_paren(e, 0) == len(e) - 1 and not CAST.match(e):
            e = e[1:-1].strip()
        m = CAST.match(e)
        if not m or m.end() >= len(e):
            return e
        e = e[m.end():].strip()


class Gen:
    def __init__(self, a):
        self.a = a
        self.exe = Exe(a.exe)
        self.funcs, self.syms = load_export(a.export)
        rows = T.read_table(self.exe.pe)
        dins, dorder = T.disassemble(a.exe)
        self.kasf = {}
        for r in rows:
            if r["name"] == "*":
                continue
            addr = T.walk(dins, dorder, T.kas_hash(r["name"]))
            self.kasf.setdefault(r["real"], dict(addr=addr, params=[t for n, t in r["params"]],
                                                 ret="SDWORD" if r["returns"] else "void"))
        self.sig = {k: dict(addr=v[0], ret=v[1], params=v[2]) for k, v in RUNTIME.items()}
        self.sig.update(self.kasf)
        # a call Ghidra shows by the thunk's target is the kasf (the exe calls the kasf's jmp thunk)
        self.thunk_of = {}
        by_addr = {v["addr"]: k for k, v in self.kasf.items() if v["addr"]}
        for addr, f in self.funcs.items():
            if addr in by_addr and f["target"]:
                self.thunk_of.setdefault(f["target"], by_addr[addr])
        self.addr_of = {v["addr"]: k for k, v in self.sig.items()}
        self.mnames = {f["name"]: a2 for a2, f in self.funcs.items() if MISSION_LO <= a2 < MISSION_HI}
        self.lstrings = {}
        self.tuples_checked = 0
        # the formation kasf (each pushes its formation and stores it at team +0x78): formation -> kasf, for Ghidra's
        # inlined tail jumps
        self.formation_kasf = {}
        for k, v in self.kasf.items():
            if k.startswith("kasfFormation") and v["addr"] and not v["params"]:
                for ad, s2 in disassemble(a.exe, v["addr"], v["addr"] + 0x30):
                    mm = re.match(r"mov\s+DWORD PTR \[\w+\+0x78\],0x([0-9a-f]+)$", s2)
                    if mm:
                        self.formation_kasf[int(mm.group(1), 16)] = k
                        break
                    if s2.startswith("ret"):
                        break
        self.ais_base = self.syms["aisTeams"]
        self.switchdata = {v for k, v in self.syms.items() if k.startswith("switchdataD_")}

    # ---------------------------------------------------------------- closure
    def refs(self, addr):
        return [self.mnames[n] for n in re.findall(r"\b((?:Init|Watch)_\w+)\b", self.funcs[addr]["body"] or "")
                if n in self.mnames]

    def closure(self, script):
        seen, st = [], [self.mnames["Init_" + script], self.mnames["Watch_" + script]]
        while st:
            x = st.pop(0)
            if x in seen:
                continue
            seen.append(x)
            st.extend(self.refs(x))
        return seen

    # ---------------------------------------------------------------- asm facts
    def asm_facts(self, addr):
        """(callees multiset, constant set) of the function's machine code."""
        size = self.funcs[addr]["size"]
        ins = disassemble(self.a.exe, addr, addr + size)
        end = addr + size
        for ad, s in ins:          # an MSVC switch's jump table follows the code: stop decoding there
            m = re.match(r"jmp\s+DWORD PTR \[\w+\*4\+0x([0-9a-f]+)\]", s)
            if m and addr < int(m.group(1), 16) < end:
                end = int(m.group(1), 16)
        calls, consts = collections.Counter(), set()
        self._aorder, self._acmp = [], set()
        acc, deltas = {}, set()     # an MSVC compare chain: mov ecx,eax; sub ecx,K1; je; sub ecx,K2; je; dec ecx ...
        for ad, s in ins:
            if ad >= end:
                break
            m = re.match(r"mov\s+(\w+),eax$", s)
            if m:
                acc[m.group(1)] = 0
            m = re.match(r"sub\s+(\w+),0x([0-9a-f]+)$", s)
            if m and m.group(1) in acc:
                k = int(m.group(2), 16)
                if acc[m.group(1)]:
                    deltas.add(k)
                acc[m.group(1)] += k
                for d in range(0, 9):
                    consts.add((acc[m.group(1)] + d) & 0xFFFFFFFF)
            m = re.match(r"dec\s+(\w+)$", s)
            if m and m.group(1) in acc:
                acc[m.group(1)] += 1
            m = re.match(r"(call|jmp)\s+0x([0-9a-f]+)$", s)
            if m:
                t = int(m.group(2), 16)
                if m.group(1) == "call" or not (addr <= t < addr + size):
                    if not (MISSION_LO <= t < MISSION_HI) or m.group(1) == "call":
                        calls[t] += 1
            for k in re.findall(r"0x([0-9a-f]+)", s if not s.startswith(("call", "j")) else ""):
                v = int(k, 16)
                if 0x10000 <= v < 0xFFFF0000 and not s.startswith(("add    esp", "sub    esp")):
                    consts.add(v)
                    self._aorder.append(v)
                    if s.startswith("cmp"):
                        self._acmp.add(v)
        consts -= deltas
        # the switch tables after the code (jmp [reg*4+T], mov al,[reg+T]) are the function's own data
        consts = {v for v in consts if not (addr < v < addr + size and v not in self.funcs) and v not in self.switchdata}
        # the argument tuples of the calls whose arguments are all pushed constants in the call's own basic block
        targets = set()
        for ad, s in ins:
            m = re.match(r"j\w+\s+0x([0-9a-f]+)$", s)
            if m:
                targets.add(int(m.group(1), 16))
        tuples, pend = collections.Counter(), []
        for ad, s in ins:
            if ad >= end:
                break
            if ad in targets:
                pend = []
            m = re.match(r"push\s+(0x[0-9a-f]+)$", s)
            if m:
                pend.append(int(m.group(1), 16))
                continue
            if s.startswith("push"):
                pend.append(None)
                continue
            m = re.match(r"(call|jmp)\s+0x([0-9a-f]+)$", s)
            if m and int(m.group(2), 16) in self.addr_of:
                t = int(m.group(2), 16)
                n = len(self.sig[self.addr_of[t]]["params"])
                if n and len(pend) >= n and None not in pend[len(pend) - n:]:
                    tuples[(t,) + tuple(reversed(pend[len(pend) - n:]))] += 1
                if n:
                    pend = pend[:len(pend) - n] if len(pend) >= n else []
            elif s.startswith(("call", "j", "ret")):
                pend = []
        self._tuples = tuples
        consts.discard(0xc3749c)
        consts.discard(0xc374a0)
        consts.discard(0xa43ee8)
        consts.discard(0xa6b00c)
        return calls, consts

    # ---------------------------------------------------------------- rules
    def lstring(self, addr):
        if addr not in self.lstrings:
            self.lstrings[addr] = [self.exe.cstring(self.exe.u32(addr + 4 * k)) for k in range(3)]
        return "LSTRING_%08x[strCurLanguage]" % addr

    def string(self, addr, consts):
        consts.add(addr)
        lit = c_literal(self.exe.cstring(addr))
        self._strs.setdefault(lit, addr)
        return lit

    def sym_addr(self, e):
        m = re.match(r"^(?:&?(?:DAT|BYTE|s_\w+|lp\w+|lParam|PTR_\w+|u_\w+)|&(?:LAB|FUN))_([0-9a-f]{8})$", e)
        return int(m.group(1), 16) if m else None

    def value(self, e, ptype, consts, words):
        """Rewrites one argument / operand e for a parameter of kas2c type ptype (None: a plain value)."""
        e0 = e
        e = strip_casts(e)
        m = re.match(r"^\(&PTR_\w+?_([0-9a-f]{8})\)\[strCurLanguage\]$", e)
        if m:
            consts.add(int(m.group(1), 16))
            return "(char *)" + self.lstring(int(m.group(1), 16))
        m = re.match(r"^GARBLE\((-?\d+)\)$", e)
        if m:
            v = int(m.group(1)) & 0xFFFFFFFF
            consts.add(v)
            return "0x%x" % v
        m = re.match(r"^RAWSTR_(\d+)$", e)
        if m:
            v = self.rawstr(int(m.group(1)))
            if ptype == "CHARPTR":
                return self.string(v, consts)
            consts.add(v)
            return "0x%x" % v
        if e == TEAM:
            return "CurrentTeamP"
        a2 = self.sym_addr(e)
        if a2 is None and ptype == "UDWORD" and e.lstrip("&") in self.syms:
            a2 = self.syms[e.lstrip("&")]      # a hash that is also a code or data address Ghidra named
        if a2 is not None:
            if ptype == "CHARPTR" or (ptype is None and e.startswith("s_")):
                return self.string(a2, consts)
            consts.add(a2)
            return "0x%x" % a2
        if re.match(r"^-?(0x[0-9a-f]+|\d+)$", e):
            v = int(e, 0) & 0xFFFFFFFF
            if 0x10000 <= v < 0xFFFF0000:
                consts.add(v)
            return e
        if e in self.mnames:
            consts.add(self.mnames[e])
            return e
        if e == TEAM:
            return "CurrentTeamP"
        if e == "0x0" or e == "0":
            return "0"
        # an expression: keep it (the rules below already renamed the globals); cast to the parameter type
        out = e0.strip()
        out = CAST.sub("", out) if CAST.match(out) and ptype is not None else out
        if ptype is not None and ptype in CT:
            ctype = CT[ptype]
            if words and re.match(r"^\w+$", out) and out in words and words[out].replace("struct ", "") == \
                    ctype.replace("struct ", ""):
                return out
            return "(%s)(%s)" % (ctype, out) if not re.match(r"^\w+$", out) else "(%s)%s" % (ctype, out)
        return out

    @staticmethod
    def phantom_arg(x):
        return bool(re.match(r"^(this\w*|extraout_\w+|in_ECX|pvVar\d+)$", x.strip()))

    def rewrite_calls(self, s, consts, calls, words):
        out, i = [], 0
        for m in re.finditer(r"\b([A-Za-z_]\w*)\s*\(", s):
            pass
        while True:
            m = re.compile(r"\b([A-Za-z_]\w*)\s*\(").search(s, i)
            if not m:
                out.append(s[i:])
                break
            name = m.group(1)
            name = self.thunk_of.get(name, name)
            if name not in self.sig:
                out.append(s[i:m.end()])
                i = m.end()
                continue
            j = match_paren(s, m.end() - 1)
            args = split_args(s[m.end():j])
            args = [self.rewrite_calls(x, consts, calls, words) for x in args]
            ps = self.sig[name]["params"]
            while len(args) > len(ps) and self.phantom_arg(args[0]):
                args = args[1:]
            if len(args) > len(ps):
                args = args[len(args) - len(ps):]        # Ghidra's invented this / extraout_ECX
            if len(args) != len(ps):
                raise ValueError("%s: %d arguments for %d parameters: %s" % (name, len(args), len(ps), s[m.start():j + 1]))
            raw = [re.match(r"^RAWSTR_(\d+)$", strip_casts(x)) for x in args]
            args = [x if r else self.value(x, p, consts, words) for x, p, r in zip(args, ps, raw)]
            calls[self.sig[name]["addr"]] += 1

            def lits(args):
                lit = []
                for x in args:
                    mm = re.match(r"^(-?)(0x[0-9a-f]+|\d+)$", x)
                    if mm:
                        lit.append((-int(mm.group(2), 0) if mm.group(1) else int(mm.group(2), 0)) & 0xFFFFFFFF)
                    elif x in self.mnames:
                        lit.append(self.mnames[x])
                    elif x.startswith('"') and x in self._strs:
                        lit.append(self._strs[x])
                    else:
                        lit.append(None)
                return lit
            for k, r in enumerate(raw):
                if r:
                    v = self.rawstr(int(r.group(1)), (self.sig[name]["addr"], k, lits(args)))
                    if ps[k] == "CHARPTR":
                        args[k] = self.string(v, consts)
                    else:
                        consts.add(v)
                        args[k] = "0x%x" % v
            lit = []
            for x in args:
                mm = re.match(r"^(-?)(0x[0-9a-f]+|\d+)$", x)
                if mm:
                    lit.append((-int(mm.group(2), 0) if mm.group(1) else int(mm.group(2), 0)) & 0xFFFFFFFF)
                elif x in self.mnames:
                    lit.append(self.mnames[x])
                elif x.startswith('"') and x in self._strs:
                    lit.append(self._strs[x])
                else:
                    lit.append(None)
            self._ctuples[(self.sig[name]["addr"],) + tuple(lit)] += 1
            out.append(s[i:m.start()] + "%s(%s)" % (name, ", ".join(args)))
            i = j + 1
        return "".join(out)

    def cstr(self, v):
        try:
            return self.exe.cstring(v)
        except Exception:
            return None

    def rawstr(self, i, ctx=None):
        """A Ghidra string literal with no symbol (a hash whose value happens to point at string-like bytes): the
        machine code's constant whose bytes are the literal's; several candidates are told apart by the call's
        all-constant argument tuple (ctx = (callee address, position, the other arguments' values))."""
        lit = self._raw[i]
        if i in self._rawpos:
            return self._rawpos[i]
        if i in self._rawlt:     # a switch tree's "<" bound: the compared case value + 1 (cmp eax,K; ja)
            cands = sorted({v + 1 for v in self._acmp if self.cstr(v + 1) == lit})
        else:     # a value the decompile also shows as a number is another use, unless it is the only one
            cands = sorted(v for v in self._aconsts - self._explicit if self.cstr(v) == lit) or \
                sorted(v for v in self._aconsts if self.cstr(v) == lit)
        if len(cands) > 1 and ctx:
            callee, pos, known = ctx
            sel = set()
            for t in self._atuples:
                if t[0] == callee and len(t) == len(known) + 1 and \
                        all(k is None or k == t[1 + j] for j, k in enumerate(known) if j != pos):
                    sel.add(t[1 + pos])
            c2 = [v for v in cands if v in sel]
            if len(c2) == 1:
                cands = c2
        if len(cands) != 1:
            raise ValueError("raw literal %r: %d candidates %s" % (lit, len(cands), [hex(v) for v in cands]))
        return cands[0]

    def transform(self, addr, script):
        f = self.funcs[addr]
        body = f["body"]
        for old, new, why in OVERRIDES.get(addr, []):
            if body.count(old) != 1:
                raise ValueError("override for 0x%08x does not apply: %r" % (addr, old))
            body = body.replace(old, new)
        body = re.sub(r"/\*.*?\*/", "", body, flags=re.S)
        # a localised table's index Ghidra wrapped onto the next line ((&PTR_x)\n   [strCurLanguage], Mission10's
        # location card 0x008cb5f4): one expression
        body = re.sub(r"(\(&PTR_\w+?_[0-9a-f]{8}\))\s*\n\s*(\[strCurLanguage\])", r"\1\2", body)
        # a localised string table Ghidra read as a byte offset (*(char **)(&DAT_x + strCurLanguage * 4)): LSTRING_x
        body = re.sub(r"\*\(char \*\*\)\(&DAT_([0-9a-f]{8}) \+ strCurLanguage \* 4\)", r"(&PTR_lstr_\1)[strCurLanguage]",
                      body)
        self._acalls, self._aconsts = self.asm_facts(addr)
        self._atuples = self._tuples
        # Ghidra's literals with no symbol: hashes that point at string-like bytes (resolved by rawstr)
        self._raw = []

        self._rawlt = set()
        self._explicit = {int(k, 16) for k in re.findall(r"\b0x([0-9a-f]+)\b", body)} | \
            {int(k, 16) for k in re.findall(r"_([0-9a-f]{8})\b", body)}

        def rawsub(mm):
            self._raw.append(ghidra_bytes(mm.group(1)))
            if re.search(r"<\s*$", body[:mm.start()]):
                self._rawlt.add(len(self._raw) - 1)
            return "RAWSTR_%d" % (len(self._raw) - 1)
        body = re.sub(r'(?<![\w])"((?:[^"\\]|\\.)*)"', rawsub, body)
        # a literal with several candidates: the k-th occurrence in the decompile is the k-th of those constants in
        # the machine code's address order (the scripts' straight-line runs of numbered names, e.g. pPatrol1..8);
        # verified with the rest (the call tuples, the constant sets)
        self._rawpos = {}
        bylit = collections.defaultdict(list)
        for k, lit in enumerate(self._raw):
            bylit[lit].append(k)
        for lit, ks in bylit.items():
            ks = [k for k in ks if k not in self._rawlt]
            cands = {v for v in self._aconsts - self._explicit if self.cstr(v) == lit} or \
                {v for v in self._aconsts if self.cstr(v) == lit}
            if len(cands) > 1:
                seq = [v for v in self._aorder if v in cands]
                if len(seq) == len(ks):
                    self._rawpos.update(zip(ks, seq))
        # kasfTeamHyperspaceOut 0x00493800 tail-jumped: Ghidra shows its body (and the return address it pushed)
        body, nho = re.subn(r"if \(DAT_00a43ee8 != 0\) \{\s*\w+(?:\[0\])?\.numShips = 0;\s*aiuFilterDisabledShips\("
                            r"\*\(SelectCommand \*\*\)\(DAT_00a43ee8 \+ 0x20\),[^;]*\);\s*RemoveShipsFromDoingStuff\([^;]*,0x13\);"
                            r"\s*spHyperspaceSelectionOut\([^;]*\);\s*_DAT_00a6ad98 = 0;\s*\}", "kasfTeamHyperspaceOut();", body)
        if nho:
            body = re.sub(r"\n\s*pSStack_\w+ = \(ShipPtr\)0x[0-9a-f]+;", "", body)
            body = re.sub(r"\n\s*(?:SelectCommand|MaxSelection) aSStack_\w+ \[\d+\];", "", body)
            body = re.sub(r"\n\s*ShipPtr pSStack_\w+;", "", body)
        body = re.sub(r"aiuKasWrapFormation\(\*\(SelectCommand \*\*\)\(DAT_00a43ee8 \+ 0x20\),(\d+)\);\s*"
                      r"\*\(undefined4 \*\)\(DAT_00a43ee8 \+ 0x78\) = \1;",
                      lambda mm: "%s();" % self.formation_kasf[int(mm.group(1))], body)
        for pat, rp in INLINED_THUNKS:
            if rp is not None:
                body = re.sub(pat, rp, body)
        m = re.search(r"^[^\n]*\b%s\s*\(void\)\s*\{" % re.escape(f["name"]), body, re.M)
        if not m:
            m = re.search(r"^[^\n]*\b%s\s*\(void\)\s*\n\s*\{" % re.escape(f["name"]), body, re.M)
        body = body[m.end():]
        body = body[:body.rstrip().rfind("}")]
        lines = [ln for ln in body.split("\n") if ln.strip()]
        # locals
        decls, stm = [], []
        words, voids = {}, set()
        for ln in lines:
            dm = re.match(r"^  ([A-Za-z_][\w ]*?\s*\**)\s*(\w+);$", ln)
            if dm and not stm and not dm.group(1).strip() in ("return", "goto") and dm.group(2) != "return":
                t, n = dm.group(1).strip(), dm.group(2)
                if DROP_LOCAL.match(n):
                    continue
                t = re.sub(r"\s*\*", " *", t)
                if t not in LOCAL_TYPES:
                    raise ValueError("0x%08x: local type %r" % (addr, t))
                ct = LOCAL_TYPES[t]
                if t == "void *":
                    voids.add(n)
                if ct == "word":
                    ct = "uintptr_t"
                words[n] = ct
                decls.append((ct, n))
            else:
                stm.append(ln)
        # the registers Ghidra took for a this / ECX argument (dropped by the table's arity) and their copies
        phantom = {n for n in re.findall(r"\b(this\w*|extraout_\w+|in_ECX)\b", "\n".join(stm))}
        joined = "\n".join(stm)
        while True:
            more = set()
            for n in voids - phantom:
                rhs = re.findall(r"^\s*%s = ([^;]*);$" % n, joined, re.M)
                if rhs and all(x.strip() in phantom for x in rhs):
                    more.add(n)
            if not more:
                break
            phantom |= more
        stm = [ln for ln in stm if not re.match(r"^\s*(\w+) = (\w+);$", ln) or
               re.match(r"^\s*(\w+) = (\w+);$", ln).group(1) not in phantom]
        decls = [(ct, n) for ct, n in decls if n not in phantom]
        for n in phantom:
            words.pop(n, None)
        text = "\n".join(stm)
        # the garbled hashes: a pointer into aisTeams Ghidra made of a message / name hash (AIS_ELEM above)
        g = AIS_ELEM

        def ais(mm):
            return self.ais_base + int(mm.group(1), 0) * AIS_ENTRY + int(mm.group(2), 0) * AIS_SELECTION + AIS_SHIPPTR
        text = re.sub(r"\(\s*\(int\)\s*" + g + r"\s*\+\s*(0x[0-9a-f]+|\d+)\s*\)",
                      lambda mm: "GARBLE(%d)" % (ais(mm) + int(mm.group(3), 0)), text)
        text = re.sub(r"\(int\)\s*" + g + r"\s*\+\s*(0x[0-9a-f]+|\d+)",
                      lambda mm: "GARBLE(%d)" % (ais(mm) + int(mm.group(3), 0)), text)
        text = re.sub(g + r"\s*\+\s*(0x[0-9a-f]+|\d+)", lambda mm: "GARBLE(%d)" % (ais(mm) + 4 * int(mm.group(3), 0)),
                      text)
        text = re.sub(g, lambda mm: "GARBLE(%d)" % ais(mm), text)
        # a name / message hash Ghidra took for a string's address plus an offset (s_Hyperspace_008c7f90 + 1 is the hash
        # 0x8c7f91 pushed at 0x0066616f; s_ES_WRONG_TMESSAGE_008d9fc0 + 0x13 the hash 0x8d9fd3 compared at 0x006213c2):
        # every such form in the mission bodies is a hash (M2-R9 part B, Mission12 / Mission17)
        text = re.sub(r"(?:\(\s*\w+\s*\*\)\s*)?\bs_\w+?_([0-9a-f]{8}) \+ (0x[0-9a-f]+|\d+)\b",
                      lambda mm: "GARBLE(%d)" % (int(mm.group(1), 16) + int(mm.group(2), 0)), text)
        text = re.sub(r"\(ShipPtr \*\)(\w+)", r"\1", text)
        # the scope prologue and the team's KAS fields
        text = text.replace("DAT_00c3749c", "CurrentMissionScope").replace("DAT_00c374a0", "CurrentMissionScopeName")
        text = re.sub(r"\*\*\(\w+ \*\*\)\(%s \+ 0x50\)" % TEAM, "CurrentTeamP->msgQueue->head", text)
        text = re.sub(r"\*\(\w+ \*\*\)\(%s \+ 0x50\)" % TEAM, "CurrentTeamP->msgQueue", text)
        text = re.sub(r"\*\(undefined4 \*\)\(%s \+ 0x58\)" % TEAM, "CurrentTeamP->kasLabel", text)
        text = re.sub(r"\(GrowSelection \*\)\(%s \+ 0x1c\)" % TEAM, "&CurrentTeamP->shipList", text)
        text = re.sub(r"\((ShipPtr|\w+ \*)\)\(%s \+ 0x1c\)" % TEAM, r"(\1)&CurrentTeamP->shipList", text)
        text = re.sub(r"\*\(SelectCommand \*\*\)\(%s \+ 0x20\)" % TEAM,
                      "(SelectCommand *)CurrentTeamP->shipList.selection", text)
        text = re.sub(r"\(\w+ \*\)0x0\b", "0", text)
        text = re.sub(r"\btrue\b", "TRUE", re.sub(r"\bfalse\b", "FALSE", text))
        consts, calls = set(), collections.Counter()
        self._ctuples, self._strs = collections.Counter(), {}
        text = self.rewrite_calls(text, consts, calls, words)
        text = text.replace(TEAM, "CurrentTeamP")
        if re.search(r"\*\s*\((?:char|int|uint|undefined\d?|sdword|udword) \*+\)", text):
            raise ValueError("0x%08x %s: unread dereference" % (addr, f["name"]))
        if re.search(r"\bCurrentTeamP\s*(\+|-(?!>))", text):
            raise ValueError("0x%08x %s: unread CurrentTeamP arithmetic" % (addr, f["name"]))
        # the remaining operands: casts of hashes, &DAT_x / s_x / GARBLE(n) in comparisons, assignments, cases

        def operand(mm):
            return self.value(mm.group(0), None, consts, words)
        text = re.sub(r"\(char \*\)\s*(?=0x)", "", text)
        text = re.sub(r"\((?:code|char|ShipPtr) \*\)\s*(?=&?\w)", "", text)
        text = re.sub(r"\(&PTR_\w+?_[0-9a-f]{8}\)\[strCurLanguage\]", operand, text)
        text = re.sub(r"GARBLE\(-?\d+\)", operand, text)
        text = re.sub(r"(?:&?\b(?:DAT|BYTE|s_\w+?|lp\w+?|lParam|PTR_\w+?|u_\w+?)|&(?:LAB|FUN))_[0-9a-f]{8}\b", operand,
                      text)
        text = re.sub(r"\bRAWSTR_\d+\b", operand, text)
        text = re.sub(r"\(int\)(\w+)", r"(sdword)\1", text)
        text = re.sub(r"\(uint\)(\w+)", r"(udword)\1", text)
        nostr = re.sub(r'"(?:[^"\\]|\\.)*"', '""', text)
        for neg, k in re.findall(r"(-?)\b0x([0-9a-f]+)\b", nostr):
            v = (-int(k, 16) if neg else int(k, 16)) & 0xFFFFFFFF
            if 0x10000 <= v < 0xFFFF0000:
                consts.add(v)
        for neg, k in re.findall(r"(-?)\b(?<!\.)([1-9]\d*)\b(?!\.)", nostr):
            v = (-int(k) if neg else int(k)) & 0xFFFFFFFF
            if 0x10000 <= v < 0xFFFF0000:
                consts.add(v)
        self._mul = set()
        for k in re.findall(r"\* (0x[0-9a-f]+|\d+)\b", nostr):   # a product the compiler strength-reduced (lea / shl)
            self._mul.add(int(k, 0))
        for k in re.findall(r"[/%] (0x[0-9a-f]+|\d+)\b", nostr):     # a division by a constant is MSVC's imul by its magic
            if int(k, 0) > 1 and int(k, 0) & (int(k, 0) - 1):
                consts.add(div_magic(int(k, 0)))
        # string literals assigned to a word local
        text = re.sub(r"(\b\w+ = )(\"(?:[^\"\\]|\\.)*\");", r"\1(uintptr_t)\2;", text)
        if re.search(r"\b(DAT_|_DAT_|PTR_|extraout_|in_ECX|unaff_|this\b|aisTeams|universe|GARBLE|RAWSTR|CONCAT|SUB\d)",
                     text):
            bad = re.findall(r"\b(DAT_\w+|_DAT_\w+|PTR_\w+|extraout_\w+|in_ECX|unaff_\w+|this|aisTeams|universe|GARBLE|"
                             r"RAWSTR\w*|CONCAT\w*)", text)
            raise ValueError("0x%08x %s: unread %s" % (addr, f["name"], sorted(set(bad))))
        for name in re.findall(r"\b([A-Za-z_]\w*)\s*\(", re.sub(r'"(?:[^"\\]|\\.)*"', '""', text)):
            if name in ("if", "while", "switch", "for", "return", "sizeof") or name in self.sig:
                continue
            raise ValueError("0x%08x %s: call to %s is not a kasf, a runtime function or a script function" % (
                addr, f["name"], name))
        return decls, text, consts, calls

    def verify(self, addr, consts, calls):
        acalls, aconsts = self.asm_facts(addr)
        # the function addresses a kasJump / kasFSMCreate pushes are in both sets
        problems = []
        if calls != acalls:
            def nm(c):
                return {("%s" % self.addr_of.get(k, self.funcs.get(k, {}).get("name", hex(k)))): v for k, v in c.items()}
            problems.append("calls C %s asm %s" % (nm(calls - acalls), nm(acalls - calls)))
        cdiff = {v for v in consts - aconsts - self._mul if ((v - 1) & 0xFFFFFFFF) not in aconsts and ((v + 1) & 0xFFFFFFFF) not in aconsts}
        adiff = {v for v in aconsts - consts if not (v - 1 in consts or v - 2 in consts) or True}
        adiff = {v for v in adiff if v not in self._derived(aconsts, consts) and
                 ((v - 1) & 0xFFFFFFFF) not in consts and ((v + 1) & 0xFFFFFFFF) not in consts}
        self.tuples_checked += sum(self._tuples.values())
        miss = self._tuples - self._ctuples
        if miss:
            problems.append("call arguments only in asm: %s" % [
                "%s(%s)" % (self.addr_of[t[0]], ", ".join(hex(x) for x in t[1:])) for t in miss])
        if cdiff:
            problems.append("constants only in C: %s" % sorted(hex(v) for v in cdiff))
        if adiff:
            problems.append("constants only in asm: %s" % sorted(hex(v) for v in adiff))
        return problems

    def _derived(self, aconsts, consts):
        # the values the asm's sub / dec compare chains add (K+1 .. K+8 for a "sub reg,K") are not required in C
        return {v for v in aconsts if any((v - d) in aconsts for d in range(1, 9)) and v not in consts}

    # ---------------------------------------------------------------- emit
    def emit(self, scripts):
        closures = {s: self.closure(s) for s in ALL_SCRIPTS}
        uses = collections.Counter(a2 for s in ALL_SCRIPTS for a2 in set(closures[s]))
        todo = []
        for s in scripts:
            todo += [a2 for a2 in closures[s] if a2 not in todo]
        out = {}
        bodies, report, failed = {}, [], []
        for a2 in todo:
            try:
                decls, text, consts, calls = self.transform(a2, None)
            except ValueError as ex:
                report.append((a2, self.funcs[a2]["name"], 0, 0, ["UNREAD: %s" % ex]))
                failed.append((a2, self.funcs[a2]["name"], [str(ex)]))
                continue
            probs = self.verify(a2, consts, calls)
            report.append((a2, self.funcs[a2]["name"], sum(calls.values()), len(consts), probs))
            if probs:
                failed.append((a2, self.funcs[a2]["name"], probs))
            bodies[a2] = (decls, text)
        if self.a.report or failed:
            for a2, n, nc, nk, probs in report:
                print("0x%08x %-58s calls %3d consts %3d %s" % (a2, n, nc, nk, "OK" if not probs else "; ".join(probs)))
        print("verified %d bodies: callees and constants equal to the machine code's, %d all-constant calls' "
              "arguments equal" % (len(todo), self.tuples_checked))
        if failed:
            print("m2_kas2c_gen: %d bodies fail the verification" % len(failed), file=sys.stderr)
            sys.exit(2)
        shared = [a2 for a2 in todo if uses[a2] > 1]
        exe_rel = "cataclysm.exe (GOG 1.01)"
        hdr_protos = []
        for s in scripts:
            mine = [a2 for a2 in closures[s] if uses[a2] == 1]
            path = "src/Missions/%s/%s.c" % ("Tutorials" if s.startswith("Tutorial") else "SinglePlayer", s)
            out[path] = self.file_text(path, s, sorted(mine), bodies, closures[s], exe_rel)
        out["src/Missions/KasShared.c"] = self.file_text("src/Missions/KasShared.c", None, sorted(shared), bodies,
                                                         None, exe_rel)
        out["src/Missions/CatMissions.h"] = self.header_text(scripts, closures, uses, todo, exe_rel)
        out["src/Missions/meson.build"] = self.meson_text(scripts)
        return out

    @staticmethod
    def meson_text(scripts):
        files = ["KasShared.c"] + ["%s/%s.c" % ("Tutorials" if s.startswith("Tutorial") else "SinglePlayer", s)
                                   for s in sorted(scripts, key=lambda x: (x.startswith("Tutorial"), x))]
        L = ["# CAT (M2-R8 kas-runtime-mission01, M2-R9 missions-02-09): Cataclysm's kas2c missions, GENERATED from",
             "#      cataclysm.exe by tools/m2_kas2c_gen.py (docs/mission-code-map.md 8; this list is written by the generator",
             "#      too): the generated scripts (%s) and the ICF-shared functions they use." % ", ".join(scripts),
             "#      HW1's compiled missions (SinglePlayer/*.kas, Tutorials/Tutorial1.kas through tools/kas2c, their",
             "#      meson.build files) are no longer built: the kasf library has cataclysm.exe's signatures (KAS names as hashes).",
             "src_missions = files(" + (",\n" + " " * 21).join("'%s'" % f for f in files) + ")",
             "",
             "inc_src_missions = include_directories('.')",
             ""]
        return "\n".join(L)

    def banner(self, path, what, exe_rel):
        name = os.path.basename(path)
        L = ["// =============================================================================",
             "//  %s" % name]
        for w in what:
            L.append("//  %s" % w)
        L += ["//  GENERATED by tools/m2_kas2c_gen.py from %s and the Ghidra export's decompile:" % exe_rel,
              "//  do not edit; rerun the generator (docs/m2-r8-kas-runtime-mission01.md)",
              "// =============================================================================",
              "//  Copyright Relic Entertainment, Inc. All rights reserved.",
              "// =============================================================================", ""]
        return L

    def file_text(self, path, script, addrs, bodies, closure, exe_rel):
        if script:
            what = ["- %s, Cataclysm's kas2c mission script %s: its FSM and state functions in the" % (
                    "the tutorial" if script.startswith("Tutorial") else "the single-player mission", script),
                    "    exe's order (docs/mission-code-map.md 8) and the mission's Init / Watch, as cataclysm.exe has",
                    "    them (block 0x%08x ..); the function list the saves convert pointers with" % min(addrs)]
        else:
            what = ["- the kas2c functions MSVC's /OPT:ICF folded across Cataclysm's mission scripts",
                    "    (docs/mission-code-map.md 7.1), under their home names, for the generated scripts"]
        L = self.banner(path, what, exe_rel)
        L += ["#include <stddef.h>", "#include <stdint.h>", "", '#include "CatMissions.h"', ""]
        used_l = set()
        for a2 in addrs:
            used_l |= set(int(x, 16) for x in re.findall(r"LSTRING_([0-9a-f]{8})", bodies[a2][1]))
        for la in sorted(used_l):
            L.append("static const char *const LSTRING_%08x[3] = {%s};" % (
                la, ", ".join(c_literal(b) for b in self.lstrings[la])))
        if used_l:
            L.append("")
        for a2 in addrs:
            decls, text = bodies[a2]
            n = self.funcs[a2]["name"]
            mark = ""
            if a2 in OVERRIDES:
                mark = " HAND-EDIT (tools/m2_kas2c_gen.py OVERRIDES): " + "; ".join(w for _, _, w in OVERRIDES[a2])
            L.append("// cataclysm.exe 0x%08x (%d bytes)%s" % (a2, self.funcs[a2]["size"], mark))
            L.append("void %s(void)" % n)
            L.append("{")
            for ct, vn in decls:
                L.append("    %s%s%s;" % (ct.rstrip(" *"), " " + "*" * ct.count("*") if "*" in ct else " ", vn))
            if decls:
                L.append("")
            for ln in text.split("\n"):
                ind = len(ln) - len(ln.lstrip(" "))
                L.append(" " * (ind * 2) + ln.strip() if ln.strip() else "")
            L.append("}")
            L.append("")
        if script:
            L.append("// every function of the script, with its cataclysm.exe address (the saves store a function as its")
            L.append("// address minus Watch_%s's, kasConvertFuncPtrToOffset 0x00491800)" % script)
            L.append("const CatKasFunction %s_Functions[] =" % script)
            L.append("{")
            for a2 in sorted(closure):
                L.append("    {(void *)%s, 0x%08x}," % (self.funcs[a2]["name"], a2))
            L.append("};")
            L.append("const udword %s_FunctionCount = sizeof(%s_Functions) / sizeof(%s_Functions[0]);" % (
                script, script, script))
            L.append("")
        return "\n".join(L)

    def header_text(self, scripts, closures, uses, todo, exe_rel):
        L = self.banner("src/Missions/CatMissions.h", [
            "- Cataclysm's generated kas2c mission scripts (docs/mission-code-map.md 4, 8): the prototypes of",
            "    their functions, the shared ones and the per-script function lists"], exe_rel)
        L += ["#ifndef ___CATMISSIONS_H", "#define ___CATMISSIONS_H", "",
              '#include "Types.h"', '#include "AITeam.h"', '#include "KAS.h"', '#include "KASFunc.h"',
              '#include "StringSupport.h"', "",
              "// the KAS runtime's globals the generated code sets and reads (KAS.c; mission-code-map 3: 0x00c3749c,",
              "// 0x00c374a0, 0x00a43ee8)",
              "extern sdword CurrentMissionScope;", "extern udword CurrentMissionScopeName;",
              "extern struct AITeam *CurrentTeamP;", "",
              "// one function of a script and its cataclysm.exe address (the save offset base, mission-code-map 4.10)",
              "typedef struct CatKasFunctionTag", "{", "    void *function;", "    udword exeAddress;", "} CatKasFunction;", ""]
        for s in scripts:
            L.append("// %s: Init 0x%08x, Watch 0x%08x" % (s, self.mnames["Init_" + s], self.mnames["Watch_" + s]))
            L.append("#define CAT_%s_WATCH_EXE 0x%08xu" % (s.upper(), self.mnames["Watch_" + s]))
            L.append("extern const CatKasFunction %s_Functions[];" % s)
            L.append("extern const udword %s_FunctionCount;" % s)
            L.append("")
        for a2 in sorted(todo):
            L.append("void %s(void);  // 0x%08x" % (self.funcs[a2]["name"], a2))
        L += ["", "#endif", ""]
        return "\n".join(L)


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--scripts", default=",".join(DEFAULT_SCRIPTS))
    ap.add_argument("--check", action="store_true")
    ap.add_argument("--report", action="store_true")
    ap.add_argument("--exe", default=os.path.join(ROOT, "original/game/cataclysm.exe"))
    ap.add_argument("--export", default=os.path.join(ROOT, "ghidra/exports/cataclysm_gog_r6"))
    a = ap.parse_args()
    g = Gen(a)
    out = g.emit(a.scripts.split(","))
    rc = 0
    for rel, text in out.items():
        p = os.path.join(HERE, rel)
        if a.check:
            cur = open(p, encoding="latin-1").read() if os.path.exists(p) else None
            if cur != text:
                print("%s differs" % rel)
                rc = 1
            else:
                print("%s current" % rel)
        else:
            os.makedirs(os.path.dirname(p), exist_ok=True)
            open(p, "w", encoding="latin-1").write(text)
            print("wrote %s" % rel)
    sys.exit(rc)


if __name__ == "__main__":
    main()
