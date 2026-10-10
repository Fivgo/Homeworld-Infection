#!/usr/bin/env python3
"""Diffstat of the imported src/ tree against Relic's ad1bb82 tree.

Path mapping (ad1bb82 -> src/): src/Game -> Game, src/Ships -> Ships,
src/Win32 -> SDL (GoK renamed it) and also -> Win32 (reference copy),
src/SinglePlayer -> Missions/SinglePlayer, src/Tutorials -> Missions/Tutorials,
tools/win32/KAS/KAS2C.{c,h,l,y} -> tools/kas2c/{kas2c.c,kas2c.h,lexer.l,parser.y}.
Files match case-insensitively. Two counts are produced per file: raw (bytes as
committed, so CRLF->LF shows as a full rewrite) and normalised (line endings and
trailing whitespace ignored), which is the meaningful one.
Usage: diffstat.py <ad1bb82_root> <worktree> <out.json>
"""
import difflib
import json
import os
import sys

orig, wt, out = sys.argv[1:4]
SRC = os.path.join(wt, 'src')
MAP = [('src/Game', 'Game'), ('src/Ships', 'Ships'), ('src/Win32', 'SDL'), ('src/Win32', 'Win32'),
       ('src/SinglePlayer', 'Missions/SinglePlayer'), ('src/Tutorials', 'Missions/Tutorials')]
KAS = {'KAS2C.c': 'kas2c.c', 'KAS2C.h': 'kas2c.h', 'KAS2C.l': 'lexer.l', 'KAS2C.y': 'parser.y'}
CODE = ('.c', '.h', '.cpp', '.kas', '.kash', '.l', '.y', '.def', '.rc')


def lines(path, normalise):
    with open(path, 'rb') as f:
        data = f.read().decode('latin-1')
    if normalise:
        return [l.rstrip() for l in data.replace('\r\n', '\n').replace('\r', '\n').split('\n')]
    return data.split('\n')


def stat(a, b):
    add = rem = 0
    for op, i1, i2, j1, j2 in difflib.SequenceMatcher(None, a, b, autojunk=False).get_opcodes():
        if op != 'equal':
            rem += i2 - i1
            add += j2 - j1
    return add, rem


def listing(d):
    res = {}
    for root, _, files in os.walk(d):
        for n in files:
            p = os.path.join(root, n)
            res[os.path.relpath(p, d).lower()] = p
    return res


rows = []
seen_new = set()
pairs = [(os.path.join(orig, o), os.path.join(SRC, n), n) for o, n in MAP]
for od, nd, label in pairs:
    ol = {k: v for k, v in listing(od).items() if k.endswith(CODE)}
    nl = {k: v for k, v in listing(nd).items() if k.endswith(CODE)} if os.path.isdir(nd) else {}
    for k in sorted(set(ol) | set(nl)):
        if k in ol and k in nl:
            a_raw, b_raw = lines(ol[k], False), lines(nl[k], False)
            a_n, b_n = lines(ol[k], True), lines(nl[k], True)
            raw = stat(a_raw, b_raw) if a_raw != b_raw else (0, 0)
            nrm = stat(a_n, b_n) if a_n != b_n else (0, 0)
            rows.append({'dir': label, 'file': os.path.relpath(nl[k], SRC), 'status': 'modified' if nrm != (0, 0) else ('eol-only' if raw != (0, 0) else 'identical'),
                         'add': nrm[0], 'del': nrm[1], 'raw_add': raw[0], 'raw_del': raw[1]})
            seen_new.add(nl[k])
        elif k in nl:
            n = len(lines(nl[k], True))
            rows.append({'dir': label, 'file': os.path.relpath(nl[k], SRC), 'status': 'added', 'add': n, 'del': 0, 'raw_add': n, 'raw_del': 0})
            seen_new.add(nl[k])
        elif label != 'Win32' or True:
            if label == 'SDL':
                continue  # counted once under the Win32 reference copy
            n = len(lines(ol[k], True))
            rows.append({'dir': label, 'file': os.path.relpath(ol[k], orig), 'status': 'deleted', 'add': 0, 'del': n, 'raw_add': 0, 'raw_del': n})
# kas2c
kd = os.path.join(orig, 'tools/win32/KAS')
for o, n in KAS.items():
    a, b = lines(os.path.join(kd, o), True), lines(os.path.join(SRC, 'tools/kas2c', n), True)
    add, rem = stat(a, b)
    rows.append({'dir': 'tools/kas2c', 'file': 'tools/kas2c/' + n, 'status': 'modified', 'add': add, 'del': rem, 'raw_add': add, 'raw_del': rem})
    seen_new.add(os.path.join(SRC, 'tools/kas2c', n))
# everything else in src/ that no mapping covered
for root, _, files in os.walk(SRC):
    for f in files:
        p = os.path.join(root, f)
        if p in seen_new or not f.endswith(CODE + ('.build', '.txt', '.md')):
            continue
        rel = os.path.relpath(p, SRC)
        if rel.startswith('Win32/'):
            continue
        n = len(lines(p, True))
        rows.append({'dir': rel.split('/')[0] if '/' in rel else '.', 'file': rel, 'status': 'added', 'add': n, 'del': 0, 'raw_add': n, 'raw_del': 0})

summary = {}
for r in rows:
    s = summary.setdefault(r['dir'], {'files': 0, 'identical': 0, 'eol-only': 0, 'modified': 0, 'added': 0, 'deleted': 0, 'add': 0, 'del': 0})
    s['files'] += 1
    s[r['status']] += 1
    s['add'] += r['add']
    s['del'] += r['del']
with open(out, 'w') as f:
    json.dump({'summary': summary, 'rows': rows}, f, indent=1)
for k, v in summary.items():
    print(k, v)
