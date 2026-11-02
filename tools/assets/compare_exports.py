#!/usr/bin/env python3
"""Compare two Assets folders: the game's own export (HWI-FEATURE(assets-autoexport), src/Game/AssetsExport.c) and
tools/assets/export_assets.py's, of the same archives (docs/features/assets-autoexport.md).

Usage:
  tools/assets/compare_exports.py REFERENCE OTHER

They are equal when:
  - both hold the same files (case kept; the manifests, their temporary files and dot files aside);
  - every file but the PNGs is byte-identical (OBJ, MTL, assets-pages.txt, assets-models.txt, README.txt, .jpg, .tga);
  - every PNG has the same size, the same colour type and the same pixels (the compressed bytes may differ);
  - each folder's assets-manifest.txt lists exactly its files, with their true size, time and sha256.
Python 3 standard library only. Exit 0 when equal, 1 when not.
"""
import hashlib
import os
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
import hwformats as hf  # noqa: E402

MANIFEST = 'assets-manifest.txt'
SKIP = (MANIFEST, 'assets-manifest.partial')
MAX_SHOWN = 20


def scan(root):
    found = {}
    for dp, dn, fn in os.walk(root):
        dn[:] = [d for d in dn if not d.startswith('.')]
        for f in fn:
            if f.startswith('.'):
                continue
            rel = os.path.relpath(os.path.join(dp, f), root).replace(os.sep, '/')
            if rel in SKIP:
                continue
            found[rel] = os.path.join(dp, f)
    return found


def png_info(path):
    with open(path, 'rb') as fh:
        head = fh.read(33)
    ctype = head[25] if len(head) > 25 else None
    w, h, mode, px = hf.png_read(path)
    return w, h, mode, ctype, hashlib.sha256(px).hexdigest()


def check_manifest(root, files, problems):
    path = os.path.join(root, MANIFEST)
    if not os.path.isfile(path):
        problems.append('%s: no %s' % (root, MANIFEST))
        return 0
    listed = {}
    with open(path, encoding='utf-8') as fh:
        for line in fh:
            line = line.rstrip('\r\n')
            if not line or line.startswith('#'):
                continue
            size, mtime, sha, rel = line.split(' ', 3)
            listed[rel] = (int(size), int(mtime), sha)
    for rel in sorted(set(files) - set(listed)):
        problems.append('%s: %s is not in the manifest' % (root, rel))
    for rel in sorted(set(listed) - set(files)):
        problems.append('%s: the manifest lists %s, which is not there' % (root, rel))
    for rel in sorted(set(listed) & set(files)):
        size, mtime, sha = listed[rel]
        st = os.stat(files[rel])
        if st.st_size != size or st.st_mtime_ns // 1000000000 != mtime:
            problems.append('%s: %s size/time %d/%d, the manifest says %d/%d' % (
                root, rel, st.st_size, st.st_mtime_ns // 1000000000, size, mtime))
            continue
        h = hashlib.sha256()
        with open(files[rel], 'rb') as fh:
            for block in iter(lambda: fh.read(1 << 20), b''):
                h.update(block)
        if h.hexdigest() != sha:
            problems.append('%s: %s sha256 differs from the manifest' % (root, rel))
    return len(listed)


def main(argv):
    if len(argv) != 2:
        sys.stderr.write(__doc__)
        return 2
    ref_root, other_root = argv
    ref, other = scan(ref_root), scan(other_root)
    problems = []
    only_ref = sorted(set(ref) - set(other))
    only_other = sorted(set(other) - set(ref))
    for rel in only_ref:
        problems.append('only in %s: %s' % (ref_root, rel))
    for rel in only_other:
        problems.append('only in %s: %s' % (other_root, rel))
    counts = {'png': 0, 'bytes': 0}
    for rel in sorted(set(ref) & set(other)):
        a, b = ref[rel], other[rel]
        if rel.lower().endswith('.png'):
            try:
                ia, ib = png_info(a), png_info(b)
            except (ValueError, OSError) as e:
                problems.append('%s: cannot decode: %s' % (rel, e))
                continue
            if ia != ib:
                what = 'size' if ia[:2] != ib[:2] else 'colour type' if ia[2:4] != ib[2:4] else 'pixels'
                problems.append('%s: the PNGs differ (%s)' % (rel, what))
            counts['png'] += 1
        else:
            with open(a, 'rb') as fa, open(b, 'rb') as fb:
                if fa.read() != fb.read():
                    problems.append('%s: the files differ' % rel)
            counts['bytes'] += 1
    nref = check_manifest(ref_root, ref, problems)
    noth = check_manifest(other_root, other, problems)
    print('compare_exports: %s (%d files, manifest %d) vs %s (%d files, manifest %d)' % (
        ref_root, len(ref), nref, other_root, len(other), noth))
    print('  PNGs compared by pixels: %d; other files compared byte for byte: %d' % (counts['png'], counts['bytes']))
    for p in problems[:MAX_SHOWN]:
        print('  ' + p)
    if len(problems) > MAX_SHOWN:
        print('  ... %d more' % (len(problems) - MAX_SHOWN))
    print('  result: %s (%d differences)' % ('EQUAL' if not problems else 'DIFFERENT', len(problems)))
    return 0 if not problems else 1


if __name__ == '__main__':
    sys.exit(main(sys.argv[1:]))
