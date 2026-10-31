#!/usr/bin/env python3
"""Fill the game's Assets folder (HWI-FEATURE(assets-folder), docs/features/assets-folder.md).

Usage:
  tools/assets/export_assets.py [--out DIR] [--data DIR --update DIR | --game DIR] [--no-models] [--force]
  tools/assets/export_assets.py --status  [--out DIR]     what changed since the export, and what it replaces
  tools/assets/export_assets.py --refresh [--out DIR]     re-stamp files whose content is still as exported
                                                          (after a copy that lost the file times)

Default --out: original/play-home/.homeworld/Assets (the settings folder of tools/play.sh). The game's own Assets
folder is <settings folder>/Assets: ~/.homeworld/Assets on Linux, %APPDATA%\\Homeworld-Infection\\Assets on Windows,
~/Library/Application Support/Homeworld-Infection/Assets on macOS.

Default source: the extracted archives original/game_extracted/{cataclysm,update} (update.big first, as the game
reads them); --game DIR reads cataclysm.big / update.big of a game folder instead (unpacked into a temporary folder,
about a minute). Python 3 standard library only.

What it writes (every path lower case):
  <archive path>.png                    every .lif texture of the archives as RGBA PNG, at the path the game uses
  <archive path>_teambase.png           TeamColor0 weight (greyscale), for a .lif with that flag
  <archive path>_teamstripe.png         TeamColor1 weight
  <archive path>.jpg / .tga             the archives' other pictures, copied as they are
  Models/<race>/<ship>/<ship>_lodN.obj|.mtl          each ship's source mesh for Blender (tools/assets/export_ships.py),
                                                     textured with the PNGs above
  Models/<race>/<ship>/ingame/<ship>_lodN.obj|.mtl   the paged mesh the game draws, with its atlas pages
  assets-pages.txt                      where each source texture sits in its atlas page (Paco's placements)
  README.txt                            for the user
  assets-manifest.txt                   "<size> <mtime> <sha256> <path>" of every file written: the game uses a file
                                        only when it is not listed or its size or time differs

A file in the folder that differs from the previous export's manifest (the user's work) is never overwritten; a file
that the previous export did not write is kept too (--force overwrites both). The tool never writes into the game's
folders (original/gog, original/game, original/game_extracted, the harness data, any folder holding cataclysm.big).
"""
import argparse
import collections
import hashlib
import os
import shutil
import struct
import sys
import tempfile
import time

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
sys.path.insert(0, os.path.dirname(HERE))
import hwformats as hf  # noqa: E402
import export_ships as es  # noqa: E402

REPO = os.path.abspath(os.path.join(HERE, '..', '..'))
# a worktree (.claude/worktrees/<name>) has no original/: use the main checkout's, as tools/play.sh does
MAIN = REPO if os.path.isdir(os.path.join(REPO, 'original', 'game')) else \
    os.path.abspath(os.path.join(REPO, '..', '..', '..'))
DEF_OUT = os.path.join(MAIN, 'original', 'play-home', '.homeworld', 'Assets')
DEF_DATA = os.path.join(MAIN, 'original', 'game_extracted', 'cataclysm')
DEF_UPDATE = os.path.join(MAIN, 'original', 'game_extracted', 'update')
MANIFEST = 'assets-manifest.txt'
PAGES = 'assets-pages.txt'
README = 'README.txt'
MASK_SUFFIX = {'team0': '_teambase', 'team1': '_teamstripe'}
NATIVE_IMAGES = ('.jpg', '.tga')


def die(msg):
    sys.stderr.write('export_assets: %s\n' % msg)
    sys.exit(2)


def sha256_file(path):
    h = hashlib.sha256()
    with open(path, 'rb') as fh:
        for block in iter(lambda: fh.read(1 << 20), b''):
            h.update(block)
    return h.hexdigest()


def stat_pair(path):
    st = os.stat(path)
    return st.st_size, st.st_mtime_ns // 1000000000      # the engine compares whole seconds


def read_manifest(out):
    """{key (lower case path): (size, mtime, sha256, path as written)}"""
    entries = collections.OrderedDict()
    path = os.path.join(out, MANIFEST)
    if not os.path.isfile(path):
        return None
    with open(path, encoding='utf-8') as fh:
        for line in fh:
            line = line.rstrip('\r\n')
            if not line or line.startswith('#'):
                continue
            parts = line.split(' ', 3)
            if len(parts) != 4:
                continue
            entries[parts[3].lower()] = (int(parts[0]), int(parts[1]), parts[2], parts[3])
    return entries


