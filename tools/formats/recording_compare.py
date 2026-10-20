#!/usr/bin/env python3
"""Compare the format of recorded games (a save <name> and its <name>.pkts) field by field.

Usage: recording_compare.py [--json OUT] REF_DIR/NAME CAND_DIR/NAME [MORE_DIR/NAME ...]

M2-R9 recorder-leftovers. A recorded game of cataclysm.exe is two files in SavedGames/RecordedGames: the save written
when the recording starts (recPackInGameStartCBSafeToStart 0x004cbbb0 -> SaveGame 0x004fdaf0) and the packets
appended while it runs (recPackRecordPacket 0x004cbe20). The first argument is the reference (a GOG session's
recording), the others are compared with it. The values differ (different games); the FORMAT is compared:

save:  the version word; the pre-game block chunk by chunk (types and sizes: single-player flag, song, player count,
       the 0x90-byte names, 8 x race / two colours, then the mission number or the 0x2B0-byte tpGameCreated, whose
       bytes after +0xC8 are listed when not 0); every chunk type's sizes (a type whose size is fixed in the exe must
       have the same size); the order of the sections (the chunk-type sequence with runs of one type collapsed, compared
       with difflib); the ship chunks' sizes against the exe's Ship 0x63C + specifics; the walk to EOF.
pkts:  every record header (validcheck "PACK", the size), every packet header (type 0x5555, from, consecutive
       frames from 0, the extra checksum word, numberOfCommands at +0x18, 0x1C bytes) and every command parsed with
       cataclysm.exe's layouts to the packet's exact end (pkts_probe.py), the body sizes per command type.
--chunks REF CAND: two saves of the SAME game (the GOG game's recording save and this engine's /oracleResave of it after
loading it): the structure (every chunk's type and size in order) and, per chunk type, the dwords that differ, each
classified by the generated ILP32 tables (src/Game/SaveCat32Layouts.c) as a pointer slot (a stale pointer of the
saving process, list links, cached pointers: not data), data, or padding.
Read-only. Exit 0 when every candidate's format matches the reference's.
"""
import argparse
import collections
import difflib
import json
import os
import struct
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import savegame_probe as sgp     # noqa: E402
import pkts_probe as pkp         # noqa: E402

# chunk types whose contents size is fixed by an exe struct (savegame.md, SaveGame.h SAVE_CAT32_*)
FIXED = {
    'BASIC|UNIVERSE': 0x3D9BC, 'BASIC|BLOB': 0x100, 'BASIC|SPACEOBJ|objtype1': 0x1B4,    # objtype 1 asteroid
    'BASIC|SHIPSINGLEPLAYERGAMEINFO': 0x48, 'BASIC|CAT_PREGAMETABLE': None, 'BASIC|COMMANDTODO': 0x330,
    'BASIC|CAMERACOMMAND': 0x94, 'BASIC|RESEARCHTOPIC': 0x18, 'BASIC|MILITARYPARADE': 0x104,
    'BASIC|SHIPSINPROGRESS': 0x1B48, 'BASIC|MINEFORMATIONINFO': 0x4C, 'BASIC|SPACEOBJ|objtype2': None,
}


def load_save(path):
    d = open(path, 'rb').read()
    res, err = sgp.walk(d)
    if res is None:
        raise ValueError('%s: not a save (%s)' % (path, err))
    ver, chunks = res
    return d, ver, chunks, err


def rle(chunks):
    out = []
    for o, t, n in chunks:
        name = sgp.type_name(t)
        if out and out[-1][0] == name:
            out[-1][1] += 1
        else:
            out.append([name, 1])
    return out


def save_format(path):
    d, ver, chunks, err = load_save(path)
    r = {'path': path, 'version': '0x%08x' % ver, 'walk': err or 'to EOF', 'chunks': len(chunks)}
    pg, nxt = sgp.pregame(d, chunks)
    r['pregame'] = [(sgp.type_name(t), n) for o, t, n in chunks[:nxt]]
    r['singlePlayer'] = pg['singlePlayer']
    r['numPlayers'] = pg['numPlayers']
    if not pg['singlePlayer']:
        o, t, n = chunks[nxt - 1]
        cg = d[o + 8:o + 8 + n]
        r['cgi_head_nonzero'] = [i for i in range(0xC8) if cg[i]]
        r['cgi_tail_nonzero'] = [i for i in range(0xC8, n) if cg[i]]
        r['cgi_head_hex'] = cg[0x90:0xC8].hex()
    sizes = collections.defaultdict(collections.Counter)
    for o, t, n in chunks:
        sizes[sgp.type_name(t)][n] += 1
    r['sizes'] = sizes
    r['rle'] = rle(chunks)
    r['ships'] = [n for o, t, n in chunks if (t & 0xFFFFFFFF) == (sgp.VARIABLE | 0x10)]
    return r


