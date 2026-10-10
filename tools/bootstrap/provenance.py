#!/usr/bin/env python3
"""Generate src/PROVENANCE.md from the import manifest and the diffstat.
Usage: provenance.py <manifest.json> <diffstat.json> <ad1bb82_root> <out.md> <hunks_line>
"""
import collections
import difflib
import json
import os
import sys

manifest = json.load(open(sys.argv[1]))
diff = json.load(open(sys.argv[2]))
orig = sys.argv[3]
out = sys.argv[4]
wt_src = os.path.join(os.path.dirname(os.path.abspath(out)))


def norm(path):
    data = open(path, 'rb').read().decode('latin-1')
    return [l.rstrip() for l in data.replace('\r\n', '\n').replace('\r', '\n').split('\n')]


# index of every original source file by lowercase basename, to detect cross-directory moves
orig_index = collections.defaultdict(list)
for root, _, files in os.walk(os.path.join(orig, 'src')):
    for f in files:
        orig_index[f.lower()].append(os.path.join(root, f))

rows = diff['rows']
moved = []
for r in rows:
    if r['status'] == 'added' and r['dir'] in ('Game', 'Ships', 'SDL', 'ThirdParty') and r['file'].endswith(('.c', '.h', '.C')):
        cands = orig_index.get(os.path.basename(r['file']).lower(), [])
        if cands:
            a, b = norm(cands[0]), norm(os.path.join(wt_src, r['file']))
            add = rem = 0
            for op, i1, i2, j1, j2 in difflib.SequenceMatcher(None, a, b, autojunk=False).get_opcodes():
                if op != 'equal':
                    rem += i2 - i1
                    add += j2 - j1
            r['status'] = 'moved'
            r['from'] = os.path.relpath(cands[0], orig)
            r['add'], r['del'] = add, rem
            moved.append(r)

moved_from = {m['from'].lower() for m in moved}
rows = [r for r in rows if not (r['status'] == 'deleted' and r['file'].lower() in moved_from)]
summary = collections.OrderedDict()
for r in rows:
    s = summary.setdefault(r['dir'], collections.Counter())
    s['files'] += 1
    s[r['status']] += 1
    s['+'] += r['add']
    s['-'] += r['del']

