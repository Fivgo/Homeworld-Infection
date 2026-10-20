#!/usr/bin/env python3
"""Probe a Homeworld: Cataclysm recorded game's packet file (<name>.pkts) against cataclysm.exe's layouts.

Usage: pkts_probe.py [-v] [--json OUT] FILE.pkts [FILE.pkts ...]
       pkts_probe.py --selftest

M2-R9 recorder-leftovers. A recording is a save (<name>, docs/formats/savegame.md; tools/formats/savegame_probe.py)
and its packets. The packet file is what recPackRecordPacket 0x004cbe20 appends for every sync packet the game
processes while it records (clProcessSyncPacket 0x00450910): records of
    udword validcheck ("PACK", 0x4b434150; "HEAD" 0x44414548 for /packetRecord's header, skipped by the playback)
    udword size
    ubyte  packet[size]
A sync packet is cataclysm.exe's 0x1C-byte header
    +0x00 uword type (0xCCCC command / 0xDDDD sync ...)   +0x02 uword from   +0x04 udword frame
    +0x08 NetSyncChecksums: uword packetnum, uword randcheck, real32 univcheck, udword blobcheck, udword extra
    +0x18 uword numberOfCommands                           +0x1A two bytes (not read)
then numberOfCommands commands, each a uword (type in the low byte, the sending player in the high byte) and a body
laid out as clProcessSyncPacket reads it (its jump table at 0x004510f4; src/Game/CommandNetwork.c lists the
offsets). The probe parses every command to its end and checks that the packet ends exactly there, that the frames
count up from 0, and reports the header fields, the command histogram and the per-type body sizes. Read-only.
"""
import argparse
import collections
import json
import struct
import sys

PACK, HEAD = 0x4B434150, 0x44414548
NAMES = {1: 'move', 2: 'attack', 3: 'formation', 4: 'dock', 5: 'createship', 6: 'collectresource', 7: 'protect',
         8: 'buildship', 9: 'special', 10: 'misc', 11: 'autolaunch', 12: 'launchmultiple', 13: 'alliance',
         14: 'rutransfer', 15: 'mphyperspace', 16: 'nothing16', 17: 'playerdropped', 18: 'deterministicbuild',
         19: 'researchinfo', 20: 'extbuildupdate', 21: 'pointorder', 22: 'mimic', 23: 'research', 24: 'upgrade',
         25: 'specialtarget', 26: 'specialtarget0', 27: 'nothing27'}


def u16(b, o):
    return struct.unpack_from('<H', b, o)[0]


def u32(b, o):
    return struct.unpack_from('<I', b, o)[0]


def sel(b, o):
    """a NetSelection: uword count, then count uword ship numbers"""
    return 2 + 2 * u16(b, o)


def targets(b, o):
    """a NetAttackSelection: uword count, then count 4-byte target IDs"""
    return 2 + 4 * u16(b, o)


def body_size(t, b, o):
    """the body length of command type t starting at o (after the command word), as clProcessSyncPacket advances"""
    if t in (1, 15):
        return 0x106 + 2 * u16(b, o + 0x104)
    if t in (2, 9, 25, 26):
        n = targets(b, o)
        return n + sel(b, o + n)
    if t in (3, 4):
        return 4 + sel(b, o + 4)
    if t in (5, 8):
        return 8
    if t == 6:
        return 2 + sel(b, o + 2)
    if t == 7:
        n = sel(b, o)
        return n + sel(b, o + n)
    if t == 10:
        return 10 + 2 * u16(b, o + 6)
    if t == 11:
        return 4
    if t == 12:
        return 2 + sel(b, o + 2)
    if t in (13, 14, 17):
        return 8
    if t == 16:
        return 0
    if t == 18:
        return 12
    if t == 19:
        return 4
    if t in (20, 23):
        return 16
    if t == 21:
        return 0xC + sel(b, o + 0xC)
    if t == 22:
        return 0x10 + sel(b, o + 0x10)
    if t == 24:
        return 4 + sel(b, o + 4)
    if t == 27:
        return 8
    return None


def detail(t, b, o):
    """a few decoded fields per command type (for -v)"""
    if t in (1, 15):
        n = u32(b, o + 0x10)
        return 'path=%d from=(%.1f,%.1f,%.1f) points=%d ships=%d' % ((u32(b, o),) + struct.unpack_from('<3f', b, o + 4) +
                                                                    (n, u16(b, o + 0x104)))
    if t == 3:
        return 'formation=%d level=%d ships=%d' % (u16(b, o), u16(b, o + 2), u16(b, o + 4))
    if t == 10:
        return 'data=%d misc=%d ships=%d' % (u32(b, o), u16(b, o + 4), u16(b, o + 6))
    if t in (5, 8):
        return 'type=%d race=%d player=%d creator=%d' % (u16(b, o), u16(b, o + 2), u16(b, o + 4), u16(b, o + 6))
    if t == 20:
        return 'player=%d progress=%g mode=%d creator=%d ship=%d' % (u32(b, o), struct.unpack_from('<f', b, o + 4)[0],
                                                                     u32(b, o + 8), u16(b, o + 12), u16(b, o + 14))
    return ''