def write_manifest(out, entries, note):
    lines = ['# Homeworld-Infection Assets folder manifest (tools/assets/export_assets.py, %s)' % note,
             '# The game uses a file of this folder only when this list does not name it or names another size or',
             '# time: so an untouched export changes nothing. Do not edit; delete it and the game uses every file',
             '# that names a game file.',
             '# <size> <mtime (seconds)> <sha256> <path>']
    for key in sorted(entries):
        size, mtime, sha, rel = entries[key]
        lines.append('%d %d %s %s' % (size, mtime, sha, rel))
    tmp = os.path.join(out, MANIFEST + '.tmp')
    with open(tmp, 'w', encoding='utf-8', newline='\n') as fh:
        fh.write('\n'.join(lines) + '\n')
    os.replace(tmp, os.path.join(out, MANIFEST))


def forbidden_roots():
    roots = [os.path.join(MAIN, 'original', d) for d in ('gog', 'game', 'game_extracted')]
    roots.append(os.path.join(MAIN, 'original', 'scratch-m2', 'm2r2-harness', 'data'))
    return [os.path.realpath(r) for r in roots]


def check_out(out, sources):
    real = os.path.realpath(out)
    home = os.path.realpath(os.path.expanduser('~'))
    if real in ('/', home) or len(real) < 4:
        die('refusing to write into %s' % real)
    for root in forbidden_roots() + [os.path.realpath(s) for s in sources if s]:
        if real == root or real.startswith(root + os.sep) or root.startswith(real + os.sep):
            die('refusing to write into or above the game data %s (--out %s)' % (root, real))
    probe = real
    while True:
        if os.path.isdir(probe) and any(n.lower() == 'cataclysm.big' for n in os.listdir(probe)):
            die('refusing to write into a game folder (%s holds cataclysm.big)' % probe)
        parent = os.path.dirname(probe)
        if parent == probe:
            break
        probe = parent


class Writer(object):
    """Writes into the Assets folder without overwriting the user's work."""

    def __init__(self, out, old, force):
        self.out = out
        self.old = old or {}
        self.had_manifest = old is not None
        self.force = force
        self.entries = collections.OrderedDict()
        self.kept = []
        self.kept_unlisted = []
        self.counts = collections.Counter()
        self.bytes = 0

    def write(self, rel, data):
        rel = rel.replace('\\', '/')
        key = rel.lower()
        path = os.path.join(self.out, rel)
        if isinstance(data, str):
            data = data.encode('utf-8')
        sha = hashlib.sha256(data).hexdigest()
        if os.path.isfile(path) and not self.force:
            prev = self.old.get(key)
            if prev is None:
                if self.had_manifest or os.path.getsize(path) != len(data) or sha256_file(path) != sha:
                    self.kept_unlisted.append(rel)       # not ours: the user's file (it stays in use)
                    return
            else:
                current = sha256_file(path)
                if current != prev[2]:
                    self.kept.append(rel)                 # changed since the last export: the user's work
                    self.entries[key] = prev              # still differs from the manifest, so it stays in use
                    return
                if current == sha:
                    self.entries[key] = (os.path.getsize(path),) + (stat_pair(path)[1], sha, rel)
                    self.counts['unchanged'] += 1
                    self.bytes += len(data)
                    return
        os.makedirs(os.path.dirname(path), exist_ok=True)
        with open(path, 'wb') as fh:
            fh.write(data)
        size, mtime = stat_pair(path)
        self.entries[key] = (size, mtime, sha, rel)
        self.counts['written'] += 1
        self.bytes += len(data)

    def write_abs(self, path, data):
        """export_ships.write_file replacement for the model pass."""
        self.write(os.path.relpath(path, self.out), data)


# ------------------------------------------------------------------------------------------------------------
# textures
# ------------------------------------------------------------------------------------------------------------

def read_sharing(tree):
    """textures.ll: {texture .lif key: share parent .lif key}. The texture registry draws the share parent's file for
    such a texture (texreg.c trRegistryRefresh: the parent's texture, or a shared file name; llelement, texreg.h)."""
    path = tree.resolve('textures.ll')
    if path is None:
        return {}
    with open(path, 'rb') as fh:
        d = fh.read()
    ident, _ver, n, _slen, _shlen, _tot = struct.unpack_from('<8siiiii', d, 0)
    if not ident.startswith(b'Event13'):
        return {}
    els = [struct.unpack_from('<IiiIIiIi', d, 28 + i * 32) for i in range(n)]
    sb = 28 + n * 32
    names = [d[sb + e[0]:d.index(b'\0', sb + e[0])].decode('latin-1').replace('\\', '/').lower() for e in els]
    return {names[i] + '.lif': names[e[7]] + '.lif' for i, e in enumerate(els) if 0 <= e[7] < n}


