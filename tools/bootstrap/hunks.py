#!/usr/bin/env python3
"""Measure how far GoK HEAD src/{Game,Ships} is from ad1bb82 src/{Game,Ships}.

Files are matched case-insensitively by basename within the same directory.
Line endings and trailing whitespace are normalised before diffing so that the
CRLF->LF conversion of the first port commit is not counted as a change.
Usage: hunks.py <orig_root> <gok_root>
"""
import difflib
import os
import sys

orig_root, gok_root = sys.argv[1], sys.argv[2]


def norm(path):
    with open(path, 'rb') as f:
        data = f.read().decode('latin-1')
    return [l.rstrip() for l in data.replace('\r\n', '\n').replace('\r', '\n').split('\n')]


tot = {'matched': 0, 'identical': 0, 'hunks': 0, 'added': 0, 'removed': 0,
       'orig_only': 0, 'gok_only': 0}
per = []
for sub in ('Game', 'Ships'):
    od = os.path.join(orig_root, 'src', sub)
    gd = os.path.join(gok_root, 'src', sub)
    om = {f.lower(): f for f in os.listdir(od) if os.path.isfile(os.path.join(od, f))}
    gm = {f.lower(): f for f in os.listdir(gd) if os.path.isfile(os.path.join(gd, f))}
    for k in sorted(set(om) | set(gm)):
        if not k.endswith(('.c', '.h', '.cpp')):
            continue
        if k not in gm:
            tot['orig_only'] += 1
            continue
        if k not in om:
            tot['gok_only'] += 1
            continue
        tot['matched'] += 1
        a, b = norm(os.path.join(od, om[k])), norm(os.path.join(gd, gm[k]))
        h = add = rem = 0
        for op, i1, i2, j1, j2 in difflib.SequenceMatcher(None, a, b, autojunk=False).get_opcodes():
            if op == 'equal':
                continue
            h += 1
            rem += i2 - i1
            add += j2 - j1
        if h == 0:
            tot['identical'] += 1
        tot['hunks'] += h
        tot['added'] += add
        tot['removed'] += rem
        per.append((h, add, rem, sub + '/' + gm[k]))
per.sort(reverse=True)
print(tot)
for row in per[:25]:
    print(row)
