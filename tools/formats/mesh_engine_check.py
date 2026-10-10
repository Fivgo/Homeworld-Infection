#!/usr/bin/env python3
"""Compare the engine's mesh loads with the files on disk, file by file (docs/m2-r2-mesh.md).

Usage: mesh_engine_check.py --data DIR [--update DIR] RUN.log [RUN.log ...]

  --data    the extracted Cataclysm data (original/game_extracted/cataclysm); every *.geo/*.peo/*.mad under it
            must appear in the engine log
  --update  the extracted update.big (original/game_extracted/update); a file there overrides --data, as
            update.big overrides cataclysm.big in the game
  RUN.log   engine logs of `hwi-engine /catMeshLoadTest <list>` (tools/m2_harness.sh --mode meshes)

The engine (src/Game/Mesh.c catMeshRunSelfTest) logs per file
    meshtest: loaded <path> ident=RMF99ba objects=17 materials=109 verts=454 polys=633 norms=641 glowverts=12
              textures=0 bad=0
A file passes when ident, objects, materials, verts, polys and norms equal what this script reads from the file
with the Cataclysm layout of docs/formats/mesh.md (0x44 header, 0x74-byte polygon objects), glowverts equals the
number of per-vertex info entries (+0x1C list) whose low 24 bits are non-zero, and bad=0 (the engine found every
polygon, vertex and hierarchy index in range). For a .mad mesh animation the engine logs
    meshtest: mad loaded <path> objects=6 animations=4 keyframes=120 bad=0
and the file passes when objects, animations and the summed keyframe counts equal the file's (header 0x28 bytes:
nObjects +0x1C, objPath +0x20, nAnimations +0x24; 0x30-byte object paths with nKeyframes at +0x0C) and bad=0
(names, CRCs, keyframe times in order). Read-only: nothing is written.
"""
import argparse
import os
import re
import struct
import sys

LOADED = re.compile(r'meshtest: loaded (\S+) ident=(\w+) objects=(\d+) materials=(\d+) verts=(\d+) polys=(\d+) '
                    r'norms=(\d+) glowverts=(\d+) textures=(\d+) bad=(\d+)')
MAD = re.compile(r'meshtest: mad loaded (\S+) objects=(\d+) animations=(\d+) keyframes=(\d+) bad=(\d+)')


def mad_counts(path):
    d = open(path, 'rb').read()
    n_obj, obj_path, n_anim = struct.unpack_from('<iIi', d, 0x1C)
    keys = sum(struct.unpack_from('<i', d, obj_path + 0x30 * i + 0x0C)[0] for i in range(n_obj))
    return n_obj, n_anim, keys


def file_counts(path):
    d = open(path, 'rb').read()
    ident = d[:8].rstrip(b'\0').decode('latin-1')
    n_pub, n_loc, _o_pub, _o_loc, n_obj = struct.unpack_from('<IIIII', d, 0x18)
    verts = polys = norms = glow = 0
    for i in range(n_obj):
        o = 0x44 + i * 0x74
        n_v, n_fn, n_vn, n_p, _p_v, p_vi = struct.unpack_from('<iiiiII', d, o + 8)
        verts += n_v
        polys += n_p
        norms += n_fn + n_vn
        for k in range(n_v):
            if struct.unpack_from('<I', d, p_vi + 16 * k)[0] & 0x00ffffff:
                glow += 1
    return ident, n_obj, n_pub + n_loc, verts, polys, norms, glow


def main(argv):
    ap = argparse.ArgumentParser(description=__doc__.split('\n')[0])
    ap.add_argument('--data', required=True)
    ap.add_argument('--update')
    ap.add_argument('logs', nargs='+')
    a = ap.parse_args(argv)

    files = {}
    for base in [a.data] + ([a.update] if a.update else []):
        for dirpath, _dirs, names in os.walk(base):
            for n in names:
                if n.lower().endswith(('.geo', '.peo', '.mad')):
                    full = os.path.join(dirpath, n)
                    files[os.path.relpath(full, base).replace(os.sep, '/').lower()] = full
    engine = {}
    for log in a.logs:
        for line in open(log, encoding='latin-1'):
            m = LOADED.search(line) or MAD.search(line)
            if m:
                engine[m.group(1).replace('\\', '/').lower()] = m.groups()[1:]
    ok = bad = missing = 0
    for rel in sorted(files):
        if rel not in engine:
            missing += 1
            print('MISSING  %s' % rel)
            continue
        if rel.endswith('.mad'):
            objs, anims, keys, nbad = engine[rel]
            want = mad_counts(files[rel])
            got = (int(objs), int(anims), int(keys))
        else:
            ident, objs, mats, verts, polys, norms, glow, _tex, nbad = engine[rel]
            want = file_counts(files[rel])
            got = (ident, int(objs), int(mats), int(verts), int(polys), int(norms), int(glow))
        if got == want and nbad == '0':
            ok += 1
        else:
            bad += 1
            print('MISMATCH %s engine=%s bad=%s file=%s' % (rel, got, nbad, want))
    extra = sorted(set(engine) - set(files))
    for rel in extra:
        print('EXTRA    %s (logged, not under --data/--update)' % rel)
    print('summary: %d files on disk, %d match, %d mismatch, %d not loaded, %d extra' % (
        len(files), ok, bad, missing, len(extra)))
    return 0 if bad == 0 and missing == 0 and not extra else 1


if __name__ == '__main__':
    sys.exit(main(sys.argv[1:]))
