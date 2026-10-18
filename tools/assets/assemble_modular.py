#!/usr/bin/env python3
"""Assemble the modular ships of Homeworld: Cataclysm from the parts that export_ships.py wrote (stdlib only).

Usage:
  tools/assets/assemble_modular.py [--out DIR] [--data DIR] [--update DIR] [--lods 0,1,..|all]
                                   [--only RACE/HULL ...] [--no-readme] [--blender [PATH]]

Run it after tools/assets/export_ships.py: a full export deletes --out, these files included, and rewrites
README.md. Defaults: --out original/assets-export, --data original/game_extracted/cataclysm,
--update original/game_extracted/update, --lods 0. --blender imports each assembled OBJ in headless Blender
(import only) and records what loaded.

Writes, per hull (sMothership, bMothership, sCarrier):
  ships/<race>/<hull>/<hull>_assembled_lod<N>.obj|.mtl   one OBJ object per part; materials point at the parts'
                                                        own exported PNGs (nothing is copied)
  ships/<race>/<hull>/<hull>_assembly.json              every part: category, dock point, transform, checks
and a section between the markers below in <out>/README.md.

Placement, and where each rule comes from (recreation tree .claude/worktrees/m2/src, which ports cataclysm.exe):
  * Modules made with the ship (cat_univCreateMothershipModules, Game/UnivUpdate.c:2219-2267) are put at the host's
    position (:2249) and clamped to it (:2251, Game/Clamp.c:28-52 stores the offset and the relative coordsys). Every
    new ship starts with defaultshipmatrix (Game/UnivUpdate.c:351 via :1949), so the clamp offset is zero and the
    relative rotation is the identity: such a part is drawn in the host's own frame, unchanged.
  * Externally built parts (Game/Dock.c:2700-2716) are put at host + coordsys * dockpoint.position and oriented by
    dockPutShipOutside (Game/Dock.c:2969-3060, exe 0x00463350): columns (up, right, heading) from the point's .shp
    heading/up codes (GetDirectionVectorOfShip, Dock.c:94-117; right = heading x up). The position is the raw MEX
    "Dok" chunk position (Game/MEX.c:354), from the .mex named by the hull's pMexData line.
  * Support parts (sMothershipSupport, sCarrierSupport, bMothershipSupport -> DPT_Support_EB, Dock.c:1500-1503) are
    also turned by the parity of their point's index within its type (Dock.c:3013-3050; index in .shp order,
    firstOfType from Game/StatScript.c:1418-1442): Sect/carrier odd -> coordsys * RotX(pi); Beast bit 2 ->
    * RotY(pi) * RotZ(+7 deg, or -7 deg when bit 1), then bit 1 -> * RotX(pi). Constants read from the exe:
    0x0072a7d8 = pi (double), 0x0072a7cc = +0.12217305, 0x0072a7d0 = -0.12217305 (float).
  * The game frame is x = up, y = right, z = forward; the exported OBJs are turned with OBJ = A * game,
    A = export_ships.to_obj_axes (OBJ (x, y, z) = game (-y, x, z)). A part transform (M, t) in game space is applied to
    OBJ vertices as v' = A (M (A^T v) + t), normals n' = A M A^T n (M is a rotation, so winding stays valid).
"""
import argparse
import collections
import json
import math
import os
import re
import shutil
import subprocess
import sys
import tempfile

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import hwformats as hf  # noqa: E402

REPO = os.path.abspath(os.path.join(os.path.dirname(os.path.abspath(__file__)), '..', '..'))
DEF_DATA = os.path.join(REPO, 'original', 'game_extracted', 'cataclysm')
DEF_UPDATE = os.path.join(REPO, 'original', 'game_extracted', 'update')
DEF_OUT = os.path.join(REPO, 'original', 'assets-export')
README_BEGIN = '<!-- assemble_modular.py: begin (written by tools/assets/assemble_modular.py; edits here are lost) -->'
README_END = '<!-- assemble_modular.py: end -->'
BLENDER_CANDIDATES = ('blender', os.path.expanduser('~/.local/share/Steam/steamapps/common/Blender/blender'))

PI_F = 3.1415927410125732       # exe 0x0072a7d8 (stored as a double)
ROT7_POS = 0.12217304855585098  # exe 0x0072a7cc (float)
ROT7_NEG = -0.12217304855585098  # exe 0x0072a7d0 (float)

UNIV = 'Game/UnivUpdate.c'
UNIVERSE = 'Game/Universe.c'
SECT_LISTS = ('start lists of cat_univMothershipModuleTypesGet (Game/Universe.c:5350-5408): tutorial, missions 1-2, '
              'mission 3, maps DemoBigGun2 and DemoSect2, and the default list (all other games)')
SECT_MP = ('in multiplayer the ship also starts with Special, Engineering, Armour, Micro, Weapons, DockingBay and '
           'BigGun unless the no-modules game option is set (Game/UnivUpdate.c:2221-2222, 2256-2267)')

