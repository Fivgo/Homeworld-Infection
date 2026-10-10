#!/usr/bin/env python3
"""Probe Homeworld: Cataclysm text scripts (.script, .shp, .lod, .missphere, .level, .mission, .dist ...)
against the key tables compiled into cataclysm.exe (docs/formats/script.md).

Usage:
  script_probe.py --exe cataclysm.exe [--export DIR] [--hw1-src ORIG/src] [--tables] FILE [FILE ...]

  --exe      the GOG cataclysm.exe; its .data is scanned (read-only) for scriptEntry (12 B: name, setVarCB,
             dataPtr) and scriptStructEntry (16 B: name, setVarCB, fieldAddr, baseAddr) tables
  --export   a Ghidra export dir with functions.csv, used to name the callbacks and to drop tables whose
             callbacks are front-end handlers rather than script setters (optional; without it every
             name/code-pointer table in .data is kept)
  --hw1-src  Relic's HW1 source tree (ad1bb82 src/); keys found in its tables are marked as HW1 keys
  --tables   print every table found and exit

For each FILE the probe tokenises lines the way parseLine (0x00526120) does: lines starting '[' or ']'
are skipped, ';' starts a comment, '//' starts a comment unless the line contains "http:", the key is the
first token up to blank/tab/'='. Keys are compared case-sensitively (findEntry uses strcmp). It prints per
file: keys, keys known to some Cataclysm table, keys unknown to every table (split into "string present in
the exe", i.e. compared by hand-written parsing code such as scriptSetGunStatics, and "not in the exe", i.e.
silently ignored), and (with --hw1-src) keys that are Cataclysm-only. Nothing is written or copied.
"""
import argparse
import collections
import csv
import os
import re
import struct
import sys


def pe_sections(d):
    pe = struct.unpack_from('<I', d, 0x3c)[0]
    nsec = struct.unpack_from('<H', d, pe + 6)[0]
    optsz = struct.unpack_from('<H', d, pe + 20)[0]
    base = struct.unpack_from('<I', d, pe + 24 + 28)[0]
    out = []
    for i in range(nsec):
        o = pe + 24 + optsz + 40 * i
        name = d[o:o + 8].rstrip(b'\0').decode()
        vsize, va, rsize, roff = struct.unpack_from('<IIII', d, o + 8)
        out.append((name, base + va, vsize, rsize, roff))
    return out


def scan_tables(exe, funcs):
    d = open(exe, 'rb').read()
    secs = pe_sections(d)
    text = [s for s in secs if s[0] == '.text'][0]

    def rd(a, n=4):
        for name, va, vs, rs, ro in secs:
            if va <= a < va + min(vs, rs) - n + 1:
                return d[ro + a - va: ro + a - va + n]
        return None

    def u32(a):
        b = rd(a)
        return None if b is None else struct.unpack('<I', b)[0]

    def cstr(a):
        out = bytearray()
        for i in range(96):
            b = rd(a + i, 1)
            if b is None:
                return None
            if b == b'\0':
                break
            out += b
        else:
            return None
        s = out.decode('latin-1')
        return s if s and re.fullmatch(r'[\x20-\x7e]+', s) else None

    def is_entry(a):
        p0, p1 = u32(a), u32(a + 4)
        if p0 is None or p1 is None or not (text[1] <= p1 < text[1] + text[2]):
            return False
        return cstr(p0) is not None

    def is_end(a):
        return u32(a) == 0 and u32(a + 4) == 0

    data = [s for s in secs if s[0] == '.data'][0]
    a, end = data[1], data[1] + min(data[2], data[3])
    tables = []
    while a < end - 16:
        if not is_entry(a):
            a += 4
            continue
        best = None
        for stride in (12, 16):
            n, b = 0, a
            while is_entry(b):
                n += 1
                b += stride
            term = is_end(b)
            if best is None or n > best[1] or (n == best[1] and term and not best[2]):
                best = (stride, n, term)
        stride, n, term = best
        if term or n >= 3:
            ents = []
            for i in range(n):
                e = a + i * stride
                cb = u32(e + 4)
                ents.append((cstr(u32(e)), funcs.get(cb, 'code_%08x' % cb)))
            tables.append((a, stride, term, ents))
            a += n * stride
        else:
            a += 4
    strings = set()
    for name, va, vs, rs, ro in secs:
        if name in ('.data', '.rdata'):
            strings.update(m.group()[:-1].decode('latin-1')
                           for m in re.finditer(rb'[\x20-\x7e]{1,96}\x00', d[ro:ro + rs]))
    return tables, strings