def compare_save(ref, cand):
    lines = []
    ok = True

    def check(what, a, b):
        nonlocal ok
        same = (a == b)
        ok &= same
        lines.append('  %-34s %s   ref %s   cand %s' % (what, 'same' if same else 'DIFFERS', a, b))

    check('version', ref['version'], cand['version'])
    check('walk', ref['walk'], cand['walk'])
    check('single-player flag', ref['singlePlayer'], cand['singlePlayer'])
    check('pre-game chunks (type, size)', ref['pregame'], cand['pregame'])
    if 'cgi_tail_nonzero' in ref:
        check('tpGameCreated bytes set after +0xC8', ref['cgi_tail_nonzero'], cand.get('cgi_tail_nonzero'))
        lines.append('  %-34s ref %s' % ('tpGameCreated +0x90..+0xC7', ref['cgi_head_hex']))
        lines.append('  %-34s cand %s' % ('', cand.get('cgi_head_hex')))
    # sizes per type: a type both have must agree on a fixed size; list the types only one has
    both = sorted(set(ref['sizes']) & set(cand['sizes']))
    for name in both:
        rs, cs = set(ref['sizes'][name]), set(cand['sizes'][name])
        if len(rs) == 1 and len(cs) == 1 and rs != cs:
            ok = False
            lines.append('  size of %-25s DIFFERS ref %s cand %s' % (name, sorted(rs), sorted(cs)))
        elif name in FIXED and FIXED[name] is not None and (rs - {FIXED[name]} or cs - {FIXED[name]}):
            ok = False
            lines.append('  size of %-25s DIFFERS from the exe\'s 0x%x: ref %s cand %s' % (name, FIXED[name], sorted(rs), sorted(cs)))
    lines.append('  chunk types in both: %d; only in ref: %s; only in cand: %s' % (
        len(both), sorted(set(ref['sizes']) - set(cand['sizes'])), sorted(set(cand['sizes']) - set(ref['sizes']))))
    if cand['ships'] and min(cand['ships']) < sgp.SHIP_BASE - 4:
        ok = False
        lines.append('  ship chunk smaller than the exe Ship 0x63C - 4')
    # section order: the collapsed type sequence
    a = [x[0] for x in ref['rle']]
    b = [x[0] for x in cand['rle']]
    sm = difflib.SequenceMatcher(a=a, b=b, autojunk=False)
    lines.append('  section order (collapsed chunk-type runs): ref %d runs, cand %d runs, match ratio %.3f' %
                 (len(a), len(b), sm.ratio()))
    for tag, i1, i2, j1, j2 in sm.get_opcodes():
        if tag != 'equal':
            lines.append('    %s ref[%d:%d] %s -> cand[%d:%d] %s' % (tag, i1, i2, a[i1:i2][:6], j1, j2, b[j1:j2][:6]))
    return ok, lines


def compare_pkts(ref, cand):
    lines = []
    ok = not cand['errors']
    lines.append('  parse: ref %s, cand %s' % ('OK' if not ref['errors'] else ref['errors'][:3],
                                             'OK' if not cand['errors'] else cand['errors'][:3]))
    for what, key in (('records without HEAD', 'heads'),):
        lines.append('  %-34s ref %d cand %d' % ('HEAD records', ref[key], cand[key]))
    rh, ch = set(k[0] for k in ref['headers']), set(k[0] for k in cand['headers'])
    same = rh == ch
    ok &= same
    lines.append('  %-34s %s   ref %s cand %s' % ('packet type word', 'same' if same else 'DIFFERS',
                                                 ['0x%04x' % x for x in rh], ['0x%04x' % x for x in ch]))
    lines.append('  %-34s ref %s..%s %s, cand %s..%s %s' % ('frames', ref['first_frame'], ref['last_frame'],
                 'consecutive' if ref['frames_ok'] else 'gaps', cand['first_frame'], cand['last_frame'],
                 'consecutive' if cand['frames_ok'] else 'gaps'))
    ok &= cand['frames_ok'] and cand['first_frame'] == ref['first_frame']
    lines.append('  %-34s ref %d cand %d' % ('extra checksum word set', ref['extra_nonzero'], cand['extra_nonzero']))
    for name in sorted(set(ref['types']) | set(cand['types'])):
        lines.append('  %-18s ref %6d %-40s cand %6d %s' % (name, ref['types'].get(name, 0),
                     dict(sorted(ref['sizes'].get(name, {}).items())), cand['types'].get(name, 0),
                     dict(sorted(cand['sizes'].get(name, {}).items()))))
    return ok, lines


