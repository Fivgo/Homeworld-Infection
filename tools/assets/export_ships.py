#!/usr/bin/env python3
"""Export every Homeworld: Cataclysm ship (all race folders, every LOD) as Wavefront OBJ + MTL + PNG textures.

Usage:
  tools/assets/export_ships.py [--data DIR] [--update DIR] [--out DIR] [--only RACE/SHIP ...] [--no-clean]
  tools/assets/export_ships.py --validate-only [--out DIR]

Defaults: --data original/game_extracted/cataclysm, --update original/game_extracted/update,
          --out original/assets-export (all relative to the repository root). Python 3 stdlib only.

What it writes (see the README.md it generates in --out, and docs/assets-export.md):
  ships/<race>/<ship>/<ship>_lodN.obj|.mtl          the ship's source mesh (.geo), one file per LOD; its
                                                    materials point at the individual source textures
  ships/<race>/<ship>/ingame/<ship>_lodN.obj|.mtl   the paged mesh (.peo) the game actually loads, whose
                                                    materials point at the packed texture pages
  textures/<race>/<ship>/lodN/<name>.png            every source .lif a mesh uses, RGBA
  textures/<race>/<ship>/lodN/<name>_teambase.png   team base-colour weight (TeamColor0), greyscale
  textures/<race>/<ship>/lodN/<name>_teamstripe.png team stripe-colour weight (TeamColor1), greyscale
  textures/<race>/<ship>/lodN/pages/pageK.png       packed texture pages (+ masks), with the placement of
                                                    every source texture inside them in manifest.json
  manifest.json, index.md, README.md, validation.json

Format grounding: tools/assets/hwformats.py (docs/formats/mesh.md, docs/formats/textures.md, HW1 LOD.c,
Mesh.c meshObjectRenderR / meshPagedName / meshTextureNameToPath, texreg.c trBufferColorRGB) and the
original Relic packer reference/gardens-of-kadesh/zarchive/original-release-snapshot/tools/win32/Paco
(Mex.cpp meshFixupSingleUV: page UV = (uv * size + packed corner) / page size, transposed when rotated).
"""
import argparse
import collections
import datetime
import hashlib
import importlib.util
import json
import os
import re
import shutil
import sys
import time

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import hwformats as hf  # noqa: E402

REPO = os.path.abspath(os.path.join(os.path.dirname(os.path.abspath(__file__)), '..', '..'))
DEF_DATA = os.path.join(REPO, 'original', 'game_extracted', 'cataclysm')
DEF_UPDATE = os.path.join(REPO, 'original', 'game_extracted', 'update')
DEF_OUT = os.path.join(REPO, 'original', 'assets-export')
RETURN_DIR = 'original/assets-return'
RACES = ('race_sect', 'race_beast', 'race_kushan', 'race_taidan', 'race_p1', 'race_traders', 'race_derelict')
SPOT_SHIPS = ('race_sect/scarrier', 'race_beast/bacolyte', 'race_kushan/kcarrier', 'race_taidan/tcarrier',
              'race_derelict/dslipgatenode', 'race_traders/trfloatingcity', 'race_p1/p1mothership')
MASK_SUFFIX = {'team0': '_teambase', 'team1': '_teamstripe'}


def relp(path, start):
    return os.path.relpath(path, start).replace(os.sep, '/')


def fmt(x):
    s = '%.6f' % x
    s = s.rstrip('0').rstrip('.')
    return '0' if s in ('-0', '') else s


def sha1(b):
    return hashlib.sha1(b).hexdigest()


def write_file(path, data):
    os.makedirs(os.path.dirname(path), exist_ok=True)
    mode = 'wb' if isinstance(data, (bytes, bytearray)) else 'w'
    with open(path, mode) as fh:
        fh.write(data)


def to_obj_axes(p):
    """Game ship space -> OBJ. The game's ship frame is x = up, y = right, z = forward (HW1 rotinfo.coordsys:
    column 1 = up, column 3 = heading, e.g. Camera.c; the meshes agree: z is the long axis with the engine
    textures at -z, y is the mirror-symmetric axis). OBJ is Y-up with +Z forward, which Blender's default import
    (Forward -Z, Up Y) turns into Z up with the nose toward the front view (-Y). OBJ (x, y, z) = game (-y, x, z):
    a rotation (determinant +1), so the winding and the normals stay valid."""
    return (-p[1], p[0], p[2])


def transpose(buf, w, h, bpp):
    """Mirror an image across its main diagonal: input w x h, output h x w (Paco pacRotate)."""
    out = bytearray(len(buf))
    for y in range(h):
        row = buf[y * w * bpp:(y + 1) * w * bpp]
        for x in range(w):
            t = (x * h + y) * bpp
            out[t:t + bpp] = row[x * bpp:(x + 1) * bpp]
    return bytes(out)


def block(buf, pw, x, y, w, h, bpp):
    return b''.join(buf[((y + r) * pw + x) * bpp:((y + r) * pw + x + w) * bpp] for r in range(h))


# --------------------------------------------------------------------------------------------------------------
# exporter
# --------------------------------------------------------------------------------------------------------------