# category: 'core' | 'structural' (every start list) | 'structural_early' (only some start lists, see starts_with)
#           | 'built_in_play' | 'cut' (a mesh and a dock point, but no ship type or .shp in the shipped game)
# place:    'identity' (made with the ship and clamped at its position) | 'dock' (external build at a named
#           dock point) | 'support' (one instance per DPT_Support_EB point, with the parity turn)
HULLS = [
    {'race': 'race_sect', 'hull': 'smothership', 'support_kind': 'sect', 'parts': [
        {'part': 'smothership', 'category': 'core', 'place': 'identity',
         'starts_with': 'the hull itself (the spine)'},
        {'part': 'smothershipenginesupper', 'category': 'structural', 'place': 'identity', 'mex': 'ENGINESUPPR',
         'starts_with': 'every game: it is in all six ' + SECT_LISTS},
        {'part': 'smothershipcommand', 'category': 'structural', 'place': 'identity', 'mex': 'COMMAND',
         'starts_with': 'every game: it is in all six ' + SECT_LISTS},
        {'part': 'smothershipengineering', 'category': 'structural', 'place': 'identity', 'mex': 'ENGINEERING',
         'starts_with': 'every game: it is in all six ' + SECT_LISTS},
        {'part': 'smothershipengineslower', 'category': 'structural_early', 'place': 'identity', 'mex': 'ENGINESLOWR',
         'starts_with': 'tutorial, single-player missions 1-3, DemoBigGun2, DemoSect2; NOT in the default list '
                        '(later missions, skirmish, multiplayer). It has no external-build dock type '
                        '(Game/Dock.c:1488-1512), so it is not built in play'},
        {'part': 'smothershiplowerspine', 'category': 'structural_early', 'place': 'identity', 'mex': 'LOWERSPINE',
         'starts_with': 'tutorial, single-player missions 1-3, DemoBigGun2, DemoSect2; NOT in the default list '
                        '(later missions, skirmish, multiplayer). It has no external-build dock type '
                        '(Game/Dock.c:1488-1512), so it is not built in play'},
        {'part': 'smothershiporecanarmour', 'category': 'structural_early', 'place': 'identity',
         'starts_with': 'tutorial and single-player missions 1-2 only. It covers the top of the spine where '
                        'Weapons, Special, Micro and Armour are built later, so it overlaps them'},
        {'part': 'smothershipweapons', 'category': 'built_in_play', 'place': 'dock', 'dock': 'Weapons',
         'starts_with': 'built in play (DPT_Module_EB, Game/Dock.c:1504-1511); starts built on DemoSect2 and, '
                        'by default, in multiplayer'},
        {'part': 'smothershiparmour', 'category': 'built_in_play', 'place': 'dock', 'dock': 'Armour',
         'starts_with': 'built in play (DPT_Module_EB); starts built on DemoSect2 and, by default, in multiplayer'},
        {'part': 'smothershipmicro', 'category': 'built_in_play', 'place': 'dock', 'dock': 'Micro',
         'starts_with': 'built in play (DPT_Module_EB); starts built on DemoSect2 and, by default, in multiplayer'},
        {'part': 'smothershipspecial', 'category': 'built_in_play', 'place': 'dock', 'dock': 'Special',
         'starts_with': 'built in play (DPT_Module_EB); starts built on DemoBigGun2, DemoSect2 and, by default, '
                        'in multiplayer'},
        {'part': 'smothershipdockingbay', 'category': 'built_in_play', 'place': 'dock', 'dock': 'Dockbay',
         'starts_with': 'built in play (DPT_Module_EB); starts built in the tutorial, on DemoSect2 and, by '
                        'default, in multiplayer'},
        {'part': 'smothershipbiggun', 'category': 'built_in_play', 'place': 'dock', 'dock': 'BigGun',
         'starts_with': 'built in play (DPT_Module_EB, research TECH_BIG_GUN); starts built on DemoBigGun2, '
                        'DemoSect2 and, by default, in multiplayer'},
        {'part': 'smothershipgenetics', 'category': 'cut', 'place': 'dock', 'dock': 'Genetics',
         'starts_with': 'never: the .shp lists a Genetics dock point and the mesh exists, but there is no '
                        'sMothershipGenetics ship type (Ships/ShipDefs.h:17-50), no .shp, no unit cap and no .lod'},
        {'part': 'smothershipsupport', 'category': 'built_in_play', 'place': 'support',
         'starts_with': 'built in play, up to 12 (DPT_Support_EB, research TECH_SUPPORT_SYSTEMS, '
                        'CapsMothershipSupport 12); each one fills the free support point nearest to it'},
    ]},
    {'race': 'race_beast', 'hull': 'bmothership', 'support_kind': 'beast', 'parts': [
        {'part': 'bmothership', 'category': 'core', 'place': 'identity', 'starts_with': 'the hull itself'},
        {'part': 'bmothershipdockingbay', 'category': 'structural', 'place': 'identity', 'mex': 'DOCKBAY',
         'starts_with': 'every game except single-player missions 1-3 (cat_univBeastMothershipModuleTypesGet, '
                        'Game/Universe.c:5412-5424)'},
        {'part': 'bmothershipengineslower', 'category': 'structural', 'place': 'identity', 'mex': 'ENGINESLOWR',
         'starts_with': 'every game except single-player missions 1-3 (cat_univBeastMothershipModuleTypesGet, '
                        'Game/Universe.c:5412-5424)'},
        {'part': 'bmothershipsupport', 'category': 'built_in_play', 'place': 'support',
         'starts_with': 'built in play, up to 12 (DPT_Support_EB, research TECH_SUPPORT_SYSTEMS)'},
    ]},
    {'race': 'race_sect', 'hull': 'scarrier', 'support_kind': 'sect', 'parts': [
        {'part': 'scarrier', 'category': 'core', 'place': 'identity', 'starts_with': 'the hull itself'},
        {'part': 'scarriersupport', 'category': 'built_in_play', 'place': 'support',
         'starts_with': 'built in play, up to 6 (DPT_Support_EB, Game/Dock.c:1500-1503)'},
    ]},
]

NOT_ASSEMBLED = {
    'race_beast/bcarrier': 'bcarrier.shp has no DPT_Support_EB or DPT_Module_EB dock point, and no code clamps a '
                           'part to it; the Beast carrier is one mesh',
    'race_beast/bsectmoship': 'a separate Beast component (aiship.script:143 "used to be bMothershipGenetics") that no '
                              'code attaches to a hull; its mesh is the Somtaaw lower section and every one of its '
                              'vertices coincides with the identity-placed smothershiplowerspine, '
                              'smothershipdockingbay and smothershipengineslower (checked in smothership_assembly.json)',
}


# ------------------------------------------------------------------------------------------------- small math
def mat_mul(a, b):
    return [[sum(a[i][k] * b[k][j] for k in range(3)) for j in range(3)] for i in range(3)]


def mat_vec(m, v):
    return tuple(m[i][0] * v[0] + m[i][1] * v[1] + m[i][2] * v[2] for i in range(3))


def transpose(m):
    return [[m[j][i] for j in range(3)] for i in range(3)]


def cross(a, b):
    return (a[1] * b[2] - a[2] * b[1], a[2] * b[0] - a[0] * b[2], a[0] * b[1] - a[1] * b[0])


IDENTITY = [[1.0, 0.0, 0.0], [0.0, 1.0, 0.0], [0.0, 0.0, 1.0]]
A_OBJ = [[0.0, -1.0, 0.0], [1.0, 0.0, 0.0], [0.0, 0.0, 1.0]]     # OBJ = A * game (export_ships.to_obj_axes)
C_BLENDER = [[1.0, 0.0, 0.0], [0.0, 0.0, -1.0], [0.0, 1.0, 0.0]]  # Blender = C * OBJ (default import: -Z fwd, Y up)


def rot_x(a):   # Game/Matrix.c matMakeRotAboutX
    c, s = math.cos(a), math.sin(a)
    return [[1.0, 0.0, 0.0], [0.0, c, -s], [0.0, s, c]]


def rot_y(a):   # matMakeRotAboutY
    c, s = math.cos(a), math.sin(a)
    return [[c, 0.0, s], [0.0, 1.0, 0.0], [-s, 0.0, c]]


def rot_z(a):   # matMakeRotAboutZ
    c, s = math.cos(a), math.sin(a)
    return [[c, -s, 0.0], [s, c, 0.0], [0.0, 0.0, 1.0]]


def direction(code):
    """GetDirectionVectorOfShip on the host's own frame (identity): 0 up, 1 right, 2 forward, +4 negates."""
    v = [0.0, 0.0, 0.0]
    if (code & 3) == 3:
        raise ValueError('direction code %d leaves the vector unset in the game' % code)
    v[code & 3] = -1.0 if code & 4 else 1.0
    return tuple(v)


