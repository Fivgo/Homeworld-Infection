#!/usr/bin/env python3
"""Probe Homeworld: Cataclysm mesh files (.geo / .peo) against the layout in docs/formats/mesh.md.

Usage: mesh_probe.py [-v] FILE [FILE ...]

Reads each file given on the command line (read-only, nothing is copied or written) and checks:
  * the 0x44-byte GeoFileHeader (ident RMF97ba / RMF99ba, version, counts, offsets);
  * that the polygon-object table uses the Cataclysm stride 0x74 (HW1 0x402 used 0x70);
  * that every object's arrays (vertices, per-vertex info, normals, polygons) lie inside the file and
    tile the file in the order the exporter wrote them;
  * that the material table (32 bytes per entry) starts right after the object table.
Prints one line per file and a summary. Exit code 1 if any file fails.
"""
import struct
import sys

HDR = struct.Struct('<8sIIIIIIIII24s')   # 0x44 bytes, HW1 GeoFileHeader
OBJ_CAT = 0x74                            # Cataclysm polygonobject (this probe's subject)
OBJ_HW1 = 0x70                            # HW1 0x402 polygonobject
MAT = 0x20                                # materialentry
VERT = 0x10                               # vertexentry
VINFO = 0x10                              # Cataclysm per-vertex info entry (new)
NORM = 0x10                               # normalentry
POLY = 0x28                               # polyentry


def probe(path, verbose=False):
    with open(path, 'rb') as fh:
        d = fh.read()
    if len(d) < HDR.size:
        return False, 'short file'
    (ident, version, pName, fileSize, localSize, nPub, nLoc, oPub, oLoc, nObj, _res) = HDR.unpack_from(d, 0)
    ident = ident.rstrip(b'\0').decode('latin-1')
    errs = []
    if ident not in ('RMF97ba', 'RMF99ba'):
        errs.append('ident %r' % ident)
    if fileSize != len(d) - HDR.size:
        errs.append('fileSize %d != len-0x44 %d' % (fileSize, len(d) - HDR.size))
    end_objs_cat = HDR.size + nObj * OBJ_CAT
    end_objs_hw1 = HDR.size + nObj * OBJ_HW1
    if oPub != end_objs_cat:
        errs.append('oPub 0x%x != 0x44+nObj*0x74 0x%x (hw1 stride would give 0x%x)' % (oPub, end_objs_cat, end_objs_hw1))
    if oLoc != oPub + nPub * MAT and not (nLoc == 0 and oLoc == oPub):
        errs.append('oLoc 0x%x unexpected' % oLoc)
    mats_end = oPub + (nPub + nLoc) * MAT
    # per-object arrays
    totals = {'vert': 0, 'poly': 0, 'norm': 0}
    spans = []
    names = []
    for i in range(nObj):
        o = HDR.size + i * OBJ_CAT
        (oname, flags, iobj, crc, nV, nFN, nVN, nP,
         pV, pVI, pN, pP, pMo, pDa, pSi) = struct.unpack_from('<IBBHiiiiIIIIIII', d, o)
        for lab, ptr, n, sz in (('vert', pV, nV, VERT), ('vinfo', pVI, nV, VINFO),
                                ('norm', pN, nFN + nVN, NORM), ('poly', pP, nP, POLY)):
            if ptr + n * sz > len(d):
                errs.append('obj%d %s 0x%x+%d*%d past EOF' % (i, lab, ptr, n, sz))
            spans.append((ptr, ptr + n * sz, '%s%d' % (lab, i)))
        for lab, ptr in (('mother', pMo), ('daughter', pDa), ('sister', pSi)):
            if ptr and (ptr - HDR.size) % OBJ_CAT:
                errs.append('obj%d %s 0x%x not on a 0x74 object boundary' % (i, lab, ptr))
        if oname:
            end = d.find(b'\0', oname)
            names.append(d[oname:end].decode('latin-1'))
        # identity-ish matrix check: m44 should be 1.0 at +0x70
        m44 = struct.unpack_from('<f', d, o + 0x70)[0]
        if abs(m44 - 1.0) > 1e-6:
            errs.append('obj%d m44=%g (matrix not at +0x34?)' % (i, m44))
        totals['vert'] += nV
        totals['poly'] += nP
        totals['norm'] += nFN + nVN
    # arrays must not overlap and must start at or after the material table
    spans = sorted(s for s in spans if s[1] > s[0])
    shared = 0
    uniq = []
    for s in spans:
        if uniq and s[0] == uniq[-1][0] and s[1] == uniq[-1][1]:
            shared += 1          # duplicate objects point at the same array (POF_DuplicateObject)
            continue
        uniq.append(s)
    for a, b in zip(uniq, uniq[1:]):
        if b[0] < a[1]:
            errs.append('overlap %s/%s' % (a[2], b[2]))
    spans = uniq
    if spans and spans[0][0] < mats_end:
        errs.append('array %s starts inside header/objects/materials' % spans[0][2])
    msg = '%s ident=%s ver=0x%x nObj=%d nMat=%d verts=%d polys=%d norms=%d shared_arrays=%d' % (
        'OK  ' if not errs else 'FAIL', ident, version, nObj, nPub + nLoc,
        totals['vert'], totals['poly'], totals['norm'], shared)
    if verbose and names:
        msg += ' objects=' + ','.join(names[:6])
    if errs:
        msg += ' :: ' + '; '.join(errs[:4])
    return not errs, msg


def main(argv):
    verbose = '-v' in argv
    files = [a for a in argv if a != '-v']
    if not files:
        print(__doc__)
        return 2
    bad = 0
    for p in files:
        ok, msg = probe(p, verbose)
        bad += not ok
        print('%s  %s' % (msg, p))
    print('summary: %d files, %d ok, %d fail' % (len(files), len(files) - bad, bad))
    return 1 if bad else 0


if __name__ == '__main__':
    sys.exit(main(sys.argv[1:]))
