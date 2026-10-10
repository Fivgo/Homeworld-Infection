#!/usr/bin/env python3
"""Tally GCC warnings in a meson/ninja build log. Usage: warntally.py <log> <out.json>"""
import collections
import json
import re
import sys

text = re.sub(r'\x1b\[[0-9;]*[mK]', '', open(sys.argv[1], encoding='utf-8', errors='replace').read())
pat = re.compile(r'^(\S+?):(\d+):(\d+): warning: (.*?)(?: \[(-W[^\]]+)\])?$', re.M)
seen = set()
flags = collections.Counter()
files = collections.Counter()
dirs = collections.Counter()
for m in pat.finditer(text):
    key = (m.group(1), m.group(2), m.group(3), m.group(5))
    if key in seen:  # headers report once per including TU; count unique sites
        continue
    seen.add(key)
    f = m.group(1).replace('../../', '')
    flags[m.group(5) or '(none)'] += 1
    files[f] += 1
    d = f.split('/')
    dirs['/'.join(d[:2]) if d[0] == 'src' else d[0]] += 1
raw = len(pat.findall(text))
res = {'raw_warning_lines': raw, 'unique_sites': len(seen), 'files_with_warnings': len(files),
       'by_flag': flags.most_common(), 'by_dir': dirs.most_common(), 'top_files': files.most_common(15)}
json.dump(res, open(sys.argv[2], 'w'), indent=1)
print('raw', raw, 'unique', len(seen), 'files', len(files))
for k, v in flags.most_common():
    print(f'{v:6d} {k}')
print(dirs.most_common())
print(files.most_common(10))
