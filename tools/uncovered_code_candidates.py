"""List pointer-referenced code entries in the game region 0x401000-0x5c9fff that lie outside
every Ghidra function body (docs/binary-map.md R5.6 gap 1).

A candidate is a 16-byte aligned address outside every [entry, entry+size) of functions.csv whose
first byte is not padding, that follows a covered byte, padding (0x90/0xCC) or a RET, and whose
address is stored as a dword somewhere in the file. Per candidate: the run size up to the next
function or candidate (trailing padding trimmed; an upper bound), who holds the pointer (the
function for a code holder, the nearest label below for a data holder), strings and named callees
inside the run. Switch tables inside a function can show up as false positives, and `size` treats
every function body as contiguous.

usage: python3 -I tools/uncovered_code_candidates.py <export dir> <cataclysm.exe> <out.json>
"""
import bisect
import collections
import csv
import json
import struct
import sys

E = sys.argv[1].rstrip('/') + '/'
EXE = sys.argv[2]
OUT = sys.argv[3]
LO, HI = 0x401000, 0x5ca000
data = open(EXE, 'rb').read()
pe = struct.unpack_from('<I', data, 0x3c)[0]
nsec = struct.unpack_from('<H', data, pe + 6)[0]
opt = struct.unpack_from('<H', data, pe + 20)[0]
base = struct.unpack_from('<I', data, pe + 24 + 28)[0]
secs = []
for i in range(nsec):
    o = pe + 24 + opt + 40 * i
    name = data[o:o + 8].rstrip(b'\0').decode()
    vs, va, rs, ro = struct.unpack_from('<IIII', data, o + 8)
    secs.append((name, base + va, vs, ro, rs))
_, tva, tvs, tro, trs = [s for s in secs if s[0] == '.text'][0]


def off2va(o):
    for name, va, vs, ro, rs in secs:
        if ro <= o < ro + rs:
            return va + o - ro, name
    return None, None


def byte(a):
    return data[tro + a - tva]


funcs = []
cov = bytearray(HI - LO)
for r in csv.DictReader(open(E + 'functions.csv')):
    a = int(r['address'], 16)
    n = int(r['size'] or 0)
    funcs.append((a, n, r['name']))
    if LO <= a < HI:
        e = min(a + n, HI)
        cov[a - LO:e - LO] = b'\x01' * (e - a)
funcs.sort()
fstarts = [f[0] for f in funcs]
fname = {f[0]: f[2] for f in funcs}
syms = []
for r in csv.DictReader(open(E + 'symbols.csv')):
    try:
        syms.append((int(r['address'], 16), r['name']))
    except ValueError:
        pass
syms.sort()
sstarts = [s[0] for s in syms]
strings = {int(r['address'], 16): r['string'] for r in csv.DictReader(open(E + 'strings.csv'))}
holders = collections.defaultdict(list)
for o in range(0, len(data) - 3):
    v = struct.unpack_from('<I', data, o)[0]
    if LO <= v < HI and v % 16 == 0:
        holders[v].append(o)
cands = []
for a in range(LO, HI, 16):
    if cov[a - LO] or byte(a) in (0x90, 0xCC, 0x00):
        continue
    if not (cov[a - 1 - LO] or byte(a - 1) in (0x90, 0xCC, 0xC3) or byte(a - 3) == 0xC2):
        continue
    if holders[a]:
        cands.append(a)
cs = set(cands)


def containing(va):
    i = bisect.bisect_right(fstarts, va) - 1
    if i >= 0 and funcs[i][0] <= va < funcs[i][0] + max(funcs[i][1], 1):
        return funcs[i][2]
    return None


def label_at_or_below(va):
    i = bisect.bisect_right(sstarts, va) - 1
    return (syms[i][1], va - syms[i][0]) if i >= 0 else (None, 0)


out = []
for a in cands:
    b = a + 1
    while b < HI and not cov[b - LO] and b not in cs:
        b += 1
    e = b
    while e > a and byte(e - 1) in (0x90, 0xCC):
        e -= 1
    run = data[tro + a - tva: tro + e - tva]
    strs, callees = [], []
    for i in range(len(run) - 3):
        v = struct.unpack_from('<I', run, i)[0]
        if v in strings and strings[v] not in strs:
            strs.append(strings[v])
        if run[i] == 0xE8 and i + 5 <= len(run):
            t = a + i + 5 + struct.unpack_from('<i', run, i + 1)[0]
            if t in fname and fname[t] not in callees:
                callees.append(fname[t])
    hl = []
    for o in holders[a][:4]:
        va, sec = off2va(o)
        if va is None:
            continue
        if sec == '.text':
            c = containing(va)
            hl.append('%s (code)' % c if c else '0x%08x (code, no function)' % va)
        else:
            lab, d = label_at_or_below(va)
            hl.append('%s+0x%x (%s)' % (lab, d, sec) if lab else '0x%08x (%s)' % (va, sec))
    out.append({'address': '0x%08x' % a, 'size': e - a, 'holders': hl, 'n_holders': len(holders[a]),
                'strings': strs[:3], 'callees': callees[:4], 'first_bytes': run[:6].hex()})
json.dump(out, open(OUT, 'w'), indent=1)
print('candidates', len(out), 'bytes', sum(x['size'] for x in out))
held_code = sum(1 for x in out if any('(code' in h for h in x['holders']))
print('held from code', held_code, 'held only from data', len(out) - held_code)