def euler_xyz_deg(r):
    """Blender's XYZ Euler (R = Rz * Ry * Rx), degrees."""
    sy = max(-1.0, min(1.0, -r[2][0]))
    y = math.asin(sy)
    if abs(math.cos(y)) > 1e-6:
        x = math.atan2(r[2][1], r[2][2])
        z = math.atan2(r[1][0], r[0][0])
    else:
        x = math.atan2(-r[1][2], r[1][1])
        z = 0.0
    return [round(math.degrees(a), 4) + 0.0 for a in (x, y, z)]


def rnd(x, n=6):
    v = round(x, n)
    return 0.0 if v == 0 else v


def rmat(m):
    return [[rnd(c) for c in row] for row in m]


def rvec(v):
    return [rnd(c, 4) for c in v]


def fmt(x):
    s = '%.6f' % x
    s = s.rstrip('0').rstrip('.')
    return '0' if s in ('-0', '') else s


# ------------------------------------------------------------------------------------------------- game data
def read_text(path):
    with open(path, 'rb') as fh:
        return fh.read().decode('latin-1')


def parse_shp(path):
    """Dock points of a .shp in file order, as scriptSetDockStatics (Game/StatScript.c:1325-1442) reads them, plus
    pMexData. Returns (points, numDockPoints, pMexData)."""
    num = None
    mex = None
    points = []
    for raw in read_text(path).splitlines():
        line = raw.split(';', 1)[0].strip()
        if not line:
            continue
        parts = line.split(None, 1)
        key = parts[0].lower()
        val = parts[1] if len(parts) > 1 else ''
        if key == 'pmexdata':
            mex = val.split()[0]
        elif key == 'number_of_dock_points':
            num = int(re.match(r'\s*(\d+)', val).group(1))
        elif key == 'dockpoint' and num is not None and len(points) < num:
            f = val.replace(',', ' ').split()
            pt = collections.OrderedDict([
                ('index', len(points)), ('name', f[0]), ('type', f[1]), ('flyawaydist', float(f[2])),
                ('mindist', float(f[3])), ('maxdist', float(f[4])), ('headingdirection', int(f[5])),
                ('updirection', int(f[6]))])
            if pt['headingdirection'] == pt['updirection']:     # StatScript.c:1394-1399
                pt['headingdirection'], pt['updirection'] = 0, 2
            points.append(pt)
    first = {}
    for pt in points:
        first.setdefault(pt['type'], pt['index'])
    for pt in points:
        pt['index_in_type'] = pt['index'] - first[pt['type']]
    return points, num, mex


def parse_mex_docks(path):
    """MEX "Dok"/"Dock" chunks (HW1 Game/MEX.h MEXDockingChunk: position, normal, cone, edge, name[20])."""
    import struct
    with open(path, 'rb') as fh:
        d = fh.read()
    if len(d) < 12:
        raise hf.FormatError('%s: too short for a MEX header' % path)
    n = struct.unpack_from('<H', d, 10)[0]
    off = 12
    out = collections.OrderedDict()
    for _ in range(n):
        typ = d[off:off + 4].split(b'\0', 1)[0].decode('latin-1')
        name = d[off + 4:off + 12].split(b'\0', 1)[0].decode('latin-1')
        size = struct.unpack_from('<I', d, off + 12)[0]
        body = d[off + 16:off + 16 + size]
        if typ.lower() == 'dok' and name.lower() == 'dock' and size >= 52:
            pos = struct.unpack_from('<3f', body, 0)
            nrm = struct.unpack_from('<3f', body, 12)
            dock = body[32:52].split(b'\0', 1)[0].decode('latin-1')
            out.setdefault(dock.upper(), {'position': pos, 'normal': nrm})
        off += 16 + size
    return out


# ------------------------------------------------------------------------------------------------- OBJ / MTL
class PartMesh(object):
    """One exported part OBJ, kept as lines plus its vertex data."""

    def __init__(self, obj_path):
        self.path = obj_path
        self.v, self.vt, self.vn = [], [], []
        self.body = []     # ('o', name) | ('usemtl', name) | ('f', [(vi, ti, ni), ...]) | ('vt', idx)
        self.faces = 0
        self.mtllib = None
        with open(obj_path) as fh:
            for ln, raw in enumerate(fh, 1):
                line = raw.strip()
                if not line or line.startswith('#'):
                    continue
                tag, _, rest = line.partition(' ')
                if tag == 'v':
                    self.v.append(tuple(float(x) for x in rest.split()[:3]))
                elif tag == 'vn':
                    self.vn.append(tuple(float(x) for x in rest.split()[:3]))
                elif tag == 'vt':
                    self.vt.append(rest.split())
                elif tag == 'f':
                    corners = []
                    for c in rest.split():
                        idx = c.split('/')
                        vals = []
                        for k in range(3):
                            s = idx[k] if k < len(idx) else ''
                            if s == '':
                                vals.append(None)
                            else:
                                i = int(s)
                                if i <= 0:
                                    raise hf.FormatError('%s:%d: relative or zero index' % (obj_path, ln))
                                vals.append(i)
                        corners.append(tuple(vals))
                    self.body.append(('f', corners))
                    self.faces += 1
                elif tag == 'o':
                    self.body.append(('o', rest.strip()))
                elif tag == 'usemtl':
                    self.body.append(('usemtl', rest.strip()))
                elif tag == 'mtllib':
                    self.mtllib = rest.strip()
                elif tag in ('g', 's'):
                    continue
                else:
                    raise hf.FormatError('%s:%d: unexpected OBJ statement %r' % (obj_path, ln, tag))


def parse_mtl(path):
    """newmtl blocks: name -> list of (kind, text). kind 'map' lines carry a path, 'mask' comments too."""
    mats = collections.OrderedDict()
    cur = None
    with open(path) as fh:
        for raw in fh:
            line = raw.rstrip('\n')
            s = line.strip()
            if s.startswith('newmtl '):
                cur = s[7:].strip()
                mats[cur] = []
            elif cur is None or not s:
                continue
            elif s.startswith('map_') or s.startswith('bump') or s.startswith('disp'):
                key, _, p = s.partition(' ')
                mats[cur].append(('map', key, p.strip()))
            elif s.startswith('# team ') and ': ' in s:
                head, _, p = s.partition(': ')
                mats[cur].append(('mask', head, p.strip()))
            else:
                mats[cur].append(('line', s, None))
    return mats


def short_label(part, hull):
    if part == hull:
        return 'core'
    if part.startswith(hull):
        return part[len(hull):]
    return part