def export_textures(tree, writer, report, page_sources):
    """Every .lif as PNG + masks. A texture textures.ll lists as a copy of another is drawn from that one's file, so
    its PNG holds those pixels (not for a page source: Paco copied the source's own file into the page)."""
    info = {}
    sharing = read_sharing(tree)
    keys = sorted(k for k in tree.index if k.endswith('.lif'))
    for key in keys:
        src = key
        parent = sharing.get(key)
        if parent is not None and parent in tree.index and key not in page_sources:
            src = parent
        try:
            lif = hf.lif_read(tree.index[src])
            if src != key:
                own = hf.lif_read(tree.index[key])
                if (own.width, own.height) != (lif.width, lif.height):
                    report['share_size_mismatch'].append(key)
                    lif = own
                    src = key
        except hf.FormatError as e:
            report['texture_errors'].append('%s: %s' % (key, e))
            continue
        if src != key:
            report['from_share_parent'] += 1
        stem = key[:-4]
        writer.write(stem + '.png', hf.png_bytes(lif.width, lif.height, lif.rgba, 'RGBA'))
        masks = {}
        for attr in ('team0', 'team1'):
            data = getattr(lif, attr)
            if data is None:
                continue
            mask = stem + MASK_SUFFIX[attr]
            if mask + '.lif' in tree.index:
                report['mask_collisions'].append(mask)
                continue
            writer.write(mask + '.png', hf.png_bytes(lif.width, lif.height, data, 'L'))
            masks[attr] = mask + '.png'
            report['masks'] += 1
        info[key] = {'alpha': hf.has_alpha(lif.rgba), 'masks': masks, 'paletted': lif.kind == 'pal8',
                     'width': lif.width, 'height': lif.height}
        report['textures'] += 1
        report['paletted' if lif.kind == 'pal8' else 'rgba'] += 1
        if lif.width & (lif.width - 1) or lif.height & (lif.height - 1):
            report['non_power_of_two'] += 1
    return info


def export_images(tree, writer, report):
    for key in sorted(k for k in tree.index if k.endswith(NATIVE_IMAGES)):
        with open(tree.index[key], 'rb') as fh:
            writer.write(key, fh.read())
        report['images'] += 1


# ------------------------------------------------------------------------------------------------------------
# atlas pages (Paco): placements solved by export_ships.Exporter.placements for every .peo with a .geo partner
# ------------------------------------------------------------------------------------------------------------

def solve_pages(tree, writer, report):
    """{page .lif key: {(source key, x, y, w, h, transposed)}} for every .peo with a .geo partner."""
    ex = es.Exporter(tree, writer.out)
    pages = collections.OrderedDict()                     # page lif key -> {(src, x, y, w, h, t)}
    for key in sorted(k for k in tree.index if k.endswith('.peo')):
        base = key[:-4]
        geo = tree.resolve(base + '.geo')
        if geo is None:
            continue
        try:
            src_mesh = hf.mesh_read(geo)
            game_mesh = hf.mesh_read(tree.index[key])
        except hf.FormatError as e:
            report['page_problems'].append('%s: %s' % (key, e))
            continue
        lod_dir = os.path.dirname(base)
        src_names = set(m.texture.lower() for m in src_mesh.materials if m.texture)
        game_names = set(m.texture.lower() for m in game_mesh.materials if m.texture)
        page_names = set(n for n in game_names - src_names if lod_dir + '/' + n + '.lif' in tree.index)
        packed = src_names - game_names
        if not page_names:
            continue
        placed, problems = ex.placements(src_mesh, game_mesh, page_names, packed, lod_dir)
        report['page_problems'].extend('%s: %s' % (key, p) for p in problems)
        for page, items in placed.items():
            slot = pages.setdefault(lod_dir + '/' + page, set())
            for it in items:
                slot.add((lod_dir + '/' + it['texture'], it['x'], it['y'], it['w'], it['h'], int(it['transposed'])))
                report['placements_inexact'] += 0 if it['exact_copy'] else 1
    return pages


