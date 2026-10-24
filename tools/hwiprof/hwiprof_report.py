#!/usr/bin/env python3
"""hwiprof_report.py: symbolise and summarise a tools/hwiprof/hwiprof.c sample file (HWI-TEST, M3-R2 mesh-batching).

Usage: python3 -I tools/hwiprof/hwiprof_report.py <prof.bin> [--within FUNC] [--top N] [--lines] [--class]
  <prof.bin>.maps must sit next to it (the shim writes both). Symbols come from addr2line (binutils) for the
  executable (built with -g: build/prof, the release flags plus -g) and from nm -D for the shared libraries.
  [audit: the executable is the mapping whose path ends in "hwi-engine": a profiled binary under any other file name
  is not symbolised (no engine frame, "0 of N within"), so copy it as .../hwi-engine as prof-run.sh does]
  --within FUNC  only the samples whose stack holds FUNC (e.g. rndRenderTask: the render task's frames)
  --lines        the engine call sites (file:line of the innermost engine frame) of the samples
  --class        the GL call classes (the call site's source text sorted into vertex / colour / material / bind /
                 matrix / state / other GL / engine CPU)
  --breakdown    the frame by part (BREAKDOWN below: each sample goes to the first part whose functions its stack holds)
"""
import bisect
import collections
import os
import re
import struct
import subprocess
import sys


def load(path):
    samples = []
    with open(path, 'rb') as f:
        if f.read(8) != b'HWIPROF1':
            sys.exit('not a hwiprof file')
        data = f.read()
    o = 0
    while o + 4 <= len(data):
        (d,) = struct.unpack_from('<I', data, o)
        o += 4
        samples.append(struct.unpack_from('<%dQ' % d, data, o))
        o += 8 * d
    return samples


def load_maps(path):
    maps = []
    base = {}
    for line in open(path):
        p = line.split()
        if len(p) < 6:
            continue
        a, b = (int(x, 16) for x in p[0].split('-'))
        off = int(p[2], 16)
        name = p[5]
        if name not in base:
            base[name] = a - off
        if 'x' in p[1]:
            maps.append((a, b, name))
    maps.sort()
    return maps, base


class Sym:
    def __init__(self, maps, base, exe):
        self.maps = maps
        self.starts = [m[0] for m in maps]
        self.base = base
        self.exe = exe
        self.cache = {}
        self.dyn = {}

    def module(self, addr):
        i = bisect.bisect_right(self.starts, addr) - 1
        if i >= 0 and self.maps[i][0] <= addr < self.maps[i][1]:
            return self.maps[i][2]
        return '?'

    def resolve_exe(self, addrs):
        todo = sorted(set(a for a in addrs if a not in self.cache))
        if not todo:
            return
        b = self.base[self.exe]
        for k in range(0, len(todo), 4000):
            chunk = todo[k:k + 4000]
            out = subprocess.run(['addr2line', '-f', '-e', self.exe] + ['%x' % (a - b) for a in chunk],
                                 capture_output=True, text=True).stdout.splitlines()
            for j, a in enumerate(chunk):
                fn = out[2 * j]
                loc = out[2 * j + 1]
                loc = re.sub(r' \(discriminator \d+\)', '', loc)
                self.cache[a] = (fn, os.path.basename(loc.split(':')[0]) + ':' + loc.split(':')[-1] if ':' in loc else loc)

    def dynsyms(self, lib):
        if lib not in self.dyn:
            syms = []
            try:
                out = subprocess.run(['nm', '-D', '--defined-only', lib], capture_output=True, text=True).stdout
                for line in out.splitlines():
                    p = line.split()
                    if len(p) == 3 and p[1] in 'TtWi':
                        syms.append((int(p[0], 16), p[2]))
            except OSError:
                pass
            syms.sort()
            self.dyn[lib] = syms
        return self.dyn[lib]

    def lib_name(self, addr, lib):
        syms = self.dynsyms(lib)
        off = addr - self.base.get(lib, 0)
        i = bisect.bisect_right([s[0] for s in syms], off) - 1 if syms else -1
        return syms[i][1] if i >= 0 and off - syms[i][0] < 0x4000 else '?'


