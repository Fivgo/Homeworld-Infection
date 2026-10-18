#!/usr/bin/env python3
"""Return path of the texture-atlas export: cut painted atlases back into the separate textures.

Usage:
  tools/assets/split_atlas.py [--in DIR] [--out DIR] [--atlas DIR] [--piece RACE/NAME ...] [--dry-run]
                              [--overwrite] [--threshold N]

Defaults: --in original/assets-return-atlas, --out original/assets-return, --atlas original/assets-export-atlas (the
atlas export, read only: it holds the <stem>_atlas.json slot tables). When run from a git worktree, original/ is
taken from the main checkout (see atlas_ships.py).

Reads   <in>/<race>/<folder>/lod0/<stem>_lit.png, <stem>_glow.png and, optionally, the masks
        <stem>_<kind>_teambase.png / _teamstripe.png (same relative layout as textures/ in the atlas export).
Writes  <out>/<race>/<part>/lod0/<texture>.png (+ <texture>_teambase.png / _teamstripe.png): the slot's
        "return_png", i.e. the path the first export's return step expects.

* The scale factor is painted width / atlas width; it must equal painted height / atlas height (any whole or
  fractional factor), and every slot must land on whole pixels at that factor, or the atlas is refused.
* A baked-repeat slot (periods Nu x Nv > 1) gives back its first (top-left) period; the other periods are compared
  with it and a warning names the texture when the mean absolute difference exceeds --threshold (0..255 scale).
* A texture in several slots (lit and glow, tiled and untiled, a piece atlas and a whole-hull atlas) is taken from the
  best-ranked returned slot (fewest periods, lit before glow, piece atlas before hull atlas); differing copies warn.
* Never writes under the atlas export or original/assets-export. An existing output file is replaced only when this
  tool wrote it (sha1 in <out>/.split_atlas_ledger.json) and nobody changed it since, or with --overwrite.
"""
import argparse
import collections
import fractions
import glob
import hashlib
import json
import os
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import atlas_ships as at  # noqa: E402

try:
    from PIL import Image, ImageChops, ImageStat
except ImportError:
    Image = None

DEF_IN = at.DEF_RETURN_ATLAS
DEF_OUT = at.DEF_RETURN
DEF_ATLAS = at.DEF_OUT
THRESHOLD = 3.0
LEDGER = '.split_atlas_ledger.json'


def sha1(b):
    return hashlib.sha1(b).hexdigest()


def cut(buf, W, bpp, x, y, w, h):
    return b''.join(buf[((y + r) * W + x) * bpp:((y + r) * W + x + w) * bpp] for r in range(h))


