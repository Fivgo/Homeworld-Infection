#!/usr/bin/env python3
"""Validate an export made by tools/assets/export_ships.py (stdlib only). Writes <out>/validation.json.

Usage: tools/assets/validate_export.py [--out DIR] [--data DIR] [--update DIR]

Checks:
  1. every OBJ parses: v / vt / vn / f counts equal the manifest (which came from the mesh reader), every face
     has three corners with indices in range, every usemtl names a material in the MTL;
  2. every MTL map_* path exists;
  3. every PNG decodes (own PNG reader, all filter types) to the size in the manifest, and its pixels equal a
     fresh decode of the source .lif (base RGBA and both team masks);
  4. independent readers: tools/formats/mesh_probe.py on every exported mesh (vertex and polygon totals must equal
     the OBJ's v and f counts) and tools/formats/textures_probe.py on every exported .lif;
  5. spot check of a few ships (SPOT_SHIPS in export_ships.py): UV ranges and the share of UV corners in 0..1;
  6. every texture-page placement marked exact_copy is re-checked on the written PNGs.
"""
import argparse
import collections
import importlib.util
import json
import os
import re
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import hwformats as hf  # noqa: E402

REPO = os.path.abspath(os.path.join(os.path.dirname(os.path.abspath(__file__)), '..', '..'))


def load_module(name, path):
    """Import a reader from tools/formats without writing a __pycache__ next to it."""
    old = sys.dont_write_bytecode
    sys.dont_write_bytecode = True
    try:
        spec = importlib.util.spec_from_file_location(name, path)
        mod = importlib.util.module_from_spec(spec)
        spec.loader.exec_module(mod)
    finally:
        sys.dont_write_bytecode = old
    return mod


def parse_obj(path):
    c = collections.Counter()
    usemtl = set()
    mtllib = None
    bad = []
    faces = []
    vts = []
    with open(path) as fh:
        for ln, line in enumerate(fh, 1):
            if line.startswith('v '):
                c['v'] += 1
            elif line.startswith('vt '):
                c['vt'] += 1
                vts.append(tuple(float(x) for x in line.split()[1:3]))
            elif line.startswith('vn '):
                c['vn'] += 1
            elif line.startswith('f '):
                c['f'] += 1
                faces.append((ln, line.split()[1:]))
            elif line.startswith('usemtl '):
                usemtl.add(line.split(None, 1)[1].strip())
            elif line.startswith('mtllib '):
                mtllib = line.split(None, 1)[1].strip()
            elif line.startswith('o '):
                c['o'] += 1
    for ln, corners in faces:
        if len(corners) != 3:
            bad.append('line %d: %d corners' % (ln, len(corners)))
            continue
        for cr in corners:
            parts = cr.split('/')
            if len(parts) != 3:
                bad.append('line %d: corner %r is not v/vt/vn' % (ln, cr))
                break
            vi, ti, ni = (int(p) for p in parts)
            if not (1 <= vi <= c['v'] and 1 <= ti <= c['vt'] and 1 <= ni <= c['vn']):
                bad.append('line %d: index out of range %r' % (ln, cr))
                break
    return c, usemtl, mtllib, bad, vts


def parse_mtl(path):
    names = set()
    maps = []
    with open(path) as fh:
        for line in fh:
            if line.startswith('newmtl '):
                names.add(line.split(None, 1)[1].strip())
            elif line.startswith('map_'):
                maps.append(line.split(None, 1)[1].strip())
    return names, maps