def write_pages(writer, pages, info, report):
    lines = ['# Homeworld-Infection atlas pages (tools/assets/export_assets.py): where each source texture sits in',
             '# the page the game draws (Relic\'s packer "Paco"). The game rebuilds a page when one of its source',
             '# textures changed. Do not edit.',
             '# page <page texture> <width> <height>',
             '# src <source texture> <x> <y> <w> <h> <transposed>   (page pixels; w x h as placed)']
    for page in sorted(pages):
        pinfo = info.get(page + '.lif')
        if pinfo is None or not pages[page]:
            continue
        lines.append('page %s %d %d' % (page, pinfo['width'], pinfo['height']))
        for (src, x, y, w, h, t) in sorted(pages[page]):
            lines.append('src %s %d %d %d %d %d' % (src, x, y, w, h, t))
            report['placements'] += 1
        report['pages'] += 1
    writer.write(PAGES, '\n'.join(lines) + '\n')


# ------------------------------------------------------------------------------------------------------------
# ship models for Blender (export_ships.Exporter, its OBJ / MTL writer; textures point at the PNGs above)
# ------------------------------------------------------------------------------------------------------------

class ModelExporter(es.Exporter):

    def __init__(self, tree, out, info):
        es.Exporter.__init__(self, tree, out)
        self.info = info

    def texture(self, lif_rel, png_rel, role, problems):
        if lif_rel in self.tex_cache:
            return self.tex_cache[lif_rel]
        inf = self.info.get(lif_rel)
        if inf is None:
            rec = {'lif': lif_rel, 'error': 'texture file not found in the archive'}
        else:
            rec = collections.OrderedDict([
                ('png', lif_rel[:-4] + '.png'), ('lif', lif_rel), ('role', role), ('alpha', inf['alpha']),
                ('team_base_png', inf['masks'].get('team0')), ('team_stripe_png', inf['masks'].get('team1'))])
        self.tex_cache[lif_rel] = rec
        return rec

    def write_obj(self, mesh, obj_rel, tex_by_name, header):
        return es.Exporter.write_obj(self, mesh, 'Models/' + obj_rel[len('ships/'):], tex_by_name, header)


def export_models(tree, writer, info, report):
    ex = ModelExporter(tree, writer.out, info)
    saved = es.write_file
    es.write_file = writer.write_abs
    try:
        for race in es.RACES:
            ships = sorted(set(k.split('/')[1] for k in tree.walk(race) if k.count('/') >= 2))
            for ship in ships:
                ex.export_ship(race, ship)
    finally:
        es.write_file = saved
    report['ships'] = ex.totals['ships']
    report['obj_files'] = ex.totals['obj_files']


# ------------------------------------------------------------------------------------------------------------

README_TEXT = """Homeworld Infection - the Assets folder
=======================================

This folder holds the game's textures as PNG pictures, and its ships as models for Blender. Change a picture, and the
game uses your version from its next start. Only the files you change are used: everything else still comes from the
game's own files.

What is here
------------
  race_sect/, race_beast/, feman/, etg/ ...   every texture of the game as a PNG, in the folder and under the name
                                              the game uses (for example race_sect/sacolyte/rl0/lod0/0aco1.png)
  <name>_teambase.png, <name>_teamstripe.png  where the team's base colour and stripe colour show on that texture
                                              (white = all team colour, black = none)
  Models/<race>/<ship>/<ship>_lod0.obj        each ship for Blender, textured with these PNGs (lod0 = most detail)
  Models/<race>/<ship>/ingame/...obj          the ship as the game draws it: many small textures combined into
                                              "pages" (page0.png, page1.png ...)
  .jpg, .tga                                  the game's other pictures (credits, backgrounds), as they are
  assets-manifest.txt, assets-pages.txt       the export's own lists: do not edit them

Repainting a texture
--------------------
  Blender: File > Import > Wavefront (.obj), pick a ship in Models. In Texture Paint, paint on the model, then
           Image > Save (Alt+S): Blender writes the PNG back into this folder.
  GIMP (or any paint program): open the PNG, paint, then File > Overwrite <name>.png. Keep the name and the folder.

  - Keep the colours opaque where the original is opaque.
  - Bigger pictures (2x, 4x the size) work for the textures of ships and other objects in space. Pictures of the
    interface (menus, icons, cursors) are shown at the original size.
  - The game shrinks textures to fit its texture memory (Options > TEXTURE DETAIL): set it to the top to see your
    bigger textures at full size.
  - Team colours: paint the _teambase / _teamstripe masks. A mask you change replaces the game's own; a mask you
    leave alone keeps working with your repainted texture.

Small textures in pages
-----------------------
Most ships draw their small textures from combined pictures ("pages"). When you change a small texture, the game
rebuilds its page by itself at the next start. You can also paint a page directly (use the ingame model); where a
changed small texture sits, it wins over the page.

Undoing
-------
Delete the file you changed: the game uses its own again at the next start. Deleting the whole folder undoes
everything. Exporting again (tools/assets/export_assets.py) restores missing files and never overwrites a file you
changed.

Other files
-----------
A file with the same path and name as one of the game's files replaces it (a .lif texture, a .geo / .peo mesh, a
script, ...). Changing ship statistics or scripts changes the game itself; in a multiplayer game every player needs
the same files. An edited .obj model is NOT used by the game: only the pictures are.

The game's log lists every file it uses from here: the lines that start with "assets:".
"""


