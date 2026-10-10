#!/usr/bin/env python3
"""Import the HW1 engine source into the Homeworld-Infection worktree.

Copies (never modifies the sources):
  GoK HEAD working tree  src/{Game,Ships,SDL,Missions,ThirdParty}, src/meson.build -> <wt>/src/...
  GoK HEAD working tree  tools/kas2c                                     -> <wt>/src/tools/kas2c
  ad1bb82 export         src/Win32 (reference only, not built)           -> <wt>/src/Win32
  GoK HEAD               COPYING.REULA                                   -> <wt>/src/COPYING.REULA
Skips autotools files (Makefile.am), Relic's prebuilt objects (src/obj) and any
non-text file. Writes a JSON manifest (path, source, sha256, bytes) to argv[4].
Usage: import_tree.py <gok_root> <ad1bb82_root> <worktree> <manifest.json>
"""
import hashlib
import json
import os
import shutil
import sys

gok, orig, wt, manifest_path = sys.argv[1:5]
SKIP_NAMES = {'Makefile.am', 'kas2c-macOS-compile.sh', 'vssver.scc'}
BINARY_EXT = {'.o', '.obj', '.pdb', '.lib', '.dll', '.exe', '.exp', '.ncb', '.opt', '.aps', '.ico', '.bmp', '.cur'}

manifest = []
skipped = []


def is_text(path):
    with open(path, 'rb') as f:
        chunk = f.read(65536)
    return b'\0' not in chunk


def copy_tree(src_dir, dst_dir, label):
    for root, dirs, files in os.walk(src_dir):
        dirs.sort()
        for name in sorted(files):
            s = os.path.join(root, name)
            rel = os.path.relpath(s, src_dir)
            if name in SKIP_NAMES or os.path.splitext(name)[1].lower() in BINARY_EXT or not is_text(s):
                skipped.append({'source': label, 'path': os.path.relpath(s, src_dir if label != 'gok' else gok), 'from': s})
                continue
            d = os.path.join(dst_dir, rel)
            os.makedirs(os.path.dirname(d), exist_ok=True)
            shutil.copyfile(s, d)
            with open(d, 'rb') as f:
                data = f.read()
            manifest.append({'path': os.path.relpath(d, wt), 'source': label,
                             'source_path': os.path.relpath(s, gok if label == 'gok@3af31fb' else orig),
                             'sha256': hashlib.sha256(data).hexdigest(), 'bytes': len(data)})


for sub in ('Game', 'Ships', 'SDL', 'Missions', 'ThirdParty'):
    copy_tree(os.path.join(gok, 'src', sub), os.path.join(wt, 'src', sub), 'gok@3af31fb')
copy_tree(os.path.join(gok, 'tools', 'kas2c'), os.path.join(wt, 'src', 'tools', 'kas2c'), 'gok@3af31fb')
copy_tree(os.path.join(orig, 'src', 'Win32'), os.path.join(wt, 'src', 'Win32'), 'relic@ad1bb82')

for name, label, base in (('COPYING.REULA', 'gok@3af31fb', gok),):
    s = os.path.join(base, name)
    d = os.path.join(wt, 'src', name)
    shutil.copyfile(s, d)
    with open(d, 'rb') as f:
        data = f.read()
    manifest.append({'path': os.path.relpath(d, wt), 'source': label, 'source_path': name,
                     'sha256': hashlib.sha256(data).hexdigest(), 'bytes': len(data)})

with open(manifest_path, 'w') as f:
    json.dump({'files': manifest, 'skipped': skipped}, f, indent=1)
print('copied', len(manifest), 'skipped', len(skipped))
for s in skipped:
    print('  skipped', s['source'], s['from'])
