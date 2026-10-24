#!/usr/bin/env python3
"""dump_outliers.py - the one-frame outlier scan (M3-R2 nis-flicker, HWI-TEST; docs/m3-r2-nis-flicker.md).

The user's report (M3-R2 item 1): "an odd 1-frame flicker where all objects were missing including the black bars ...
only the backdrop during that frame". This scans a per-frame file for frames that stand out from BOTH neighbours
while the neighbours agree with each other.

Input (python3 -I tools/dump_outliers.py FILE [FILE ..]):
  * a /catFrameSig file (harness --frame-sig: <run>/frame.sig): "F <n> tick=<t> seg=<s> dump=<d|-> sig mv=.. rl=..
    vis=.. lb=-|full|<fade> fd=.. nis=<t|-> cut=.. a=<alpha|-> ft=<ms> [bar=.. mid=.. lum=.. d1=.. d2=..]" (the
    bracketed read-back part only on the virtual clock: an attended run, tools/play.sh -- /catFrameSig <file>, and a
    --real-clock run have none, so only the bars / objects / runs tests apply there);
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
  runs    (--max-run N, default 12: a universe tick at 144 Hz is 9 frames, so a state that lasts one tick in play shows
          for up to 9-10 frames) a run of 1..N frames, inside one level segment, where the late bars are not drawn (lb -)
          while the frames on both sides of the run have them, or where the main view draws under --min-objects visible
          objects while the frames on both sides draw at least that many (a frame under a full black fade, fd=1, skips
          the main view on purpose: neither good nor bad, so a run touching one is not reported); reported as "run(<what> frames a-b)"
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


def runs(frames, a):
    out = []
    def scan_runs(bad, good, what):
        i, n = 1, len(frames)
        while i < n - 1:
            if bad(frames[i]) and good(frames[i - 1]):
                j = i
                while j < n and bad(frames[j]) and j - i < a.max_run:
                    j += 1
                if j < n and good(frames[j]) and j - i <= a.max_run:
                    seg = frames[i - 1].get('seg')
                    if all(frames[k].get('seg') == seg for k in range(i - 1, j + 1)):
                        out.append((frames[i], ['run(%s frames %d-%d, %d long)' % (what, frames[i]['n'],
                                                                                    frames[j - 1]['n'], j - i)]))
                i = j if j > i else i + 1
            else:
                i += 1
    def sig(f):
        return f.get('sig') or {}
    scan_runs(lambda f: sig(f).get('lb', '-') == '-' and 'lb' in sig(f),
              lambda f: sig(f).get('lb', '-') != '-', 'bars')
    def objs(f):
        s = sig(f)
        v = num(s, 'vis')
        if v is not None and num(s, 'fd') is not None and num(s, 'fd') >= 1.0:
            return None                             # a full black fade (the NIS's fadeTo 100%): the main view is skipped on purpose
        return None if v is None else (v if num(s, 'mv') else 0.0)
    scan_runs(lambda f: objs(f) is not None and objs(f) < a.min_objects,
              lambda f: objs(f) is not None and objs(f) >= a.min_objects, 'objects')
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
    ap.add_argument('--max-run', type=int, default=12)
    ap.add_argument('--oneline', action='store_true')
    a = ap.parse_args()
    for path in a.files:
        frames = parse(path)
        res = scan(frames, a)
        rr = runs(frames, a)
        rb = sum(1 for f in frames if f.get('sig') and 'd1' in f['sig'])
        segs = len(set(f.get('seg') for f in frames))
        counts = {t: sum(1 for _, w in res if any(x.startswith(t) for x in w))
                  for t in ('image', 'bars', 'objects', 'camera')}
        counts['bars'] += sum(1 for _, w in res if any(x.startswith('barpixels') for x in w)
                              and not any(x.startswith('bars(') for x in w))
        first = ('%d@%s/s%s' % (res[0][0]['n'], res[0][0].get('tick', '?'), res[0][0].get('seg', '0'))) if res else 'none'
        firstrun = ('%d@%s/s%s' % (rr[0][0]['n'], rr[0][0].get('tick', '?'), rr[0][0].get('seg', '0'))) if rr else 'none'
        line = ('frames=%d readback=%d segments=%d outliers=%d (image %d, bars %d, objects %d, camera %d) first=%s; '
                'short runs=%d (bars %d, objects %d) first=%s' % (
            len(frames), rb, segs, len(res), counts['image'], counts['bars'], counts['objects'], counts['camera'], first,
            len(rr), sum(1 for _, w in rr if 'run(bars' in w[0]), sum(1 for _, w in rr if 'run(objects' in w[0]),
            firstrun))
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
        for fr, why in rr:
            i = frames.index(fr)
            print('RUN frame=%d tick=%s seg=%s dump=%s: %s' % (fr['n'], fr.get('tick'), fr.get('seg', '-'),
                                                              fr.get('dump', '-'), '; '.join(why)))
            print('    before ' + brief(frames[i - 1]))
            print('    first  ' + brief(fr))
        print(line)
    return 0


if __name__ == '__main__':
    sys.exit(main())