# ------------------------------------------------------------------------------------------------- assembly
class Assembler(object):

    def __init__(self, out, tree, manifest):
        self.out = out
        self.tree = tree
        self.manifest = manifest

    def lod_entry(self, race, ship, level):
        rec = self.manifest['ships'].get('%s/%s' % (race, ship))
        if not rec:
            return None
        by_level = {e['level']: e for e in rec.get('lods', [])}
        e = by_level.get(level)
        seen = set()
        while e is not None and 'obj' not in e and e.get('same_mesh_as_level') is not None and e['level'] not in seen:
            seen.add(e['level'])
            e = by_level.get(e['same_mesh_as_level'])
        return e if e is not None and 'obj' in e else None

    def hull_levels(self, race, hull):
        rec = self.manifest['ships'].get('%s/%s' % (race, hull))
        return sorted(e['level'] for e in rec.get('lods', []) if self.lod_entry(race, hull, e['level']))

    def dock_data(self, h):
        race, hull = h['race'], h['hull']
        shp_rel = '%s/%s.shp' % (race, hull)
        shp_path = self.tree.resolve(shp_rel)
        if not shp_path:
            raise hf.FormatError('missing %s' % shp_rel)
        points, num, mex_field = parse_shp(shp_path)
        if not mex_field:
            raise hf.FormatError('%s has no pMexData line' % shp_rel)
        mex_rel = race + '/' + mex_field.replace('\\', '/')
        mex_path = self.tree.resolve(mex_rel)
        if not mex_path:
            raise hf.FormatError('missing %s (pMexData of %s)' % (mex_rel, shp_rel))
        mex = parse_mex_docks(mex_path)
        # the same dock positions in every LOD's .mex? (the game reads only pMexData = LOD 0)
        lod_mex = []
        for k in self.tree.walk('%s/%s/rl0' % (race, hull)):
            if k.endswith('.mex') and k != mex_rel.lower():
                other = parse_mex_docks(self.tree.resolve(k))
                diff = [nm for nm, c in mex.items() if nm in other and
                        max(abs(a - b) for a, b in zip(c['position'], other[nm]['position'])) > 1e-3]
                lod_mex.append({'mex': k, 'dock_points': len(other), 'positions_differing_from_pMexData': diff})
        return {'shp': hf_rel(shp_path), 'shp_numDockPoints': num, 'shp_dock_points_read': len(points),
                'mex': hf_rel(mex_path), 'mex_dock_chunks': len(mex), 'other_lod_mex': lod_mex,
                'points': points, 'mex_docks': mex}

    def point_transform(self, pt, mexd, support_kind):
        """(M, t, details) for a ship launched from dock point pt (Dock.c:2700-2716 + dockPutShipOutside)."""
        chunk = mexd.get(pt['name'].upper())
        if chunk is None:
            pos = (0.0, 0.0, 0.0)       # MEX.c:357-369: a point with no chunk gets position 0
        else:
            pos = chunk['position']
        h = direction(pt['headingdirection'])
        u = direction(pt['updirection'])
        r = cross(h, u)
        m = [[u[i], r[i], h[i]] for i in range(3)]      # matCreateMatFromVecs(up, right, heading): columns
        turns = []
        parity = pt['index_in_type']
        if pt['type'].upper() == 'DPT_SUPPORT_EB':
            if support_kind == 'sect':
                if parity & 1:
                    m = mat_mul(m, rot_x(PI_F))
                    turns.append('RotX(pi)')
            elif support_kind == 'beast':
                if parity & 2:
                    m = mat_mul(m, rot_y(PI_F))
                    a = ROT7_NEG if parity & 1 else ROT7_POS
                    m = mat_mul(m, rot_z(a))
                    turns += ['RotY(pi)', 'RotZ(%+.0f deg)' % math.degrees(a)]
                if parity & 1:
                    m = mat_mul(m, rot_x(PI_F))
                    turns.append('RotX(pi)')
        details = collections.OrderedDict([
            ('name', pt['name']), ('type', pt['type']), ('shp_index', pt['index']),
            ('index_in_type', parity), ('headingdirection', pt['headingdirection']),
            ('updirection', pt['updirection']), ('mex_chunk_found', chunk is not None),
            ('mex_position_game', rvec(pos)),
            ('mex_normal_game', rvec(chunk['normal']) if chunk else None),
            ('support_turns', turns)])
        return m, pos, details

    def instances(self, h, dock):
        """[(instance object name, part record, M, t, placement dict)]"""
        out = []
        pts = dock['points']
        by_name = {p['name'].upper(): p for p in pts}
        for prec in h['parts']:
            part = prec['part']
            if prec['place'] == 'identity':
                pl = collections.OrderedDict([
                    ('source', 'code: made with the ship at the host position and clamped to it '
                               '(%s:2249-2251, Game/Clamp.c:28-52, defaultshipmatrix %s:351)' % (UNIV, UNIV)
                     if prec['category'] != 'core' else 'the hull itself')])
                if prec.get('mex'):
                    pm = dock['mex_docks'].get(prec['mex'])
                    pl['mex_dock_chunk'] = prec['mex']
                    pl['mex_dock_chunk_position_game'] = rvec(pm['position']) if pm else None
                    pl['agrees_with_mex_chunk'] = bool(pm) and max(abs(c) for c in pm['position']) < 1e-3
                out.append((part, prec, IDENTITY, (0.0, 0.0, 0.0), pl))
            elif prec['place'] == 'dock':
                pt = by_name.get(prec['dock'].upper())
                if pt is None:
                    raise hf.FormatError('%s: no dock point %s in the .shp' % (h['hull'], prec['dock']))
                m, t, det = self.point_transform(pt, dock['mex_docks'], h['support_kind'])
                ident = max(abs(m[i][j] - IDENTITY[i][j]) for i in range(3) for j in range(3)) < 1e-6 and \
                    max(abs(c) for c in t) < 1e-3
                pl = collections.OrderedDict([
                    ('source', 'MEX dock point %s + .shp heading/up codes (external build, Game/Dock.c:2700-2716, '
                               '2969-3003)' % pt['name']),
                    ('dock_point', det),
                    ('agrees_with_clamp_at_host_position', ident)])
                out.append((part, prec, m, t, pl))
            elif prec['place'] == 'support':
                sup = [p for p in pts if p['type'].upper() == 'DPT_SUPPORT_EB']
                if not sup:
                    raise hf.FormatError('%s: no DPT_Support_EB dock points' % h['hull'])
                for k, pt in enumerate(sup, 1):
                    m, t, det = self.point_transform(pt, dock['mex_docks'], h['support_kind'])
                    pl = collections.OrderedDict([
                        ('source', 'MEX dock point %s + .shp heading/up codes + support parity turn '
                                   '(Game/Dock.c:2969-3050)' % pt['name']),
                        ('dock_point', det)])
                    out.append(('%s_%02d' % (part, k), prec, m, t, pl))
        return out

    def assemble(self, h, level, dock):
        race, hull = h['race'], h['hull']
        hull_dir_rel = 'ships/%s/%s' % (race, hull)
        obj_rel = '%s/%s_assembled_lod%d.obj' % (hull_dir_rel, hull, level)
        mtl_rel = obj_rel[:-4] + '.mtl'
        obj_path = os.path.join(self.out, obj_rel)
        mtl_path = os.path.join(self.out, mtl_rel)
        mtl_dir = os.path.dirname(mtl_path)
        insts = self.instances(h, dock)
        meshes, mtls, entries, skipped = {}, {}, {}, []
        for prec in h['parts']:
            e = self.lod_entry(race, prec['part'], level)
            if e is None:
                skipped.append(prec['part'])
                continue
            entries[prec['part']] = e
            meshes[prec['part']] = PartMesh(os.path.join(self.out, e['obj']))
            mtls[prec['part']] = parse_mtl(os.path.join(self.out, e['mtl']))
        lines = ['# Homeworld: Cataclysm modular ship %s/%s, LOD %d, assembled by tools/assets/assemble_modular.py'
                 % (race, hull, level),
                 '# One object per part (o <part>); the sub-objects of each part file are kept as groups (g).',
                 '# Vertices are in the hull\'s ship space, axes as export_ships.py: OBJ (x, y, z) = game (-y, x, z).',
                 '# Every part keeps its own materials, named <label>__<material>; see %s_assembly.json.' % hull,
                 'mtllib ' + os.path.basename(mtl_path)]
        nv = nvt = nvn = nf = 0
        parts_out = []
        used_mats = collections.OrderedDict()
        all_names = set()
        for (oname, prec, m, t, pl) in insts:
            part = prec['part']
            if part not in meshes:
                continue
            pm = meshes[part]
            label = short_label(part, hull)
            m_obj = mat_mul(mat_mul(A_OBJ, m), transpose(A_OBJ))
            t_obj = mat_vec(A_OBJ, t)
            if oname in all_names:
                raise hf.FormatError('duplicate object name %s' % oname)
            all_names.add(oname)
            lines.append('o ' + oname)
            vb, tb, nb = nv, nvt, nvn
            mn = [float('inf')] * 3
            mx = [float('-inf')] * 3
            xv = []
            for v in pm.v:
                w = mat_vec(m_obj, v)
                w = (w[0] + t_obj[0], w[1] + t_obj[1], w[2] + t_obj[2])
                xv.append(w)
                for i in range(3):
                    mn[i] = min(mn[i], w[i])
                    mx[i] = max(mx[i], w[i])
                lines.append('v %s %s %s' % tuple(fmt(c) for c in w))
            for n in pm.vn:
                lines.append('vn %s %s %s' % tuple(fmt(c) for c in mat_vec(m_obj, n)))
            for uv in pm.vt:
                lines.append('vt ' + ' '.join(uv))
            nv += len(pm.v)
            nvt += len(pm.vt)
            nvn += len(pm.vn)
            for kind, val in pm.body:
                if kind == 'o':
                    lines.append('g %s__%s' % (oname, re.sub(r'\s+', '_', val)))
                elif kind == 'usemtl':
                    if val not in mtls[part]:
                        raise hf.FormatError('%s: usemtl %s not in its MTL' % (pm.path, val))
                    mname = '%s__%s' % (label, val)
                    used_mats[mname] = (part, val)
                    lines.append('usemtl ' + mname)
                else:
                    cs = []
                    for (vi, ti, ni) in val:
                        s = str(vi + vb)
                        if ti is not None or ni is not None:
                            s += '/' + (str(ti + tb) if ti is not None else '')
                        if ni is not None:
                            s += '/' + str(ni + nb)
                        cs.append(s)
                    lines.append('f ' + ' '.join(cs))
            nf += pm.faces
            e = entries[part]
            rec = collections.OrderedDict([
                ('object', oname), ('part', part), ('category', prec['category']),
                ('built_in_play', prec['category'] == 'built_in_play'), ('starts_with', prec['starts_with']),
                ('part_obj', e['obj']), ('material_prefix', label + '__'),
                ('placement', pl),
                ('transform_game', collections.OrderedDict([
                    ('rotation_rows', rmat(m)), ('translation', rvec(t)),
                    ('note', 'x = up, y = right, z = forward; v_hull = R v_part + t')])),
                ('transform_obj', collections.OrderedDict([
                    ('rotation_rows', rmat(m_obj)), ('translation', rvec(t_obj))])),
                ('transform_blender', collections.OrderedDict([
                    ('location', rvec(mat_vec(C_BLENDER, t_obj))),
                    ('rotation_euler_xyz_deg', euler_xyz_deg(mat_mul(mat_mul(C_BLENDER, m_obj),
                                                                      transpose(C_BLENDER)))),
                    ('note', 'Blender frame after the default OBJ import (Z up, nose toward -Y); the transform is '
                             'already baked into the vertices, so the imported objects have identity transforms')])),
                ('counts', {'v': len(pm.v), 'vt': len(pm.vt), 'vn': len(pm.vn), 'f': pm.faces}),
                ('bbox_obj', {'min': rvec(mn), 'max': rvec(mx)}),
                ('bbox_centre_obj', rvec([(a + b) / 2 for a, b in zip(mn, mx)])),
            ])
            parts_out.append((rec, xv))
        # MTL: one block per used material, with paths made relative to the assembled MTL
        mtl = ['# Homeworld: Cataclysm materials for %s/%s LOD %d, assembled by tools/assets/assemble_modular.py'
               % (race, hull, level),
               '# Each block is the part\'s own exported material, renamed <label>__<name>; map_* paths point at',
               '# the part\'s own PNGs under textures/<race>/<part>/lod<N>/ (nothing is copied).', '']
        missing = []
        for mname, (part, src) in used_mats.items():
            src_dir = os.path.dirname(os.path.join(self.out, entries[part]['mtl']))
            mtl.append('newmtl ' + mname)
            for kind, a, b in mtls[part][src]:
                if kind == 'line':
                    mtl.append(a)
                else:
                    full = os.path.normpath(os.path.join(src_dir, b))
                    if not os.path.isfile(full):
                        missing.append(full)
                    p = os.path.relpath(full, mtl_dir).replace(os.sep, '/')
                    mtl.append('%s %s' % (a, p) if kind == 'map' else '%s: %s' % (a, p))
            mtl.append('')
        write_file(obj_path, '\n'.join(lines) + '\n')
        write_file(mtl_path, '\n'.join(mtl) + '\n')
        stats = collections.OrderedDict([('obj', obj_rel), ('mtl', mtl_rel), ('objects', len(parts_out)),
                                         ('v', nv), ('vt', nvt), ('vn', nvn), ('f', nf),
                                         ('materials', len(used_mats)), ('parts_missing_this_lod', skipped),
                                         ('texture_paths_missing_at_write', missing)])
        return stats, parts_out


