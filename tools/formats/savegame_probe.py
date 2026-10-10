#!/usr/bin/env python3
"""Probe Homeworld: Cataclysm save games against docs/formats/savegame.md.

Usage: savegame_probe.py [-v] FILE [FILE ...]
       savegame_probe.py --selftest

A save is a raw udword version (0x10ad0027, shared with HW1 1.04) followed by chunks
{sdword type; sdword contentsSize; ubyte contents[contentsSize]}. The probe reads each file given on the command
line (read-only), walks the whole chunk chain, checks that it ends exactly at EOF, decodes the pre-game block in
Cataclysm order (single-player flag, song, player count, 0x90-byte player names, 8 x {race, base colour, detail
colour}, then currentMission + one extra number for single player or a 0x2B0-byte CaptainGameInfo for
multiplayer), and reports the chunk type histogram, the Cataclysm-only chunk types (0x80004000, 0x80009000,
0x8000A000, 0x8000B000, the 0x9C-byte state block) and ship chunk sizes (base Ship 0x63C).
--selftest builds a synthetic in-memory stream (no game data) and checks the parser.
"""
import collections
import struct
import sys

VERSION = 0x10AD0027
BASIC, VARIABLE, INFO, LLSPACEOBJS, AIPLAYER = 0x80000000, 0x40000000, 0x20000000, 0x10000000, 0x08000000
SUBTYPES = {0x1000: 'PING', 0x0F00: 'IDTOPTRTABLE', 0x0E00: 'SALVAGEINFO', 0x0D00: 'STRING', 0x0C00: 'PATH',
            0x0B00: 'AITEAMMOVE', 0x0A00: 'AITEAM', 0x0900: 'SELECTION', 0x0800: 'NEBTENDRIL',
            0x0700: 'NEBCHUNK', 0x0600: 'SHIPSINPROGRESS', 0x0500: 'ATTACKATOM', 0x0400: 'RETREATATOM',
            0x0300: 'SLAVEINFO', 0x0200: 'STATICINFO', 0x0100: 'MINEFORMATIONINFO', 0x00F0: 'SHIPSINSIDEME',
            0x00E0: 'MILITARYSLOT', 0x00D0: 'MILITARYPARADE', 0x00C0: 'RESEARCHTOPIC', 0x00B0: 'CLOUDSYSTEM',
            0x00A0: 'COMMANDTODO', 0x0090: 'CAMERACOMMAND', 0x0080: 'CAMERASTACKENTRY',
            0x0070: 'SHIPSINGLEPLAYERGAMEINFO', 0x0060: 'DOCKINFO', 0x0050: 'GUNINFO', 0x0040: 'BLOB',
            0x0030: 'ATTACKTARGETS', 0x0020: 'UNIVERSE', 0x0010: 'SPACEOBJ',
            # Cataclysm-only sub-types seen in the exe (names are descriptive guesses)
            0x4000: 'CAT_PREGAMETABLE', 0x9000: 'CAT_MISC9', 0xA000: 'CAT_MISCA', 0xB000: 'CAT_MISCB'}
CAT_ONLY = {0x80004000, 0x80009000, 0x8000A000, 0x8000B000}
SHIP_BASE = 0x63C


def type_name(t):
    t &= 0xFFFFFFFF
    flags = [n for n, b in (('BASIC', BASIC), ('VARIABLE', VARIABLE), ('INFO', INFO),
                            ('LLSPACEOBJS', LLSPACEOBJS), ('AIPLAYER', AIPLAYER)) if t & b]
    low = t & 0x07FFFFFF
    sub = SUBTYPES.get(low & 0xFFF0) or SUBTYPES.get(low)
    if sub is None and low:
        sub = '0x%x' % low
    if (low & 0xFFF0) == 0x10:
        sub = 'SPACEOBJ|objtype%d' % (low & 0xF)
    return '|'.join(flags + ([sub] if sub else [])) or '0'


def walk(d):
    if len(d) < 4:
        return None, 'too short'
    ver = struct.unpack_from('<I', d, 0)[0]
    chunks = []
    o = 4
    while o + 8 <= len(d):
        t, n = struct.unpack_from('<Ii', d, o)
        if n < 0 or o + 8 + n > len(d):
            return (ver, chunks), 'bad chunk at 0x%x (type 0x%08x size %d)' % (o, t, n)
        chunks.append((o, t, n))
        o += 8 + n
    if o != len(d):
        return (ver, chunks), '%d trailing bytes' % (len(d) - o)
    return (ver, chunks), None


