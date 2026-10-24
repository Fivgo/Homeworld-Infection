#!/usr/bin/env python3
"""dump_outliers.py - the one-frame outlier scan (M3-R2 nis-flicker, HWI-TEST; docs/m3-r2-nis-flicker.md).

The user's report (M3-R2 item 1): "an odd 1-frame flicker where all objects were missing including the black bars ...
only the backdrop during that frame". This scans a per-frame file for frames that stand out from BOTH neighbours
while the neighbours agree with each other.

Input (python3 -I tools/dump_outliers.py FILE [FILE ..]):
  * a /catFrameSig file (harness --frame-sig: <run>/frame.sig): "F <n> tick=<t> seg=<s> dump=<d|-> sig mv=.. rl=..
    vis=.. lb=-|full|<fade> fd=.. nis=<t|-> cut=.. a=<alpha|-> [bar=.. mid=.. lum=.. d1=.. d2=..]";
  * a motion / nis dump (MotionTest.c), with or without the " sig ..." columns after crc=: without them only the camera
    test runs (the eye / look point of the frame against its neighbours), with them every test.

The tests (frame k, neighbours k-1 and k+1; a test needs all three frames in one level segment):
  image   the 16x16-block luma thumbnail: d1(k) >= --image (default 4.0) and d1(k+1) >= --image (k differs from both
          neighbours) and d2(k+1) (k+1 against k-1) <= --ratio (default 0.35) x min(d1(k), d1(k+1)): the neighbours agree
  bars    the late letterbox bars drawn at k-1 and k+1 but not at k (lb), or the read-back's bar-row pixels at k above
          --bar-pixels (default 2000) more than at both neighbours
  objects the main view drawn at both neighbours with --min-objects (default 2) or more visible objects each, and at k
          none drawn (mv=0) or under half of the smaller neighbour's (vis)
  camera  the eye or look point at k more than --camera (default 50 units) from both neighbours' while they are within
          a third of that of each other (a one-frame pose)
Output: one line per outlier with its frame, tick, segment, dump frame, the tests it failed and the three frames'
values; then a summary. --oneline prints only "frames=N readback=N segments=N outliers=K (image a, bars b, objects c,
camera d) first=<frame>@<tick>/s<seg>|none". Exit status 0 always (a scan, not a gate; the harness prints the line).
"""

import argparse
import math
import re
import sys

TOKEN = re.compile(r'(\w+)=(\S+)')


def parse(path):
    frames = []
    with open(path, 'r', errors='replace') as f:
        for line in f:
            if not line.startswith('F '):
                continue
            parts = line.split()
            fr = {'n': int(parts[1]), 'line': line.rstrip('\n')}
            head, _, sig = line.partition(' sig ')
            for k, v in TOKEN.findall(head):
                if k in ('tick', 'seg', 'dump', 'crc'):
                    fr[k] = v
                elif k in ('eye', 'look'):
                    try:
                        fr[k] = tuple(float(x) for x in v.split(','))
                    except ValueError:
                        pass
            if sig:
                fr['sig'] = dict(TOKEN.findall(sig))
            frames.append(fr)
    return frames


def num(d, k):
    v = d.get(k)
    if v is None or v == '-':
        return None
    try:
        return float(v)
    except ValueError:
        return None


def dist(a, b):
    return math.sqrt(sum((x - y) ** 2 for x, y in zip(a, b)))