def hf_rel(path):
    return os.path.relpath(path, REPO).replace(os.sep, '/')


def write_file(path, text):
    os.makedirs(os.path.dirname(path), exist_ok=True)
    with open(path, 'w') as fh:
        fh.write(text)


# ------------------------------------------------------------------------------------------------- validation
def aabb_gap(a, b):
    d = 0.0
    for i in range(3):
        g = max(a['min'][i] - b['max'][i], b['min'][i] - a['max'][i], 0.0)
        d += g * g
    return math.sqrt(d)


def defined_names(mtl_path):
    with open(mtl_path) as fh:
        return set(l.strip()[7:].strip() for l in fh if l.strip().startswith('newmtl '))


def validate(out, stats, parts_out, expect_parts):
    """Parse the written OBJ/MTL back and check counts, indices, materials, texture paths and placement."""
    obj_path = os.path.join(out, stats['obj'])
    mtl_path = os.path.join(out, stats['mtl'])
    res = collections.OrderedDict()
    cnt = collections.Counter()
    objects, usemtl, bad_faces = [], set(), 0
    with open(obj_path) as fh:
        for raw in fh:
            tag, _, rest = raw.strip().partition(' ')
            if tag in ('v', 'vt', 'vn'):
                cnt[tag] += 1
            elif tag == 'o':
                objects.append(rest)
            elif tag == 'usemtl':
                usemtl.add(rest)
            elif tag == 'f':
                cnt['f'] += 1
                cs = rest.split()
                if len(cs) != 3:
                    bad_faces += 1
                    continue
                for c in cs:
                    idx = c.split('/')
                    lim = (cnt['v'], cnt['vt'], cnt['vn'])
                    for k, s in enumerate(idx[:3]):
                        if s and not (1 <= int(s) <= lim[k]):
                            bad_faces += 1
    defined, maps, maps_missing = set(), 0, []
    mdir = os.path.dirname(mtl_path)
    with open(mtl_path) as fh:
        for raw in fh:
            s = raw.strip()
            if s.startswith('newmtl '):
                defined.add(s[7:].strip())
            elif s.startswith('map_'):
                maps += 1
                p = os.path.join(mdir, s.split(None, 1)[1])
                if not os.path.isfile(p):
                    maps_missing.append(p)
            elif s.startswith('# team ') and ': ' in s:
                p = os.path.join(mdir, s.partition(': ')[2])
                if not os.path.isfile(p):
                    maps_missing.append(p)
    want = {k: sum(r['counts'][k] for r, _ in parts_out) for k in ('v', 'vt', 'vn', 'f')}
    res['counts_written'] = {k: cnt[k] for k in ('v', 'vt', 'vn', 'f')}
    res['counts_expected_sum_of_parts'] = want
    res['counts_match'] = all(cnt[k] == want[k] for k in want)
    res['manifest_counts_match'] = all(
        r['counts']['v'] == e['vertices'] and r['counts']['f'] == e['polygons'] and r['counts']['vt'] == e['uvs'] and
        r['counts']['vn'] == e['normals'] for (r, _), e in zip(parts_out, expect_parts))
    res['objects'] = len(objects)
    res['object_names_unique'] = len(objects) == len(set(objects))
    # Blender cuts object and material names at 63 bytes, which could merge two of them
    res['names_over_63_bytes'] = sorted(n for n in set(objects) | defined_names(mtl_path) if len(n.encode()) > 63)
    res['faces_bad'] = bad_faces
    res['usemtl_undefined'] = sorted(usemtl - defined)
    res['map_paths'] = maps
    res['texture_paths_missing'] = maps_missing
    # placement: the gap from each part's box to the nearest other part's box (0 = touching or overlapping)
    boxes = [(r['object'], r['bbox_obj']) for r, _ in parts_out]
    place = []
    for name, box in boxes:
        best = None
        for other, ob in boxes:
            if other == name:
                continue
            g = aabb_gap(box, ob)
            if best is None or g < best[0]:
                best = (g, other)
        c = [(a + b) / 2 for a, b in zip(box['min'], box['max'])]
        place.append(collections.OrderedDict([
            ('object', name), ('bbox_centre_obj', rvec(c)),
            ('distance_from_hull_origin', rnd(math.sqrt(sum(x * x for x in c)), 1)),
            ('nearest_other_part', best[1] if best else None),
            ('aabb_gap_to_nearest', rnd(best[0], 3) if best else None)]))
    res['placement'] = place
    res['parts_not_touching_any_other'] = [p['object'] for p in place
                                          if p['aabb_gap_to_nearest'] is not None and p['aabb_gap_to_nearest'] > 1.0]
    # support pairs: an even and an odd point at mirrored positions (game y <-> -y; the same position when y = 0)
    # must hold mirror images (OBJ x <-> -x, same height). The error also absorbs the small asymmetries of the dock
    # positions themselves (under 0.5 units in the Beast mothership's .mex).
    sup = []
    for r, xv in parts_out:
        dp = r['placement'].get('dock_point')
        if dp and dp['type'].upper() == 'DPT_SUPPORT_EB':
            sup.append((dp, r))
    mirror = []
    taken = set()
    for da, ra in sup:
        if da['index_in_type'] & 1:
            continue
        pa = da['mex_position_game']
        for db, rb in sup:
            pb = db['mex_position_game']
            if (db['index_in_type'] & 1) and rb['object'] not in taken and abs(pa[0] - pb[0]) < 1.0 and \
                    abs(pa[1] + pb[1]) < 1.0 and abs(pa[2] - pb[2]) < 1.0:
                taken.add(rb['object'])
                a, b = ra['bbox_obj'], rb['bbox_obj']
                err = max(abs(a['min'][0] + b['max'][0]), abs(a['max'][0] + b['min'][0]),
                          abs(a['min'][1] - b['min'][1]), abs(a['max'][1] - b['max'][1]))
                mirror.append({'objects': [ra['object'], rb['object']], 'mex_positions_game': [pa, pb],
                               'mirror_error': rnd(err, 3)})
    if sup:
        res['support_pairs_mirrored'] = mirror
        res['support_pairs_mirrored_ok'] = len(mirror) * 2 == len(sup) and all(m['mirror_error'] < 1.0
                                                                               for m in mirror)
    res['ok'] = bool(res['counts_match'] and res['manifest_counts_match'] and res['object_names_unique'] and
                     not bad_faces and not res['usemtl_undefined'] and not maps_missing and
                     not res['names_over_63_bytes'] and
                     not res['parts_not_touching_any_other'] and res.get('support_pairs_mirrored_ok', True))
    return res