def validate(out, data_root=None, update_root=None):
    data_root = data_root or os.path.join(REPO, 'original', 'game_extracted', 'cataclysm')
    update_root = update_root or os.path.join(REPO, 'original', 'game_extracted', 'update')
    with open(os.path.join(out, 'manifest.json')) as fh:
        manifest = json.load(fh)
    tree = hf.DataTree(data_root, update_root)
    mesh_probe = load_module('mesh_probe', os.path.join(REPO, 'tools', 'formats', 'mesh_probe.py'))
    tex_probe = load_module('textures_probe', os.path.join(REPO, 'tools', 'formats', 'textures_probe.py'))
    sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
    spot_ships = __import__('export_ships').SPOT_SHIPS
    res = collections.OrderedDict()
    fail = collections.defaultdict(list)
    cnt = collections.Counter()
    spot = collections.OrderedDict()

    # 1-2, 4: OBJ / MTL and the independent mesh probe
    for key, ship in manifest['ships'].items():
        for lod in ship['lods']:
            if not lod.get('obj') or 'same_mesh_as_level' in lod:
                continue
            variants = [('source', lod, lod.get('source_mesh'))]
            if lod.get('ingame'):
                variants.append(('ingame', lod['ingame'], lod.get('ingame_mesh')))
            for vname, rec, mesh_rel in variants:
                obj = os.path.join(out, rec['obj'])
                if not os.path.exists(obj):
                    fail['obj_missing'].append(rec['obj'])
                    continue
                c, usemtl, mtllib, bad, vts = parse_obj(obj)
                cnt['obj_parsed'] += 1
                if bad:
                    fail['obj_bad_faces'].append('%s: %s' % (rec['obj'], bad[0]))
                for k_obj, k_man in (('v', 'vertices'), ('vt', 'uvs'), ('f', 'polygons')):
                    if c[k_obj] != rec[k_man]:
                        fail['obj_count_mismatch'].append('%s: %s %d != manifest %s %d'
                                                          % (rec['obj'], k_obj, c[k_obj], k_man, rec[k_man]))
                if 'normals' in rec and c['vn'] != rec['normals']:
                    fail['obj_count_mismatch'].append('%s: vn %d != %d' % (rec['obj'], c['vn'], rec['normals']))
                mtl = os.path.join(os.path.dirname(obj), mtllib or '')
                if not mtllib or not os.path.exists(mtl):
                    fail['mtl_missing'].append(rec['obj'])
                    continue
                names, maps = parse_mtl(mtl)
                cnt['mtl_parsed'] += 1
                if usemtl - names:
                    fail['usemtl_undefined'].append('%s: %s' % (rec['obj'], sorted(usemtl - names)[:3]))
                for m in maps:
                    cnt['mtl_map_refs'] += 1
                    if not os.path.exists(os.path.normpath(os.path.join(os.path.dirname(mtl), m))):
                        fail['mtl_map_missing'].append('%s -> %s' % (rec['mtl'], m))
                # independent reader
                mp = tree.resolve(mesh_rel) if mesh_rel else None
                if mp:
                    ok, msg = mesh_probe.probe(mp)
                    m = re.search(r'verts=(\d+) polys=(\d+)', msg)
                    cnt['mesh_probe_runs'] += 1
                    if not ok:
                        fail['mesh_probe_fail'].append('%s: %s' % (mesh_rel, msg))
                    elif not m or int(m.group(1)) != c['v'] or int(m.group(2)) != c['f']:
                        fail['mesh_probe_count_mismatch'].append('%s: probe %s vs obj v=%d f=%d'
                                                                 % (mesh_rel, m.group(0) if m else msg, c['v'],
                                                                    c['f']))
                    else:
                        cnt['mesh_probe_agree'] += 1
                if key in spot_ships and vname == 'source':
                    inside = sum(1 for (u, v) in vts if 0.0 <= u <= 1.0 and 0.0 <= v <= 1.0)
                    us = [u for u, _ in vts] or [0]
                    vs = [v for _, v in vts] or [0]
                    probe_counts = m.group(0) if mp and m else None
                    spot.setdefault(key, []).append(collections.OrderedDict([
                        ('lod', lod['level']), ('obj', rec['obj']), ('obj_v', c['v']), ('obj_f', c['f']),
                        ('mesh_probe', probe_counts),
                        ('faces_match_reader', bool(m) and int(m.group(1)) == c['v'] and int(m.group(2)) == c['f']),
                        ('vt_count', len(vts)), ('vt_in_0_1', inside),
                        ('vt_in_0_1_pct', round(100.0 * inside / max(1, len(vts)), 1)),
                        ('u_range', [round(min(us), 4), round(max(us), 4)]),
                        ('v_range', [round(min(vs), 4), round(max(vs), 4)]),
                        ('raw_uv_range_st', rec['uv_range']),
                        ('uv_corners_outside_0_1', rec['uv_corners_outside_0_1'])]))

    # 3, 4: PNGs against the .lif, and the independent texture probe
    seen = set()
    for key, ship in manifest['ships'].items():
        recs = list(ship['textures'].values()) + list(ship['pages'].values())
        for t in recs:
            if t['png'] in seen:
                continue
            seen.add(t['png'])
            lp = tree.resolve(t['lif'])
            with open(lp, 'rb') as fh:
                errs, _msg = tex_probe.probe_lif(lp, fh.read())
            cnt['textures_probe_runs'] += 1
            if errs:
                fail['textures_probe_fail'].append('%s: %s' % (t['lif'], errs[:2]))
            lif = hf.lif_read(lp)
            for png_key, want, mode in (('png', lif.rgba, 'RGBA'), ('team_base_png', lif.team0, 'L'),
                                        ('team_stripe_png', lif.team1, 'L')):
                if not t.get(png_key):
                    if png_key != 'png' and want is not None:
                        fail['mask_not_written'].append('%s %s' % (t['lif'], png_key))
                    continue
                w, h, md, px = hf.png_read(os.path.join(out, t[png_key]))
                cnt['png_decoded'] += 1
                if (w, h, md) != (t['width'], t['height'], mode):
                    fail['png_header_mismatch'].append('%s: %dx%d %s' % (t[png_key], w, h, md))
                elif px != want:
                    fail['png_pixels_differ_from_lif'].append(t[png_key])
                else:
                    cnt['png_pixels_equal_lif'] += 1

    # 6: page placements on the written PNGs
    for key, ship in manifest['ships'].items():
        for pg in ship['pages'].values():
            pw, ph, _m, ppx = hf.png_read(os.path.join(out, pg['png']))
            for it in pg['placements']:
                cnt['page_placements'] += 1
                if not it['exact_copy']:
                    cnt['page_placements_inexact'] += 1
                    continue
                sw, sh, _m2, spx = hf.png_read(os.path.join(out, it['texture_png']))
                if it['transposed']:
                    spx = __import__('export_ships').transpose(spx, sw, sh, 4)
                got = b''.join(ppx[((it['y'] + r) * pw + it['x']) * 4:((it['y'] + r) * pw + it['x'] + it['w']) * 4]
                               for r in range(it['h']))
                if got != spx:
                    fail['page_placement_mismatch'].append('%s %s' % (pg['png'], it['texture']))
                else:
                    cnt['page_placements_verified'] += 1

    res['counts'] = collections.OrderedDict(sorted(cnt.items()))
    res['failures'] = collections.OrderedDict((k, {'count': len(v), 'examples': v[:10]}) for k, v in sorted(fail.items()))
    res['spot_check'] = spot
    res['ok'] = not fail
    with open(os.path.join(out, 'validation.json'), 'w') as fh:
        json.dump(res, fh, indent=1)
        fh.write('\n')
    print('validation: %s' % ('PASS' if res['ok'] else 'FAIL'))
    for k, v in res['counts'].items():
        print('  %-28s %d' % (k, v))
    for k, v in res['failures'].items():
        print('  FAIL %-23s %d  e.g. %s' % (k, v['count'], v['examples'][:2]))
    for k, lods in spot.items():
        l0 = lods[0]
        print('  spot %-30s lod%d v=%d f=%d probe=[%s] vt in 0..1: %.1f%% u%s v%s' % (
            k, l0['lod'], l0['obj_v'], l0['obj_f'], l0['mesh_probe'], l0['vt_in_0_1_pct'], l0['u_range'],
            l0['v_range']))
    return 0 if res['ok'] else 1


def main(argv):
    ap = argparse.ArgumentParser(description=__doc__.split('\n')[0])
    ap.add_argument('--out', default=os.path.join(REPO, 'original', 'assets-export'))
    ap.add_argument('--data', default=None)
    ap.add_argument('--update', default=None)
    a = ap.parse_args(argv)
    return validate(os.path.abspath(a.out), a.data, a.update)


if __name__ == '__main__':
    sys.exit(main(sys.argv[1:]))
