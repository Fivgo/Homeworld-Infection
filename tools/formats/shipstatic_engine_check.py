#!/usr/bin/env python3
"""Compare the engine's ship-static parse with tools/formats/shipstatic_probe.py, file by file.

Usage: shipstatic_engine_check.py --exe cataclysm.exe --data DIR RUN.log [RUN.log ...]

  --exe   the GOG cataclysm.exe (read-only; the probe rebuilds its tables from it)
  --data  the extracted Cataclysm data (original/game_extracted/cataclysm); every *.shp under it is probed
  RUN.log engine logs of `hwi-engine /catShipStaticsTest` (tools/m2_harness.sh --mode statics). Several logs are
          merged (a later log fills files an earlier one did not reach).

The engine (src/Game/Universe.c InitStatShipInfo, docs/m2-r2-shipstatics.md) logs per ship
    shipstatic: loaded RACE_SECT/sRecon.shp type=0 race=0 keys=69 guns=1 docks=0 navs=0 lod=yes staticinit=...
where keys = ShipStaticScriptTable keys its scriptSetStruct pass matched (logged before CustShipStaticInit), and guns/docks/navs = the counts it
allocated (the declared NUMBER_OF_* values). A ship-type file passes when
  * type and race directory agree with the probe,
  * keys equals the probe's count of ShipStaticScriptTable keys on every line (the probe's "staticinfo" count
    plus any such key inside a GUN block, which scriptSetStruct also sees; the two are equal for all 173 files),
  * guns/docks/navs equal the probe's block/line counts, or, for a file whose declared NUMBER_OF_* disagrees with
    its lines (probe FAIL), equal the declared count, which is what the exe uses.
Files that are not ship-type names (missiles, mines, resources, derelicts, ...) pass when the engine logged
"shipstatic: other ... loaded" for them. Nothing is written.
"""
import argparse
import os
import re
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import shipstatic_probe as P  # noqa: E402

LOADED = re.compile(r'shipstatic: loaded (\S+) type=(\d+) race=(\d+) keys=(\d+) guns=(-?\d+) docks=(-?\d+) '
                    r'navs=(-?\d+) lod=(\w+) staticinit=(\w+)')
MISSING = re.compile(r'shipstatic: (\S+) not found.*ship type (\d+)')
SKIPPED = re.compile(r'shipstatic: skipped type (\d+)')
OTHER = re.compile(r'shipstatic: other (\w+) (\S+) loaded')


def norm(path):
    return path.replace('\\', '/').lower()


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument('--exe', required=True)
    ap.add_argument('--data', required=True)
    ap.add_argument('-q', action='store_true', help='print only the non-passing files and the summary')
    ap.add_argument('logs', nargs='+')
    a = ap.parse_args()

    exe = P.Exe(a.exe)
    types, first, last, ship, gun = P.read_tables(exe)
    files = {}
    for dp, dn, fn in os.walk(a.data):
        for f in fn:
            if f.lower().endswith('.shp'):
                full = os.path.join(dp, f)
                files[norm(os.path.relpath(full, a.data))] = full

    loaded, missing, skipped, other = {}, {}, set(), {}
    for log in a.logs:
        for line in open(log, encoding='latin-1'):
            m = LOADED.search(line)
            if m:
                loaded.setdefault(norm(m.group(1)), m.groups())
                continue
            m = MISSING.search(line)
            if m:
                missing.setdefault(int(m.group(2)), norm(m.group(1)))
                continue
            m = SKIPPED.search(line)
            if m:
                skipped.add(int(m.group(1)))
                continue
            m = OTHER.search(line)
            if m:
                other.setdefault(norm(m.group(2)), m.group(1))

    tally = {}
    rows = []
    for rel in sorted(files):
        r = P.analyse(files[rel], types, first, last, ship, gun)
        if r['type'] is None:
            status = 'OTHER-LOADED' if rel in other else 'OTHER-NOT-LOADED'
            detail = other.get(rel, 'no engine loader ran for this file')
        elif rel not in loaded:
            status = 'SKIPPED' if r['type'] in skipped else 'NOT-LOADED'
            detail = 'type %d' % r['type']
        else:
            path, t, race, keys, g, d, n, lod, si = loaded[rel]
            t, race, keys, g, d, n = int(t), int(race), int(keys), int(g), int(d), int(n)
            c = r['counts']
            decl = {k: (c[k] if c.get(k) is not None and c[k] > 0 else 0)
                    for k in ('NUMBER_OF_GUNS', 'NUMBER_OF_DOCK_POINTS', 'NUMBER_OF_NAV_LIGHTS')}
            problems = []
            if t != r['type']:
                problems.append('type %d, probe %d' % (t, r['type']))
            if r['race'] is None or race != P.RACES.index(r['race']):
                problems.append('race %d, probe %s' % (race, r['race']))
            if keys != r['engine_keys']:
                problems.append('keys %d, probe %d' % (keys, r['engine_keys']))
            declared_used = []
            for name, eng, lines, k in (('guns', g, r['guns'], 'NUMBER_OF_GUNS'),
                                        ('docks', d, r['docks'], 'NUMBER_OF_DOCK_POINTS'),
                                        ('navs', n, r['navs'], 'NUMBER_OF_NAV_LIGHTS')):
                if eng == lines:
                    continue
                if eng == decl[k] and k in c:
                    declared_used.append('%s=%d declared (%d lines in the file)' % (name, eng, lines))
                else:
                    problems.append('%s %d, probe %d lines, declared %s' % (name, eng, lines, c.get(k)))
            if problems:
                status, detail = 'MISMATCH', '; '.join(problems)
            elif declared_used:
                status, detail = 'OK-DECLARED-COUNT', '; '.join(declared_used)
            else:
                status = 'OK'
                detail = 'type=%d keys=%d guns=%d docks=%d navs=%d' % (t, keys, g, d, n)
            if keys != len(r['known']):
                detail += ' [probe staticinfo=%d; %d key(s) inside GUN blocks]' % (len(r['known']), keys - len(r['known']))
        tally[status] = tally.get(status, 0) + 1
        rows.append((status, rel, detail))
    for status, rel, detail in rows:
        if not a.q or status not in ('OK', 'OTHER-LOADED'):
            print('%-17s %-50s %s' % (status, rel, detail))
    for t, path in sorted(missing.items()):
        print('%-17s %-50s type %d has no .shp in the data; the engine logged it disabled' % ('TYPE-NO-FILE', path, t))
    ship_files = sum(v for k, v in tally.items() if not k.startswith('OTHER'))
    print('summary: %d .shp files: %s; ship types without a file: %d' % (
        len(rows), ', '.join('%s %d' % kv for kv in sorted(tally.items())), len(missing)))
    print('ship-type files parsed as the probe does (OK + OK-DECLARED-COUNT): %d of %d' % (
        tally.get('OK', 0) + tally.get('OK-DECLARED-COUNT', 0), ship_files))
    return 0 if not tally.get('MISMATCH') and not tally.get('NOT-LOADED') else 1


if __name__ == '__main__':
    sys.exit(main())