def export(args):
    t0 = time.time()
    out = os.path.abspath(args.out)
    tmp = None
    data, update = args.data, args.update
    if args.game:
        data, update, tmp = unpack_game(args.game)
    if not os.path.isdir(data):
        die('no extracted archives at %s (use --game DIR to read the game folder\'s .big files)' % data)
    check_out(out, [data, update, args.game])
    old = read_manifest(out) if os.path.isdir(out) else None
    if old is None and os.path.isdir(out) and os.listdir(out) and not args.force:
        print('note: %s has files but no %s: they are kept and stay in use' % (out, MANIFEST))
    os.makedirs(out, exist_ok=True)
    tree = hf.DataTree(data, update)
    writer = Writer(out, old, args.force)
    report = collections.Counter()
    for k in ('texture_errors', 'mask_collisions', 'page_problems', 'share_size_mismatch'):
        report[k] = []
    print('atlas pages ...')
    pages = solve_pages(tree, writer, report)
    page_sources = set(src + '.lif' for slot in pages.values() for (src, _x, _y, _w, _h, _t) in slot)
    print('textures ...')
    info = export_textures(tree, writer, report, page_sources)
    write_pages(writer, pages, info, report)
    print('pictures ...')
    export_images(tree, writer, report)
    if not args.no_models:
        print('ship models ...')
        export_models(tree, writer, info, report)
    writer.write(README, README_TEXT)
    write_manifest(out, writer.entries, 'exported %s from %s' % (time.strftime('%Y-%m-%d %H:%M:%S'), data))
    if tmp:
        shutil.rmtree(tmp, ignore_errors=True)
    files = sum(len(fn) for _dp, _dn, fn in os.walk(out))
    size = sum(os.path.getsize(os.path.join(dp, f)) for dp, _dn, fn in os.walk(out) for f in fn)
    print('Assets folder: %s' % out)
    print('  textures %d (paletted %d, rgba %d, not a power of two %d), team masks %d, other pictures %d' % (
        report['textures'], report['paletted'], report['rgba'], report['non_power_of_two'], report['masks'],
        report['images']))
    print('  atlas pages %d with %d placements (%d not exact copies)' % (report['pages'], report['placements'],
                                                                         report['placements_inexact']))
    print('  textures drawn from their share parent (textures.ll), PNG from that file: %d' % report['from_share_parent'])
    if not args.no_models:
        print('  ship models: %d ships, %d OBJ files' % (report['ships'], report['obj_files']))
    print('  files written %d, already as exported %d, kept (changed by you) %d, kept (not from an export) %d' % (
        writer.counts['written'], writer.counts['unchanged'], len(writer.kept), len(writer.kept_unlisted)))
    print('  folder: %d files, %.1f MB; manifest %d entries; %.1f s' % (files, size / 1e6, len(writer.entries),
                                                                       time.time() - t0))
    for k in ('texture_errors', 'mask_collisions', 'share_size_mismatch'):
        for item in report[k][:20]:
            print('  %s: %s' % (k, item))
    if report['page_problems']:
        print('  page notes: %d (first: %s)' % (len(report['page_problems']), report['page_problems'][0]))
    for rel in writer.kept[:20]:
        print('  kept your %s' % rel)
    return 0