def pregame(d, chunks):
    """Decode the PreLoadGame block; returns (dict, index of next chunk) or raises ValueError."""
    i = 0

    def info():
        nonlocal i
        o, t, n = chunks[i]
        if t != INFO or n != 4:
            raise ValueError('chunk %d: expected INFO, got %s size %d' % (i, type_name(t), n))
        i += 1
        return struct.unpack_from('<i', d, o + 8)[0]

    def basic(size):
        nonlocal i
        o, t, n = chunks[i]
        if t != BASIC or n != size:
            raise ValueError('chunk %d: expected BASIC size 0x%x, got %s size 0x%x' % (i, size, type_name(t), n))
        i += 1
        return d[o + 8:o + 8 + n]

    r = {'singlePlayer': info(), 'song': info(), 'numPlayers': info() & 0xFFFF}
    names = basic(0x90)
    r['playerNames'] = [names[k * 18:(k + 1) * 18].split(b'\0')[0].decode('latin-1') for k in range(8)]
    r['players'] = [(info(), info() & 0xFFFFFFFF, info() & 0xFFFFFFFF) for _ in range(8)]
    if r['singlePlayer']:
        r['currentMission'] = info()
        r['spExtra'] = info()
    else:
        basic(0x2B0)
        r['captainGameInfo'] = 0x2B0
    return r, i


def probe(path, d, verbose):
    res, err = walk(d)
    if res is None:
        return False, 'not a save (%s)' % err
    ver, chunks = res
    msgs = []
    ok = err is None and ver == VERSION
    if ver != VERSION:
        msgs.append('version 0x%08x != 0x10ad0027' % ver)
    if err:
        msgs.append(err)
    try:
        pg, nxt = pregame(d, chunks)
        msgs.append('sp=%d mission=%s players=%d' % (pg['singlePlayer'], pg.get('currentMission'), pg['numPlayers']))
    except (ValueError, IndexError) as e:
        ok = False
        msgs.append('pregame: %s' % e)
        nxt = 0
    hist = collections.Counter(type_name(t) for o, t, n in chunks)
    cat = collections.Counter('0x%08x' % (t & 0xFFFFFFFF) for o, t, n in chunks if (t & 0xFFFFFFFF) in CAT_ONLY)
    state = [n for o, t, n in chunks if (t & 0xFFFFFFFF) == BASIC and n == 0x9C]
    ships = [n for o, t, n in chunks if (t & 0xFFFFFFFF) == (VARIABLE | 0x10)]  # OBJ_ShipType = 0
    msgs.append('chunks=%d cat_only=%s state9C=%d ship_chunks=%d min_ship=0x%x' % (
        len(chunks), dict(cat), len(state), len(ships), min(ships) if ships else 0))
    if ships and min(ships) < SHIP_BASE - 4:
        ok = False
        msgs.append('ship chunk smaller than Cataclysm Ship (0x63c-4)')
    if verbose:
        msgs.append('types: ' + ', '.join('%s x%d' % kv for kv in hist.most_common()))
    return ok, '; '.join(msgs)


def selftest():
    def ch(t, payload):
        return struct.pack('<Ii', t, len(payload)) + payload

    def info(v):
        return ch(INFO, struct.pack('<i', v))

    b = struct.pack('<I', VERSION) + info(1) + info(3) + info(2) + ch(BASIC, b'Player\0'.ljust(0x90, b'\0'))
    for k in range(8):
        b += info(k % 2) + info(0x112233) + info(0x445566)
    b += info(5) + info(0) + ch(BASIC, bytes(0x9C)) + ch(0x80004000, bytes(12))
    b += ch(VARIABLE | 0x10, bytes(SHIP_BASE)) + ch(0x8000B000, bytes(0x17A0))
    ok, msg = probe('<selftest>', b, True)
    print('selftest', 'OK' if ok else 'FAIL', msg)
    return 0 if ok else 1


def main(argv):
    if '--selftest' in argv:
        return selftest()
    verbose = '-v' in argv
    files = [a for a in argv if a != '-v']
    if not files:
        print(__doc__)
        return 2
    bad = 0
    for p in files:
        with open(p, 'rb') as fh:
            d = fh.read()
        ok, msg = probe(p, d, verbose)
        bad += not ok
        print('%s %s  %s' % ('OK  ' if ok else 'FAIL', msg, p))
    print('summary: %d files, %d ok, %d fail' % (len(files), len(files) - bad, bad))
    return 1 if bad else 0


if __name__ == '__main__':
    sys.exit(main(sys.argv[1:]))
