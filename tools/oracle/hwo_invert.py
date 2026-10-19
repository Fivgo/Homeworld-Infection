#!/usr/bin/env python3
"""Recover ships' position, velocity or health words from a reference stream's per-tick hash.

M2-R8 oracle-rung-physics (docs/m2-r8-oracle-rung-physics.md "dump"): the oracle stream hashes every live ship's
words with FNV-1a-64 (tools/oracle/hwo_format.h: ships_pos / ships_vel = shipID u16 then x, y, z f32 bits;
ships_health = shipID u16 then the health f32 bits), in ShipList order. FNV-1a is invertible byte by byte, so when a
tick of the reference (the GOG stream) differs from ours in one or two "units" only, their words can be recovered:
the other ships are taken from our engine's /oracleShipDump "all" dump of the same tick (the 'S' lines, in list
order), the hash is run forward to the first unknown and unwound backward from the reference hash over the ships
after the last one, and the unknown words are searched within +-N ulps of ours (meet in the middle).

A unit is one ship (N) or a group of ships that share their words (N+M+K: a ship and the components clamped to it,
such as a mothership and its modules, which have the same position and velocity).

Usage:
  hwo_invert.py REF.hwo SHIPDUMP.txt --tick T --ship UNIT [--field vel|pos|health] [--ulps X,Y,Z]
  hwo_invert.py REF.hwo SHIPDUMP.txt --tick T --ship UNIT,UNIT --ulps 12,12,12   (two units; keep the range small)
  hwo_invert.py REF.hwo SHIPDUMP.txt --tick T --field res --ship INDEX   (a resource: its index in the 'R' lines)
Prints the recovered words, ours, and the difference REF - ours in raw bits per component. Exit 0 found (one
solution), 1 none or several. Only standard-library Python; reads a .hwo stream and a dump text, never game data.
"""
import argparse
import os
import struct
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import hwo_compare  # noqa: E402  (the stream reader)

M64 = (1 << 64) - 1
FNV_INIT = 0xcbf29ce484222325
FNV_PRIME = 0x00000100000001b3
FNV_INV = pow(FNV_PRIME, -1, 1 << 64)
FIELDS = {"pos": ("ships_pos", "pos", 3), "vel": ("ships_vel", "vel", 3), "health": ("ships_health", "health", 1),
          "res": ("resources_h", "pos", 3)}


def fnv_bytes(h, data):
    for b in data:
        h = ((h ^ b) * FNV_PRIME) & M64
    return h


def unfnv_bytes(h, data):
    for b in reversed(data):
        h = ((h * FNV_INV) & M64) ^ b
    return h


RESOURCES = False     # --field res: the 'resources' hash has no id, positions only, of the 'R' lines


def ship_bytes(sid, words):
    if RESOURCES:
        return b"".join(struct.pack("<I", w) for w in words)
    return struct.pack("<H", sid) + b"".join(struct.pack("<I", w) for w in words)


def read_dump(path, tick, key):
    ships = []
    prefix = ("R %d " if RESOURCES else "S %d ") % tick
    with open(path) as f:
        for line in f:
            if not line.startswith(prefix):
                continue
            parts = line.split()
            sid = int(parts[2])
            words = None
            for p in parts:
                if p.startswith(key + "="):
                    words = [int(v.split(":")[0], 16) for v in p[len(key) + 1:].split(",")]
            ships.append((sid, words))
    return ships


def candidates(word, ulps):
    return [word + k for k in range(-ulps, ulps + 1) if 0 <= word + k <= 0xFFFFFFFF]


def cube(words, ulps, n):
    out = [[]]
    for c in range(n):
        out = [o + [w] for o in out for w in candidates(words[c], ulps[c])]
    return out


class Unit:
    """A ship or a group of ships sharing one candidate; first / last are list indices, the span between them is
    hashed with the candidate for the members and the dump's words for the others."""

    def __init__(self, ships, members):
        pos = {sid: i for i, (sid, _) in enumerate(ships)}
        missing = [m for m in members if m not in pos]
        if missing:
            raise KeyError("ships %s are not in the dump" % missing)
        self.members = set(members)
        idx = sorted(pos[m] for m in members)
        self.first, self.last = idx[0], idx[-1]
        self.ships = ships
        self.mine = ships[idx[0]][1]
        self.name = "+".join(str(m) for m in members)

    def span(self, cand):
        return b"".join(ship_bytes(sid, cand if sid in self.members else w)
                        for sid, w in self.ships[self.first:self.last + 1])