def bsectmoship_check(out, manifest, level):
    """Relic's own merged mesh of the Somtaaw lower section (race_beast/bsectmoship) against the identity-placed
    lower spine, docking bay and lower engines: an independent test of the shared-frame placement."""
    rec = manifest['ships'].get('race_beast/bsectmoship')
    if not rec:
        return None
    lods = {e['level']: e for e in rec['lods'] if 'obj' in e}
    if level not in lods:
        return None
    parts = ('smothershiplowerspine', 'smothershipdockingbay', 'smothershipengineslower')
    key = lambda v: tuple(int(round(c / 0.05)) for c in v)  # noqa: E731
    target = PartMesh(os.path.join(out, lods[level]['obj'])).v
    have = set()
    part_cover = {}
    for p in parts:
        e = {x['level']: x for x in manifest['ships']['race_sect/' + p]['lods'] if 'obj' in x}.get(level)
        if e is None:
            return None
        vs = PartMesh(os.path.join(out, e['obj'])).v
        tk = set(key(v) for v in target)
        part_cover[p] = '%d/%d' % (sum(1 for v in vs if key(v) in tk), len(vs))
        have.update(key(v) for v in vs)
    hit = sum(1 for v in target if key(v) in have)
    return collections.OrderedDict([
        ('mesh', lods[level]['obj']), ('bsectmoship_vertices_on_a_part_vertex', '%d/%d' % (hit, len(target))),
        ('part_vertices_present_in_bsectmoship', part_cover), ('tolerance', 0.05),
        ('pass', hit == len(target))])


