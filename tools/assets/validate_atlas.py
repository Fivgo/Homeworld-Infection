#!/usr/bin/env python3
"""Check the texture-atlas export made by atlas_ships.py against the ship export it came from.

Usage:
  tools/assets/validate_atlas.py [--src DIR] [--atlas DIR] [--scratch DIR] [--samples N] [--seed N] [--keep]
                                 [--skip-roundtrip]

Checks (results printed and written to <atlas>/validation.json):
  (a) round trip: the unpainted atlases (and masks) are copied into a temporary return folder under --scratch and
      split with split_atlas.py at 1x; every texture it writes must be pixel-identical to the original PNG in --src,
      and every packed texture (and mask) must come back. (a2) the same with every atlas upscaled 2x
      (nearest neighbour): each texture must come back as its own 2x nearest-neighbour upscale.
  (b) geometry: every atlas OBJ has the same v / vn / f counts as its source, identical v and vn lines in the same
      order, the same v and vn indices on every face corner, and each face keeps its material (lit / glow, or the
      exception material) with a group naming the original material.
  (c) every atlas UV used by an atlased face lies within 0..1.
  (d) sampling: --samples random face corners per OBJ (nudged 5% toward the face centre, off the texel edges) must
      land in their slot on the texel that the original UV samples under GL_REPEAT, and that texel's colour must equal
      the original texture's.
  (e) images: power-of-two sizes as in the JSON, slots inside the atlas and not overlapping (gutter included), every
      slot + gutter equal to the source texture (and masks) tiled Nu x Nv with an edge-replicated gutter, and the
      space outside the slots empty.
"""
import argparse
import collections
import datetime
import json
import math
import os
import random
import shutil
import sys
import tempfile
import time

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import atlas_ships as at  # noqa: E402
import split_atlas  # noqa: E402


class Images(object):
    def __init__(self):
        self.cache = {}

    def get(self, path, mode):
        key = (path, mode)
        if key not in self.cache:
            if len(self.cache) > 64:
                self.cache.clear()
            w, h, data, _ = at.read_png(path, mode)
            self.cache[key] = (w, h, data)
        return self.cache[key]


def is_p2(n):
    return n > 0 and n & (n - 1) == 0


def slot_lookup(atlas_root, j, mname, jcache):
    """(atlas json, kind, slot, material record) for an atlased material, following hull fallbacks."""
    mr = j['materials'][mname]
    if mr.get('status') != 'atlas':
        return None
    if mr.get('piece_atlas_json'):
        pj_path = os.path.join(atlas_root, mr['piece_atlas_json'])
        if pj_path not in jcache:
            with open(pj_path) as fh:
                jcache[pj_path] = json.load(fh)
        pj = jcache[pj_path]
        pm = mname.split('__', 1)[1]
        pmr = pj['materials'][pm]
        kind = pmr['atlas']
        slot = [s for s in pj['atlases'][kind]['slots'] if s['id'] == pmr['slot']][0]
        return pj, kind, slot, pmr
    kind = mr['atlas']
    slot = [s for s in j['atlases'][kind]['slots'] if s['id'] == mr['slot']][0]
    return j, kind, slot, mr