def scan(frames, a):
    out = []
    for i in range(1, len(frames) - 1):
        p, k, n = frames[i - 1], frames[i], frames[i + 1]
        seg = k.get('seg')
        if p.get('seg') != seg or n.get('seg') != seg:
            continue
        why = []
        ps, ks, ns = p.get('sig'), k.get('sig'), n.get('sig')
        if ps is not None and ks is not None and ns is not None:
            d1k, d1n, d2n = num(ks, 'd1'), num(ns, 'd1'), num(ns, 'd2')
            if d1k is not None and d1n is not None and d2n is not None:
                if d1k >= a.image and d1n >= a.image and d2n <= a.ratio * min(d1k, d1n):
                    why.append('image(d1=%.2f,%.2f d2=%.2f)' % (d1k, d1n, d2n))
            lp, lk, ln = ps.get('lb', '-'), ks.get('lb', '-'), ns.get('lb', '-')
            if lp != '-' and ln != '-' and lk == '-':
                why.append('bars(lb %s,%s,%s)' % (lp, lk, ln))
            bp, bk, bn = num(ps, 'bar'), num(ks, 'bar'), num(ns, 'bar')
            if bp is not None and bk is not None and bn is not None and bk - max(bp, bn) > a.bar_pixels:
                why.append('barpixels(%d,%d,%d)' % (bp, bk, bn))
            mp, mk, mn = num(ps, 'mv'), num(ks, 'mv'), num(ns, 'mv')
            vp, vk, vn = num(ps, 'vis'), num(ks, 'vis'), num(ns, 'vis')
            if mp and mn and vp is not None and vn is not None and min(vp, vn) >= a.min_objects:
                if not mk or (vk is not None and vk < 0.5 * min(vp, vn)):
                    why.append('objects(mv %s,%s,%s vis %s,%s,%s)' % (ps.get('mv'), ks.get('mv'), ns.get('mv'),
                                                                     ps.get('vis'), ks.get('vis'), ns.get('vis')))
        for key in ('eye', 'look'):
            if key in p and key in k and key in n:
                dp, dn, pn = dist(k[key], p[key]), dist(k[key], n[key]), dist(p[key], n[key])
                if dp > a.camera and dn > a.camera and pn < a.camera / 3.0:
                    why.append('camera(%s %.1f,%.1f vs %.1f)' % (key, dp, dn, pn))
                    break
        if why:
            out.append((k, why))
    return out


def brief(fr):
    s = fr.get('sig') or {}
    keys = ('mv', 'vis', 'lb', 'fd', 'nis', 'cut', 'a', 'bar', 'mid', 'lum', 'd1', 'd2')
    return 'F%d t%s %s' % (fr['n'], fr.get('tick', '?'), ' '.join('%s=%s' % (k, s[k]) for k in keys if k in s))


def main():
    ap = argparse.ArgumentParser(description=__doc__.split('\n')[0])
    ap.add_argument('files', nargs='+')
    ap.add_argument('--image', type=float, default=4.0)
    ap.add_argument('--ratio', type=float, default=0.35)
    ap.add_argument('--bar-pixels', type=float, default=2000.0)
    ap.add_argument('--min-objects', type=float, default=2.0)
    ap.add_argument('--camera', type=float, default=50.0)
    ap.add_argument('--oneline', action='store_true')
    a = ap.parse_args()
    for path in a.files:
        frames = parse(path)
        res = scan(frames, a)
        rb = sum(1 for f in frames if f.get('sig') and 'd1' in f['sig'])
        segs = len(set(f.get('seg') for f in frames))
        counts = {t: sum(1 for _, w in res if any(x.startswith(t) for x in w))
                  for t in ('image', 'bars', 'objects', 'camera')}
        counts['bars'] += sum(1 for _, w in res if any(x.startswith('barpixels') for x in w)
                              and not any(x.startswith('bars(') for x in w))
        first = ('%d@%s/s%s' % (res[0][0]['n'], res[0][0].get('tick', '?'), res[0][0].get('seg', '0'))) if res else 'none'
        line = 'frames=%d readback=%d segments=%d outliers=%d (image %d, bars %d, objects %d, camera %d) first=%s' % (
            len(frames), rb, segs, len(res), counts['image'], counts['bars'], counts['objects'], counts['camera'], first)
        if a.oneline:
            print(('%s: ' % path if len(a.files) > 1 else '') + line)
            continue
        print('# %s' % path)
        for fr, why in res:
            i = frames.index(fr)
            print('OUTLIER frame=%d tick=%s seg=%s dump=%s: %s' % (fr['n'], fr.get('tick'), fr.get('seg', '-'),
                                                                     fr.get('dump', '-'), '; '.join(why)))
            for g in (frames[i - 1], fr, frames[i + 1]):
                print('    ' + brief(g))
        print(line)
    return 0


if __name__ == '__main__':
    sys.exit(main())
