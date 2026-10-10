#!/usr/bin/env python3
"""Probe Homeworld: Cataclysm ship static files (.shp) against docs/formats/shipstatic.md.

Usage: shipstatic_probe.py --exe cataclysm.exe [-v] FILE.shp [FILE.shp ...]

Reads cataclysm.exe (read-only) to rebuild, at run time:
  * the 145-entry ship type table (shiptypeinfo, .data 0x00889530: {ShipType, char *name}) and the per-race
    first/last ship type tables (0x008894e8 / 0x00889504);
  * ShipStaticScriptTable (.data 0x008b2428, 16-byte scriptStructEntry: field offset = offset1 - offset2)
    and the gun table (0x008975a0, GunStatic offsets).
Then, for each .shp: maps the file to its ShipType by name, checks the race directory against the race's
type range, resolves every ShipStaticInfo key to its Cataclysm offset, counts GUN blocks against
NUMBER_OF_GUNS, DockPoint lines against NUMBER_OF_DOCK_POINTS and NavLight lines against
NUMBER_OF_NAV_LIGHTS, and resolves keys inside GUN blocks against the gun table. Nothing is written or copied.
"""
import argparse
import os
import re
import struct
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import script_probe as sp  # noqa: E402

SHIPTYPEINFO = 0x00889530
FIRST_OF_RACE = 0x008894E8
LAST_OF_RACE = 0x00889504
SHIP_TABLE = 0x008B2428
GUN_TABLE = 0x008975A0
RACES = ['race_sect', 'race_beast', 'race_kushan', 'race_taidan', 'race_p1', 'race_traders', 'race_derelict']


class Exe:
    def __init__(self, path):
        self.d = open(path, 'rb').read()
        self.secs = sp.pe_sections(self.d)

    def u32(self, a):
        for name, va, vs, rs, ro in self.secs:
            if va <= a < va + min(vs, rs) - 3:
                return struct.unpack_from('<I', self.d, ro + a - va)[0]
        return None

    def cstr(self, a):
        for name, va, vs, rs, ro in self.secs:
            if va <= a < va + min(vs, rs):
                o = ro + a - va
                return self.d[o:self.d.index(b'\0', o)].decode('latin-1')
        return None

    def struct_table(self, addr):
        out = {}
        while True:
            p0, p1, p2, p3 = (self.u32(addr + 4 * i) for i in range(4))
            if not p0:
                return out
            out[self.cstr(p0)] = (p2 - p3) & 0xffffffff
            addr += 16


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument('--exe', required=True)
    ap.add_argument('-v', action='store_true', help='list every key with its offset')
    ap.add_argument('files', nargs='+')
    a = ap.parse_args()
    exe = Exe(a.exe)
    types = {}
    for i in range(146):
        t, p = exe.u32(SHIPTYPEINFO + 8 * i), exe.u32(SHIPTYPEINFO + 8 * i + 4)
        if not p:
            break
        types[exe.cstr(p).lower()] = t
    first = [exe.u32(FIRST_OF_RACE + 4 * i) for i in range(7)]
    last = [exe.u32(LAST_OF_RACE + 4 * i) for i in range(7)]
    ship = exe.struct_table(SHIP_TABLE)
    gun = exe.struct_table(GUN_TABLE)
    print('exe: %d ship types, ShipStaticScriptTable %d keys (max offset 0x%x), gun table %d keys' % (
        len(types), len(ship), max(ship.values()), len(gun)))
    bad = 0
    for path in a.files:
        errs, notes = [], []
        name = os.path.splitext(os.path.basename(path))[0].lower()
        race = next((r for r in RACES if r in path.lower().replace('\\', '/').split('/')), None)
        t = types.get(name)
        if t is None:
            notes.append('not a ShipType name (derelict/variant/unused)')
        elif race is not None:
            ri = RACES.index(race)
            if not first[ri] <= t <= last[ri]:
                errs.append('type %d outside %s range %d..%d' % (t, race, first[ri], last[ri]))
        text = open(path, 'rb').read().decode('latin-1').splitlines()
        keys = []
        in_gun = depth = 0
        gun_blocks = dock_lines = nav_lines = 0
        counts = {}
        gun_unknown = set()
        for raw in text:
            line = raw.split(';', 1)[0].strip()
            if not line or line[0] == '[':
                continue
            tok = re.split(r'[ \t=]+', line, 1)
            k = tok[0]
            v = tok[1].strip() if len(tok) > 1 else ''
            if k == '{':
                depth += 1
                continue
            if k == '}':
                depth -= 1
                in_gun = 0
                continue
            if depth and in_gun:
                if k not in gun:
                    gun_unknown.add(k)
                continue
            if k == 'GUN':
                gun_blocks += 1
                in_gun = 1
                continue
            if k in ('NUMBER_OF_GUNS', 'NUMBER_OF_DOCK_POINTS', 'NUMBER_OF_NAV_LIGHTS'):
                counts[k] = int(re.match(r'-?\d+', v).group()) if re.match(r'-?\d+', v) else None
                continue
            if k == 'DockPoint':
                dock_lines += 1
                continue
            if k == 'NavLight':
                nav_lines += 1
                continue
            keys.append(k)
        for k, n in (('NUMBER_OF_GUNS', gun_blocks), ('NUMBER_OF_DOCK_POINTS', dock_lines),
                     ('NUMBER_OF_NAV_LIGHTS', nav_lines)):
            if k in counts and counts[k] != n:
                errs.append('%s=%s but %d blocks/lines' % (k, counts[k], n))
        known = [k for k in keys if k in ship]
        unknown = sorted(set(k for k in keys if k not in ship))
        msg = '%s type=%s keys=%d staticinfo=%d other=%d guns=%d docks=%d navs=%d' % (
            'FAIL' if errs else 'OK  ', t, len(keys), len(known), len(unknown), gun_blocks, dock_lines, nav_lines)
        if unknown:
            msg += ' other_keys=' + ','.join(unknown[:10])
        if gun_unknown:
            msg += ' gun_other=' + ','.join(sorted(gun_unknown)[:8])
        if notes:
            msg += ' (' + '; '.join(notes) + ')'
        if errs:
            msg += ' :: ' + '; '.join(errs)
            bad += 1
        print('%s  %s' % (msg, path))
        if a.v:
            for k in known:
                print('    +0x%03x %s' % (ship[k], k))
    print('summary: %d files, %d ok, %d fail' % (len(a.files), len(a.files) - bad, bad))
    return 1 if bad else 0


if __name__ == '__main__':
    sys.exit(main())