def check_images(atlas_root, src_root, recs, res, imgs):
    """(e)"""
    g_fail = []
    n_slots = n_imgs = 0
    for j in recs:
        g = j['gutter_px']
        for kind, a in j.get('atlases', {}).items():
            W, H = a['width'], a['height']
            if not (is_p2(W) and is_p2(H)):
                g_fail.append('%s %s: %dx%d not a power of two' % (j['stem'], kind, W, H))
            boxes = []
            for s in a['slots']:
                x, y, w, h = s['rect']
                if x - g < 0 or y - g < 0 or x + w + g > W or y + h + g > H:
                    g_fail.append('%s %s slot %d outside the atlas' % (j['stem'], kind, s['id']))
                if w != s['texture_size'][0] * s['periods'][0] or h != s['texture_size'][1] * s['periods'][1]:
                    g_fail.append('%s %s slot %d: rect size does not match texture x periods' % (j['stem'], kind,
                                                                                               s['id']))
                boxes.append((x - g, y - g, w + 2 * g, h + 2 * g, s['id']))
            for i in range(len(boxes)):
                for k in range(i + 1, len(boxes)):
                    ax, ay, aw, ah, ai = boxes[i]
                    bx, by, bw, bh, bi = boxes[k]
                    if ax < bx + bw and bx < ax + aw and ay < by + bh and by < ay + ah:
                        g_fail.append('%s %s slots %d and %d overlap' % (j['stem'], kind, ai, bi))
            images = [(a['png'], a['mode'], None)] + [(a[mk + '_png'], 'L', mk) for mk, _s in at.MASKS
                                                      if a.get(mk + '_png')]
            for png, mode, mk in images:
                path = os.path.join(atlas_root, png)
                if (at.png_info(path)) != (W, H):
                    g_fail.append('%s: size differs from the JSON' % png)
                    continue
                n_imgs += 1
                _w, _h, data = imgs.get(path, mode)
                bpp = at._BPP[mode]
                expect = bytearray(W * H * bpp)
                blocks = []
                for s in a['slots']:
                    x, y, w, h = s['rect']
                    tw, th = s['texture_size']
                    if mk is None:
                        _tw, _th, tex = imgs.get(os.path.join(src_root, s['texture']), mode)
                    elif s.get(mk):
                        _tw, _th, tex = imgs.get(os.path.join(src_root, s[mk]), 'L')
                    else:
                        tex = bytes(tw * th)
                    rows = at.slot_rows(tex, tw, th, bpp, s['periods'][0], s['periods'][1], g)
                    at.blit(expect, W, bpp, x - g, y - g, rows)
                    blocks.append((s, rows))
                    if mk is None:
                        n_slots += 1
                if bytes(expect) == data:
                    continue
                bad = 0
                for s, rows in blocks:
                    x, y, w, h = s['rect']
                    got = split_atlas.cut(data, W, bpp, x - g, y - g, w + 2 * g, h + 2 * g)
                    if got != b''.join(rows):
                        bad += 1
                        g_fail.append('%s slot %d (%s): pixels or gutter differ from the source' % (
                            png, s['id'], os.path.basename(s['texture'])))
                if not bad:
                    g_fail.append('%s: pixels outside every slot are not empty' % png)
    res['e_images'] = collections.OrderedDict([
        ('pass', not g_fail), ('atlas_images', n_imgs), ('slots', n_slots), ('failures', g_fail[:50]),
        ('failure_count', len(g_fail))])