def mean_abs_diff(a, b, mode, w, h):
    """Mean absolute difference per channel value, 0..255."""
    if a == b:
        return 0.0
    if Image is not None:
        ia, ib = Image.frombytes(mode, (w, h), a), Image.frombytes(mode, (w, h), b)
        m = ImageStat.Stat(ImageChops.difference(ia, ib)).mean
        return sum(m) / len(m)
    step = max(1, len(a) // 200000)
    n = s = 0
    for i in range(0, len(a), step):
        s += abs(a[i] - b[i])
        n += 1
    return s / float(n)


class Splitter(object):

    def __init__(self, in_dir, out_dir, atlas_dir, dry_run=False, overwrite=False, threshold=THRESHOLD, log=print):
        self.inp = os.path.abspath(in_dir)
        self.out = os.path.abspath(out_dir)
        self.atlas = os.path.abspath(atlas_dir)
        self.dry_run = dry_run
        self.overwrite = overwrite
        self.threshold = threshold
        self.log = log
        self.warnings = []
        self.errors = []
        self.candidates = collections.OrderedDict()   # out rel -> [candidate]
        self.consumed = set()
        forbidden = [self.atlas, at.DEF_OUT, at.DEF_SRC, os.path.join(os.path.dirname(self.atlas), 'assets-export')]
        for f in forbidden:
            if at.under(self.out, f) or at.under(f, self.out):
                raise SystemExit('refusing: --out %s overlaps %s (the exports are read only)' % (self.out, f))

    def warn(self, msg):
        self.warnings.append(msg)
        self.log('WARNING: ' + msg)

    def error(self, msg):
        self.errors.append(msg)
        self.log('REFUSED: ' + msg)

    # ---------------------------------------------------------------- reading
    def atlas_jsons(self, pieces=None):
        recs = []
        for jp in sorted(glob.glob(os.path.join(self.atlas, 'textures', '*', '*', 'lod0', '*_atlas.json'))):
            with open(jp) as fh:
                j = json.load(fh)
            if pieces and '%s/%s' % (j['race'], j['stem']) not in pieces:
                continue
            recs.append(j)
        # piece atlases first, so that they win over whole-hull atlases
        recs.sort(key=lambda j: (j['kind'] != 'piece', j['race'], j['stem']))
        return recs

    def factor(self, label, pw, ph, a):
        fx = fractions.Fraction(pw, a['width'])
        fy = fractions.Fraction(ph, a['height'])
        if fx != fy:
            self.error('%s is %d x %d but the atlas is %d x %d: width x%s, height x%s. Scale both sides by the '
                       'same factor (2x, 4x ...) and do not crop or pad.' % (label, pw, ph, a['width'], a['height'],
                                                                             fx, fy))
            return None
        bad = []
        for s in a['slots']:
            x, y = s['rect'][:2]
            tw, th = s['texture_size']
            if any((v * fx).denominator != 1 for v in (x, y, tw, th)):
                bad.append(os.path.basename(s['texture']))
        if bad:
            self.error('%s: scale factor %s does not put every slot on whole pixels (%s); use a whole factor such '
                       'as 2 or 4' % (label, fx, ', '.join(bad[:5]) + (' ...' if len(bad) > 5 else '')))
            return None
        return fx

    # ---------------------------------------------------------------- one returned atlas
    def take(self, j, order, kind, a):
        race, folder, stem = j['race'], j['folder'], j['stem']
        rel_dir = '%s/%s/lod0' % (race, folder)
        base_path = os.path.join(self.inp, rel_dir, '%s_%s.png' % (stem, kind))
        found = False
        f_base = None
        if os.path.isfile(base_path):
            found = True
            self.consumed.add(os.path.realpath(base_path))
            label = '%s/%s_%s.png' % (rel_dir, stem, kind)
            try:
                pw, ph, rgba, has_a = at.read_png(base_path, 'RGBA')
            except Exception as e:  # noqa: BLE001 - report any decoder failure as a refusal
                self.error('%s: cannot read (%s)' % (label, e))
                return found
            f_base = self.factor(label, pw, ph, a)
            if f_base is not None:
                if f_base != 1:
                    self.log('%s: scale x%s' % (label, f_base))
                for s in a['slots']:
                    self.slot(j, order, kind, s, rgba, pw, 4, f_base, label, 'base', has_a)
        for mk, sfx in at.MASKS:
            mp = os.path.join(self.inp, rel_dir, '%s_%s%s.png' % (stem, kind, sfx))
            if not os.path.isfile(mp):
                continue
            found = True
            self.consumed.add(os.path.realpath(mp))
            label = '%s/%s_%s%s.png' % (rel_dir, stem, kind, sfx)
            try:
                pw, ph, lum, _ = at.read_png(mp, 'L')
            except Exception as e:  # noqa: BLE001
                self.error('%s: cannot read (%s)' % (label, e))
                continue
            fm = self.factor(label, pw, ph, a)
            if fm is None:
                continue
            if f_base is not None and fm != f_base:
                self.error('%s is scaled x%s but its base atlas x%s; scale masks by the same factor as the base' %
                           (label, fm, f_base))
                continue
            for s in a['slots']:
                self.slot(j, order, kind, s, lum, pw, 1, fm, label, mk, False)
        return found

    def slot(self, j, order, kind, s, buf, W, bpp, f, label, what, file_has_alpha):
        x, y = (int(v * f) for v in s['rect'][:2])
        tw, th = (int(v * f) for v in s['texture_size'])
        nu, nv = s['periods']
        tex = os.path.basename(s['texture'])
        first = cut(buf, W, bpp, x, y, tw, th)
        mode = 'RGBA' if bpp == 4 else 'L'
        if what != 'base' and not s.get(what):
            if first.count(0) == len(first):
                return                      # no mask originally and nothing painted into it
            self.log('note: %s: slot %s:%d (%s) has no original %s mask; writing a new one' % (
                label, kind, s['id'], tex, what))
        worst = (0.0, None)
        for pj in range(nv):
            for pi in range(nu):
                if pi == 0 and pj == 0:
                    continue
                other = cut(buf, W, bpp, x + pi * tw, y + pj * th, tw, th)
                d = mean_abs_diff(first, other, mode, tw, th)
                if d > worst[0]:
                    worst = (d, (pi, pj))
        if worst[0] > self.threshold:
            self.warn('%s: %s (%s) repeats %d x %d times in its slot and the copies differ (mean %.1f/255 at copy '
                      '%d,%d); only the first (top-left) copy is kept' % (label, tex, what, nu, nv, worst[0],
                                                                         worst[1][0], worst[1][1]))
        if what == 'base':
            alpha = first[3::4]
            if s['flags'].get('alpha') and not file_has_alpha:
                self.warn('%s: %s uses transparency but the returned atlas has no alpha channel; it comes back '
                          'opaque' % (label, tex))
            elif not s['flags'].get('alpha') and alpha.count(255) != len(alpha):
                self.warn('%s: %s was opaque but comes back with transparent pixels' % (label, tex))
            rel = s['return_png']
        else:
            rel = s['return_png'][:-4] + dict(at.MASKS)[what] + '.png'
        rank = at.slot_rank(kind, s, order)
        self.candidates.setdefault(rel, []).append({
            'rank': rank, 'w': tw, 'h': th, 'mode': mode, 'pixels': first,
            'source': '%s slot %s:%d' % (label, kind, s['id'])})

    # ---------------------------------------------------------------- writing
    def resolve_and_write(self):
        ledger_path = os.path.join(self.out, LEDGER)
        ledger = {}
        if os.path.isfile(ledger_path):
            with open(ledger_path) as fh:
                ledger = json.load(fh)
        written, unchanged, kept = [], [], []
        for rel, cands in sorted(self.candidates.items()):
            cands.sort(key=lambda c: c['rank'])
            best = cands[0]
            for c in cands[1:]:
                if (c['w'], c['h']) != (best['w'], best['h']):
                    self.warn('%s: copies differ in size (%dx%d from %s, %dx%d from %s); using %s' % (
                        rel, best['w'], best['h'], best['source'], c['w'], c['h'], c['source'], best['source']))
                    continue
                d = mean_abs_diff(best['pixels'], c['pixels'], best['mode'], best['w'], best['h'])
                if d > self.threshold:
                    self.warn('%s: two returned copies differ (mean %.1f/255): using %s, ignoring %s' % (
                        rel, d, best['source'], c['source']))
            data = at.hf.png_bytes(best['w'], best['h'], best['pixels'], best['mode'])
            path = os.path.join(self.out, rel)
            if os.path.isfile(path):
                with open(path, 'rb') as fh:
                    cur = fh.read()
                try:
                    cw, ch, cpx, _ = at.read_png(path, best['mode'])
                    same = (cw, ch, cpx) == (best['w'], best['h'], best['pixels'])
                except Exception:  # noqa: BLE001
                    same = False
                if same:
                    unchanged.append(rel)
                    continue
                if ledger.get(rel) != sha1(cur) and not self.overwrite:
                    kept.append(rel)
                    self.warn('%s exists and was not written by split_atlas.py (or was edited since): kept; use '
                              '--overwrite to replace it' % rel)
                    continue
            if self.dry_run:
                self.log('would write %s  (%dx%d %s, from %s)' % (rel, best['w'], best['h'], best['mode'],
                                                                 best['source']))
            else:
                at.write_file(path, data)
                ledger[rel] = sha1(data)
            written.append(rel)
        if not self.dry_run and written:
            at.write_file(ledger_path, json.dumps(ledger, indent=1, sort_keys=True) + '\n')
        return written, unchanged, kept

    def run(self, pieces=None):
        if not os.path.isdir(self.inp):
            self.error('no returned atlases: %s does not exist' % self.inp)
            return {'written': [], 'unchanged': [], 'kept': [], 'warnings': self.warnings, 'errors': self.errors}
        recs = self.atlas_jsons(pieces)
        atlases_found = 0
        for j in recs:
            order = 0 if j['kind'] == 'piece' else 1
            for kind, a in j.get('atlases', {}).items():
                if self.take(j, order, kind, a):
                    atlases_found += 1
        for p in sorted(glob.glob(os.path.join(self.inp, '**', '*.png'), recursive=True)):
            if os.path.realpath(p) not in self.consumed and not pieces:
                self.warn('%s does not match any atlas (expected <race>/<ship>/lod0/<ship>_lit.png, _glow.png or '
                          'their _teambase/_teamstripe masks); ignored' % at.relp(p, self.inp))
        written, unchanged, kept = self.resolve_and_write()
        self.log('%d returned atlas image set(s); %s %d texture file(s), %d unchanged, %d kept, %d warning(s), '
                 '%d refused' % (atlases_found, 'would write' if self.dry_run else 'wrote', len(written),
                                 len(unchanged), len(kept), len(self.warnings), len(self.errors)))
        return {'written': written, 'unchanged': unchanged, 'kept': kept, 'warnings': self.warnings,
                'errors': self.errors, 'atlases': atlases_found}


def main(argv):
    ap = argparse.ArgumentParser(description=__doc__.split('\n')[0])
    ap.add_argument('--in', dest='inp', default=DEF_IN, help='painted atlases (default original/assets-return-atlas)')
    ap.add_argument('--out', default=DEF_OUT, help='separate textures (default original/assets-return)')
    ap.add_argument('--atlas', default=DEF_ATLAS, help='the atlas export with the slot tables (read only)')
    ap.add_argument('--piece', action='append', help='race/piece or race/<hull>_assembled; repeatable')
    ap.add_argument('--dry-run', action='store_true', help='list what would be written, write nothing')
    ap.add_argument('--overwrite', action='store_true', help='replace output files this tool did not write')
    ap.add_argument('--threshold', type=float, default=THRESHOLD,
                    help='mean difference (0..255) above which differing repeat copies warn (default %(default)s)')
    args = ap.parse_args(argv)
    sp = Splitter(args.inp, args.out, args.atlas, args.dry_run, args.overwrite, args.threshold)
    pieces = set(p.strip('/').lower() for p in args.piece) if args.piece else None
    res = sp.run(pieces)
    return 1 if res['errors'] else 0


if __name__ == '__main__':
    sys.exit(main(sys.argv[1:]))
