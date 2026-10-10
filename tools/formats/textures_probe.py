#!/usr/bin/env python3
"""Probe Homeworld: Cataclysm texture files (.lif images and the textures.ll listing) against
docs/formats/textures.md.

Usage: textures_probe.py FILE [FILE ...]     (.lif files and/or textures.ll; read-only, nothing copied)

.lif: checks ident "Willy 7", version 0x104, and that the offsets in the 0x30-byte header describe the
      file exactly: paletted (flag 0x2) = indices w*h @0x30, palette 256*4, team effect 0/1 256 bytes each;
      RGBA (flag 0x8 Alpha, with or without 0x2) = w*h*4 pixels @0x30, then a w*h byte team map per
      TeamColor flag (offsets of absent blocks are left-over values and are ignored).
.ll : checks ident "Event13", version 0x104, element count, string/sharing block sizes, that names are
      sorted case-insensitively (trImageMeasureFromListing does a binary search with _stricmp) and that
      sharedFrom / sharedTo indices are in range.
"""
import struct
import sys

TRF_Paletted, TRF_Alpha, TRF_Team0, TRF_Team1, TRF_IgnoreDim = 0x2, 0x8, 0x10, 0x20, 0x100
LIF = struct.Struct('<8siiiiIIIIII')   # 0x30
LLH = struct.Struct('<8siiiii')        # 0x1c
LLE = struct.Struct('<Iiiiiiii')       # 0x20


def probe_lif(path, d):
    ident, ver, flags, w, h, pcrc, icrc, oData, oPal, oT0, oT1 = LIF.unpack_from(d, 0)
    errs = []
    if ident != b'Willy 7\0':
        errs.append('ident %r' % ident)
    if ver != 0x104:
        errs.append('version 0x%x' % ver)
    if oData != 0x30:
        errs.append('data @0x%x' % oData)
    if flags & TRF_Paletted and not flags & TRF_Alpha:
        expect = [(oData, w * h), (oPal, 1024), (oT0, 256), (oT1, 256)]
        kind = 'pal8'
    else:
        expect = [(oData, w * h * 4)]
        if flags & TRF_Team0:
            expect.append((oT0, w * h))
        if flags & TRF_Team1:
            expect.append((oT1, w * h))
        kind = 'rgba'
    end = 0x30
    for off, size in expect:
        if off != end:
            errs.append('block @0x%x expected 0x%x' % (off, end))
        end = off + size
    if end != len(d):
        errs.append('ends at 0x%x, file 0x%x' % (end, len(d)))
    return errs, '%s %dx%d flags=0x%x' % (kind, w, h, flags)


def probe_ll(path, d):
    ident, ver, n, strLen, shareLen, total = LLH.unpack_from(d, 0)
    errs = []
    if ident != b'Event13\0':
        errs.append('ident %r' % ident)
    if ver != 0x104:
        errs.append('version 0x%x' % ver)
    if total != len(d) - LLH.size:
        errs.append('totalLength %d != %d' % (total, len(d) - LLH.size))
    if n * LLE.size + strLen + shareLen != total:
        errs.append('n*32+str+share %d != total %d' % (n * LLE.size + strLen + shareLen, total))
    sb = LLH.size + n * LLE.size
    shb = sb + strLen
    names = []
    nshared = nsharedfrom = 0
    for i in range(n):
        oName, w, h, flags, crc, nShared, oSharedTo, sharedFrom = LLE.unpack_from(d, LLH.size + i * LLE.size)
        e = d.index(b'\0', sb + oName)
        names.append(d[sb + oName:e].decode('latin-1'))
        if sharedFrom >= n:
            errs.append('elem %d sharedFrom %d' % (i, sharedFrom))
        if sharedFrom >= 0:
            nsharedfrom += 1
        if nShared:
            nshared += 1
            for k in range(nShared):
                j = struct.unpack_from('<i', d, shb + oSharedTo + 4 * k)[0]
                if not 0 <= j < n:
                    errs.append('elem %d sharedTo[%d]=%d' % (i, k, j))
                    break
    low = [s.lower() for s in names]
    unsorted = sum(1 for a, b in zip(low, low[1:]) if a > b)
    if unsorted:
        errs.append('%d adjacent pairs out of _stricmp order' % unsorted)
    return errs, 'll n=%d strings=%d sharing=%d sharers=%d shared_from=%d first=%s' % (
        n, strLen, shareLen, nshared, nsharedfrom, names[0] if names else '')


def main(argv):
    if not argv:
        print(__doc__)
        return 2
    bad = 0
    for p in argv:
        with open(p, 'rb') as fh:
            d = fh.read()
        if d[:8] == b'Event13\0':
            errs, msg = probe_ll(p, d)
        else:
            errs, msg = probe_lif(p, d)
        bad += bool(errs)
        print('%s %s%s  %s' % ('FAIL' if errs else 'OK  ', msg, (' :: ' + '; '.join(errs[:4])) if errs else '', p))
    print('summary: %d files, %d ok, %d fail' % (len(argv), len(argv) - bad, bad))
    return 1 if bad else 0


if __name__ == '__main__':
    sys.exit(main(sys.argv[1:]))