SCRIPTISH = re.compile(r'(script|CB$|Set$|Read$|Load|set[A-Z]|Parse|SetCB|^str|^nis|^lod|^light|^tr|^te|^sub|'
                       r'Bind|Info|ToBeNeeded|^llExclude|^llAvailable|Scan|^colPick|^bat|^SetOn|^to[A-Z]|^code_|'
                       r'^cat_\w*(CB|Set|Parse|[Ss]cript))')


def is_script_table(ents):
    real = [cb for _, cb in ents if not cb.startswith('code_')]
    if not real:
        return False
    return sum(1 for cb in real if SCRIPTISH.search(cb)) * 2 >= len(real)


def hw1_keys(root):
    keys = set()
    pq = re.compile(r'\{\s*"([^"]+)"\s*,\s*\(?\s*(?:setVarCback\s*\)\s*)?[A-Za-z_]\w*')
    pm = re.compile(r'makeEntry\(\s*([\w\[\]\.]+)\s*,')
    ps = re.compile(r'\bstrEntry\(\s*(\w+)\s*\)')
    pt = re.compile(r'make(SPR1|SPR2|MPR1|MPR2|MPCR1|MPCR2)TechEntry\(\s*(\w+)\s*\)')
    for dp, _, fs in os.walk(root):
        for f in fs:
            if f.lower().endswith(('.c', '.cpp', '.h')):
                s = open(os.path.join(dp, f), errors='replace').read()
                s = re.sub(r'/\*.*?\*/', '', s, flags=re.S)
                s = re.sub(r'//[^\n]*', '', s)
                keys.update(m.group(1) for m in pq.finditer(s))
                keys.update(m.group(1) for m in pm.finditer(s))
                keys.update(m.group(1) for m in ps.finditer(s))
                keys.update(m.group(1) + m.group(2) for m in pt.finditer(s))
    return keys


def parse_keys(path):
    out = []
    for raw in open(path, 'rb').read().decode('latin-1').splitlines():
        line = raw.rstrip(' \t\r')
        if not line or line[0] in '[]':
            continue
        if ';' in line:
            line = line[:line.index(';')]
        if 'http:' not in line and '//' in line:
            line = line[:line.index('//')]
        line = line.lstrip(' \t')
        if not line:
            continue
        m = re.match(r'[^ \t=]+', line)
        if m:
            out.append(m.group())
    return out


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument('--exe', required=True)
    ap.add_argument('--export')
    ap.add_argument('--hw1-src')
    ap.add_argument('--tables', action='store_true')
    ap.add_argument('files', nargs='*')
    a = ap.parse_args()
    funcs = {}
    if a.export:
        for r in csv.DictReader(open(os.path.join(a.export, 'functions.csv'))):
            funcs[int(r['address'], 16)] = r['name']
    tables, exe_strings = scan_tables(a.exe, funcs)
    if funcs:   # without --export every table is kept (callbacks cannot be named, so none can be filtered)
        tables = [t for t in tables if is_script_table(t[3])]
    keymap = collections.defaultdict(set)
    for addr, stride, term, ents in tables:
        for k, cb in ents:
            keymap[k].add('%s@0x%08x' % (cb, addr))
    if a.tables:
        for addr, stride, term, ents in tables:
            print('0x%08x stride=%d n=%d %s' % (addr, stride, len(ents), ', '.join('%s:%s' % e for e in ents[:6])))
        print('tables=%d keys=%d' % (len(tables), len(keymap)))
        return 0
    hw = hw1_keys(a.hw1_src) if a.hw1_src else None
    unknown_total = 0
    for p in a.files:
        ks = parse_keys(p)
        known = [k for k in ks if k in keymap]
        unknown = sorted(set(k for k in ks if k not in keymap))
        unknown_total += len(unknown)
        msg = 'keys=%d known=%d unknown=%d' % (len(ks), len(known), len(unknown))
        if hw is not None:
            catonly = sorted(set(k for k in known if k not in hw))
            msg += ' cataclysm_only=%d %s' % (len(catonly), ','.join(catonly[:12]))
        if unknown:
            coded = [k for k in unknown if k in exe_strings]
            dead = [k for k in unknown if k not in exe_strings]
            msg += ' UNKNOWN(string in exe, parsed by code)=%s' % ','.join(coded[:12])
            msg += ' UNKNOWN(not in exe, ignored)=%s' % ','.join(dead[:12])
        print('%s  %s' % (msg, p))
    print('summary: %d files, %d unknown keys summed over files (tables=%d, table keys=%d)' % (
        len(a.files), unknown_total, len(tables), len(keymap)))
    return 0


if __name__ == '__main__':
    sys.exit(main())