def report(unit, ref):
    print("ship %s ref %s" % (unit.name, " ".join("%08x" % w for w in ref)))
    print("ship %s ours %s" % (unit.name, " ".join("%08x" % w for w in unit.mine)))
    print("ship %s ref - ours (raw bits) %s" % (unit.name, " ".join("%+d" % (r - o) for r, o in zip(ref, unit.mine))))
    print("ship %s ref values %s" % (unit.name, " ".join("%.9g" % hwo_compare.f32(w) for w in ref)))


def main(argv):
    ap = argparse.ArgumentParser(description=__doc__.split("\n")[0])
    ap.add_argument("ref")
    ap.add_argument("dump")
    ap.add_argument("--tick", type=int, required=True)
    ap.add_argument("--ship", required=True, help="UNIT or UNIT,UNIT; a unit is N or N+M+... (shared words)")
    ap.add_argument("--field", default="vel", choices=sorted(FIELDS))
    ap.add_argument("--ulps", default="1000,1000,150")
    a = ap.parse_args(argv)
    global RESOURCES
    RESOURCES = (a.field == "res")
    hname, key, n = FIELDS[a.field]
    ulps = [int(u) for u in a.ulps.split(",")] + [0, 0, 0]

    ref = hwo_compare.read_hwo(a.ref)
    rec = [t for t in ref["ticks"] if t["tick"] == a.tick]
    if not rec:
        print("no T record for tick %d in %s" % (a.tick, a.ref))
        return 1
    target_hash = rec[0][hname]
    ships = read_dump(a.dump, a.tick, key)
    if not ships or any(w is None or len(w) != n for _, w in ships):
        print("no complete 'S' lines for tick %d in %s (run with /oracleShipDump all)" % (a.tick, a.dump))
        return 1
    ours = FNV_INIT
    for sid, w in ships:
        ours = fnv_bytes(ours, ship_bytes(sid, w))
    print("tick %d %s: ref %016x ours (from the dump) %016x %s" % (a.tick, hname, target_hash, ours,
                                                                    "EQUAL" if ours == target_hash else "differ"))
    try:
        units = [Unit(ships, [int(m) for m in u.split("+")]) for u in a.ship.split(",")]
    except KeyError as e:
        print(e)
        return 1
    units.sort(key=lambda u: u.first)
    if len(units) == 2 and units[0].last >= units[1].first:
        print("the two units overlap in the list")
        return 1
    fwd = FNV_INIT
    for sid, w in ships[:units[0].first]:
        fwd = fnv_bytes(fwd, ship_bytes(sid, w))
    back = target_hash
    for sid, w in reversed(ships[units[-1].last + 1:]):
        back = unfnv_bytes(back, ship_bytes(sid, w))

    found = []
    if len(units) == 1:
        u = units[0]
        if len(u.members) == 1 and n == 3:
            # meet in the middle inside the one ship: x forward, y and z backward
            table = {}
            pre = fwd if RESOURCES else fnv_bytes(fwd, struct.pack("<H", ships[u.first][0]))
            for wx in candidates(u.mine[0], ulps[0]):
                table.setdefault(fnv_bytes(pre, struct.pack("<I", wx)), []).append(wx)
            for wy in candidates(u.mine[1], ulps[1]):
                for wz in candidates(u.mine[2], ulps[2]):
                    for wx in table.get(unfnv_bytes(back, struct.pack("<II", wy, wz)), ()):
                        found.append(([wx, wy, wz],))
        else:
            for c in cube(u.mine, ulps, n):
                if fnv_bytes(fwd, u.span(c)) == back:
                    found.append((c,))
    else:
        ua, ub = units
        between = b"".join(ship_bytes(sid, w) for sid, w in ships[ua.last + 1:ub.first])
        table = {}
        for c in cube(ua.mine, ulps, n):
            table.setdefault(fnv_bytes(fnv_bytes(fwd, ua.span(c)), between), []).append(c)
        for c in cube(ub.mine, ulps, n):
            for ca in table.get(unfnv_bytes(back, ub.span(c)), ()):
                found.append((ca, c))
    print("ship %s %s: %d solution(s) within +-%s ulps" % (",".join(u.name for u in units), key, len(found), ulps[:n]))
    for sol in found[:4]:
        for u, ref_words in zip(units, sol):
            report(u, ref_words)
    return 0 if len(found) == 1 else 1


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