BLENDER_SCRIPT = r'''
import bpy, json, os, sys
argv = sys.argv[sys.argv.index('--') + 1:]
res = []
for path in argv:
    bpy.ops.wm.read_factory_settings(use_empty=True)
    bpy.ops.wm.obj_import(filepath=path)
    objs = [o for o in bpy.context.scene.objects if o.type == 'MESH']
    mats, imgs, bad = set(), {}, []
    faces = 0
    for o in objs:
        faces += len(o.data.polygons)
        for s in o.material_slots:
            if s.material is None:
                continue
            mats.add(s.material.name)
            if s.material.node_tree is None:
                continue
            for n in s.material.node_tree.nodes:
                if n.type == 'TEX_IMAGE':
                    im = n.image
                    if im is None:
                        bad.append(s.material.name + ': no image')
                        continue
                    ok = os.path.isfile(bpy.path.abspath(im.filepath)) and im.size[0] > 0 and im.size[1] > 0
                    imgs[im.name] = ok
                    if not ok:
                        bad.append(s.material.name + ': ' + im.filepath)
    res.append({'obj': path, 'mesh_objects': len(objs), 'object_names': sorted(o.name for o in objs),
                'faces': faces, 'materials': len(mats), 'images': len(imgs),
                'images_loaded': sum(1 for v in imgs.values() if v), 'problems': bad,
                'blender': bpy.app.version_string})
print('ASSEMBLE_MODULAR_BLENDER ' + json.dumps(res))
'''


def find_blender(arg):
    if arg and arg != 'auto':
        return arg if os.path.isfile(arg) else None
    for c in BLENDER_CANDIDATES:
        p = shutil.which(c) or (c if os.path.isfile(c) else None)
        if p:
            return p
    return None


def blender_check(blender, obj_paths):
    tmp = tempfile.mkdtemp(prefix='assemble_modular_')
    try:
        script = os.path.join(tmp, 'blender_import_check.py')
        write_file(script, BLENDER_SCRIPT)
        cmd = [blender, '--background', '--factory-startup', '--python', script, '--'] + obj_paths
        p = subprocess.run(cmd, stdout=subprocess.PIPE, stderr=subprocess.STDOUT, timeout=600,
                           universal_newlines=True)
        for line in p.stdout.splitlines():
            if line.startswith('ASSEMBLE_MODULAR_BLENDER '):
                return json.loads(line[len('ASSEMBLE_MODULAR_BLENDER '):])
        return [{'error': 'no result from Blender (exit %d)' % p.returncode, 'tail': p.stdout[-2000:]}]
    finally:
        shutil.rmtree(tmp, ignore_errors=True)


# ------------------------------------------------------------------------------------------------- README
README_SECTION = r'''## 7. Modular ships: assembled models

Three ships are built from parts: the Somtaaw mothership (the Kuun-Lan), the Beast mothership and the Somtaaw
carrier. Each part is a ship of its own in the game files, so its normal export (`ships/<race>/<part>/`) holds only
that one piece. The assembled files put all the pieces together:

| file | what it is |
|---|---|
| `ships/race_sect/smothership/smothership_assembled_lod0.obj` | the whole Kuun-Lan: spine, every module and all 12 supports |
| `ships/race_beast/bmothership/bmothership_assembled_lod0.obj` | the Beast mothership with its docking bay, lower engines and 12 supports |
| `ships/race_sect/scarrier/scarrier_assembled_lod0.obj` | the Somtaaw carrier with its 6 supports |
| `<hull>_assembly.json` (next to each) | every part: whether it is structural or built in play, its dock point, position and rotation |

Open them like any other ship (section 1). Every part is its own object, named after the part, for example
`smothershipcommand`; the supports are numbered (`smothershipsupport_01` to `_12`). The materials still point at each
part's own textures in `textures/<race>/<part>/lod0/`: nothing was copied, so upscale the PNGs where they are, as
usual. Material names start with a short part label (`command__`, `weapons__`, `support__`, `core__` for the hull)
followed by the part's usual material name, so the Shading workspace still tells you which PNG a face uses.

**Showing and hiding parts in Blender:** in the Outliner (top right), click the eye icon next to an object. Or click a
part in the 3D view and press `H` to hide it, `Shift H` to hide everything else, and `Alt H` to show all again.

Which parts are always there and which are built in play:

| ship | always part of the ship | only in early missions | built by the player in play | not in the game |
|---|---|---|---|---|
| Kuun-Lan | `smothership` (spine), `smothershipenginesupper`, `smothershipcommand`, `smothershipengineering` | `smothershipengineslower`, `smothershiplowerspine` (tutorial and missions 1-3), `smothershiporecanarmour` (tutorial and missions 1-2) | `smothershipweapons`, `smothershiparmour`, `smothershipmicro`, `smothershipspecial`, `smothershipdockingbay`, `smothershipbiggun`, up to 12 `smothershipsupport` | `smothershipgenetics` (cut: the mesh and its slot exist, the ship type does not) |
| Beast mothership | `bmothership`, `bmothershipdockingbay`, `bmothershipengineslower` (missing only in single-player missions 1-3) | | up to 12 `bmothershipsupport` | |
| Somtaaw carrier | `scarrier` | | up to 6 `scarriersupport` | |

Things to know:

* **In play, a built module appears only once the player has built it**, and a support only once it is built; the
  assembled files show every part at once. In multiplayer the Kuun-Lan starts with its seven modules already built,
  unless the game is set up without them.
* The **ore-can armour** (`smothershiporecanarmour`) is the early-game plating over the top of the spine, exactly where
  Weapons, Special, Micro and Armour are built later. It overlaps them: hide it to see the four top modules.
* The Kuun-Lan's **lower spine and lower engines** are part of the ship only in the tutorial, missions 1-3 and two
  demo maps (DemoBigGun2, DemoSect2). Later missions and other skirmish maps start without them.
  `race_beast/bsectmoship` is that same lower section as one mesh.
* The 12 **supports** fill the space inside the lower spine, so with the lower spine shown you see little of them:
  hide `smothershiplowerspine` to see them as they look in later missions.
* The parts sit exactly where the game puts them: the modules share the mothership's own frame, and the supports go
  to their dock points, turned to face left or right as the game turns them. Each object's position and rotation are
  already applied to its vertices; `<hull>_assembly.json` lists them.
* If the export is made again, run `tools/assets/assemble_modular.py` again: a new export deletes these files.
'''


