#!/usr/bin/env python3
"""Texture-atlas iteration of the ship export: two images ('lit' and 'glow') per ship piece and per assembled hull.

Usage:
  tools/assets/atlas_ships.py [--src DIR] [--out DIR] [--piece RACE/NAME ...] [--force] [--gutter PX]
                              [--max-periods N] [--max-slot PX] [--hull-max PX] [--readme-only]

Defaults: --src original/assets-export (READ ONLY: this tool never writes there), --out original/assets-export-atlas.
When run from a git worktree under .claude/worktrees/, `original/` is taken from the main checkout (it is gitignored
and exists only there). --piece takes race/piece (`race_sect/scarrier`) or race/<hull>_assembled
(`race_sect/scarrier_assembled`) and may be repeated. Pieces whose outputs already exist (and whose source OBJ/MTL
did not change) are skipped unless --force. README.md and atlas_index.json in --out are rebuilt from every
<stem>_atlas.json on disk at the end of every run. Python 3 standard library; PIL is used for PNG reading when present.

Writes, for every LOD 0 piece with at least one textured material and for every <hull>_assembled_lod0.obj:
  ships/<race>/<piece>/<stem>_atlas_lod0.obj|.mtl        the source OBJ with v / vn / f copied unchanged (same order),
                                                         only vt rewritten; materials 'lit' and 'glow' (+ exceptions)
  textures/<race>/<piece>/lod0/<stem>_lit.png            lit atlas (RGB, or RGBA when a packed texture has alpha)
  textures/<race>/<piece>/lod0/<stem>_glow.png           glow (self-illuminated) atlas, only when there are glow faces
  textures/<race>/<piece>/lod0/<stem>_<kind>_teambase.png / _teamstripe.png   mask atlases, same slot layout
  textures/<race>/<piece>/lod0/<stem>_atlas.json         slots, periods, UV origins, flags, materials, exceptions
where <stem> is the piece name, or <hull>_assembled for an assembled hull.

Rules (see README.md in --out for the user-facing version):
  * One slot per (lit|glow, texture file, Nu, Nv). Faces leaving 0..1 repeat the texture in the game (OpenGL default
    GL_REPEAT; the ship texture loader never sets a wrap mode), so a material's repeats are baked: u0 = floor(min u),
    u1 = ceil(max u), Nu = u1 - u0 (same for v, on the OBJ vt as written), the slot holds the texture Nu x Nv times and
    u' = (u - u0) / Nu. Corners within CLAMP_EPS past a whole number are clamped first, so 1.01 does not cost a period.
  * Slot (x, y, w, h) in atlas pixels, row 0 = top: atlas u = (x + u' w) / W, atlas v = 1 - (y + (1 - v') h) / H, so
    the slot's top-left period is the texture itself, unshifted.
  * A material over the cap (more than --max-periods periods on an axis, or a slot side over --max-slot px at 1x)
    stays out of the atlas as its own material pointing at the original PNG in --src. Untextured (colour-only)
    materials stay as they are.
  * Gutter: --gutter px around every slot, filled by edge replication. Power-of-two atlas sizes, max-rects packing
    (best short side fit), lit and glow packed independently.
  * Assembled hulls get one whole-hull atlas when it fits in --hull-max (4096) at 1x; otherwise their OBJ points at
    the per-piece atlases of their parts.
"""
import argparse
import collections
import datetime
import glob
import hashlib
import json
import math
import os
import re
import sys
import time

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import hwformats as hf  # noqa: E402

try:
    from PIL import Image
except ImportError:          # PIL is optional: hwformats.png_read handles the PNGs this tool and the exporter write
    Image = None

REPO = os.path.abspath(os.path.join(os.path.dirname(os.path.abspath(__file__)), '..', '..'))


def original_root():
    """REPO/original, or the main checkout's original/ when this file runs from a .claude/worktrees/ checkout."""
    here = os.path.join(REPO, 'original')
    if os.path.isdir(here):
        return here
    m = re.match(r'^(.*)[/\\]\.claude[/\\]worktrees[/\\][^/\\]+$', REPO)
    if m and os.path.isdir(os.path.join(m.group(1), 'original')):
        return os.path.join(m.group(1), 'original')
    return here


ORIGINAL = original_root()
DEF_SRC = os.path.join(ORIGINAL, 'assets-export')
DEF_OUT = os.path.join(ORIGINAL, 'assets-export-atlas')
DEF_RETURN_ATLAS = os.path.join(ORIGINAL, 'assets-return-atlas')
DEF_RETURN = os.path.join(ORIGINAL, 'assets-return')

GUTTER = 8             # px at 1x around every slot (>= 4 required)
MAX_PERIODS = 16       # baked repeats per axis
MAX_SLOT = 1024        # slot side at 1x, px
HULL_MAX = 4096        # whole-hull atlas side at 1x, px
PIECE_MAX = 8192       # a piece atlas that does not fit this is an error
CLAMP_EPS = 0.02
KINDS = ('lit', 'glow')
MASKS = (('teambase', '_teambase'), ('teamstripe', '_teamstripe'))
TOOL = 'tools/assets/atlas_ships.py'


# ------------------------------------------------------------------------------------------------------- helpers
def relp(path, start):
    return os.path.relpath(path, start).replace(os.sep, '/')


def fmt(x, nd=7):
    s = ('%.' + str(nd) + 'f') % x
    s = s.rstrip('0').rstrip('.')
    return '0' if s in ('-0', '', '-') else s


def write_file(path, data):
    os.makedirs(os.path.dirname(path), exist_ok=True)
    tmp = path + '.tmp'
    with open(tmp, 'wb' if isinstance(data, (bytes, bytearray)) else 'w') as fh:
        fh.write(data)
    os.replace(tmp, path)


def file_sha1(path):
    h = hashlib.sha1()
    with open(path, 'rb') as fh:
        h.update(fh.read())
    return h.hexdigest()


def under(path, root):
    """True when path is root or inside it (real paths)."""
    p, r = os.path.realpath(path), os.path.realpath(root)
    return p == r or os.path.commonpath([p, r]) == r


# ------------------------------------------------------------------------------------------------------- PNG I/O
_BPP = {'L': 1, 'RGB': 3, 'RGBA': 4}