GL_CLASSES = [
    ('vertex', re.compile(r'glVertex|glArrayElement|glTexCoord|glNormal|glDrawElements|glDrawArrays|glBegin|glEnd\b|'
                          r'glVertexPointer|glNormalPointer|glColorPointer|glTexCoordPointer|ClientState|meshColour'
                          r'|glBufferData|glBindBuffer|GLVERTEX')),
    ('colour', re.compile(r'glColor')),
    ('material', re.compile(r'glMaterial|glLightModel|glShadeModel|glLight\b|glLightfv|shSetMaterial')),
    ('bind', re.compile(r'glBindTexture|trMakeCurrent|glTexEnv|rndTextureEnvironment|glTexParameter')),
    ('matrix', re.compile(r'glPushMatrix|glPopMatrix|glMultMatrix|glLoadMatrix|glLoadIdentity|glTranslate|glRotate|'
                          r'glScale|glMatrixMode|glGet')),
    ('state', re.compile(r'glEnable|glDisable|glBlendFunc|glDepth|glCullFace|glPolygonMode|rndBackFaceCullEnable|'
                         r'rndLightingEnable|rndTextureEnable|rndAdditiveBlends|glAlphaFunc|glFog')),
    ('flip/finish', re.compile(r'glFinish|SwapWindow|SwapBuffers|rndFlush|glFlush|ReadPixels')),
]


# the frame's parts for --breakdown, in priority order (a sample counts once, for the first part its stack matches)
BREAKDOWN = [
    ('flip / glFinish', ['SDL_GL_SwapWindow', 'rndFlush', 'catLodStatsFrameEnd']),
    ('mesh: CPU lighting', ['shColourSet0', 'shColourLit0', 'shSpecularColour', 'cat_shColourAddGlow', 'cat_shGlowUpdate',
                            'meshSpecColour']),
    ('mesh: material / texture switch', ['meshCurrentMaterialDefault', 'cat_meshCurrentMaterialAlt', 'meshAlmostCurrentMaterial',
                                         'meshCurrentMaterialTex', 'cat_meshMaterialSwitch']),
    ('mesh: batched draw (glDrawArrays)', ['meshBatchDraw']),
    ('mesh: corner submission (immediate calls / recording)', ['meshObjectRenderLit', 'cat_meshObjectRenderLitBatched',
                                                               'meshSpecObjectRender', 'cat_meshSpecObjectRenderBatched']),
    ('mesh: other renderers (unlit / wireframe / warped / morphed)', ['meshObjectRender', 'cat_meshObjectRenderWarped',
                                                                      'meshMorphedObjectRender']),
    ('mesh: hierarchy, matrices, per-ship setup', ['cat_rndShipMeshDraw', 'meshRender', 'meshRenderShipHierarchy']),
    ('trails and engine glows', ['trailDraw', 'trailDrawCapitalGlow', 'trailLineSequence']),
    ('nav lights', ['RenderNAVLights']),
    ('background (BTG) and stars', ['btgRender', 'rndRenderAHomeworld', 'rndDrawStars']),
    ('rest of the main view (object list, effects, culling)', ['rndMainViewRenderFunction']),
    ('2D: regions, HUD, text', ['regFunctionsDraw']),
    ('rest of the render task', ['rndRenderTask']),
]