def update_readme(out, text):
    path = os.path.join(out, 'README.md')
    with open(path) as fh:
        cur = fh.read()
    block = README_BEGIN + '\n' + text.rstrip('\n') + '\n' + README_END + '\n'
    if README_BEGIN in cur and README_END in cur:
        head, _, rest = cur.partition(README_BEGIN)
        _, _, tail = rest.partition(README_END + '\n')
        new = head + block + tail
    else:
        new = cur.rstrip('\n') + '\n\n' + block
    with open(path, 'w') as fh:
        fh.write(new)


# ------------------------------------------------------------------------------------------------- main
def main(argv):
    ap = argparse.ArgumentParser(description=__doc__.split('\n')[0])
    ap.add_argument('--out', default=DEF_OUT)
    ap.add_argument('--data', default=DEF_DATA)
    ap.add_argument('--update', default=DEF_UPDATE)
    ap.add_argument('--lods', default='0', help='comma-separated LOD levels, or "all" (default 0)')
    ap.add_argument('--only', nargs='*', help='race/hull to assemble (default: all three)')
    ap.add_argument('--no-readme', action='store_true', help='do not update <out>/README.md')
    ap.add_argument('--blender', nargs='?', const='auto', default=None,
                    help='import every assembled OBJ in headless Blender (optionally its path)')
    args = ap.parse_args(argv)
    out = os.path.abspath(args.out)
    man_path = os.path.join(out, 'manifest.json')
    if not os.path.isfile(man_path):
        print('no %s: run tools/assets/export_ships.py first' % man_path)
        return 2
    with open(man_path) as fh:
        manifest = json.load(fh)
    tree = hf.DataTree(os.path.abspath(args.data), os.path.abspath(args.update))
    asm = Assembler(out, tree, manifest)
    only = set(o.lower().strip('/') for o in args.only) if args.only else None
    blender = find_blender(args.blender) if args.blender else None
    if args.blender and not blender:
        print('Blender not found (%s); skipping the import check' % args.blender)
    all_ok = True
    for h in HULLS:
        key = '%s/%s' % (h['race'], h['hull'])
        if only and key not in only and h['hull'] not in only:
            continue
        levels = asm.hull_levels(h['race'], h['hull']) if args.lods == 'all' else \
            [int(x) for x in args.lods.split(',') if x.strip()]
        dock = asm.dock_data(h)
        doc = collections.OrderedDict([
            ('tool', 'tools/assets/assemble_modular.py'), ('race', h['race']), ('hull', h['hull']),
            ('axes', 'game ship frame x = up, y = right, z = forward; OBJ (x, y, z) = game (-y, x, z); Blender '
                     '(default OBJ import) (X, Y, Z) = OBJ (x, -z, y)'),
            ('placement_rules', [
                'parts made with the ship: clamped at the host position with the default orientation -> identity '
                '(.claude/worktrees/m2/src/Game/UnivUpdate.c:2219-2267, Game/Clamp.c:28-52)',
                'externally built parts: host + dockpoint.position (raw MEX Dok chunk), columns (up, right, heading) '
                'from the .shp heading/up codes (Game/Dock.c:2700-2716, 2969-3003, Game/MEX.c:354)',
                'support parts: parity of the point index within DPT_Support_EB turns them (Game/Dock.c:3013-3050; '
                'exe constants 0x0072a7d8 pi, 0x0072a7cc +7 deg, 0x0072a7d0 -7 deg)']),
            ('dock_sources', collections.OrderedDict(
                (k, v) for k, v in dock.items() if k not in ('points', 'mex_docks'))),
            ('lods', [])])
        for level in levels:
            stats, parts_out = asm.assemble(h, level, dock)
            expect = [asm.lod_entry(h['race'], r['part'], level) for r, _ in parts_out]
            val = validate(out, stats, parts_out, expect)
            if h['hull'] == 'smothership':
                bc = bsectmoship_check(out, manifest, level)
                if bc is not None:
                    val['bsectmoship_cross_check'] = bc
            all_ok = all_ok and val['ok']
            doc['lods'].append(collections.OrderedDict([
                ('level', level), ('files', stats), ('parts', [r for r, _ in parts_out]), ('validation', val)]))
            print('%s LOD %d: %d objects, v %d, f %d, %d materials, %d texture paths -> %s  [%s]'
                  % (key, level, stats['objects'], stats['v'], stats['f'], stats['materials'],
                     val['map_paths'], stats['obj'], 'ok' if val['ok'] else 'FAILED'))
            for p in val['placement']:
                print('    %-30s centre (%9.1f %9.1f %9.1f)  nearest %-28s gap %.1f'
                      % (p['object'], p['bbox_centre_obj'][0], p['bbox_centre_obj'][1], p['bbox_centre_obj'][2],
                         p['nearest_other_part'], p['aabb_gap_to_nearest']))
            if 'support_pairs_mirrored' in val:
                mp = val['support_pairs_mirrored']
                print('    support pairs mirrored left/right: %d pairs, worst error %.3f [%s]'
                      % (len(mp), max([m['mirror_error'] for m in mp] or [0.0]),
                         'ok' if val['support_pairs_mirrored_ok'] else 'FAILED'))
            if 'bsectmoship_cross_check' in val:
                bc = val['bsectmoship_cross_check']
                print('    bsectmoship cross-check: %s vertices on a part vertex [%s]'
                      % (bc['bsectmoship_vertices_on_a_part_vertex'], 'ok' if bc['pass'] else 'FAILED'))
        if blender and doc['lods']:
            paths = [os.path.join(out, e['files']['obj']) for e in doc['lods']]
            br = blender_check(blender, paths)
            for e, b in zip(doc['lods'], br):
                e['validation']['blender'] = b
                bok = ('error' not in b and b.get('mesh_objects') == e['files']['objects'] and
                       b.get('images_loaded') == b.get('images') and not b.get('problems'))
                e['validation']['blender_ok'] = bok
                all_ok = all_ok and bok
                print('    Blender %s: %s mesh objects, %s faces, images %s/%s loaded, problems %s'
                      % (b.get('blender', '?'), b.get('mesh_objects'), b.get('faces'), b.get('images_loaded'),
                         b.get('images'), len(b.get('problems', [])) if 'problems' in b else b.get('error')))
        doc['not_assembled'] = NOT_ASSEMBLED
        write_file(os.path.join(out, 'ships', h['race'], h['hull'], '%s_assembly.json' % h['hull']),
                   json.dumps(doc, indent=1) + '\n')
    if not args.no_readme:
        update_readme(out, README_SECTION)
    return 0 if all_ok else 1


if __name__ == '__main__':
    sys.exit(main(sys.argv[1:]))