L = []
w = L.append
w('# src/ provenance')
w('')
w('**Licence: everything in this directory is derived from Relic Entertainment\'s Homeworld 1 source and is bound by the Relic End-User License Agreement (REULA), non-commercial use only. Full text: [`COPYING.REULA`](COPYING.REULA); summary at the top of [`README.md`](README.md).**')
w('')
w('Written by the M2-R1 bootstrap (2026-10-10). Regenerate the lists with the commands in `docs/m2-r1-bootstrap.md` if files are re-imported.')
w('')
w('## Commits')
w('')
w('| Role | Repository | Commit | What it is |')
w('|---|---|---|---|')
w('| **Base** (licence origin, comparison point) | Gardens of Kadesh, https://gitlab.com/gardens-of-kadesh/gardens-of-kadesh | `ad1bb82ca3c62dcc8be9d35f2e836d0bb7cf7caa` (root commit) | "Original Relic code, as published in 2003": Relic\'s Homeworld 1 source exactly as released, CRLF line endings, lowercase file names, Win32/MSVC only |')
w('| **Donor** (what is actually built) | same | `3af31fbca2acfb4e791386a041087b340e77892f` (GoK `master` HEAD on 2026-10-10; identical to `reference/gardens-of-kadesh`) | the HomeworldSDL / Gardens of Kadesh port of that source to SDL2 + OpenGL, 64-bit and Linux |')
w('')
w('How the files were obtained: `ad1bb82` from the GitLab archive endpoint (`.../-/archive/ad1bb82/gardens-of-kadesh-ad1bb82.tar.gz`, pax comment `ad1bb82ca3c6...`), extracted to `original/scratch-m2/gok-export/` (the reference clone is shallow and does not contain `ad1bb82`); `3af31fb` from the working tree of the read-only clone `reference/gardens-of-kadesh` (`.git/HEAD` -> `refs/heads/master` = `3af31fb`; a fresh full clone in `original/scratch-m2/gok-hist-bootstrap` shows `origin/master` at the same commit).')
w('')
w('## Decision: whole GoK `src/` instead of ad1bb82 + hand-picked hunks')
w('')
w('The brief asked for Relic\'s `ad1bb82` tree plus only the GoK portability fixes the build needs, falling back to GoK\'s whole `src/` if that meant hundreds of hand-picked hunks. Measured before importing: ' + sys.argv[5] + ' Relic\'s headers do not even compile outside MSVC (`ad1bb82:src/Game/TYPES.H:11-13` is `#if defined(_WIN32) && defined(_M_IX86)` ... `#error`), `udword` is `unsigned long` (8 bytes on LP64), the task scheduler and ETG calls are x86 inline asm, and the GoK SDL layer depends on GoK\'s reworked headers (`memsize`, `bool32`, `*_disk` loaders). So the fallback was taken: **GoK `src/` at `3af31fb` is imported whole** (minus autotools files), and `ad1bb82` is kept as the comparison base (diffstat below) and, for `Win32/`, as an in-tree reference copy. This is recorded for the user in `cycle/STATE.md` "Decisions the user owns".')
w('')
w('Consequence: "unmodified HW1" here means *no Cataclysm changes*. The tree carries every change HomeworldSDL/GoK made since 2003, not only portability fixes. Behavioural ones that matter for a 1:1 recreation (each verified in this tree):')
w('')
w('| GoK change | Where | Effect vs Relic 1.05 |')
w('|---|---|---|')
w('| 64 Hz universe update by default | `Game/Universe.c:75` `UNIVERSE_UPDATE_RATE_SHIFT = 2`; `/vanilla` restores 16 Hz (`SDL/main.c:848,2195`) | simulation rate x4 unless `/vanilla` |')
w('| Portable save format | `Game/SaveGame.h:16-22` (`SAVE_VERSION_NUMBER` = `HWSDL_2` 0x10ad0029) | Relic saves (0x10ad0027) not loadable |')
w('| HWSDL options in `Homeworld.cfg` | `SDL/utility.c:778-783` (`CollectResourcesAtEndOfMission`, `ShipsAlwaysUseOwnerColors`, `TimeCompressionFactor`) | off/neutral by default except time compression factor 8 |')
w('| Float-to-int, x87 vs SSE, task tick from ms timer | see `docs/hw1-port-lessons.md` A3, A7 | numerics differ from the 1999/2003 binary |')
w('| Remaster `homeworld.big` support, widescreen, MSAA, vsync, stb_image, FFmpeg movies, SDL_net LAN (compiled out) | `docs/hw1-port-lessons.md` A9 | presentation / platform |')
w('')
w('## Local changes made by Homeworld-Infection (all build files; no C source edited)')
w('')
w('| File | Change |')
w('|---|---|')
w('| `../meson.build`, `../meson_options.txt` | new, adapted from GoK `meson.build`/`meson_options.txt`: project and executable `hwi-engine`; `-Werror` via meson\'s `werror` option (default true); `-Wno-*` list through `get_supported_arguments()`, clang-only extras for clang; option `legacy_warnings`; emscripten/darwin branches, `-Ddemo` and GoK\'s optional tools dropped |')
w('| `meson.build` (this dir) | same content as GoK `src/meson.build` |')
w('| `tools/kas2c/meson.build` | GoK `tools/kas2c/meson.build` + `include_directories(\'..\')`, because `parser.y:18` includes `"../../src/Game/Types.h"`, a path written for GoK\'s `tools/kas2c` location |')
w('| `README.md`, `PROVENANCE.md` | new |')
w('')
w('Not imported from GoK `3af31fb`: every `Makefile.am`, `tools/kas2c/kas2c-macOS-compile.sh`, `src/homeworld.c` (autotools placeholder), `src/obj/` (Relic\'s prebuilt codec `.obj`/`.o`, replaced by the reverse-engineered C in `SDL/`), and everything outside `src/` except `tools/kas2c` and `COPYING.REULA`. Not imported from `ad1bb82`: binary files in `src/Win32` (`Homeworld.ico`, `HomeworldLogo.ico`, `res.geo`, `vc50.pdb`, `vssver.scc`).')
w('')
w('## Diffstat of src/ against ad1bb82')
w('')
w('Line counts are after normalising line endings and trailing whitespace (Relic\'s files are CRLF; a raw diff would count every line). Path mapping: `ad1bb82:src/Game` -> `Game/`, `src/Ships` -> `Ships/`, `src/Win32` -> `SDL/` (GoK\'s rename) and -> `Win32/` (reference copy), `src/SinglePlayer` -> `Missions/SinglePlayer/`, `src/Tutorials` -> `Missions/Tutorials/`, `tools/win32/KAS/KAS2C.{c,h,l,y}` -> `tools/kas2c/{kas2c.c,kas2c.h,lexer.l,parser.y}`. Files match case-insensitively (GoK renamed `universe.c` to `Universe.c` etc.); "moved" = no same-name file in the mapped original directory but one elsewhere in `ad1bb82:src/`. Only code files are counted (`.c .h .cpp .kas .kash .l .y .def .rc`, plus new `meson.build`/text files). Deletions count the lines of files that exist only in `ad1bb82`.')
w('')
w('| Directory | Files | Identical | Modified | Moved in | Added | Deleted (in ad1bb82 only) | Lines + | Lines - |')
w('|---|---|---|---|---|---|---|---|---|')
tot = collections.Counter()
for k, s in summary.items():
    w(f"| `{k}` | {s['files']} | {s['identical']} | {s['modified']} | {s['moved']} | {s['added']} | {s['deleted']} | {s['+']:,} | {s['-']:,} |")
    tot.update(s)