def unpack_game(game):
    import bigfile
    tmp = tempfile.mkdtemp(prefix='hwi-assets-')
    out = {}
    for name in ('cataclysm.big', 'update.big'):
        path = None
        for n in os.listdir(game):
            if n.lower() == name:
                path = os.path.join(game, n)
        dest = os.path.join(tmp, name[:-4])
        out[name] = dest
        if path is None:
            continue
        print('unpacking %s ...' % path)
        arc = bigfile.BigArchive(path)
        for e in arc.entries:
            data, _problems = arc.read(e)
            target = os.path.join(dest, *e.unix_name.lower().split('/'))
            os.makedirs(os.path.dirname(target), exist_ok=True)
            with open(target, 'wb') as fh:
                fh.write(data)
        arc.close()
    return out['cataclysm.big'], out['update.big'], tmp


def target_of(tree, key):
    """What a file of the folder replaces (the engine's rule), or None."""
    if key in tree.index:
        return key
    if key.endswith('.png'):
        stem = key[:-4]
        if stem + '.lif' in tree.index:
            return stem + '.lif'
        for suffix in ('_teambase', '_teamstripe'):
            if stem.endswith(suffix) and stem[:-len(suffix)] + '.lif' in tree.index:
                return stem[:-len(suffix)] + '.lif (%s mask)' % suffix[1:]
    return None


def scan(out):
    found = {}
    for dp, dn, fn in os.walk(out):
        dn[:] = [d for d in dn if not d.startswith('.')]
        for f in fn:
            if f.startswith('.'):
                continue
            path = os.path.join(dp, f)
            rel = os.path.relpath(path, out).replace(os.sep, '/')
            found[rel.lower()] = (rel, path)
    return found


def status(args, refresh=False):
    out = os.path.abspath(args.out)
    old = read_manifest(out)
    if old is None:
        die('no %s in %s' % (MANIFEST, out))
    tree = hf.DataTree(args.data, args.update) if os.path.isdir(args.data) else None
    found = scan(out)
    changed, touched, new, missing = [], [], [], []
    for key, (rel, path) in sorted(found.items()):
        if key == MANIFEST:
            continue
        prev = old.get(key)
        if prev is None:
            new.append(rel)
            continue
        size, mtime = stat_pair(path)
        if (size, mtime) == prev[:2]:
            continue
        if size == prev[0] and sha256_file(path) == prev[2]:
            touched.append(key)
        else:
            changed.append(rel)
    missing = [old[k][3] for k in old if k not in found]
    if refresh:
        for key in touched:
            rel, path = found[key]
            size, mtime = stat_pair(path)
            old[key] = (size, mtime, old[key][2], old[key][3])
        write_manifest(out, old, 'refreshed %s' % time.strftime('%Y-%m-%d %H:%M:%S'))
        print('re-stamped %d files whose content is as exported' % len(touched))
        return 0
    skip = ('models/', 'feman/backgrounds/')

    def what(rel):
        key = rel.lower()
        if key.startswith(skip) or key in (README.lower(), PAGES):
            return 'not used by the game'
        t = target_of(tree, key) if tree else '?'
        return 'replaces %s' % t if t else 'matches no game file: ignored'
    print('Assets folder %s: %d files, manifest %d' % (out, len(found), len(old)))
    print('changed since the export (in use): %d' % len(changed))
    for rel in changed:
        print('  %s  -> %s' % (rel, what(rel)))
    print('new files (in use if they name a game file): %d' % len(new))
    for rel in new:
        print('  %s  -> %s' % (rel, what(rel)))
    print('touched but identical (in use until --refresh): %d' % len(touched))
    for key in touched[:20]:
        print('  %s' % found[key][0])
    print('missing (the game\'s own is used): %d' % len(missing))
    for rel in missing[:20]:
        print('  %s' % rel)
    return 0


def main(argv=None):
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument('--out', default=DEF_OUT, help='the Assets folder (default %(default)s)')
    ap.add_argument('--data', default=DEF_DATA, help='extracted cataclysm.big (default %(default)s)')
    ap.add_argument('--update', default=DEF_UPDATE, help='extracted update.big (default %(default)s)')
    ap.add_argument('--game', help='read cataclysm.big / update.big from this game folder instead')
    ap.add_argument('--no-models', action='store_true', help='textures only, no OBJ models')
    ap.add_argument('--force', action='store_true', help='overwrite files changed since the last export')
    ap.add_argument('--status', action='store_true', help='list what changed since the export')
    ap.add_argument('--refresh', action='store_true', help='re-stamp files whose content is as exported')
    args = ap.parse_args(argv)
    if args.status or args.refresh:
        return status(args, refresh=args.refresh)
    return export(args)


if __name__ == '__main__':
    sys.exit(main())