def check_objs(atlas_root, src_root, recs, res, imgs, samples, seed):
    """(b), (c), (d)"""
    rng = random.Random(seed)
    jcache = {}
    b_fail, c_fail, d_fail, ties = [], [], [], []
    clamp_max = [0.0, None]
    tot = collections.Counter()
    for j in recs:
        src = at.SrcObj(os.path.join(src_root, j['source_obj']))
        dst = at.SrcObj(os.path.join(atlas_root, j['atlas_obj']))
        name = j['stem']
        # (b) counts and geometry
        sv = [l for l in src.lines if l.startswith('v ')]
        dv = [l for l in dst.lines if l.startswith('v ')]
        sn = [l for l in src.lines if l.startswith('vn ')]
        dn = [l for l in dst.lines if l.startswith('vn ')]
        tot['objs'] += 1
        tot['faces'] += len(src.faces)
        if sv != dv:
            b_fail.append('%s: v lines differ (%d vs %d)' % (name, len(sv), len(dv)))
        if sn != dn:
            b_fail.append('%s: vn lines differ (%d vs %d)' % (name, len(sn), len(dn)))
        if len(src.faces) != len(dst.faces):
            b_fail.append('%s: %d faces vs %d' % (name, len(src.faces), len(dst.faces)))
            continue
        # group of every atlas face
        group_at = {}
        grp = None
        for i, raw in enumerate(dst.lines):
            if raw.startswith('g '):
                grp = raw[2:].strip()
            elif raw.startswith('o '):
                grp = None
            elif raw.startswith('f '):
                group_at[i] = grp
        bad_faces = 0
        cand = []
        for fi, ((_sl, sm, sc), (dl, dm, dc)) in enumerate(zip(src.faces, dst.faces)):
            if [(c[0], c[2]) for c in sc] != [(c[0], c[2]) for c in dc]:
                bad_faces += 1
                continue
            mr = j['materials'][sm]
            want = mr['atlas'] if mr.get('status') == 'atlas' else sm
            g = group_at.get(dl) or ''
            if dm != want or not (g == sm or g.endswith('__' + sm)):
                bad_faces += 1
                continue
            if mr.get('status') != 'atlas':
                tot['faces_exception'] += 1
                continue
            # clamped corners: how far the clamp moved them, in texels of the original texture
            if mr.get('clamped_corners'):
                look = slot_lookup(atlas_root, j, sm, jcache)
                u0, v0 = look[3]['uv_origin']
                nu, nv = look[3]['periods']
                tw, th = look[2]['texture_size']
                for (_v, ti, _n) in sc:
                    u, v = float(src.vt[ti - 1][0]), float(src.vt[ti - 1][1])
                    dpx = max(max(0.0, u0 - u, u - (u0 + nu)) * tw, max(0.0, v0 - v, v - (v0 + nv)) * th)
                    if dpx > clamp_max[0]:
                        clamp_max[:] = [dpx, '%s %s uv %s %s (%dx%d texture)' % (name, sm, u, v, tw, th)]
            # (c)
            for (_v, ti, _n) in dc:
                u, v = float(dst.vt[ti - 1][0]), float(dst.vt[ti - 1][1])
                tot['uv_corners_checked'] += 1
                if not (0.0 <= u <= 1.0 and 0.0 <= v <= 1.0):
                    c_fail.append('%s face %d: atlas uv %s %s outside 0..1' % (name, fi, u, v))
            cand.append(fi)
        if bad_faces:
            b_fail.append('%s: %d faces differ in v/vn indices or material/group' % (name, bad_faces))
        # (d) sampling
        for fi in rng.sample(cand, min(samples, len(cand))):
            _sl, sm, sc = src.faces[fi]
            _dl, _dm, dc = dst.faces[fi]
            k = rng.randrange(len(sc))
            look = slot_lookup(atlas_root, j, sm, jcache)
            aj, kind, s, mr = look
            a = aj['atlases'][kind]
            W, H = a['width'], a['height']
            u0, v0 = mr['uv_origin']
            nu, nv = mr['periods']
            S = [(float(src.vt[c[1] - 1][0]), float(src.vt[c[1] - 1][1])) for c in sc]
            A = [(float(dst.vt[c[1] - 1][0]), float(dst.vt[c[1] - 1][1])) for c in dc]
            if any(not (u0 <= u <= u0 + nu and v0 <= v <= v0 + nv) for (u, v) in S):
                tot['samples_skipped_clamped_face'] += 1
                continue
            t = 0.05
            cs = (sum(p[0] for p in S) / len(S), sum(p[1] for p in S) / len(S))
            ca = (sum(p[0] for p in A) / len(A), sum(p[1] for p in A) / len(A))
            ps = (S[k][0] + t * (cs[0] - S[k][0]), S[k][1] + t * (cs[1] - S[k][1]))
            pa = (A[k][0] + t * (ca[0] - A[k][0]), A[k][1] + t * (ca[1] - A[k][1]))
            tw, th = s['texture_size']
            fu = (ps[0] - math.floor(ps[0])) * tw
            sv_ = 1.0 - ps[1]
            fv = (sv_ - math.floor(sv_)) * th
            tx, ty = min(int(fu), tw - 1), min(int(fv), th - 1)
            axf, ayf = pa[0] * W, (1.0 - pa[1]) * H
            ax, ay = int(math.floor(axf)), int(math.floor(ayf))
            sx, sy, sw, sh = s['rect']
            pos_ok = sx <= ax < sx + sw and sy <= ay < sy + sh and (ax - sx) % tw == tx and (ay - sy) % th == ty
            mode = a['mode']
            bpp = at._BPP[mode]
            _aw, _ah, apx = imgs.get(os.path.join(atlas_root, a['png']), mode)
            _tw, _th, tpx = imgs.get(os.path.join(src_root, s['texture']), mode)
            col_ok = False
            if 0 <= ax < W and 0 <= ay < H:
                col_ok = apx[(ay * W + ax) * bpp:(ay * W + ax + 1) * bpp] == tpx[(ty * tw + tx) * bpp:
                                                                                 (ty * tw + tx + 1) * bpp]
            tot['samples'] += 1
            if pos_ok and col_ok:
                tot['samples_pass'] += 1
                continue
            near = min(abs(x - round(x)) for x in (fu, fv, axf, ayf)) < 0.01
            if near:
                tot['samples_texel_edge_tie'] += 1
                ties.append('%s face %d corner %d: src texel position %.4f %.4f, atlas pixel position %.4f %.4f' % (
                    name, fi, k, fu, fv, axf, ayf))
            else:
                d_fail.append('%s face %d corner %d (%s): src uv %.5f %.5f -> texel %d,%d; atlas uv %.6f %.6f -> '
                              'pixel %d,%d (slot %s); position %s, colour %s' % (
                                  name, fi, k, sm, S[k][0], S[k][1], tx, ty, A[k][0], A[k][1], ax, ay, s['rect'],
                                  'ok' if pos_ok else 'WRONG', 'ok' if col_ok else 'WRONG'))
        tot['clamped_corners'] += sum(m.get('clamped_corners', 0) for m in j['materials'].values())
    res['b_geometry'] = collections.OrderedDict([('pass', not b_fail), ('objs', tot['objs']), ('faces', tot['faces']),
                                                 ('faces_on_exception_materials', tot['faces_exception']),
                                                 ('failures', b_fail[:50]), ('failure_count', len(b_fail))])
    res['c_uv_range'] = collections.OrderedDict([('pass', not c_fail), ('corners', tot['uv_corners_checked']),
                                                 ('failures', c_fail[:50]), ('failure_count', len(c_fail))])
    res['d_sampling'] = collections.OrderedDict([
        ('pass', not d_fail), ('samples', tot['samples']), ('same_texel_and_colour', tot['samples_pass']),
        ('texel_edge_ties', tot['samples_texel_edge_tie']),
        ('skipped_faces_with_clamped_corners', tot['samples_skipped_clamped_face']),
        ('clamped_corners_total', tot['clamped_corners']),
        ('clamped_corner_max_shift_texels', round(clamp_max[0], 3)), ('clamped_corner_max_shift_at', clamp_max[1]),
        ('texel_edge_tie_examples', ties[:10]),
        ('note', 'corners clamped by the 0.02 overshoot rule sample the slot edge instead of the wrapped texel; faces '
                 'with such a corner are not sampled'),
        ('failures', d_fail[:50]), ('failure_count', len(d_fail))])