def probe(path, verbose=False):
    d = open(path, 'rb').read()
    res = {'file': path, 'bytes': len(d), 'records': 0, 'heads': 0, 'packets': 0, 'commands': 0, 'errors': [],
           'types': collections.Counter(), 'sizes': collections.defaultdict(collections.Counter),
           'headers': collections.Counter(), 'frames_ok': True, 'nonzero_tail': 0, 'extra_nonzero': 0,
           'first_frame': None, 'last_frame': None}
    o = 0
    expect = 0
    while o < len(d):
        if o + 8 > len(d):
            res['errors'].append('truncated record header at 0x%x' % o)
            break
        chk, size = struct.unpack_from('<II', d, o)
        o += 8
        if o + size > len(d):
            res['errors'].append('truncated record at 0x%x (size %d)' % (o - 8, size))
            break
        b = d[o:o + size]
        o += size
        res['records'] += 1
        if chk == HEAD:
            res['heads'] += 1
            continue
        if chk != PACK:
            res['errors'].append('bad validcheck 0x%08x at record %d' % (chk, res['records']))
            break
        if size < 0x1C:
            res['errors'].append('packet %d shorter than the 0x1C header' % res['packets'])
            continue
        typ, frm, frame = struct.unpack_from('<HHI', b, 0)
        pnum, rcheck, ucheck, bcheck, extra = struct.unpack_from('<HHfII', b, 8)
        ncmd = u16(b, 0x18)
        res['headers'][(typ, frm)] += 1
        if extra:
            res['extra_nonzero'] += 1
        if u16(b, 0x1A):
            res['nonzero_tail'] += 1
        if res['first_frame'] is None:
            res['first_frame'] = frame
            expect = frame
        if frame != expect:
            res['frames_ok'] = False
            res['errors'].append('frame %d where %d was expected' % (frame, expect))
            expect = frame
        expect += 1
        res['last_frame'] = frame
        p = 0x1C
        for c in range(ncmd):
            if p + 2 > size:
                res['errors'].append('packet frame %d: command %d past the end' % (frame, c))
                break
            word = u16(b, p)
            t = word & 0xFF
            p += 2
            n = body_size(t, b, p)
            if n is None:
                res['errors'].append('packet frame %d: unknown command type %d' % (frame, t))
                break
            if p + n > size:
                res['errors'].append('packet frame %d: command %d (%s) body %d past the end (%d left)' %
                                     (frame, c, NAMES.get(t, t), n, size - p))
                break
            if verbose:
                print('  frame %d cmd %d %s from=%d body=%d %s' % (frame, c, NAMES.get(t, t), word >> 8, n, detail(t, b, p)))
            res['types'][NAMES.get(t, str(t))] += 1
            res['sizes'][NAMES.get(t, str(t))][n] += 1
            res['commands'] += 1
            p += n
        else:
            if p != size:
                res['errors'].append('packet frame %d: %d bytes after the last command' % (frame, size - p))
        res['packets'] += 1
    return res


def report(r):
    ok = not r['errors']
    print('%s %s: %d bytes, %d records (%d HEAD), %d packets frames %s..%s %s, %d commands; headers (type, from): %s; '
          'extra word set in %d, header tail set in %d' %
          ('OK  ' if ok else 'FAIL', r['file'], r['bytes'], r['records'], r['heads'], r['packets'], r['first_frame'],
           r['last_frame'], 'consecutive' if r['frames_ok'] else 'NOT consecutive', r['commands'],
           ', '.join('(0x%04x, %d) x%d' % (k[0], k[1], v) for k, v in sorted(r['headers'].items())),
           r['extra_nonzero'], r['nonzero_tail']))
    for name, cnt in sorted(r['types'].items(), key=lambda kv: -kv[1]):
        print('    %-18s %6d   body sizes %s' % (name, cnt, dict(sorted(r['sizes'][name].items()))))
    for e in r['errors'][:20]:
        print('    error: %s' % e)
    return ok


def selftest():
    hdr = struct.pack('<HHIHHfIIHH', 0xDDDD, 0, 0, 0, 0, 0.0, 0, 0, 2, 0)
    move = struct.pack('<H', 1) + struct.pack('<I3fI', 0, 1.0, 2.0, 3.0, 1) + b'\0' * (0x104 - 0x14) + struct.pack('<HH', 1, 7)
    form = struct.pack('<H', 3) + struct.pack('<HHHH', 5, 0, 1, 7)
    pkt = hdr + move + form
    data = struct.pack('<II', PACK, len(pkt)) + pkt
    import tempfile, os
    fd, name = tempfile.mkstemp()
    os.write(fd, data)
    os.close(fd)
    r = probe(name)
    os.unlink(name)
    assert not r['errors'], r['errors']
    assert r['types'] == {'move': 1, 'formation': 1}, r['types']
    print('selftest OK')


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('files', nargs='*')
    ap.add_argument('-v', action='store_true')
    ap.add_argument('--json')
    ap.add_argument('--selftest', action='store_true')
    a = ap.parse_args()
    if a.selftest:
        selftest()
        return 0
    allok = True
    out = []
    for f in a.files:
        r = probe(f, a.v)
        allok &= report(r)
        r['types'] = dict(r['types'])
        r['sizes'] = {k: dict(v) for k, v in r['sizes'].items()}
        r['headers'] = {'0x%04x/%d' % k: v for k, v in r['headers'].items()}
        out.append(r)
    if a.json:
        json.dump(out, open(a.json, 'w'), indent=1)
    print('summary: %d files, %s' % (len(a.files), 'all OK' if allok else 'FAILURES'))
    return 0 if allok else 1


if __name__ == '__main__':
    sys.exit(main())
