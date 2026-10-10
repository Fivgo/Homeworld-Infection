#!/usr/bin/env python3
"""Probe Homeworld: Cataclysm front-end screen files (.fib) against docs/formats/fib.md.

Usage: fib_probe.py [-v] FILE.fib [FILE.fib ...]

Reads each file given on the command line (read-only, nothing is copied) and walks it the way feScreensLoad
(0x004744e0) does: 12-byte fibfileheader {char identify[8] "Bananna"; uword version 0x212; uword nScreens},
nScreens x 20-byte fescreen {name, flags, uword nLinks, uword nAtoms, links, atoms}, 12-byte felink
{name, flags, linkToName} and 76-byte featom records. All pointers are file offsets. Checks: ident, version,
every offset inside the file, names NUL-terminated, links/atoms 4-byte aligned, atom types in 0..21 (0 = text or plain region in the files; HW1 enum starts at 1)
(FA_UserRegion..FA_OpaqueDecorativeRegion), stored rectangles as 2-point (x < x2, y < y2), and that string
attribs of static-text atoms name a font. -v prints each screen.
"""
import collections
import struct
import sys

FA = ['0(text-or-plain)', 'UserRegion', 'StaticText', 'Button', 'CheckBox', 'ToggleButton', 'ScrollBar', 'StatusBar',
      'TextEntry', 'ListViewExpandButton', 'TitleBar', 'MenuItem', 'RadioButton', 'CutoutRegion',
      'DecorativeRegion', 'Divider', 'ListWindow', 'BitmapButton', 'HorizSlider', 'VertSlider', 'DragButton',
      'OpaqueDecorativeRegion']
FAF_Bitmap = 0x4
ATOM = struct.Struct('<IIIBBHII8hII B5s2s 2I I 2I')   # 76 bytes


def cstr(d, o):
    if not 0 <= o < len(d):
        return None
    e = d.find(b'\0', o)
    return None if e < 0 else d[o:e].decode('latin-1')


def probe(path, d, verbose):
    errs = []
    if len(d) < 12:
        return ['too short'], {}
    ident, ver, nscr = struct.unpack_from('<8sHH', d, 0)
    if ident != b'Bananna\0':
        errs.append('ident %r' % ident)
    if ver != 0x212:
        errs.append('version 0x%x' % ver)
    stats = collections.Counter()
    types = collections.Counter()
    fonts = collections.Counter()
    for s in range(nscr):
        so = 12 + 20 * s
        if so + 20 > len(d):
            errs.append('screen %d past EOF' % s)
            break
        oname, flags, nlinks, natoms, olinks, oatoms = struct.unpack_from('<IIHHII', d, so)
        name = cstr(d, oname)
        if name is None:
            errs.append('screen %d bad name offset 0x%x' % (s, oname))
        if olinks % 4 or oatoms % 4:
            errs.append('screen %s links/atoms not 4-byte aligned' % name)
        if olinks + 12 * nlinks > len(d) or oatoms + ATOM.size * natoms > len(d):
            errs.append('screen %s lists past EOF' % name)
            continue
        stats['screens'] += 1
        stats['links'] += nlinks
        stats['atoms'] += natoms
        for k in range(nlinks):
            lname, lflags, lto = struct.unpack_from('<III', d, olinks + 12 * k)
            if lflags & 1 and (cstr(d, lname) is None or cstr(d, lto) is None):
                errs.append('screen %s link %d bad names' % (name, k))
        for k in range(natoms):
            a = ATOM.unpack_from(d, oatoms + ATOM.size * k)
            aname, aflags, status, typ, bw, tab, bcol, ccol = a[:8]
            x, lx, y, ly, x2, lw, y2, lh = a[8:16]
            pdata, attribs = a[16], a[17]
            if not 0 <= typ <= 21:
                errs.append('screen %s atom %d type %d' % (name, k, typ))
            types[FA[typ] if typ < len(FA) else str(typ)] += 1
            if aname and cstr(d, aname) is None:
                errs.append('screen %s atom %d name offset' % (name, k))
            if not (x2 > x and y2 > y):
                stats['non2point'] += 1
            effective = typ
            if pdata and typ != 12 and not aflags & FAF_Bitmap:
                effective = 2                     # feScreensLoad turns text-bearing atoms into StaticText
            if attribs and effective == 2:
                f = cstr(d, attribs)
                if f is None:
                    errs.append('screen %s atom %d font offset' % (name, k))
                else:
                    fonts[f] += 1
        if verbose:
            print('    screen %-32s flags=0x%x links=%d atoms=%d' % (name, flags, nlinks, natoms))
    stats['types'] = dict(types)
    stats['fonts'] = len(fonts)
    return errs, stats


def main(argv):
    verbose = '-v' in argv
    files = [a for a in argv if a != '-v']
    if not files:
        print(__doc__)
        return 2
    bad = 0
    total = collections.Counter()
    for p in files:
        with open(p, 'rb') as fh:
            d = fh.read()
        errs, st = probe(p, d, verbose)
        bad += bool(errs)
        for t, n in st.get('types', {}).items():
            total[t] += n
        print('%s screens=%s links=%s atoms=%s fonts=%s non2point=%s%s  %s' % (
            'FAIL' if errs else 'OK  ', st.get('screens'), st.get('links'), st.get('atoms'), st.get('fonts'),
            st.get('non2point', 0), (' :: ' + '; '.join(errs[:4])) if errs else '', p))
    print('summary: %d files, %d ok, %d fail; atom types %s' % (len(files), len(files) - bad, bad,
                                                               dict(total.most_common())))
    return 1 if bad else 0


if __name__ == '__main__':
    sys.exit(main(sys.argv[1:]))