def main():
    args = sys.argv[1:]
    if not args:
        sys.exit(__doc__)
    path = args[0]
    within = None
    top = 40
    if '--within' in args:
        within = args[args.index('--within') + 1]
    if '--top' in args:
        top = int(args[args.index('--top') + 1])
    samples = load(path)
    maps, base = load_maps(path + '.maps')
    exe = None
    for _a, _b, n in maps:
        if n.endswith('hwi-engine'):
            exe = n
    sym = Sym(maps, base, exe)
    addrs = set()
    for s in samples:
        for k, a in enumerate(s):
            if sym.module(a) == exe:
                addrs.add(a if k == 0 else a - 1)
    sym.resolve_exe(addrs)

    def frames(s):
        out = []
        for k, a in enumerate(s):
            m = sym.module(a)
            if m == exe:
                fn, loc = sym.cache[a if k == 0 else a - 1]
                out.append(('E', fn, loc))
            else:
                out.append(('L', os.path.basename(m), None))
        return out

    total_all = len(samples)
    chosen = []
    for s in samples:
        fr = frames(s)
        if within is None or any(f[0] == 'E' and f[1] == within for f in fr):
            chosen.append(fr)
    n = len(chosen)
    print('samples: %d of %d%s' % (n, total_all, (' within %s' % within) if within else ''))
    if n == 0:
        return
    incl = collections.Counter()
    selfc = collections.Counter()
    site = collections.Counter()
    for fr in chosen:
        seen = set()
        for f in fr:
            if f[0] == 'E' and f[1] not in seen:
                seen.add(f[1])
                incl[f[1]] += 1
        # self: the innermost engine frame, with the library the PC is in (if any)
        lib = fr[0][1] if fr[0][0] == 'L' else None
        inner = next((f for f in fr if f[0] == 'E'), None)
        key = (inner[1] if inner else '<no engine frame>', lib or '-')
        selfc[key] += 1
        site[(inner[1] if inner else '-', inner[2] if inner else '-', lib or '-')] += 1
    print('\n== inclusive (share of the chosen samples) ==')
    for fn, c in incl.most_common(top):
        print('%6.2f%%  %6d  %s' % (100.0 * c / n, c, fn))
    print('\n== self: innermost engine function / library the PC was in ==')
    for (fn, lib), c in selfc.most_common(top):
        print('%6.2f%%  %6d  %-40s %s' % (100.0 * c / n, c, fn, lib))
    if '--breakdown' in args:
        part = collections.Counter()
        for fr in chosen:
            names = set(f[1] for f in fr if f[0] == 'E')
            for label, fns in BREAKDOWN:
                if names.intersection(fns):
                    part[label] += 1
                    break
            else:
                part['other'] += 1
        print('\n== breakdown (each sample once, first matching part) ==')
        for label, _f in BREAKDOWN + [('other', [])]:
            if part[label]:
                print('%6.2f%%  %6d  %s' % (100.0 * part[label] / n, part[label], label))
    if '--lines' in args or '--class' in args:
        srcroot = os.path.join(os.path.dirname(os.path.abspath(__file__)), '..', '..', 'src')
        srcfiles = {}
        for dp, _dn, fns in os.walk(srcroot):
            if 'reference' in dp:
                continue
            for f in fns:
                srcfiles.setdefault(f, os.path.join(dp, f))

        def text(loc):
            try:
                f, ln = loc.rsplit(':', 1)
                with open(srcfiles[f], errors='replace') as fh:
                    return fh.readlines()[int(ln) - 1].strip()
            except (KeyError, ValueError, IndexError, OSError):
                return ''
        if '--lines' in args:
            print('\n== call sites (innermost engine frame: file:line, the library the PC was in) ==')
            for (fn, loc, lib), c in site.most_common(top):
                print('%6.2f%%  %6d  %-28s %-22s %-24s %s' % (100.0 * c / n, c, fn, loc, lib, text(loc)[:70]))
        if '--class' in args:
            cls = collections.Counter()
            for (fn, loc, lib), c in site.items():
                t = text(loc)
                k = None
                for name, rx in GL_CLASSES:
                    if rx.search(t):
                        k = name
                        break
                if k is None:
                    k = 'other GL / driver' if lib != '-' else 'engine CPU'
                cls[k] += c
            print('\n== classes ==')
            for k, c in cls.most_common():
                print('%6.2f%%  %6d  %s' % (100.0 * c / n, c, k))


if __name__ == '__main__':
    main()