w(f"| **total** | {tot['files']} | {tot['identical']} | {tot['modified']} | {tot['moved']} | {tot['added']} | {tot['deleted']} | {tot['+']:,} | {tot['-']:,} |")
w('')
w('(`Win32/` is byte-identical to `ad1bb82`. Relic `src/Win32` files that GoK deleted are not counted as deletions under `SDL/`, since `Win32/` keeps them.)')
w('')
w('Most-changed files (normalised lines changed, + / -):')
w('')
top = sorted([r for r in rows if r['status'] in ('modified', 'moved')], key=lambda r: -(r['add'] + r['del']))[:25]
w('| File | + | - |')
w('|---|---|---|')
for r in top:
    w(f"| `{r['file']}` | {r['add']:,} | {r['del']:,} |")
w('')
w('Moved into `Game/`, `Ships/`, `SDL/` or `ThirdParty/` from another original directory (not counted again as deletions):')
w('')
for r in sorted(moved, key=lambda r: r['file']):
    w(f"- `{r['file']}` <- `ad1bb82:{r['from']}` (+{r['add']:,} / -{r['del']:,})")
w('')
w('New in GoK (no same-name file anywhere in `ad1bb82:src/`):')
w('')
w(', '.join(f"`{r['file']}`" for r in sorted(rows, key=lambda r: r['file']) if r['status'] == 'added'))
w('')
w('In `ad1bb82` but gone from GoK `Game/`/`Ships/`/`Missions/` (merged, renamed or moved to `ThirdParty/`):')
w('')
w(', '.join(f"`{r['file']}`" for r in sorted(rows, key=lambda r: r['file']) if r['status'] == 'deleted'))
w('')
w('## Files taken from each commit')
w('')
by = collections.defaultdict(lambda: collections.defaultdict(list))
for f in manifest['files']:
    p = f['path'][len('src/'):] if f['path'].startswith('src/') else f['path']
    d, n = os.path.split(p)
    by[f['source']][d or '.'].append(n)
for src_label, title in (('gok@3af31fb', 'From GoK `3af31fb` (built)'), ('relic@ad1bb82', 'From Relic `ad1bb82` (reference only, not built)')):
    n = sum(len(v) for v in by[src_label].values())
    w(f'### {title}: {n} files')
    w('')
    for d in sorted(by[src_label]):
        names = sorted(by[src_label][d], key=str.lower)
        w(f"- `{d}/` ({len(names)}): " + ', '.join(names))
    w('')
w('Source paths: GoK files keep their GoK path under `src/` (GoK `src/X` -> `X`), except GoK `tools/kas2c/*` -> `tools/kas2c/*` and GoK `COPYING.REULA` -> `COPYING.REULA`. Relic files: `ad1bb82:src/Win32/*` -> `Win32/*`, byte for byte (CRLF kept). The import manifest (per-file source path and sha256) and the diffstat data are kept outside git in `original/scratch-m2/bootstrap-logs/` (`manifest.json`, `diffstat.json`). Every imported file is byte-identical to its source except `tools/kas2c/meson.build` (see Local changes).')
w('')
open(out, 'w').write('\n'.join(L))
print('wrote', out, dict(tot))