def upscale2(data, w, h, bpp):
    """Nearest-neighbour 2x (every pixel becomes a 2 x 2 block)."""
    out = bytearray()
    for r in range(h):
        row = data[r * w * bpp:(r + 1) * w * bpp]
        dbl = bytearray(2 * w * bpp)
        for k in range(bpp):
            ch = row[k::bpp]
            dbl[k::2 * bpp] = ch
            dbl[bpp + k::2 * bpp] = ch
        out += dbl
        out += dbl
    return bytes(out)


def roundtrip(atlas_root, src_root, recs, res, scratch, keep, factor=1, key='a_roundtrip'):
    """(a) at 1x: copy the atlases and split them. At 2x: nearest-upscale them first and compare with the
    nearest-upscaled originals (exercises the scale handling of split_atlas.py)."""
    work = tempfile.mkdtemp(prefix='atlas-roundtrip-%dx-' % factor, dir=scratch)
    ret_in = os.path.join(work, 'assets-return-atlas')
    ret_out = os.path.join(work, 'assets-return')
    expected = set()
    n_copied = 0
    for j in recs:
        for a in j.get('atlases', {}).values():
            for png in [a['png']] + [a.get(mk + '_png') for mk, _s in at.MASKS]:
                if not png:
                    continue
                dst = os.path.join(ret_in, png[len('textures/'):])
                os.makedirs(os.path.dirname(dst), exist_ok=True)
                if factor == 1:
                    shutil.copyfile(os.path.join(atlas_root, png), dst)
                else:
                    mode = a['mode'] if png == a['png'] else 'L'
                    w, h, data, _ = at.read_png(os.path.join(atlas_root, png), mode)
                    at.write_png(dst, 2 * w, 2 * h, upscale2(data, w, h, at._BPP[mode]), mode)
                n_copied += 1
            for s in a['slots']:
                expected.add((s['return_png'], 'RGBA', s['texture']))
                for mk, sfx in at.MASKS:
                    if s.get(mk):
                        expected.add((s['return_png'][:-4] + sfx + '.png', 'L', s[mk]))
    logs = []
    sp = split_atlas.Splitter(ret_in, ret_out, atlas_root, log=logs.append)
    r = sp.run()
    fails = []
    identical = 0
    written = set(r['written'])
    for rel, mode, src_rel in sorted(expected):
        if rel not in written:
            fails.append('not written: %s' % rel)
            continue
        a = at.read_png(os.path.join(ret_out, rel), mode)
        b = at.read_png(os.path.join(src_root, src_rel), mode)
        if factor == 2:
            b = (2 * b[0], 2 * b[1], upscale2(b[2], b[0], b[1], at._BPP[mode]))
        if a[:3] == b[:3]:
            identical += 1
        else:
            fails.append('differs: %s' % rel)
    extra = written - set(e[0] for e in expected)
    for e in sorted(extra):
        fails.append('unexpected output: %s' % e)
    res[key] = collections.OrderedDict([
        ('pass', not fails and not r['errors'] and not r['warnings']), ('atlas_images_copied', n_copied),
        ('textures_expected', len(expected)), ('written', len(written)), ('pixel_identical', identical),
        ('splitter_warnings', r['warnings'][:20]), ('splitter_errors', r['errors'][:20]),
        ('failures', fails[:50]), ('failure_count', len(fails)), ('scratch', work if keep else None)])
    if not keep:
        shutil.rmtree(work)