# --chunks: the save writer checked byte by byte against a save of the same game by cataclysm.exe
LAYOUT_OF = {'VARIABLE|SPACEOBJ|objtype0': 'Ship', 'BASIC|SPACEOBJ|objtype1': 'Bullet', 'BASIC|SPACEOBJ|objtype2': 'Asteroid',
             'BASIC|SPACEOBJ|objtype3': 'Nebula', 'BASIC|SPACEOBJ|objtype5': 'DustCloud', 'BASIC|SPACEOBJ|objtype6': 'Derelict',
             'BASIC|SPACEOBJ|objtype8': 'Missile', 'BASIC|BLOB': 'blob', 'BASIC|COMMANDTODO': 'CommandToDo',
             'BASIC|MILITARYPARADE': 'MilitaryParadeCommand', 'BASIC|SHIPSINPROGRESS': 'shipsinprogress',
             'BASIC|UNIVERSE': 'Universe', 'BASIC|CAMERACOMMAND': 'CameraCommand', 'BASIC|CAMERASTACKENTRY': 'CameraStackEntry',
             'BASIC|AIPLAYER': 'AIPlayer', 'BASIC|RESEARCHTOPIC': 'ResearchTopic', 'BASIC|SHIPSINSIDEME': 'ShipsInsideMe',
             'BASIC|SHIPSINGLEPLAYERGAMEINFO': 'ShipSinglePlayerGameInfo', 'BASIC|CLOUDSYSTEM': 'cloudSystem',
             'BASIC|MINEFORMATIONINFO': 'MineFormationInfo', 'BASIC|AITEAMMOVE': 'AITeamMove', 'BASIC|NEBCHUNK': 'nebChunk',
             'BASIC|NEBTENDRIL': 'nebTendril'}


def load_layouts(root):
    """the generated ILP32 tables (src/Game/SaveCat32Layouts.c): per layout, its ops (kind, off32, size32 covered, sub)"""
    import re
    src = open(os.path.join(root, 'src/Game/SaveCat32Layouts.c')).read()
    ops = {}
    for m in re.finditer(r'static const SaveCat32Op (ops_\w+)\[\] = \{(.*?)\n\};', src, re.S):
        lst = []
        for o in re.finditer(r'\{ (SC32_\w+), (0x[0-9a-f]+|\d+), (0x[0-9a-f]+|\d+), (0x[0-9a-f]+|\d+), (\d+), (0x[0-9a-f]+|\d+), '
                             r'(0x[0-9a-f]+|\d+), (NULL|&saveCat32Layout_\w+) \}', m.group(2)):
            kind, off32, n, count, s32 = o.group(1), int(o.group(2), 0), int(o.group(4), 0), int(o.group(5)), int(o.group(6), 0)
            sub = None if o.group(8) == 'NULL' else o.group(8)[len('&saveCat32Layout_'):]
            size = n if kind == 'SC32_COPY' else (count * s32)
            lst.append((kind, off32, size, s32, sub))
        ops[m.group(1)[4:]] = lst
    lay = {}
    for m in re.finditer(r'const SaveCat32Layout saveCat32Layout_(\w+) = \{ "[^"]*", (0x[0-9a-f]+), (0x[0-9a-f]+), \d+, ops_(\w+) \};', src):
        lay[m.group(1)] = (int(m.group(2), 0), ops.get(m.group(4), []))
    return lay