def convert_pixels(data, src, dst):
    """Raw 8-bit pixel conversion between L / RGB / RGBA (grey = ITU-R 601 luma, as PIL; exact for grey pixels)."""
    if src == dst:
        return bytes(data)
    if src == 'L':
        if dst == 'RGB':
            return bytes(b for l in data for b in (l, l, l))
        return bytes(b for l in data for b in (l, l, l, 255))
    n = _BPP[src]
    if dst == 'L':
        r, g, b = data[0::n], data[1::n], data[2::n]
        return bytes((x * 19595 + y * 38470 + z * 7471 + 0x8000) >> 16 for x, y, z in zip(r, g, b))
    if src == 'RGBA' and dst == 'RGB':
        out = bytearray(len(data) // 4 * 3)
        out[0::3], out[1::3], out[2::3] = data[0::4], data[1::4], data[2::4]
        return bytes(out)
    if src == 'RGB' and dst == 'RGBA':
        out = bytearray(b'\xff' * (len(data) // 3 * 4))
        out[0::4], out[1::4], out[2::4] = data[0::3], data[1::3], data[2::3]
        return bytes(out)
    raise ValueError('cannot convert %s to %s' % (src, dst))


def read_png(path, mode):
    """Decode a PNG into raw 8-bit pixels of `mode` ('L', 'RGB' or 'RGBA'). Returns (w, h, bytes, file_has_alpha).
    With PIL: any PNG PIL reads (1/8/16-bit, grey, grey+alpha, RGB, RGBA, palette, interlaced). Without PIL:
    8-bit L / RGB / RGBA, not interlaced (hwformats.png_read)."""
    if Image is not None:
        with Image.open(path) as im:
            im.load()
            if im.mode == 'P' and 'transparency' in im.info:
                im = im.convert('RGBA')
            has_a = im.mode in ('RGBA', 'LA', 'PA', 'RGBa', 'La')
            if im.mode.startswith('I') or im.mode == 'F':
                im = im.convert('I').point(lambda v: v * (1.0 / 257.0)).convert('L')
            if im.mode != mode:
                im = im.convert(mode)
            return im.size[0], im.size[1], im.tobytes(), has_a
    w, h, m, data = hf.png_read(path)
    return w, h, convert_pixels(data, m, mode), m == 'RGBA'


def png_info(path):
    """(w, h) from the IHDR chunk."""
    with open(path, 'rb') as fh:
        head = fh.read(24)
    if head[:8] != b'\x89PNG\r\n\x1a\n':
        raise ValueError('%s: not a PNG' % path)
    return int.from_bytes(head[16:20], 'big'), int.from_bytes(head[20:24], 'big')


def write_png(path, w, h, data, mode):
    write_file(path, hf.png_bytes(w, h, data, mode))


# ------------------------------------------------------------------------------------------------------- OBJ / MTL
class SrcObj(object):
    """A Wavefront OBJ kept line by line, with the vt table and every face's corners and material."""

    def __init__(self, path):
        self.path = path
        with open(path) as fh:
            self.lines = fh.read().split('\n')
        if self.lines and self.lines[-1] == '':
            self.lines.pop()
        self.vt = []           # [(u_str, v_str)]
        self.faces = []        # [(line_index, material, [(v_str, vt_index or None, vn_str or None)])]
        self.mtllib = None
        self.counts = collections.Counter()
        mat = None
        for i, raw in enumerate(self.lines):
            line = raw.strip()
            tag, _, rest = line.partition(' ')
            if tag == 'vt':
                parts = rest.split()
                self.vt.append((parts[0], parts[1] if len(parts) > 1 else '0'))
            elif tag == 'f':
                corners = []
                for c in rest.split():
                    p = c.split('/')
                    ti = int(p[1]) if len(p) > 1 and p[1] != '' else None
                    if ti is not None and ti <= 0:
                        raise ValueError('%s:%d: relative vt index not supported' % (path, i + 1))
                    corners.append((p[0], ti, p[2] if len(p) > 2 and p[2] != '' else None))
                self.faces.append((i, mat, corners))
            elif tag == 'usemtl':
                mat = rest.strip()
            elif tag == 'mtllib':
                self.mtllib = rest.strip()
            if tag in ('v', 'vn', 'vt', 'f', 'o', 'g', 'usemtl'):
                self.counts[tag] += 1


def parse_mtl(path):
    """newmtl blocks: name -> {'lines': [...raw...], 'keys': {key: value}, 'masks': {'teambase': path, ...}}."""
    mats = collections.OrderedDict()
    cur = None
    with open(path) as fh:
        for raw in fh:
            s = raw.strip()
            if s.startswith('newmtl '):
                cur = s[7:].strip()
                mats[cur] = {'lines': [], 'keys': {}, 'masks': {}}
                continue
            if cur is None or not s:
                continue
            mats[cur]['lines'].append(s)
            if s.startswith('# team base-colour mask: '):
                mats[cur]['masks']['teambase'] = s.split(': ', 1)[1]
            elif s.startswith('# team stripe-colour mask: '):
                mats[cur]['masks']['teamstripe'] = s.split(': ', 1)[1]
            elif not s.startswith('#'):
                k, _, v = s.partition(' ')
                mats[cur]['keys'][k] = v.strip()
    return mats


def material_flags(name, tex_stem, hull):
    """glow / 2sided / smooth from the exporter's naming: <texture>[_glow][_2sided][_smooth][_vN], with a
    '<label>__' prefix in assembled hulls."""
    base = name.split('__', 1)[1] if hull and '__' in name else name
    stem = re.sub(r'[^A-Za-z0-9_.-]', '_', tex_stem.lower()) if tex_stem else None
    suffix = base[len(stem):] if stem and base.startswith(stem) else None
    m = re.match(r'^(_glow)?(_2sided)?(_smooth)?(_v\d+)?$', suffix) if suffix is not None else None
    if m is None:
        m = re.search(r'(_glow)?(_2sided)?(_smooth)?(_v\d+)?$', base)
    return {'glow': bool(m.group(1)), 'two_sided': bool(m.group(2)), 'smooth': bool(m.group(3))}


# ------------------------------------------------------------------------------------------------------- periods
def periods(lo, hi, eps=CLAMP_EPS):
    """(origin, count) of whole texture periods covering lo..hi, ignoring overshoots of at most eps past a whole
    number (those corners are clamped onto the slot edge)."""
    a = math.floor(lo + eps)
    b = math.ceil(hi - eps)
    if b - a < 1:
        a = math.floor(lo)
        b = max(a + 1, math.ceil(hi))
    return a, b - a


# ------------------------------------------------------------------------------------------------------- packing
def _maxrects(W, H, sizes, order):
    free = [(0, 0, W, H)]
    pos = [None] * len(sizes)
    for i in order:
        w, h = sizes[i]
        best = None
        for (fx, fy, fw, fh) in free:
            if w <= fw and h <= fh:
                key = (min(fw - w, fh - h), max(fw - w, fh - h), fy, fx)
                if best is None or key < best[0]:
                    best = (key, fx, fy)
        if best is None:
            return None
        x, y = best[1], best[2]
        pos[i] = (x, y)
        nf = []
        for f in free:
            fx, fy, fw, fh = f
            if x >= fx + fw or x + w <= fx or y >= fy + fh or y + h <= fy:
                nf.append(f)
                continue
            if x > fx:
                nf.append((fx, fy, x - fx, fh))
            if x + w < fx + fw:
                nf.append((x + w, fy, fx + fw - x - w, fh))
            if y > fy:
                nf.append((fx, fy, fw, y - fy))
            if y + h < fy + fh:
                nf.append((fx, y + h, fw, fy + fh - y - h))
        free = []
        for k, a in enumerate(nf):
            ax, ay, aw, ah = a
            dominated = False
            for j, b in enumerate(nf):
                if j == k:
                    continue
                bx, by, bw, bh = b
                if bx <= ax and by <= ay and bx + bw >= ax + aw and by + bh >= ay + ah and (b != a or j < k):
                    dominated = True
                    break
            if not dominated:
                free.append(a)
    return pos


def pack(sizes, max_side):
    """Pack rectangles (w, h) into the smallest power-of-two atlas (W, H), W >= H preferred. Returns
    (W, H, [(x, y), ...]) or None when nothing up to max_side x max_side fits."""
    if not sizes:
        return None
    p2 = [1 << k for k in range(4, 15) if (1 << k) <= max_side]
    area = sum(w * h for w, h in sizes)
    mw = max(w for w, h in sizes)
    mh = max(h for w, h in sizes)
    cands = sorted(((W, H) for W in p2 for H in p2 if W * H >= area and W >= mw and H >= mh),
                   key=lambda s: (s[0] * s[1], abs(math.log2(s[0]) - math.log2(s[1])), -s[0]))
    order = sorted(range(len(sizes)), key=lambda i: (-max(sizes[i]), -sizes[i][0] * sizes[i][1], i))
    for W, H in cands:
        pos = _maxrects(W, H, sizes, order)
        if pos is not None:
            return W, H, pos
    return None


# ------------------------------------------------------------------------------------------------------- pixels
def slot_rows(tex, tw, th, bpp, nu, nv, g):
    """Rows (bytes) of one slot with its gutter: the texture tiled nu x nv times, then g edge-replicated pixels
    on every side."""
    stride = tw * bpp
    body = []
    for r in range(th):
        body.append(tex[r * stride:(r + 1) * stride] * nu)
    body = body * nv
    rows = [row[:bpp] * g + row + row[-bpp:] * g for row in body]
    return [rows[0]] * g + rows + [rows[-1]] * g


def blit(dst, W, bpp, x, y, rows):
    for k, row in enumerate(rows):
        o = ((y + k) * W + x) * bpp
        dst[o:o + len(row)] = row


# ------------------------------------------------------------------------------------------------------- building
class AtlasBuilder(object):

    def __init__(self, src, out, gutter=GUTTER, max_periods=MAX_PERIODS, max_slot=MAX_SLOT, hull_max=HULL_MAX):
        self.src = os.path.abspath(src)
        self.out = os.path.abspath(out)
        if under(self.out, self.src) or under(self.src, self.out):
            raise SystemExit('refusing: --out %s and --src %s overlap' % (self.out, self.src))
        if gutter < 4:
            raise SystemExit('--gutter must be at least 4 px')
        self.gutter = gutter
        self.max_periods = max_periods
        self.max_slot = max_slot
        self.hull_max = hull_max
        self.tex_cache = {}

    # ---------------------------------------------------------------- sources
    def pieces(self):
        """[(race, folder, stem, obj_rel, is_hull)] for every LOD 0 OBJ, pieces first, then assembled hulls."""
        out, hulls = [], []
        for path in sorted(glob.glob(os.path.join(self.src, 'ships', '*', '*', '*_lod0.obj'))):
            rel = relp(path, self.src)
            _, race, folder, name = rel.split('/')
            stem = name[:-len('_lod0.obj')]
            if stem.endswith('_assembled'):
                hulls.append((race, folder, stem, rel, True))
            else:
                out.append((race, folder, stem, rel, False))
        return out + hulls

    def texture(self, tex_rel):
        """(w, h, rgba, alpha) of a source PNG; cached for the current item."""
        if tex_rel not in self.tex_cache:
            w, h, data, _ = read_png(os.path.join(self.src, tex_rel), 'RGBA')
            self.tex_cache[tex_rel] = (w, h, data, hf.has_alpha(data))
        return self.tex_cache[tex_rel]

    def mask(self, mask_rel, w, h):
        if mask_rel is None:
            return None
        key = ('mask', mask_rel)
        if key not in self.tex_cache:
            mw, mh, data, _ = read_png(os.path.join(self.src, mask_rel), 'L')
            if (mw, mh) != (w, h):
                raise ValueError('%s is %dx%d, its texture %dx%d' % (mask_rel, mw, mh, w, h))
            self.tex_cache[key] = data
        return self.tex_cache[key]

    # ---------------------------------------------------------------- outputs
    def paths(self, race, folder, stem):
        tex_dir = 'textures/%s/%s/lod0' % (race, folder)
        return {
            'obj': 'ships/%s/%s/%s_atlas_lod0.obj' % (race, folder, stem),
            'mtl': 'ships/%s/%s/%s_atlas_lod0.mtl' % (race, folder, stem),
            'json': '%s/%s_atlas.json' % (tex_dir, stem),
            'tex_dir': tex_dir,
        }

    def up_to_date(self, race, folder, stem, obj_rel):
        p = self.paths(race, folder, stem)
        jpath = os.path.join(self.out, p['json'])
        if not os.path.isfile(jpath):
            return False
        try:
            with open(jpath) as fh:
                j = json.load(fh)
        except ValueError:
            return False
        if j.get('source_sha1') != self.source_sha1(obj_rel):
            return False
        files = [j.get('atlas_obj'), j.get('atlas_mtl')]
        for a in j.get('atlases', {}).values():
            files += [a.get('png'), a.get('teambase_png'), a.get('teamstripe_png')]
        return all(os.path.isfile(os.path.join(self.out, f)) for f in files if f)

    def clean_previous(self, p):
        """Delete the atlas PNGs a previous build listed, so that none is left over (e.g. a glow atlas that is no
        longer needed, or whole-hull atlases after a fallback)."""
        jpath = os.path.join(self.out, p['json'])
        if not os.path.isfile(jpath):
            return
        try:
            with open(jpath) as fh:
                j = json.load(fh)
        except ValueError:
            return
        for a in j.get('atlases', {}).values():
            for k in ('png', 'teambase_png', 'teamstripe_png'):
                f = a.get(k)
                fp = os.path.join(self.out, f) if f else None
                if fp and under(fp, self.out) and os.path.isfile(fp):
                    os.remove(fp)

    def source_sha1(self, obj_rel):
        obj_path = os.path.join(self.src, obj_rel)
        h = hashlib.sha1()
        h.update(file_sha1(obj_path).encode())
        mtl = obj_path[:-4] + '.mtl'
        if os.path.isfile(mtl):
            h.update(file_sha1(mtl).encode())
        return h.hexdigest()

    # ---------------------------------------------------------------- analysis
    def analyse(self, obj_rel, hull):
        obj_path = os.path.join(self.src, obj_rel)
        obj = SrcObj(obj_path)
        mtl_path = os.path.join(os.path.dirname(obj_path), obj.mtllib or os.path.basename(obj_path)[:-4] + '.mtl')
        mtl = parse_mtl(mtl_path)
        mtl_dir = os.path.dirname(mtl_path)
        stats = collections.OrderedDict()
        for (_li, mname, corners) in obj.faces:
            s = stats.setdefault(mname, [0, 0, float('inf'), float('-inf'), float('inf'), float('-inf')])
            s[0] += 1
            for (_v, ti, _n) in corners:
                if ti is None:
                    continue
                u, v = float(obj.vt[ti - 1][0]), float(obj.vt[ti - 1][1])
                s[1] += 1
                s[2], s[3] = min(s[2], u), max(s[3], u)
                s[4], s[5] = min(s[4], v), max(s[5], v)
        mats = collections.OrderedDict()
        for mname, s in stats.items():
            if mname not in mtl:
                raise ValueError('%s: usemtl %s is not in %s' % (obj_rel, mname, relp(mtl_path, self.src)))
            blk = mtl[mname]
            keys = blk['keys']
            tex_rel = None
            if keys.get('map_Kd'):
                tex_rel = relp(os.path.normpath(os.path.join(mtl_dir, keys['map_Kd'])), self.src)
            flags = material_flags(mname, os.path.splitext(os.path.basename(tex_rel))[0] if tex_rel else None, hull)
            if keys.get('map_Ke') or 'Ke' in keys:
                flags['glow'] = True
            m = collections.OrderedDict()
            m['name'] = mname
            m['texture'] = tex_rel
            m['glow'] = flags['glow']
            m['two_sided'] = flags['two_sided']
            m['smooth'] = flags['smooth']
            m['Ka'], m['Kd'], m['Ks'] = keys.get('Ka'), keys.get('Kd'), keys.get('Ks')
            m['faces'] = s[0]
            m['masks'] = collections.OrderedDict()
            for mk, _sfx in MASKS:
                if blk['masks'].get(mk):
                    m['masks'][mk] = relp(os.path.normpath(os.path.join(mtl_dir, blk['masks'][mk])), self.src)
            m['_block'] = blk['lines']
            m['_mtl_dir'] = mtl_dir
            if s[1]:
                m['uv_range'] = [round(s[2], 6), round(s[3], 6), round(s[4], 6), round(s[5], 6)]
            if tex_rel is None:
                m['status'] = 'untextured'
                m['reason'] = 'colour-only material (no texture): kept unchanged'
            elif not os.path.isfile(os.path.join(self.src, tex_rel)):
                m['status'] = 'exception'
                m['reason'] = 'texture file missing in the export: kept unchanged'
            elif not s[1]:
                m['status'] = 'exception'
                m['reason'] = 'faces have no UVs: kept unchanged'
            else:
                tw, th, _d, alpha = self.texture(tex_rel)
                u0, nu = periods(s[2], s[3])
                v0, nv = periods(s[4], s[5])
                m['texture_size'] = [tw, th]
                m['alpha'] = alpha
                m['uv_origin'] = [u0, v0]
                m['periods'] = [nu, nv]
                over = []
                if nu > self.max_periods or nv > self.max_periods:
                    over.append('%d x %d periods (cap %d per axis)' % (nu, nv, self.max_periods))
                if nu * tw > self.max_slot or nv * th > self.max_slot:
                    over.append('slot %d x %d px at 1x (cap %d)' % (nu * tw, nv * th, self.max_slot))
                if over:
                    m['status'] = 'exception'
                    m['reason'] = 'over the repeat cap: ' + ', '.join(over) + '; kept out of the atlas, uses the ' \
                                  'original texture with its original (repeating) UVs'
                else:
                    m['status'] = 'atlas'
                    m['atlas'] = 'glow' if m['glow'] else 'lit'
            mats[mname] = m
        return obj, mtl_path, mats

    # ---------------------------------------------------------------- slots + images
    def layout(self, mats, max_side, label):
        """Group atlas materials into slots, pack each kind. Returns {kind: atlas dict} or raises ValueError."""
        g = self.gutter
        atlases = collections.OrderedDict()
        for kind in KINDS:
            slots = collections.OrderedDict()
            for m in sorted((m for m in mats.values() if m['status'] == 'atlas' and m['atlas'] == kind),
                            key=lambda m: (m['texture'], m['periods'], m['name'])):
                key = (m['texture'], m['periods'][0], m['periods'][1])
                if key not in slots:
                    tw, th = m['texture_size']
                    slots[key] = collections.OrderedDict([
                        ('id', None), ('texture', m['texture']), ('texture_size', [tw, th]),
                        ('rect', None), ('periods', list(m['periods'])), ('uv_origin', list(m['uv_origin'])),
                        ('flags', collections.OrderedDict([('glow', kind == 'glow'), ('alpha', m['alpha']),
                                                           ('two_sided', False), ('smooth', False)])),
                        ('teambase', None), ('teamstripe', None),
                        ('return_png', m['texture'][len('textures/'):] if m['texture'].startswith('textures/')
                         else m['texture']),
                        ('materials', [])])
                s = slots[key]
                s['flags']['two_sided'] = s['flags']['two_sided'] or m['two_sided']
                s['flags']['smooth'] = s['flags']['smooth'] or m['smooth']
                for mk, _sfx in MASKS:
                    if m['masks'].get(mk):
                        s[mk] = s[mk] or m['masks'][mk]
                s['materials'].append(m['name'])
            if not slots:
                continue
            slist = sorted(slots.values(), key=lambda s: (s['texture'], s['periods']))
            sizes = [(s['texture_size'][0] * s['periods'][0] + 2 * g, s['texture_size'][1] * s['periods'][1] + 2 * g)
                     for s in slist]
            res = pack(sizes, max_side)
            if res is None:
                raise ValueError('%s %s atlas does not fit %d x %d' % (label, kind, max_side, max_side))
            W, H, pos = res
            used = 0
            for i, (s, (x, y)) in enumerate(zip(slist, pos)):
                s['id'] = i
                w, h = s['texture_size'][0] * s['periods'][0], s['texture_size'][1] * s['periods'][1]
                s['rect'] = [x + g, y + g, w, h]
                used += w * h
            a = collections.OrderedDict()
            a['width'], a['height'] = W, H
            a['mode'] = 'RGBA' if any(s['flags']['alpha'] for s in slist) else 'RGB'
            a['slot_area_fraction'] = round(used / float(W * H), 4)
            a['slots'] = slist
            atlases[kind] = a
        return atlases

    def render(self, atlases, stem, tex_dir):
        """Write the atlas PNGs; fills in the png / mask paths of each atlas."""
        g = self.gutter
        for kind, a in atlases.items():
            W, H, mode = a['width'], a['height'], a['mode']
            bpp = _BPP[mode]
            img = bytearray(W * H * bpp)
            want_mask = {mk: any(s[mk] for s in a['slots']) for mk, _sfx in MASKS}
            mimg = {mk: bytearray(W * H) for mk in want_mask if want_mask[mk]}
            for s in a['slots']:
                tw, th, rgba, _al = self.texture(s['texture'])
                px = rgba if mode == 'RGBA' else convert_pixels(rgba, 'RGBA', 'RGB')
                x, y = s['rect'][0] - g, s['rect'][1] - g
                nu, nv = s['periods']
                blit(img, W, bpp, x, y, slot_rows(px, tw, th, bpp, nu, nv, g))
                for mk in mimg:
                    md = self.mask(s[mk], tw, th)
                    if md is not None:
                        blit(mimg[mk], W, 1, x, y, slot_rows(md, tw, th, 1, nu, nv, g))
            a['png'] = '%s/%s_%s.png' % (tex_dir, stem, kind)
            write_png(os.path.join(self.out, a['png']), W, H, bytes(img), mode)
            for mk, sfx in MASKS:
                if mk in mimg:
                    a[mk + '_png'] = '%s/%s_%s%s.png' % (tex_dir, stem, kind, sfx)
                    write_png(os.path.join(self.out, a[mk + '_png']), W, H, bytes(mimg[mk]), 'L')
                else:
                    a[mk + '_png'] = None
            a['masks_note'] = ('slots whose texture has no such mask are black (0 = no team colour)'
                               if mimg else 'no packed texture has team-colour masks')

    # ---------------------------------------------------------------- OBJ rewrite
    def rewrite_obj(self, obj, header, mtllib, mapper, hull):
        """New OBJ text: lines copied, vt replaced segment by segment, usemtl replaced by g + usemtl.
        mapper(material) -> (new_material, uv_fn or None); uv_fn(u, v) -> (u', v') floats."""
        out = list(header)
        face_at = {li: (mname, corners) for (li, mname, corners) in obj.faces}
        segs = []                 # [(out_index_of_placeholder, OrderedDict uvstr -> local index)]
        base = 0                  # global vt count before the current segment
        cur_seg = None
        in_vt = False
        src_group = None
        emitted_mtl = None
        cur_src_mtl = None
        uv_fn = None
        new_mtl = None
        stats = collections.Counter()
        for li, raw in enumerate(obj.lines):
            line = raw.strip()
            tag, _, rest = line.partition(' ')
            if tag == 'vt':
                if not in_vt:
                    if cur_seg is not None:
                        base += len(cur_seg)
                    cur_seg = collections.OrderedDict()
                    segs.append((len(out), cur_seg))
                    out.append(None)
                    in_vt = True
                continue
            in_vt = False
            if tag == 'mtllib':
                out.append('mtllib ' + mtllib)
            elif tag == 'o':
                out.append(raw)
                emitted_mtl = None
                src_group = None
            elif tag == 'g':
                out.append(raw)
                src_group = rest.strip()
            elif tag == 'usemtl':
                cur_src_mtl = rest.strip()
                new_mtl, uv_fn = mapper(cur_src_mtl)
                grp = re.sub(r'\s+', '_', cur_src_mtl)
                out.append('g ' + (src_group + '__' + grp if (hull and src_group) else grp))
                if new_mtl != emitted_mtl:
                    out.append('usemtl ' + new_mtl)
                    emitted_mtl = new_mtl
            elif tag == 'f':
                if cur_seg is None:
                    cur_seg = collections.OrderedDict()
                    segs.append((len(out), cur_seg))
                    out.append(None)
                _m, corners = face_at[li]
                cs = []
                for (vs, ti, ns) in corners:
                    if ti is None:
                        cs.append(vs + ('//' + ns if ns else ''))
                        continue
                    su, sv = obj.vt[ti - 1]
                    if uv_fn is None:
                        key = '%s %s' % (su, sv)
                    else:
                        nu_, nv_ = uv_fn(float(su), float(sv))
                        key = '%s %s' % (fmt(nu_), fmt(nv_))
                    if key not in cur_seg:
                        cur_seg[key] = len(cur_seg)
                    idx = base + cur_seg[key] + 1
                    cs.append('%s/%d' % (vs, idx) + ('/' + ns if ns else ''))
                out.append('f ' + ' '.join(cs))
                stats['f'] += 1
            else:
                out.append(raw)
        lines = []
        seg_at = {pos: seg for pos, seg in segs}
        nvt = 0
        for k, l in enumerate(out):
            if l is None:
                for key in seg_at[k]:
                    lines.append('vt ' + key)
                    nvt += 1
            else:
                lines.append(l)
        stats['vt'] = nvt
        return '\n'.join(lines) + '\n', stats

    # ---------------------------------------------------------------- one OBJ
    def build(self, race, folder, stem, obj_rel, hull, log):
        self.tex_cache = {}
        t0 = time.time()
        p = self.paths(race, folder, stem)
        obj, _mtl_path, mats = self.analyse(obj_rel, hull)
        textured = [m for m in mats.values() if m['status'] != 'untextured']
        if not textured:
            log('%-44s no textured material: nothing to atlas (skipped)' % (race + '/' + stem))
            return None
        self.clean_previous(p)
        atlases = None
        hull_note = None
        if hull:
            try:
                atlases = self.layout(mats, self.hull_max, stem)
            except ValueError as e:
                hull_note = str(e)
        else:
            atlases = self.layout(mats, PIECE_MAX, stem)
        if atlases is None:
            return self.build_hull_fallback(race, folder, stem, obj_rel, obj, mats, hull_note, log, t0)
        self.render(atlases, stem, p['tex_dir'])
        # material -> slot
        slot_of = {}
        for kind, a in atlases.items():
            for s in a['slots']:
                for mn in s['materials']:
                    slot_of[mn] = (kind, a, s)
        clamped = collections.Counter()
        shift = collections.defaultdict(float)

        def mapper(mname):
            m = mats[mname]
            if m['status'] != 'atlas':
                return m['name'], None
            kind, a, s = slot_of[mname]
            W, H = a['width'], a['height']
            sx, sy, sw, sh = s['rect']
            u0, v0 = m['uv_origin']
            nu, nv = m['periods']

            def fn(u, v):
                lu = (u - u0) / nu
                lv = (v - v0) / nv
                if lu < 0.0 or lu > 1.0 or lv < 0.0 or lv > 1.0:
                    cu, cv = min(1.0, max(0.0, lu)), min(1.0, max(0.0, lv))
                    clamped[mname] += 1
                    shift[mname] = max(shift[mname], abs(cu - lu) * sw, abs(cv - lv) * sh)
                    lu, lv = cu, cv
                return (sx + lu * sw) / W, 1.0 - (sy + (1.0 - lv) * sh) / H
            return kind, fn

        mtl_name = os.path.basename(p['mtl'])
        header = ['# Texture-atlas iteration of %s, written by %s' % (obj_rel, TOOL),
                  '# v, vn and f (face order, corner order, v and vn indices) are copied unchanged from the source',
                  '# OBJ; only vt is new. Materials: lit = %s_lit.png, glow = %s_glow.png (self-illuminated,'
                  % (stem, stem),
                  '# drawn unlit; only when the piece has glowing faces).',
                  '# a "g" line before each run of faces keeps the original material name. Slots: %s.'
                  % os.path.basename(p['json'])]
        text, ostats = self.rewrite_obj(obj, header, mtl_name, mapper, hull)
        write_file(os.path.join(self.out, p['obj']), text)
        self.write_mtl(p, stem, obj_rel, mats, atlases)
        for m in mats.values():
            if m['name'] in clamped:
                m['clamped_corners'] = clamped[m['name']]
                m['clamp_max_shift_px'] = round(shift[m['name']], 3)
        j = self.json_record(race, folder, stem, obj_rel, hull, p, obj, mats, atlases, ostats)
        if hull:
            j['whole_hull_atlas'] = True
            j['hull_note'] = 'whole-hull atlas: fits %d x %d at 1x' % (self.hull_max, self.hull_max)
        write_file(os.path.join(self.out, p['json']), json.dumps(j, indent=1) + '\n')
        log('%-44s lit %-9s glow %-9s slots %3d  exceptions %d  %.1fs' % (
            race + '/' + stem,
            '%dx%d' % (atlases['lit']['width'], atlases['lit']['height']) if 'lit' in atlases else '-',
            '%dx%d' % (atlases['glow']['width'], atlases['glow']['height']) if 'glow' in atlases else '-',
            sum(len(a['slots']) for a in atlases.values()), len(j['exceptions']), time.time() - t0))
        return j

    def write_mtl(self, p, stem, obj_rel, mats, atlases, prefix='', extra_blocks=None):
        mtl_dir = os.path.dirname(os.path.join(self.out, p['mtl']))
        lines = ['# Texture-atlas materials for %s, written by %s' % (obj_rel, TOOL),
                 '# lit = every lit textured face, glow = every self-illuminated (unlit in game) textured face.',
                 '# Ka/Kd/Ks: the colours most used by the original materials of that kind (per-material colours,',
                 '# the _2sided flag and the rest are in %s).' % os.path.basename(p['json']),
                 '# Any other material is an exception kept as it was (see the JSON and README.md).', '']
        for kind, a in atlases.items():
            ms = [m for m in mats.values() if m['status'] == 'atlas' and m['atlas'] == kind]
            col = collections.Counter()
            for m in ms:
                col[(m['Ka'], m['Kd'], m['Ks'])] += m['faces']
            ka, kd, ks = col.most_common(1)[0][0]
            png = relp(os.path.join(self.out, a['png']), mtl_dir)
            lines.append('newmtl ' + prefix + kind)
            lines += ['Ka ' + (ka or '1 1 1'), 'Kd ' + (kd or '1 1 1'), 'Ks ' + (ks or '0 0 0'), 'd 1', 'illum 2',
                      'map_Kd ' + png]
            if a['mode'] == 'RGBA':
                lines.append('map_d ' + png)
            if kind == 'glow':
                lines += ['Ke 1 1 1', 'map_Ke ' + png]
            for mk, label in (('teambase', 'team base-colour mask atlas'), ('teamstripe', 'team stripe-colour mask '
                                                                                         'atlas')):
                if a.get(mk + '_png'):
                    lines.append('# %s: %s' % (label, relp(os.path.join(self.out, a[mk + '_png']), mtl_dir)))
            lines.append('')
        for m in mats.values():
            if m['status'] == 'atlas':
                continue
            lines.append('newmtl ' + m['name'])
            lines.append('# kept out of the atlas: ' + m['reason'])
            for s in m['_block']:
                key, _, val = s.partition(' ')
                if key.startswith('map_') or s.startswith('# team '):
                    if s.startswith('# team '):
                        head, _, val = s.partition(': ')
                        key = head
                    full = os.path.normpath(os.path.join(m['_mtl_dir'], val.strip()))
                    val = relp(full, mtl_dir)
                    lines.append(('%s: %s' if key.startswith('#') else '%s %s') % (key, val))
                else:
                    lines.append(s)
            lines.append('')
        if extra_blocks:
            lines += extra_blocks
        write_file(os.path.join(self.out, p['mtl']), '\n'.join(lines) + '\n')

    def json_record(self, race, folder, stem, obj_rel, hull, p, obj, mats, atlases, ostats):
        j = collections.OrderedDict()
        j['tool'] = TOOL
        j['generated'] = datetime.datetime.now().strftime('%Y-%m-%d %H:%M')
        j['kind'] = 'assembled_hull' if hull else 'piece'
        j['race'] = race
        j['folder'] = folder
        j['stem'] = stem
        j['source_obj'] = obj_rel
        j['source_root'] = 'original/assets-export (paths "texture", "source_obj" and "teambase"/"teamstripe" are '\
                           'relative to it)'
        j['source_sha1'] = self.source_sha1(obj_rel)
        j['atlas_obj'] = p['obj']
        j['atlas_mtl'] = p['mtl']
        j['atlas_root'] = 'original/assets-export-atlas (paths "atlas_obj", "atlas_mtl", "png", "*_png" are relative ' \
                          'to it)'
        j['return_root'] = 'original/assets-return ("return_png" is relative to it)'
        j['gutter_px'] = self.gutter
        j['caps'] = {'max_periods_per_axis': self.max_periods, 'max_slot_px_at_1x': self.max_slot,
                     'clamp_eps': CLAMP_EPS}
        j['uv_rule'] = ('per material: u0 = floor(min u), Nu = ceil(max u) - u0 on the source vt as written, corners '
                        'within clamp_eps past a whole number clamped; u\' = (u - u0) / Nu, v\' = (v - v0) / Nv; '
                        'atlas u = (x + u\' w) / width, atlas v = 1 - (y + (1 - v\') h) / height, with rect = '
                        '(x, y, w, h) in atlas pixels (row 0 = top of the PNG, gutter excluded), w = Nu * texture '
                        'width, h = Nv * texture height. The top-left period of a slot is the texture unshifted.')
        j['counts'] = {'source': {k: obj.counts[k] for k in ('v', 'vt', 'vn', 'f')},
                       'atlas': {'v': obj.counts['v'], 'vt': ostats['vt'], 'vn': obj.counts['vn'], 'f': ostats['f']}}
        j['atlases'] = atlases
        mrec = collections.OrderedDict()
        for m in mats.values():
            r = collections.OrderedDict((k, v) for k, v in m.items() if not k.startswith('_'))
            if m['status'] == 'atlas' and m.get('atlas') in atlases:
                kind = m['atlas']
                for s in atlases[kind]['slots']:
                    if m['name'] in s['materials']:
                        r['slot'] = s['id']
            mrec[m['name']] = r
        j['materials'] = mrec
        j['exceptions'] = [collections.OrderedDict([
            ('material', m['name']), ('texture', m['texture']), ('periods', m.get('periods')),
            ('texture_size', m.get('texture_size')), ('faces', m['faces']), ('reason', m['reason'])])
            for m in mats.values() if m['status'] == 'exception']
        j['untextured'] = [m['name'] for m in mats.values() if m['status'] == 'untextured']
        # textures that sit in more than one slot (lit and glow, or tiled and untiled)
        seen = collections.defaultdict(list)
        for kind, a in atlases.items():
            for s in a['slots']:
                seen[s['texture']].append((kind, s))
        multi = []
        for tex, lst in sorted(seen.items()):
            ranked = sorted(lst, key=lambda ks: slot_rank(ks[0], ks[1], 0))
            for i, (kind, s) in enumerate(ranked):
                s['primary'] = i == 0
                s['other_slots'] = ['%s:%d' % (k2, s2['id']) for (k2, s2) in ranked if s2 is not s]
            if len(lst) > 1:
                multi.append({'texture': tex, 'slots': ['%s:%d' % (k, s['id']) for (k, s) in ranked]})
        j['textures_in_several_slots'] = multi
        lit_cols = set((m['Ka'], m['Kd'], m['Ks']) for m in mats.values() if m['status'] == 'atlas' and
                       m['atlas'] == 'lit')
        j['notes'] = []
        if len(lit_cols) > 1:
            j['notes'].append('the lit materials had %d different Ka/Kd/Ks colours; the atlas material uses the most '
                              'used one (each original colour is under "materials")' % len(lit_cols))
        return j

    # ---------------------------------------------------------------- hull fallback
    def build_hull_fallback(self, race, folder, stem, obj_rel, obj, mats, why, log, t0):
        """The whole-hull atlas did not fit: point every part's faces at that part's own piece atlas."""
        p = self.paths(race, folder, stem)
        hull = stem[:-len('_assembled')]
        with open(os.path.join(self.src, 'ships', race, folder, '%s_assembly.json' % hull)) as fh:
            asm = json.load(fh)
        label_part = {}
        for lod in asm.get('lods', []):
            if lod.get('level') == 0:
                for part in lod['parts']:
                    label_part[part['material_prefix'][:-2]] = part['part']
        piece_json = {}
        mtl_dir = os.path.dirname(os.path.join(self.out, p['mtl']))
        used_labels = collections.OrderedDict()
        clamped = collections.Counter()
        shift = collections.defaultdict(float)

        def part_atlas(label):
            part = label_part[label]
            if part not in piece_json:
                jp = os.path.join(self.out, 'textures', race, part, 'lod0', '%s_atlas.json' % part)
                if not os.path.isfile(jp):
                    raise SystemExit('%s: whole-hull atlas does not fit and part atlas %s is missing; build the '
                                     'pieces first' % (stem, relp(jp, self.out)))
                with open(jp) as fh:
                    piece_json[part] = json.load(fh)
            return piece_json[part]

        def mapper(mname):
            m = mats[mname]
            if m['status'] != 'atlas':
                return m['name'], None
            label, _, pm = mname.partition('__')
            pj = part_atlas(label)
            pmr = pj['materials'][pm]
            if pmr.get('status') != 'atlas':
                return m['name'], None
            kind = pmr['atlas']
            a = pj['atlases'][kind]
            s = [s for s in a['slots'] if s['id'] == pmr['slot']][0]
            used_labels.setdefault(label, set()).add(kind)
            W, H = a['width'], a['height']
            sx, sy, sw, sh = s['rect']
            u0, v0 = pmr['uv_origin']
            nu, nv = pmr['periods']

            def fn(u, v):
                lu, lv = (u - u0) / nu, (v - v0) / nv
                if lu < 0.0 or lu > 1.0 or lv < 0.0 or lv > 1.0:
                    cu, cv = min(1.0, max(0.0, lu)), min(1.0, max(0.0, lv))
                    clamped[mname] += 1
                    shift[mname] = max(shift[mname], abs(cu - lu) * sw, abs(cv - lv) * sh)
                    lu, lv = cu, cv
                return (sx + lu * sw) / W, 1.0 - (sy + (1.0 - lv) * sh) / H
            return '%s__%s' % (label, kind), fn

        header = ['# Texture-atlas iteration of %s, written by %s' % (obj_rel, TOOL),
                  '# v, vn and f are copied unchanged from the source OBJ; only vt is new. The whole-hull atlas',
                  '# did not fit %d x %d, so each part uses its own piece atlas (materials <label>__lit, __glow).'
                  % (self.hull_max, self.hull_max),
                  '# A "g" line before each run of faces keeps the original group and material name.']
        text, ostats = self.rewrite_obj(obj, header, os.path.basename(p['mtl']), mapper, True)
        # materials whose part material was an exception in the piece atlas
        for mname, m in mats.items():
            if m['status'] == 'atlas':
                label, _, pm = mname.partition('__')
                if part_atlas(label)['materials'][pm].get('status') != 'atlas':
                    m['status'] = 'exception'
                    m['reason'] = 'not in its part\'s atlas: kept unchanged'
        blocks = []
        for label, kinds in used_labels.items():
            pj = part_atlas(label)
            for kind in KINDS:
                if kind not in kinds:
                    continue
                a = pj['atlases'][kind]
                with open(os.path.join(self.out, pj['atlas_mtl'])) as fh:
                    pmtl = parse_mtl_text(fh.read())
                blocks.append('newmtl %s__%s' % (label, kind))
                for s in pmtl[kind]:
                    key, _, val = s.partition(' ')
                    if key.startswith('map_') or s.startswith('# team '):
                        head, sep, val2 = s.partition(': ') if s.startswith('# team ') else (key, ' ', val)
                        full = os.path.normpath(os.path.join(os.path.dirname(os.path.join(self.out, pj['atlas_mtl'])),
                                                             val2.strip()))
                        blocks.append('%s%s%s' % (head, sep, relp(full, mtl_dir)))
                    else:
                        blocks.append(s)
                blocks.append('')
        write_file(os.path.join(self.out, p['obj']), text)
        self.write_mtl(p, stem, obj_rel, mats, {}, extra_blocks=blocks)
        for m in mats.values():
            if m['name'] in clamped:
                m['clamped_corners'] = clamped[m['name']]
                m['clamp_max_shift_px'] = round(shift[m['name']], 3)
        j = self.json_record(race, folder, stem, obj_rel, True, p, obj, mats, {}, ostats)
        j['whole_hull_atlas'] = False
        j['hull_note'] = ('whole-hull atlas does not fit (%s): the hull OBJ uses the per-piece atlases of its parts'
                          % why)
        j['piece_atlas_json'] = collections.OrderedDict(
            (label, self.paths(race, label_part[label], label_part[label])['json']) for label in used_labels)
        for mname, m in mats.items():
            label, _, pm = mname.partition('__')
            if m['status'] == 'atlas' and label in label_part:
                pmr = part_atlas(label)['materials'][pm]
                j['materials'][mname]['atlas'] = '%s__%s' % (label, pmr['atlas'])
                j['materials'][mname]['slot'] = pmr['slot']
                j['materials'][mname]['piece_atlas_json'] = j['piece_atlas_json'][label]
        write_file(os.path.join(self.out, p['json']), json.dumps(j, indent=1) + '\n')
        log('%-44s whole-hull atlas does not fit: uses %d part atlases  %.1fs' % (race + '/' + stem, len(used_labels),
                                                                                  time.time() - t0))
        return j


def parse_mtl_text(text):
    mats = collections.OrderedDict()
    cur = None
    for raw in text.split('\n'):
        s = raw.strip()
        if s.startswith('newmtl '):
            cur = s[7:].strip()
            mats[cur] = []
        elif cur is not None and s:
            mats[cur].append(s)
    return mats


def slot_rank(kind, slot, atlas_order):
    """Which slot is the main copy of a texture that sits in several: fewest periods, then lit before glow, then
    piece atlases before hull atlases (atlas_order), then slot id."""
    return (slot['periods'][0] * slot['periods'][1], KINDS.index(kind) if kind in KINDS else 9, atlas_order, slot['id'])


# ------------------------------------------------------------------------------------------------------- index / README
def collect(out):
    recs = []
    for jp in sorted(glob.glob(os.path.join(out, 'textures', '*', '*', 'lod0', '*_atlas.json'))):
        with open(jp) as fh:
            recs.append(json.load(fh))
    return recs


def untextured_pieces(src, built_stems):
    res = []
    for path in sorted(glob.glob(os.path.join(src, 'ships', '*', '*', '*_lod0.obj'))):
        rel = relp(path, src)
        _, race, folder, name = rel.split('/')
        stem = name[:-len('_lod0.obj')]
        if (race, stem) in built_stems:
            continue
        mtl = parse_mtl(path[:-4] + '.mtl')
        textured = any(b['keys'].get('map_Kd') for b in mtl.values())
        res.append((race, folder, stem, rel, textured))
    return res


def size_range(recs, kind):
    sizes = [(r['atlases'][kind]['width'], r['atlases'][kind]['height']) for r in recs if kind in r.get('atlases', {})]
    if not sizes:
        return None, None, 0
    key = lambda s: (s[0] * s[1], s[0])  # noqa: E731
    return min(sizes, key=key), max(sizes, key=key), len(sizes)


def write_index_and_readme(src, out, gutter_default):
    recs = collect(out)
    built = set((r['race'], r['stem']) for r in recs)
    missing = untextured_pieces(src, built)
    pieces = [r for r in recs if r['kind'] == 'piece']
    hulls = [r for r in recs if r['kind'] == 'assembled_hull']
    index = collections.OrderedDict()
    index['tool'] = TOOL
    index['generated'] = datetime.datetime.now().strftime('%Y-%m-%d %H:%M')
    tot = collections.Counter()
    rows = []
    for r in recs:
        a = r.get('atlases', {})
        row = collections.OrderedDict([
            ('race', r['race']), ('stem', r['stem']), ('kind', r['kind']), ('obj', r['atlas_obj']),
            ('json', r['atlas_obj'].replace('ships/', 'textures/', 1).rsplit('/', 1)[0] + '/lod0/%s_atlas.json'
             % r['stem'])])
        for kind in KINDS:
            if kind in a:
                row[kind] = '%dx%d' % (a[kind]['width'], a[kind]['height'])
                row[kind + '_slots'] = len(a[kind]['slots'])
                row[kind + '_masks'] = [mk for mk, _s in MASKS if a[kind].get(mk + '_png')]
                tot['%s_atlases' % kind] += 1
                tot['slots'] += len(a[kind]['slots'])
                tot['mask_atlases'] += len(row[kind + '_masks'])
        row['exceptions'] = len(r['exceptions'])
        row['untextured_materials'] = len(r['untextured'])
        row['textures_in_several_slots'] = len(r['textures_in_several_slots'])
        if r['kind'] == 'assembled_hull':
            row['whole_hull_atlas'] = r.get('whole_hull_atlas')
        rows.append(row)
        tot['exceptions'] += len(r['exceptions'])
        tot['untextured_materials'] += len(r['untextured'])
        tot['textures_in_several_slots'] += len(r['textures_in_several_slots'])
    clamp = [(m.get('clamp_max_shift_px', 0.0), m.get('clamped_corners', 0)) for r in recs
             for m in r['materials'].values()]
    tot['clamped_corners'] = sum(c for _s, c in clamp)
    index['clamp_max_shift_px'] = max([s_ for s_, _c in clamp] or [0.0])
    tot['pieces'] = len(pieces)
    tot['assembled_hulls'] = len(hulls)
    tot['pieces_with_glow'] = sum(1 for r in pieces if 'glow' in r.get('atlases', {}))
    tot['skipped_no_textures'] = sum(1 for m in missing if not m[4])
    tot['not_built_yet'] = sum(1 for m in missing if m[4])
    index['totals'] = collections.OrderedDict(sorted(tot.items()))
    index['size_range'] = collections.OrderedDict()
    for kind in KINDS:
        for label, sel in (('pieces', pieces), ('hulls', hulls)):
            lo, hi, n = size_range(sel, kind)
            index['size_range']['%s_%s' % (kind, label)] = {'min': lo, 'max': hi, 'count': n}
    index['exceptions'] = [dict(e, race=r['race'], stem=r['stem'], folder=r['folder']) for r in recs
                           for e in r['exceptions']]
    index['untextured_materials'] = [{'race': r['race'], 'stem': r['stem'], 'material': m}
                                     for r in recs for m in r['untextured']]
    index['skipped_no_textures'] = [m[3] for m in missing if not m[4]]
    index['not_built_yet'] = [m[3] for m in missing if m[4]]
    index['textures_in_several_slots'] = [dict(t, race=r['race'], stem=r['stem'], folder=r['folder']) for r in recs
                                          for t in r['textures_in_several_slots']]
    index['hulls'] = [{'stem': r['stem'], 'race': r['race'], 'whole_hull_atlas': r.get('whole_hull_atlas'),
                       'note': r.get('hull_note')} for r in hulls]
    index['notes'] = [{'race': r['race'], 'stem': r['stem'], 'notes': r['notes']} for r in recs if r['notes']]
    index['ships'] = rows
    write_file(os.path.join(out, 'atlas_index.json'), json.dumps(index, indent=1) + '\n')
    write_file(os.path.join(out, 'README.md'), readme_text(index, recs, gutter_default))
    return index


def _sz(s):
    return '%d x %d' % tuple(s) if s else '-'


def readme_text(index, recs, gutter_default):
    t = index['totals']
    sr = index['size_range']
    gutters = sorted(set(r['gutter_px'] for r in recs)) or [gutter_default]
    caps = recs[0]['caps'] if recs else {'max_periods_per_axis': MAX_PERIODS, 'max_slot_px_at_1x': MAX_SLOT}
    exc_rows = '\n'.join('| `%s/%s` | `%s` | `%s` | %s | %s |' % (
        e['race'], e['stem'], e['material'], e['texture'], ('%d x %d' % tuple(e['periods'])) if e.get('periods')
        else '-', e['reason'].split(';')[0]) for e in index['exceptions']) or '| (none) | | | | |'
    by_ship = collections.OrderedDict()
    for u in index['untextured_materials']:
        by_ship.setdefault('%s/%s' % (u['race'], u['stem']), []).append('`%s`' % u['material'])
    unt_rows = '\n'.join('| `%s` | %s |' % (k, ', '.join(v)) for k, v in by_ship.items()) or '| (none) | |'
    skipped = ', '.join('`%s`' % s.split('/')[2] for s in index['skipped_no_textures']) or '(none)'
    hull_rows = '\n'.join('| `%s` | %s | %s |' % (
        h['stem'], 'yes' if h['whole_hull_atlas'] else '**no**, uses the per-piece atlases', h['note'] or '')
        for h in index['hulls']) or '| (none) | | |'
    multi_n = t.get('textures_in_several_slots', 0)
    return README.format(
        pieces=t.get('pieces', 0), hulls=t.get('assembled_hulls', 0), glow=t.get('pieces_with_glow', 0),
        lit_min=_sz(sr['lit_pieces']['min']), lit_max=_sz(sr['lit_pieces']['max']),
        glow_min=_sz(sr['glow_pieces']['min']), glow_max=_sz(sr['glow_pieces']['max']),
        hlit_min=_sz(sr['lit_hulls']['min']), hlit_max=_sz(sr['lit_hulls']['max']),
        hglow_min=_sz(sr['glow_hulls']['min']), hglow_max=_sz(sr['glow_hulls']['max']),
        gutter=', '.join(str(g) for g in gutters), max_periods=caps['max_periods_per_axis'],
        max_slot=caps['max_slot_px_at_1x'], n_exc=len(index['exceptions']), exc_rows=exc_rows,
        n_unt=len(index['untextured_materials']), unt_rows=unt_rows, skipped=skipped,
        n_skipped=len(index['skipped_no_textures']), hull_rows=hull_rows, multi_n=multi_n,
        masks=t.get('mask_atlases', 0), generated=index['generated'], n_colour=len(index['notes']),
        clamp_n=t.get('clamped_corners', 0), clamp_px='%.1f' % index['clamp_max_shift_px'],
        not_built=('\n\n**Not built yet:** ' + ', '.join('`%s`' % s for s in index['not_built_yet']) +
                   ' (run `tools/assets/atlas_ships.py` again).') if index['not_built_yet'] else '')


README = r"""# Homeworld: Cataclysm ships, texture-atlas version (two images per ship)

This folder is a second version of the ship export in `../assets-export/`, made for repainting and upscaling in
Blender. It was made by `tools/assets/atlas_ships.py` (last run {generated}) from that export, and it is copyrighted
game data too: keep it on this machine, do not share it, and do not put it in git.

**What is different:** in the first export a ship has many small textures (about 16 per ship, 32 to 256 pixels a
side) and many materials. Here every ship has **one image for its normal surfaces (`lit`) and one for its glowing
parts (`glow`)**, with all its textures packed side by side in it (a "texture atlas"), and its model has just those
two materials. You paint one picture per ship instead of sixteen.

**What is the same:** the shapes. The models here are the same models: every vertex, normal and face is copied
unchanged and in the same order from `../assets-export/`; only the texture coordinates (UVs) were moved so that they
point into the atlas. **Nothing in `../assets-export/` was changed**; that export stays the historical reference.

Contents at a glance:

| folder or file | what it is |
|---|---|
| `ships/<race>/<ship>/<ship>_atlas_lod0.obj` | the ship model with atlas UVs. Open this one in Blender |
| `textures/<race>/<ship>/lod0/<ship>_lit.png` | **the lit atlas**: every normal texture of the ship. Paint or upscale this |
| `textures/<race>/<ship>/lod0/<ship>_glow.png` | **the glow atlas**: the self-illuminated textures (lit windows, engine glows). Only ships with glowing faces have one |
| `textures/<race>/<ship>/lod0/<ship>_lit_teambase.png` (and `_teamstripe`, and the same for `_glow`) | team-colour masks of the atlas, laid out exactly like it |
| `textures/<race>/<ship>/lod0/<ship>_atlas.json` | where every original texture sits in the atlas, and which material used it |
| `ships/race_sect/smothership/smothership_assembled_atlas_lod0.obj` (and the Beast mothership and the carrier) | the three assembled modular ships, each with **one atlas for the whole ship** (`smothership_assembled_lit.png` and so on) |
| `atlas_index.json` | the list of every ship, atlas size and exception |

Only LOD 0 (the close-up model) is here. This version has {pieces} ships and {hulls} assembled ships; {glow} of the ships
also have a glow atlas, and there are {masks} team-colour mask atlases.

## 1. Opening a ship in Blender

1. Start Blender and delete the default cube (select it, press `X`, choose Delete).
2. Choose **File > Import > Wavefront (.obj)**, go to `ships/<race>/<ship>/` and pick `<ship>_atlas_lod0.obj`,
   for example `ships/race_sect/scarrier/scarrier_atlas_lod0.obj`. Keep the default settings; the `.mtl` next to it
   loads the atlas images.
3. Hover over the 3D view, press `Numpad .` to zoom to the ship, and switch to **Material Preview** (press `Z`).

The ship has two materials, `lit` and `glow` (Blender renames them `lit.001`, `glow.001` and so on when you import a
second ship). Everything from section 1 of `../assets-export/README.md` still applies (clip end, axes, moving parts).

**Finding the original materials:** each run of faces carries the name of its original material as an OBJ group.
Tick **Vertex Groups** in the import options and every original material becomes a vertex group (Object Data
properties, green triangle icon): select one and press **Select** in Edit Mode to see which faces used it. The JSON
lists, for every original material, its texture, its slot and its flags; the `_2sided` flag (no back-face culling in
the game) is kept there, because OBJ has no place for it. Blender shows both sides of every face by default anyway.
In the assembled ships the group names are `<part>__<sub-object>__<label>__<material>`, so they also say which part
and sub-object a face belongs to.

**Seeing an upscaled atlas on the model:** the UVs are fractions of the image, so they stay right at any scale. In the
**Shading** workspace, click the image node of `lit` (or `glow`) and use the folder icon (or **Image > Replace** in the
Image Editor) to pick your upscaled file instead.

## 2. Painting and upscaling the two images

Each atlas is a normal PNG. Every original texture sits in its own rectangle (a "slot"), with a thin border of
repeated edge pixels around it (the gutter, {gutter} pixels at the original size) so that blurring, mipmaps and AI
upscalers do not mix neighbouring textures. The space between slots is unused (black or transparent).

* **Upscale or paint the whole image at once.** Scale width and height by the same factor, best a whole number
  (2x, 4x): a 1024 x 512 atlas becomes 4096 x 2048 at 4x. Never crop, pad, rotate, flip or rearrange the image, and do
  not move anything inside it: the slots must stay where they are.
* **Masks:** if you upscale the base atlas, upscale its `_teambase` / `_teamstripe` masks by the same factor (smooth
  scaling is fine, keep them greyscale). White = repainted in the player's base or stripe colour, black = keeps the
  texture colour. Slots whose original texture has no mask are black in the mask atlas (black means "no team colour",
  so this changes nothing).
* **Alpha:** an atlas is RGBA only when one of its textures uses transparency (engine glows, lights); keep the alpha
  channel then. The others are plain RGB.
* **Painting in Blender:** in the **Texture Paint** workspace you can paint straight onto the model; the brush writes
  into the atlas image. Then save with **Image > Save As** into the return folder (section 3), not over the file
  here, so this folder stays a clean reference.
* **The same texture can appear twice:** a texture used by both normal and glowing faces sits in both atlases, and a
  texture used both repeated and not repeated gets two slots. Paint both copies the same way. When the copies differ,
  the split tool keeps the copy marked `"primary": true` in the JSON and prints a warning. This version has
  {multi_n} such textures; `atlas_index.json` lists them under `textures_in_several_slots`.

### Repeated textures (important)

In the game a texture can repeat across a face: a long hull strip may show the same 32-pixel pattern six times. An
atlas cannot repeat, so those repeats are **baked**: the slot holds the texture repeated as many times as the faces
need (up to {max_periods} times per direction), and the UVs were squeezed to match. In the image you see a strip
of identical copies. When the textures are split back out, **only the first copy (top left) of such a slot is kept**,
because the game still repeats one copy. So:

* paint every copy the same way (paint the first copy, then copy it over the others), and
* anything painted across a repeated strip that is not the same in every copy (a long scratch, a name along the hull)
  is reduced to its first copy on return. The split tool compares the copies and prints a warning, naming the
  texture, when they differ by more than a small amount.

Corners that went past a whole repeat by a hair (0.02 of the texture or less) were pulled back onto the edge of the
slot so that such a sliver does not cost a whole extra copy. That moves {clamp_n} face corners by at most {clamp_px}
pixels at the original size (most by less than one); nothing else moves.

## 3. Handing the atlases back

Put each painted atlas under `original/assets-return-atlas/` (next to this folder), at the same relative path it has
under `textures/`:

```
original/assets-export-atlas/textures/race_sect/scarrier/lod0/scarrier_lit.png            (exported here)
original/assets-return-atlas/race_sect/scarrier/lod0/scarrier_lit.png                     (your painted version)
original/assets-return-atlas/race_sect/scarrier/lod0/scarrier_lit_teambase.png            (its mask, if you changed it)
```

You only need to return the atlases you changed; the masks are optional. Then run, from the repository folder:

```
python3 tools/assets/split_atlas.py --dry-run     # shows what it would write
python3 tools/assets/split_atlas.py               # writes the separate textures
```

The split tool cuts every slot back out of your atlas (without the gutter, at your scale), keeps one copy of a
repeated slot, and writes the separate textures to `original/assets-return/<race>/<ship>/lod0/<texture>.png` (plus
`_teambase` / `_teamstripe`), exactly where the first export's return path expects them (see section 4 of
`../assets-export/README.md`). From there on everything works as in the first export. It never writes into
`assets-export/` or `assets-export-atlas/`, and it does not overwrite a texture in `assets-return/` that you put
there yourself (it remembers the files it wrote; use `--overwrite` to replace others). Options: `--piece
race_sect/scarrier` to do one ship, `--threshold` for the repeat-copy warning.

It works out your scale from the image width (painted width / original width) and needs **the same factor on both
axes**; whole factors (2x, 4x) always work, and a fraction such as 3/2 works only when every slot lands on whole
pixels; otherwise it refuses with a message. It reads PNG files only: with Pillow installed (it is on this machine)
any PNG works (8 or 16 bit, grey, RGB, RGBA, palette, interlaced); without Pillow, 8-bit grey, RGB or RGBA,
not interlaced. Base atlases come back as RGBA textures, masks as greyscale.

## 4. Assembled modular ships

The three assembled ships (Kuun-Lan, Beast mothership, Somtaaw carrier, see section 7 of `../assets-export/README.md`)
get one atlas for the whole ship, so every part of the ship is in the same two images, if that atlas fits in
4096 x 4096 at the original size; otherwise their model uses the separate atlas of each part. The split tool sends
each slot back to its part's own folder. Return either the whole-ship atlas or the part atlases for a texture, not
both differently painted: when two returned atlases contain the same texture, the part atlas wins and a warning is
printed.

| assembled ship | whole-ship atlas | note |
|---|---|---|
{hull_rows}

## 5. Sizes and exceptions

* Atlas sizes at the original scale (every side a power of two): lit atlases of single ships from {lit_min} to
  {lit_max}, glow atlases from {glow_min} to {glow_max}; whole-ship atlases of the assembled ships: lit {hlit_min} to
  {hlit_max}, glow {hglow_min} to {hglow_max}.
* **Kept out of the atlas ({n_exc}):** a material that repeats its texture more than {max_periods} times in one
  direction, or would need a slot over {max_slot} pixels a side, keeps its own material pointing at the original
  texture in `../assets-export/textures/` with its original repeating UVs. Upscale that texture there, in the usual
  way of the first export (its README, sections 3 and 4).

| ship | material | original texture (in `../assets-export/`) | repeats | why |
|---|---|---|---|---|
{exc_rows}

* **Colour-only materials ({n_unt}):** a few materials have no texture at all, only a colour; they are kept as they
  were, as their own material:

| ship | colour-only materials |
|---|---|
{unt_rows}

* **Ships without any texture ({n_skipped}):** {skipped}. There is nothing to put in an atlas, so they have no files
  here; open them from `../assets-export/`.{not_built}

## 6. Technical notes

* UVs: OBJ `vt` = (s, 1 - t) as in the first export. For each original material, u0 = floor(min u),
  Nu = ceil(max u) - u0 (the same for v), the slot holds Nu x Nv copies, u' = (u - u0) / Nu, and the atlas
  u = (x + u' w) / width, atlas v = 1 - (y + (1 - v') h) / height, with the slot rectangle (x, y, w, h) in atlas pixels
  from the top left, gutter excluded. The top-left copy of every slot is the original texture, not shifted.
* `<ship>_atlas.json` per slot: `texture` (the original PNG, relative to `assets-export/`), `rect`, `periods`
  (Nu, Nv), `uv_origin` (u0, v0), `flags` (glow, alpha, two_sided, smooth), `materials` (the original material names),
  `teambase` / `teamstripe` (the original masks), `return_png` and `primary`. Per original material: Ka/Kd/Ks colours,
  flags, face count, UV range and its own `uv_origin`. The atlas materials use the colour most of their faces used;
  {n_colour} ship(s) had lit materials of different colours (listed under `notes` in `atlas_index.json`).
* Packing: max-rects (best short side fit), lit and glow packed separately, gutter by edge replication.
* `tools/assets/validate_atlas.py` checks this folder: the model geometry is identical to the first export, every
  UV is inside 0..1, random face corners sample the same texel as before, and splitting the unpainted atlases gives
  back every original texture pixel for pixel. It writes `validation.json` here.
* If `../assets-export/` is made again, run `tools/assets/atlas_ships.py --force` again.
"""


# ------------------------------------------------------------------------------------------------------- main
def main(argv):
    ap = argparse.ArgumentParser(description=__doc__.split('\n')[0])
    ap.add_argument('--src', default=DEF_SRC, help='the ship export to read (never written)')
    ap.add_argument('--out', default=DEF_OUT)
    ap.add_argument('--piece', action='append', help='race/piece or race/<hull>_assembled; repeatable')
    ap.add_argument('--force', action='store_true', help='rebuild even when the outputs exist')
    ap.add_argument('--gutter', type=int, default=GUTTER)
    ap.add_argument('--max-periods', type=int, default=MAX_PERIODS)
    ap.add_argument('--max-slot', type=int, default=MAX_SLOT)
    ap.add_argument('--hull-max', type=int, default=HULL_MAX)
    ap.add_argument('--readme-only', action='store_true', help='only rebuild README.md and atlas_index.json')
    args = ap.parse_args(argv)
    src, out = os.path.abspath(args.src), os.path.abspath(args.out)
    if not os.path.isdir(os.path.join(src, 'ships')):
        print('no ships/ folder in --src %s' % src)
        return 2
    b = AtlasBuilder(src, out, args.gutter, args.max_periods, args.max_slot, args.hull_max)
    t0 = time.time()
    if not args.readme_only:
        want = set(p.strip('/').lower() for p in args.piece) if args.piece else None
        items = b.pieces()
        if want:
            items = [it for it in items if '%s/%s' % (it[0], it[2]) in want]
            if len(items) != len(want):
                found = set('%s/%s' % (it[0], it[2]) for it in items)
                print('unknown --piece: %s' % ', '.join(sorted(want - found)))
                return 2
        built = skipped = 0
        for race, folder, stem, obj_rel, hull in items:
            if not args.force and b.up_to_date(race, folder, stem, obj_rel):
                skipped += 1
                continue
            j = b.build(race, folder, stem, obj_rel, hull, print)
            built += 1 if j else 0
        print('built %d, skipped %d up-to-date, in %.0fs' % (built, skipped, time.time() - t0))
    idx = write_index_and_readme(src, out, args.gutter)
    t = idx['totals']
    print('index: %d pieces + %d hulls, %d lit / %d glow / %d mask atlases, %d exceptions, %d untextured pieces '
          'skipped -> %s' % (t.get('pieces', 0), t.get('assembled_hulls', 0), t.get('lit_atlases', 0),
                             t.get('glow_atlases', 0), t.get('mask_atlases', 0), t.get('exceptions', 0),
                             t.get('skipped_no_textures', 0), os.path.join(out, 'README.md')))
    return 0


if __name__ == '__main__':
    sys.exit(main(sys.argv[1:]))