def main(argv):
    ap = argparse.ArgumentParser(description=__doc__.split('\n')[0])
    ap.add_argument('--src', default=at.DEF_SRC)
    ap.add_argument('--atlas', default=at.DEF_OUT)
    ap.add_argument('--scratch', default=None, help='parent folder for the round-trip copy (default: system temp)')
    ap.add_argument('--samples', type=int, default=30, help='random face corners per OBJ for check (d)')
    ap.add_argument('--seed', type=int, default=1)
    ap.add_argument('--keep', action='store_true', help='keep the round-trip folder')
    ap.add_argument('--skip-roundtrip', action='store_true')
    args = ap.parse_args(argv)
    src, atlas = os.path.abspath(args.src), os.path.abspath(args.atlas)
    t0 = time.time()
    recs = at.collect(atlas)
    imgs = Images()
    res = collections.OrderedDict()
    res['tool'] = 'tools/assets/validate_atlas.py'
    res['generated'] = datetime.datetime.now().strftime('%Y-%m-%d %H:%M')
    res['atlas_json_files'] = len(recs)
    check_objs(atlas, src, recs, res, imgs, args.samples, args.seed)
    check_images(atlas, src, recs, res, imgs)
    if not args.skip_roundtrip:
        if args.scratch:
            os.makedirs(args.scratch, exist_ok=True)
        roundtrip(atlas, src, recs, res, args.scratch, args.keep)
        roundtrip(atlas, src, recs, res, args.scratch, args.keep, 2, 'a2_roundtrip_2x')
    res['seconds'] = round(time.time() - t0, 1)
    ok = all(v['pass'] for k, v in res.items() if isinstance(v, dict) and 'pass' in v)
    res['all_pass'] = ok
    at.write_file(os.path.join(atlas, 'validation.json'), json.dumps(res, indent=1) + '\n')
    for k, v in res.items():
        if isinstance(v, dict) and 'pass' in v:
            brief = ', '.join('%s=%s' % (kk, vv) for kk, vv in v.items()
                              if kk not in ('pass', 'failures', 'note', 'splitter_warnings', 'splitter_errors',
                                            'scratch', 'texel_edge_tie_examples', 'clamped_corner_max_shift_at'))
            print('%-14s %s  %s' % (k, 'PASS' if v['pass'] else 'FAIL', brief))
            for f in v.get('failures', [])[:8]:
                print('    ' + f)
            for f in (v.get('splitter_warnings') or [])[:5] + (v.get('splitter_errors') or [])[:5]:
                print('    splitter: ' + f)
    print('%s in %.0fs -> %s' % ('ALL PASS' if ok else 'FAILURES', res['seconds'],
                                 os.path.join(atlas, 'validation.json')))
    return 0 if ok else 1


if __name__ == '__main__':
    sys.exit(main(sys.argv[1:]))