def classify(lay, name, off, depth=0):
    """the op of layout name covering byte off: 'ptr', 'data', or 'none' (padding / not in the layout)"""
    if name not in lay or depth > 8:
        return 'none'
    size32, ops = lay[name]
    for kind, off32, size, s32, sub in ops:
        if off32 <= off < off32 + size:
            if kind == 'SC32_ARRAY':
                return classify(lay, sub, (off - off32) % s32, depth + 1)
            return 'ptr' if kind == 'SC32_PTR' else 'data'
    return 'none'


def chunks_compare(ref, cand, root):
    lay = load_layouts(root)
    a, b = open(ref, 'rb').read(), open(cand, 'rb').read()
    (va, ca), _ = sgp.walk(a)
    (vb, cb), _ = sgp.walk(b)
    print('ref  %s: version 0x%08x, %d chunks, %d bytes' % (ref, va, len(ca), len(a)))
    print('cand %s: version 0x%08x, %d chunks, %d bytes' % (cand, vb, len(cb), len(b)))
    structure = (va == vb) and len(ca) == len(cb) and all(x[1:] == y[1:] for x, y in zip(ca, cb))
    print('structure (every chunk\'s type and size in order): %s' % ('SAME' if structure else 'DIFFERS'))
    same = 0
    per = collections.defaultdict(lambda: collections.Counter())
    dataoffs = collections.defaultdict(collections.Counter)
    for i, ((oa, ta, na), (ob, tb, nb)) in enumerate(zip(ca, cb)):
        x, y = a[oa + 8:oa + 8 + na], b[ob + 8:ob + 8 + nb]
        if (ta, na) != (tb, nb):
            print('  chunk %d: %s %d vs %s %d' % (i, sgp.type_name(ta), na, sgp.type_name(tb), nb))
            continue
        if x == y:
            same += 1
            continue
        name = sgp.type_name(ta)
        L = LAYOUT_OF.get(name)
        per[name]['chunks'] += 1
        for d in sorted(set(k & ~3 for k in range(na) if x[k] != y[k])):
            if L is None:
                cls = 'unmapped'
            elif L == 'Ship' and d >= 0x638:
                cls = 'specifics+blocks'
            else:
                cls = classify(lay, L, d)
            per[name][cls] += 1
            if cls not in ('ptr',):
                dataoffs[(name, cls)][d] += 1
    print('byte-identical chunks: %d of %d' % (same, len(ca)))
    for name, c in sorted(per.items(), key=lambda kv: -kv[1]['chunks']):
        print('  %-34s %5d chunks differ: dwords in pointer slots %d, in data %d, in padding %d, %s' % (
            name, c['chunks'], c['ptr'], c['data'], c['none'],
            ', '.join('%s %d' % (k, v) for k, v in c.items() if k not in ('chunks', 'ptr', 'data', 'none'))))
    for (name, cls), offs in sorted(dataoffs.items()):
        print('    %s %s dwords: %s' % (name, cls, ', '.join('0x%x(x%d)' % kv for kv in sorted(offs.items())[:24])))
    return structure


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('recordings', nargs='+')
    ap.add_argument('--json')
    ap.add_argument('--chunks', action='store_true', help='byte-compare two saves of the same game (the writer check)')
    a = ap.parse_args()
    if a.chunks:
        root = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
        return 0 if chunks_compare(a.recordings[0], a.recordings[1], root) else 1
    if len(a.recordings) < 2:
        ap.error('a reference and at least one candidate')
    ref_s = save_format(a.recordings[0])
    ref_p = pkp.probe(a.recordings[0] + '.pkts')
    allok = True
    out = []
    print('reference: %s (%d chunks, %d packets)' % (a.recordings[0], ref_s['chunks'], ref_p['packets']))
    for c in a.recordings[1:]:
        cs = save_format(c)
        cp = pkp.probe(c + '.pkts')
        ok1, l1 = compare_save(ref_s, cs)
        ok2, l2 = compare_pkts(ref_p, cp)
        print('%s %s (%d chunks, %d packets)' % ('SAME FORMAT' if ok1 and ok2 else 'DIFFERS', c, cs['chunks'], cp['packets']))
        print(' save:')
        print('\n'.join(l1))
        print(' pkts:')
        print('\n'.join(l2))
        allok &= ok1 and ok2
        out.append({'candidate': c, 'save_ok': ok1, 'pkts_ok': ok2, 'save': l1, 'pkts': l2})
    if a.json:
        json.dump(out, open(a.json, 'w'), indent=1)
    return 0 if allok else 1


if __name__ == '__main__':
    sys.exit(main())