class Exporter(object):

    def __init__(self, tree, out, only=None):
        self.tree = tree
        self.out = out
        self.only = set(o.lower().strip('/') for o in only) if only else None
        self.tex_cache = {}          # lif rel path -> texture record
        self.ships = collections.OrderedDict()
        self.not_exported = []
        self.totals = collections.Counter()

    # ---------------------------------------------------------------- textures
    def texture(self, lif_rel, png_rel, role, problems):
        """Decode one .lif and write its PNG(s). png_rel is relative to --out, without extension."""
        if lif_rel in self.tex_cache:
            return self.tex_cache[lif_rel]
        path = self.tree.resolve(lif_rel)
        if path is None:
            rec = {'lif': lif_rel, 'error': 'texture file not found in the archive'}
            self.not_exported.append({'path': lif_rel, 'kind': 'texture', 'reason': rec['error']})
            self.tex_cache[lif_rel] = rec
            return rec
        try:
            lif = hf.lif_read(path)
        except hf.FormatError as e:
            rec = {'lif': lif_rel, 'error': 'cannot decode: %s' % e}
            self.not_exported.append({'path': lif_rel, 'kind': 'texture', 'reason': rec['error']})
            self.tex_cache[lif_rel] = rec
            return rec
        base_png = png_rel + '.png'
        write_file(os.path.join(self.out, base_png), hf.png_bytes(lif.width, lif.height, lif.rgba, 'RGBA'))
        rec = collections.OrderedDict()
        rec['png'] = base_png
        rec['lif'] = lif_rel
        rec['role'] = role
        rec['width'] = lif.width
        rec['height'] = lif.height
        rec['flags'] = '0x%x' % lif.flags
        rec['kind'] = 'paletted-8bit' if lif.kind == 'pal8' else 'rgba-32bit'
        rec['alpha'] = hf.has_alpha(lif.rgba)
        rec['team_base_png'] = None
        rec['team_stripe_png'] = None
        for attr, key in (('team0', 'team_base_png'), ('team1', 'team_stripe_png')):
            data = getattr(lif, attr)
            if data is None:
                continue
            mpng = png_rel + MASK_SUFFIX[attr] + '.png'
            if self.tree.resolve(lif_rel[:-4] + MASK_SUFFIX[attr] + '.lif'):
                problems.append('mask name %s collides with a real texture; mask not written' % mpng)
                continue
            write_file(os.path.join(self.out, mpng), hf.png_bytes(lif.width, lif.height, data, 'L'))
            rec[key] = mpng
            self.totals['png_masks'] += 1
        rec['pixels_sha1'] = sha1(lif.rgba)
        # pages are rebuilt from the source textures by the return tool, so they have no return path
        rec['return_png'] = None if role == 'page' else RETURN_DIR + '/' + base_png[len('textures/'):]
        self.tex_cache[lif_rel] = rec
        self.totals['png_base'] += 1
        self.totals['png_pages' if role == 'page' else 'png_source'] += 1
        return rec

    # ---------------------------------------------------------------- page placements
    def placements(self, src_mesh, game_mesh, page_names, packed_names, lod_dir_rel):
        """Where each packed source texture sits inside each page, from the polygons' UVs (Paco
        meshFixupSingleUV), confirmed against the pixels. Returns {page: [placement, ...]}, problems."""
        problems = []
        lifs = {}
        for n in list(page_names) + list(packed_names):
            p = self.tree.resolve(lod_dir_rel + '/' + n + '.lif')
            try:
                lifs[n] = hf.lif_read(p) if p else None
            except hf.FormatError:
                lifs[n] = None
        cand = {}                      # (page, src) -> set of (x, y, transposed) consistent with every polygon
        poly_total = collections.Counter()
        unmatched = 0
        same_shape = (len(src_mesh.objects) == len(game_mesh.objects) and
                      all(a.verts == b.verts for a, b in zip(src_mesh.objects, game_mesh.objects)))
        if same_shape:
            for so, go in zip(src_mesh.objects, game_mesh.objects):
                bytri = collections.defaultdict(list)
                for sp in so.polys:
                    bytri[sp[1:4]].append(sp)
                for gp in go.polys:
                    pt = (game_mesh.materials[gp[4]].texture or '').lower()
                    if pt not in page_names or lifs.get(pt) is None:
                        continue
                    P = lifs[pt]
                    hit = False
                    for sp in bytri.get(gp[1:4], []):
                        st = (src_mesh.materials[sp[4]].texture or '').lower()
                        S = lifs.get(st)
                        if st not in packed_names or S is None:
                            continue
                        found = set()
                        for tr in (False, True):
                            w, h = (S.height, S.width) if tr else (S.width, S.height)
                            xs, ys = [], []
                            for k in range(3):
                                s, t = sp[5][2 * k], sp[5][2 * k + 1]
                                s = s + 1.0 if s < 0 else s
                                t = t + 1.0 if t < 0 else t
                                a, b = (t, s) if tr else (s, t)
                                xs.append(gp[5][2 * k] * P.width - a * w)
                                ys.append(gp[5][2 * k + 1] * P.height - b * h)
                            x, y = round(xs[0]), round(ys[0])
                            if all(abs(v - x) < 0.02 for v in xs) and all(abs(v - y) < 0.02 for v in ys) \
                                    and 0 <= x and 0 <= y and x + w <= P.width and y + h <= P.height:
                                found.add((x, y, tr))
                        if found:
                            key = (pt, st)
                            cand[key] = cand[key] & found if key in cand else set(found)
                            poly_total[key] += 1
                            hit = True
                    if not hit:
                        unmatched += 1
        else:
            problems.append('source and in-game meshes differ in objects/vertices; placements from pixels only')
        if unmatched:
            problems.append('%d in-game polygons on a page could not be matched to a source texture by UV'
                            % unmatched)
        out = collections.OrderedDict((p, []) for p in sorted(page_names))
        placed = set()
        for (pt, st), opts in sorted(cand.items()):
            P, S = lifs[pt], lifs[st]
            best = None
            for (x, y, tr) in sorted(opts):
                src = transpose(S.rgba, S.width, S.height, 4) if tr else S.rgba
                w, h = (S.height, S.width) if tr else (S.width, S.height)
                exact = block(P.rgba, P.width, x, y, w, h, 4) == src
                if best is None or (exact and not best[3]):
                    best = (x, y, tr, exact)
            if best is None:
                problems.append('%s: UVs of %s on %s disagree between polygons' % (lod_dir_rel, st, pt))
                continue
            x, y, tr, exact = best
            w, h = (S.height, S.width) if tr else (S.width, S.height)
            out[pt].append(collections.OrderedDict([
                ('texture', st), ('x', x), ('y', y), ('w', w), ('h', h), ('transposed', tr),
                ('exact_copy', exact), ('found_by', 'uv'), ('polygons', poly_total[(pt, st)])]))
            placed.add(st)
            if not exact:
                problems.append('%s/%s: the copy of %s inside %s differs from the current %s.lif; '
                                'placement taken from the UVs' % (lod_dir_rel, pt, st, pt, st))
        # pixel search for packed textures the UV pass did not place (e.g. meshes that do not line up)
        for st in sorted(packed_names):
            if st in placed or lifs.get(st) is None:
                continue
            S = lifs[st]
            hit = None
            for pt in sorted(page_names):
                P = lifs.get(pt)
                if P is None:
                    continue
                for tr in (False, True):
                    src = transpose(S.rgba, S.width, S.height, 4) if tr else S.rgba
                    w, h = (S.height, S.width) if tr else (S.width, S.height)
                    row0 = src[:w * 4]
                    for y in range(P.height - h + 1):
                        line = P.rgba[y * P.width * 4:(y + 1) * P.width * 4]
                        k = line.find(row0)
                        while k >= 0 and hit is None:
                            if k % 4 == 0 and k // 4 + w <= P.width and \
                                    block(P.rgba, P.width, k // 4, y, w, h, 4) == src:
                                hit = (pt, k // 4, y, w, h, tr)
                            k = line.find(row0, k + 1)
                        if hit:
                            break
                    if hit:
                        break
                if hit:
                    break
            if hit:
                pt, x, y, w, h, tr = hit
                out[pt].append(collections.OrderedDict([
                    ('texture', st), ('x', x), ('y', y), ('w', w), ('h', h), ('transposed', tr),
                    ('exact_copy', True), ('found_by', 'pixels'), ('polygons', 0)]))
            else:
                problems.append('%s: packed texture %s not located in any page' % (lod_dir_rel, st))
        return out, problems

    # ---------------------------------------------------------------- OBJ / MTL
    def write_obj(self, mesh, obj_rel, tex_by_name, header):
        """Write OBJ + MTL. tex_by_name: lower texture name -> texture record. Returns stats."""
        obj_path = os.path.join(self.out, obj_rel)
        mtl_rel = obj_rel[:-4] + '.mtl'
        mtl_path = os.path.join(self.out, mtl_rel)
        mtl_dir = os.path.dirname(mtl_path)
        W = hf.world_matrices(mesh)
        # materials: merge identical entries (same texture, flags, colours) and give them readable names
        keys, names, mat_name = {}, {}, {}
        used = sorted(set(p[4] for ob in mesh.objects for p in ob.polys))
        for mi in used:
            m = mesh.materials[mi]
            key = ((m.texture or '').lower(), m.flags, m.ambient, m.diffuse, m.specular)
            if key not in keys:
                base = (m.texture or 'untextured_%06x' % (m.diffuse & 0xffffff)).lower()
                base = re.sub(r'[^A-Za-z0-9_.-]', '_', base)
                if m.flags & hf.MDF_SelfIllum:
                    base += '_glow'
                if m.flags & hf.MDF_2Sided:
                    base += '_2sided'
                if m.flags & hf.MDF_Smoothing:
                    base += '_smooth'
                name = base
                k = 2
                while name in names:
                    name = '%s_v%d' % (base, k)
                    k += 1
                keys[key] = name
                names[name] = m
            mat_name[mi] = keys[key]
        mtl = ['# Homeworld: Cataclysm materials, written by tools/assets/export_ships.py'] + \
              ['# ' + h for h in header] + \
              ['# Ka/Kd/Ks come from the mesh material; the game multiplies the lit material colour by the texture.',
               '# *_glow = self-illuminated in game (drawn unlit), *_2sided = no back-face culling,',
               '# *_smooth = smooth shading (the OBJ normals already carry this).', '']
        missing_tex = []
        for name in sorted(names):
            m = names[name]
            mtl.append('newmtl ' + name)
            mtl.append('Ka %s %s %s' % tuple(fmt(c) for c in hf.color_rgb(m.ambient)))
            mtl.append('Kd %s %s %s' % tuple(fmt(c) for c in hf.color_rgb(m.diffuse)))
            mtl.append('Ks %s %s %s' % tuple(fmt(c) for c in hf.color_rgb(m.specular)))
            mtl.append('d 1')
            mtl.append('illum 2')
            if m.texture:
                rec = tex_by_name.get(m.texture.lower())
                if rec and rec.get('png'):
                    p = relp(os.path.join(self.out, rec['png']), mtl_dir)
                    mtl.append('map_Kd ' + p)
                    if rec['alpha']:
                        mtl.append('map_d ' + p)
                    if m.flags & hf.MDF_SelfIllum:
                        mtl.append('Ke 1 1 1')
                        mtl.append('map_Ke ' + p)
                    if rec.get('team_base_png'):
                        mtl.append('# team base-colour mask: ' + relp(os.path.join(self.out, rec['team_base_png']),
                                                                       mtl_dir))
                    if rec.get('team_stripe_png'):
                        mtl.append('# team stripe-colour mask: ' +
                                   relp(os.path.join(self.out, rec['team_stripe_png']), mtl_dir))
                else:
                    mtl.append('# texture %s could not be exported (see manifest.json)' % m.texture)
                    missing_tex.append(m.texture)
            mtl.append('')
        lines = ['# Homeworld: Cataclysm mesh, written by tools/assets/export_ships.py'] + \
                ['# ' + h for h in header] + \
                ['# Vertices are in ship space with each object\'s hierarchy matrices applied (rest pose),',
                 '# axes turned for Blender: OBJ (x, y, z) = game (-y, x, z), i.e. game (x, y, z) = OBJ (y, -x, z).',
                 '# vt = (s, 1 - t): PNG row 0 is the first row of the .lif, which the game samples at t = 0.',
                 'mtllib ' + os.path.basename(mtl_path)]
        nv = nvt = nvn = nf = 0
        out_uv = 0
        smin = tmin = float('inf')
        smax = tmax = float('-inf')
        obj_names = set()
        for oi, ob in enumerate(mesh.objects):
            w = W[oi]
            oname = re.sub(r'\s+', '_', ob.name or 'object%d' % oi)
            if oname in obj_names:
                oname = '%s_%d' % (oname, oi)
            obj_names.add(oname)
            lines.append('o ' + oname)
            vb, nb = nv, nvn
            for (x, y, z, _ivn) in ob.verts:
                lines.append('v %s %s %s' % tuple(fmt(c) for c in to_obj_axes(hf.xform_point(w, x, y, z))))
            nv += len(ob.verts)
            for (x, y, z) in ob.normals:
                lines.append('vn %s %s %s' % tuple(fmt(c) for c in to_obj_axes(hf.xform_normal(w, x, y, z))))
            nvn += len(ob.normals)
            uvi = {}
            faces = []
            cur = None
            for p in ob.polys:
                i_fn, i0, i1, i2, i_mat, uv, _pf, _r0, _r1 = p
                m = mesh.materials[i_mat]
                if mat_name[i_mat] != cur:
                    cur = mat_name[i_mat]
                    faces.append('usemtl ' + cur)
                corners = []
                for k, vi in enumerate((i0, i1, i2)):
                    s, t = uv[2 * k], uv[2 * k + 1]
                    smin, smax, tmin, tmax = min(smin, s), max(smax, s), min(tmin, t), max(tmax, t)
                    if not (0.0 <= s <= 1.0 and 0.0 <= t <= 1.0):
                        out_uv += 1
                    key = (fmt(s), fmt(1.0 - t))
                    if key not in uvi:
                        uvi[key] = nvt + len(uvi) + 1
                    ni = ob.verts[vi][3] if m.flags & hf.MDF_Smoothing else i_fn
                    corners.append('%d/%d/%d' % (vb + vi + 1, uvi[key], nb + ni + 1))
                faces.append('f ' + ' '.join(corners))
            for (s, t) in sorted(uvi, key=uvi.get):
                lines.append('vt %s %s' % (s, t))
            nvt += len(uvi)
            lines.extend(faces)
            nf += len(ob.polys)
        write_file(obj_path, '\n'.join(lines) + '\n')
        write_file(mtl_path, '\n'.join(mtl) + '\n')
        self.totals['obj_files'] += 1
        self.totals['mtl_files'] += 1
        return collections.OrderedDict([
            ('obj', obj_rel), ('mtl', mtl_rel), ('objects', len(mesh.objects)), ('vertices', nv),
            ('uvs', nvt), ('normals', nvn), ('polygons', nf), ('materials_in_mesh', len(mesh.materials)),
            ('mtl_materials', len(names)),
            ('uv_range', {'s': [round(smin, 4), round(smax, 4)] if nf else None,
                          't': [round(tmin, 4), round(tmax, 4)] if nf else None}),
            ('uv_corners_outside_0_1', out_uv), ('missing_textures', missing_tex)])

    # ---------------------------------------------------------------- ships
    def lod_levels(self, race, ship, problems):
        """[(level, type, lod_field, base_rel)] from the .lod script, or from the rl0/lodN folders when absent."""
        lod_rel = '%s/%s.lod' % (race, ship)
        lod_path = self.tree.resolve(lod_rel)
        levels = []
        if lod_path:
            for lv, info in sorted(hf.lod_read(lod_path).items()):
                typ = info.get('type', '?')
                field = info.get('mesh')
                if typ != 'MESH' or not field:
                    levels.append((lv, typ, field, None))
                    continue
                rel = race + '/' + field.replace('\\', '/').lower()
                levels.append((lv, typ, field, os.path.splitext(rel)[0]))
            return lod_rel, levels
        problems.append('no %s.lod: LODs taken from the rl0/lodN folders' % ship)
        found = collections.OrderedDict()
        for key in self.tree.walk('%s/%s' % (race, ship)):
            m = re.match(r'^%s/%s/rl0/lod(\d+)/([^/]+)\.(geo|peo)$' % (re.escape(race), re.escape(ship)), key)
            if m:
                found.setdefault((int(m.group(1)), m.group(2)), None)
        for (lv, base) in sorted(found):
            levels.append((lv, 'MESH', None, '%s/%s/rl0/lod%d/%s' % (race, ship, lv, base)))
        return None, levels

    def export_ship(self, race, ship):
        problems = []
        rec = collections.OrderedDict()
        rec['race'] = race
        rec['ship'] = ship
        lod_rel, levels = self.lod_levels(race, ship, problems)
        rec['lod_script'] = lod_rel
        rec['lods'] = []
        rec['textures'] = collections.OrderedDict()
        rec['pages'] = collections.OrderedDict()
        used_meshes = set()
        first_level = {}                 # source mesh -> the level record that exported it
        bases_per_level = collections.Counter(lv for (lv, t, f, b) in levels if b)
        for (lv, typ, field, base) in levels:
            lrec = collections.OrderedDict([('level', lv), ('type', typ), ('lod_field', field)])
            if base is None:
                lrec['note'] = 'not a MESH level; nothing to export'
                rec['lods'].append(lrec)
                continue
            geo = self.tree.resolve(base + '.geo')
            peo = self.tree.resolve(base + '.peo')
            if not geo and not peo:
                lrec['note'] = 'mesh file missing (HW1 lodMeshFileLoad falls back to defaultmesh)'
                self.not_exported.append({'path': base + '.geo/.peo', 'kind': 'mesh',
                                          'reason': 'named by %s level %d but not in the archive' % (lod_rel, lv)})
                rec['lods'].append(lrec)
                continue
            src_rel = base + ('.geo' if geo else '.peo')
            game_rel = base + ('.peo' if peo else '.geo')   # meshLoad prefers the paged .peo (mainAllowPacking 1)
            used_meshes.update([base + '.geo', base + '.peo'])
            lrec['source_mesh'] = src_rel
            lrec['ingame_mesh'] = game_rel
            if src_rel in first_level:
                # the .lod names the same mesh for several levels: export it once and point at it
                prev = first_level[src_rel]
                lrec['same_mesh_as_level'] = prev['level']
                for k in ('mesh_ident', 'mesh_version', 'mesh_layout', 'obj', 'mtl', 'objects', 'vertices', 'uvs',
                          'normals', 'polygons', 'textures', 'ingame'):
                    if k in prev:
                        lrec[k] = prev[k]
                self.totals['lod_levels_reusing_a_mesh'] += 1
                rec['lods'].append(lrec)
                continue
            try:
                src_mesh = hf.mesh_read(self.tree.resolve(src_rel))
                game_mesh = src_mesh if game_rel == src_rel else hf.mesh_read(self.tree.resolve(game_rel))
            except hf.FormatError as e:
                lrec['note'] = 'cannot decode: %s' % e
                self.not_exported.append({'path': src_rel, 'kind': 'mesh', 'reason': str(e)})
                rec['lods'].append(lrec)
                continue
            lrec['mesh_ident'] = src_mesh.ident
            lrec['mesh_version'] = '0x%x' % src_mesh.version
            lrec['mesh_layout'] = src_mesh.layout
            lod_dir = os.path.dirname(base)
            sub = lod_dir[len(race) + 1 + len(ship) + 1:] if lod_dir != '%s/%s' % (race, ship) else 'root'
            sub = sub[4:] if sub.startswith('rl0/') else sub.replace('/', '_')
            tex_root = 'textures/%s/%s/%s' % (race, ship, sub)
            stem = ship if os.path.basename(base) == ship or bases_per_level[lv] == 1 else \
                '%s_%s' % (ship, os.path.basename(base))
            src_names = sorted(set(m.texture.lower() for m in src_mesh.materials if m.texture))
            game_names = sorted(set(m.texture.lower() for m in game_mesh.materials if m.texture))
            page_names = [n for n in game_names if n not in src_names]
            packed_names = [n for n in src_names if n not in game_names]
            tex_by_name = {}
            for n in src_names:
                t = self.texture(lod_dir + '/' + n + '.lif', '%s/%s' % (tex_root, n), 'source', problems)
                tex_by_name[n] = t
                if t.get('png'):
                    rec['textures'].setdefault('%s/%s' % (sub, n), t)
            game_tex = dict(tex_by_name)
            for n in page_names:
                t = self.texture(lod_dir + '/' + n + '.lif', '%s/pages/%s' % (tex_root, n), 'page', problems)
                game_tex[n] = t
            header = ['ship %s/%s, LOD %d' % (race, ship, lv)]
            obj_rel = 'ships/%s/%s/%s_lod%d.obj' % (race, ship, stem, lv)
            st = self.write_obj(src_mesh, obj_rel, tex_by_name, header + ['source mesh ' + src_rel])
            lrec.update(st)
            lrec['textures'] = [tex_by_name[n]['png'] for n in src_names if tex_by_name[n].get('png')]
            if game_rel != src_rel:
                ig_rel = 'ships/%s/%s/ingame/%s_lod%d.obj' % (race, ship, stem, lv)
                st2 = self.write_obj(game_mesh, ig_rel, game_tex,
                                     header + ['in-game (paged) mesh ' + game_rel])
                lrec['ingame'] = collections.OrderedDict(
                    (k, st2[k]) for k in ('obj', 'mtl', 'vertices', 'uvs', 'polygons', 'mtl_materials',
                                          'uv_range', 'uv_corners_outside_0_1', 'missing_textures'))
                lrec['ingame']['textures'] = [game_tex[n]['png'] for n in game_names if game_tex[n].get('png')]
                self.totals['ingame_obj_files'] += 1
                if page_names:
                    pl, pprob = self.placements(src_mesh, game_mesh, set(page_names), set(packed_names), lod_dir)
                    problems.extend(pprob)
                    for pg, items in pl.items():
                        trec = game_tex[pg]
                        if not trec.get('png'):
                            continue
                        prec = collections.OrderedDict(trec)
                        for it in items:
                            it['texture_png'] = tex_by_name.get(it['texture'], {}).get('png')
                        prec['placements'] = items
                        rec['pages']['%s/%s' % (sub, pg)] = prec
                        self.totals['page_placements'] += len(items)
                        self.totals['page_placements_inexact'] += sum(1 for i in items if not i['exact_copy'])
            first_level[src_rel] = lrec
            self.totals['lod_meshes'] += 1
            self.totals['polygons'] += lrec['polygons']
            rec['lods'].append(lrec)
        # files under the ship folder that no exported LOD used
        rec['stray_meshes'] = []
        rec['unreferenced_lifs'] = []
        used_lifs = set(t['lif'] for t in self.tex_cache.values() if t.get('lif', '').startswith(
            '%s/%s/' % (race, ship)))
        for key in self.tree.walk('%s/%s' % (race, ship)):
            if key.endswith(('.geo', '.peo')) and key not in used_meshes:
                rec['stray_meshes'].append(key)
                self.not_exported.append({'path': key, 'kind': 'mesh',
                                          'reason': 'not named by the ship\'s .lod (stray file); skipped'})
            elif key.endswith('.lif') and key not in used_lifs:
                rec['unreferenced_lifs'].append(key)
        self.totals['unreferenced_lifs'] += len(rec['unreferenced_lifs'])
        rec['problems'] = problems
        self.ships['%s/%s' % (race, ship)] = rec
        self.totals['ships'] += 1

    def run(self):
        for race in RACES:
            if not os.path.isdir(os.path.join(self.tree.data_root, race)):
                self.not_exported.append({'path': race, 'kind': 'race', 'reason': 'race folder not found'})
                continue
            for ship in sorted(os.listdir(os.path.join(self.tree.data_root, race))):
                if not os.path.isdir(os.path.join(self.tree.data_root, race, ship)):
                    continue
                if self.only and '%s/%s' % (race, ship) not in self.only and race not in self.only:
                    continue
                t0 = time.time()
                self.export_ship(race, ship)
                r = self.ships['%s/%s' % (race, ship)]
                print('%-38s lods=%d polys(lod0)=%-5s textures=%-3d pages=%-2d %.1fs%s' % (
                    race + '/' + ship, len(r['lods']), r['lods'][0].get('polygons', '-') if r['lods'] else '-',
                    len(r['textures']), len(r['pages']), time.time() - t0,
                    ('  [%d note(s)]' % len(r['problems'])) if r['problems'] else ''))


# --------------------------------------------------------------------------------------------------------------
# manifest / index / readme
# --------------------------------------------------------------------------------------------------------------

def dir_size(path):
    total = files = 0
    for dp, _dn, fn in os.walk(path):
        for f in fn:
            total += os.path.getsize(os.path.join(dp, f))
            files += 1
    return total, files


def write_index(out, manifest):
    rows = ['# Homeworld: Cataclysm ship export: index', '',
            'Generated %s by `tools/assets/export_ships.py`. One row per ship folder. "LOD 0 polys" is the close-up '
            'mesh. "Textures" counts the PNGs to upscale for that ship (all LODs, not counting team masks or pages); '
            '"team" counts how many of them carry team-colour masks. See README.md for how to use these files.'
            % manifest['generated'], '',
            '| race | ship | LOD meshes | LOD 0 polys | textures | team | LOD 0 OBJ | notes |',
            '|---|---|---|---|---|---|---|---|']
    for key, s in manifest['ships'].items():
        lods = [l for l in s['lods'] if l.get('obj') and 'same_mesh_as_level' not in l]
        lod0 = lods[0] if lods else {}
        team = sum(1 for t in s['textures'].values() if t.get('team_base_png') or t.get('team_stripe_png'))
        notes = []
        if not s['lod_script']:
            notes.append('no .lod')
        reuse = sum(1 for l in s['lods'] if 'same_mesh_as_level' in l)
        if reuse:
            notes.append('%d LOD level(s) reuse a lower level\'s mesh' % reuse)
        missing = sum(1 for l in s['lods'] if l.get('note', '').startswith('mesh file missing'))
        if missing:
            notes.append('%d LOD level(s) name a missing mesh' % missing)
        if s['stray_meshes']:
            notes.append('%d stray mesh file(s) skipped' % len(s['stray_meshes']))
        inexact = sum(1 for p in s['pages'].values() for i in p['placements'] if not i['exact_copy'])
        if inexact:
            notes.append('%d page copy differs from its source' % inexact)
        rows.append('| %s | %s | %d | %s | %d | %d | %s | %s |' % (
            s['race'], s['ship'], len(lods), lod0.get('polygons', '-'), len(s['textures']), team,
            ('[%s](%s)' % (os.path.basename(lod0['obj']), lod0['obj'])) if lod0 else '-', '; '.join(notes)))
    write_file(os.path.join(out, 'index.md'), '\n'.join(rows) + '\n')


README = r"""# Homeworld: Cataclysm ship models and textures for Blender

This folder holds every ship of Homeworld: Cataclysm as 3D models (OBJ files) with their textures (PNG files).
It was made by `tools/assets/export_ships.py` from the game's own data. It is copyrighted game data:
keep it on this machine, do not share it, and do not put it in git.

Contents at a glance (exact numbers are in `manifest.json` under `totals`):

| folder or file | what it is |
|---|---|
| `ships/<race>/<ship>/<ship>_lod0.obj` | the ship model, close-up detail. Open this one in Blender |
| `ships/<race>/<ship>/<ship>_lod1.obj` ... `_lod4.obj` | the same ship at lower detail, used far from the camera |
| `ships/<race>/<ship>/ingame/` | the models exactly as the game loads them (see "Texture pages" below). Optional |
| `textures/<race>/<ship>/lod0/*.png` | the textures of the LOD 0 model: **these are the files to upscale** |
| `textures/<race>/<ship>/lod1/` ... | the textures of the lower-detail models (smaller versions) |
| `textures/<race>/<ship>/lodN/pages/` | texture pages: copies of small textures packed together. Do not edit |
| `index.md` | a table of every ship with links |
| `manifest.json` | the full machine-readable list: every file, size and where it came from |

Race folders: `race_sect` (the Somtaaw), `race_beast` (the Beast), `race_kushan`, `race_taidan` (Taiidan), `race_p1`,
`race_traders` (traders, junkyards, stations) and `race_derelict` (derelicts, slipgates, planets, props).

## 1. Opening a ship in Blender

1. Start Blender and delete the default cube (select it, press `X`, choose Delete).
2. Choose **File > Import > Wavefront (.obj)**.
3. Go to `ships/<race>/<ship>/` and pick `<ship>_lod0.obj`, for example `ships/race_sect/scarrier/scarrier_lod0.obj`.
   Keep the default import settings. The `.mtl` file next to it is read automatically and loads the textures.
4. Hover over the 3D view and press `Numpad .` (View > Frame Selected) to zoom to the ship.
5. To see the textures, switch the viewport to **Material Preview** (press `Z`, then choose Material Preview,
   or click the third sphere icon at the top right of the 3D view).

Tips:
* Ships come in level, with the nose toward Blender's front view (`Numpad 1`) and the top up.
* Ships are in the game's own units, and big ships are large. If parts vanish while you zoom, raise
  **View > Clip End** in the side panel (press `N`, View tab), for example to 100000.
* Ships with moving parts (doors, gun turrets, gimbals) have each part as its own object, already placed where the
  game puts it at rest.
* Faces are split by material, and each material is named after its texture, so in the **Shading** workspace you
  can click a part and see which PNG it uses.

## 2. Which PNG is which

Every texture comes as one main PNG, plus up to two team-colour masks:

| file | what it is | upscale it? |
|---|---|---|
| `<name>.png` | **the base texture** (the colour you see). RGBA. A few textures (marked `"alpha": true` in `manifest.json`) use the alpha channel for transparency; all the others are fully opaque | yes |
| `<name>_teambase.png` | **team base-colour mask**, greyscale. White = this pixel is repainted in the player's chosen *base* colour, black = it keeps the texture colour, grey = a blend | yes, same factor |
| `<name>_teamstripe.png` | **team stripe-colour mask**, greyscale. The same for the player's *stripe* (trim) colour | yes, same factor |

How the game uses the masks (Relic's `trBufferColorRGB`): it blends the base texture toward the player's base
colour by the base mask, then toward the stripe colour by the stripe mask. So the base PNG shows the ship
*before* team colour. Only textures whose `.lif` has team colour get mask files.

Most original textures are small (16 to 128 pixels a side) and were stored with a 256-colour palette. The PNGs
are already converted to full colour, so you can paint or upscale them freely.

## 3. Rules for upscaling

1. **Keep the proportions.** Scale width and height by the same whole factor; 2x or 4x are best, so 64x32
   becomes 256x128 at 4x. The models' UV coordinates are fractions of the image, so a texture scaled evenly still
   lines up. Never crop, pad, rotate or flip a texture. Use one factor for all textures of a ship, so that its
   parts keep the same sharpness next to each other.
2. **Keep the file name and the folder.** `textures/race_sect/scarrier/lod0/0sca12.png` must come back as
   `0sca12.png` for `race_sect/scarrier/lod0`. Different LOD folders can hold different images with the same name,
   which is why the `lodN` folder is part of the address.
3. **Upscale the masks with the same factor** as their base texture, so that they still line up. Smooth
   (bilinear or bicubic) scaling is fine for masks; keep them greyscale.
4. **Keep the alpha channel** of textures that have one (it cuts out shapes such as engine glows and lights).
5. **Do not edit `pages/`.** Those are copies of the small textures packed into one image the way the game
   loads them. The return tool rebuilds them from your upscaled source textures, using the positions recorded
   in `manifest.json`.
6. **Start with `lod0`.** LOD 0 is what you see up close. LOD 1-4 textures are only seen from far away; you can
   skip them, or upscale them too if you want everything consistent.
7. Some textures are reused by several ships, and some are identical across LOD folders. Upscale each file where
   it is; `manifest.json` records a pixel checksum (`pixels_sha1`) for every texture so duplicates can be found.

## 4. Handing the textures back

Put each revised PNG under `original/assets-return/` (next to this folder), **at the same relative path it has under
`textures/`**:

```
original/assets-export/textures/race_sect/scarrier/lod0/0sca12.png            (exported here)
original/assets-return/race_sect/scarrier/lod0/0sca12.png                      (your upscaled version)
original/assets-return/race_sect/scarrier/lod0/0sca12_teambase.png             (its upscaled mask, if any)
```

Every texture's return path is also listed in `manifest.json` as `return_png`. You only need to return the files
you changed; the return tool keeps the original for anything you leave out. That later tool will:

* convert each returned PNG back into a `.lif` (with the team-colour masks), rebuild the texture pages it belongs
  to, and place the result as loose files that override the game archive for the GOG game;
* load the PNGs directly in the recreation.

## 5. Texture pages (why there is an `ingame` folder)

Relic's packer ("Paco") copied many small textures of each LOD into a few larger images called pages
(`page0.png`, `page1.png`, ...) and wrote a second copy of each model, the `.peo` file, whose UVs point into those
pages. The game always loads the paged model. The models in `ships/.../ingame/` are those paged models; they look
the same as the main ones but use the page images. They are there so you can check what the game actually draws.
The main models use the original, separate textures, which are easier to upscale. Each page's contents (which
texture sits where, and whether it was stored turned sideways) are recorded in `manifest.json` under `pages`.

## 6. Technical notes

* OBJ UVs are written as `vt s (1 - t)`: the first pixel row of each PNG is the first row of the original `.lif`.
* Some UVs go past 0..1: those faces repeat (tile) the texture, which the game allows. Blender repeats too.
* Normals come from the game files; smooth materials use per-vertex normals, the others use face normals.
* Axes: the game's ship frame is x = up, y = right, z = forward. The OBJ files are turned for Blender:
  OBJ (x, y, z) = game (-y, x, z). Positions are otherwise unchanged (same units, same origin).
* Blender's importer drops a triangle that repeats the three corners of another one. A few models have such
  pairs (double-sided panels), so Blender can show a handful fewer faces than the OBJ holds. In most pairs both
  faces use the same texture and UVs. The OBJ files keep every face, and every texture is exported either way.
* Materials named `*_glow` are self-illuminated in the game (lit windows, engine glows) and get an emission map.
"""


def main(argv):
    ap = argparse.ArgumentParser(description=__doc__.split('\n')[0])
    ap.add_argument('--data', default=DEF_DATA)
    ap.add_argument('--update', default=DEF_UPDATE)
    ap.add_argument('--out', default=DEF_OUT)
    ap.add_argument('--only', nargs='*', help='race or race/ship folders to export (default: all)')
    ap.add_argument('--no-clean', action='store_true', help='do not delete --out before exporting')
    ap.add_argument('--validate-only', action='store_true')
    ap.add_argument('--no-validate', action='store_true')
    args = ap.parse_args(argv)
    out = os.path.abspath(args.out)
    if not args.validate_only:
        if os.path.exists(out) and not args.no_clean:
            if not os.path.exists(os.path.join(out, 'manifest.json')) and os.listdir(out):
                print('refusing to delete %s: it exists and has no manifest.json from this tool' % out)
                return 2
            shutil.rmtree(out)
        os.makedirs(out, exist_ok=True)
        t0 = time.time()
        tree = hf.DataTree(os.path.abspath(args.data), os.path.abspath(args.update))
        ex = Exporter(tree, out, args.only)
        ex.run()
        size, nfiles = dir_size(out)
        totals = collections.OrderedDict(sorted(ex.totals.items()))
        totals['update_files_overriding_archive'] = tree.overrides
        totals['exported_files_read_from_update'] = len(tree.used_from_update())
        totals['not_exported'] = len(ex.not_exported)
        manifest = collections.OrderedDict()
        manifest['tool'] = 'tools/assets/export_ships.py'
        manifest['generated'] = datetime.datetime.now().strftime('%Y-%m-%d %H:%M')
        manifest['source'] = {'data': relp(os.path.abspath(args.data), REPO),
                              'update': relp(os.path.abspath(args.update), REPO)}
        manifest['conventions'] = collections.OrderedDict([
            ('uv', 'OBJ vt = (s, 1 - t). PNG row 0 = .lif row 0, sampled by the game at t = 0.'),
            ('geometry', 'ship-space coordinates with every object\'s localMatrix chain applied (rest pose); '
                         'winding counter-clockwise, as the stored face normals'),
            ('axes', 'OBJ (x, y, z) = game (-y, x, z); game ship frame x = up, y = right, z = forward. '
                     'Inverse: game (x, y, z) = OBJ (y, -x, z). Blender default import: ship level, nose to -Y'),
            ('normals', 'materials with MDF_Smoothing (0x2) use the per-vertex normals, the others the face '
                        'normal'),
            ('texture_png', 'RGBA 8-bit; paletted .lif expanded through its palette (all used entries opaque)'),
            ('team_masks', '<name>_teambase.png = TeamColor0 weight, <name>_teamstripe.png = TeamColor1 weight, '
                           '0..255 per pixel; paletted textures map the 256-entry teamEffect tables per index'),
            ('pages', 'placement x, y, w, h in page pixels (row 0 = top of the PNG); transposed = the source is '
                      'mirrored across its diagonal (Paco pacRotate), so w = source height, h = source width'),
            ('return', 'revised PNGs go to %s/<race>/<ship>/<lod>/<name>.png (= the path under textures/)'
                       % RETURN_DIR)])
        totals['seconds'] = round(time.time() - t0, 1)
        totals['output_files'] = nfiles + 4
        manifest['totals'] = totals
        manifest['not_exported'] = ex.not_exported
        manifest['ships'] = ex.ships
        write_file(os.path.join(out, 'README.md'), README)
        write_index(out, manifest)
        write_file(os.path.join(out, 'manifest.json'), json.dumps(manifest, indent=1) + '\n')
        size, nfiles = dir_size(out)
        manifest['totals']['output_bytes_before_validation'] = size
        write_file(os.path.join(out, 'manifest.json'), json.dumps(manifest, indent=1) + '\n')
        print('exported %d ships, %d LOD meshes, %d OBJ (+%d in-game), %d base PNGs + %d masks, %.1f MB in %.0fs'
              % (totals.get('ships', 0), totals.get('lod_meshes', 0), totals.get('obj_files', 0) -
                 totals.get('ingame_obj_files', 0), totals.get('ingame_obj_files', 0), totals.get('png_base', 0),
                 totals.get('png_masks', 0), size / 1e6, time.time() - t0))
    if args.no_validate:
        return 0
    import validate_export
    return validate_export.validate(out, os.path.abspath(args.data), os.path.abspath(args.update))


if __name__ == '__main__':
    sys.exit(main(sys.argv[1:]))
