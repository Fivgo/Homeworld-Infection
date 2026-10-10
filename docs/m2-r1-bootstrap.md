<!-- done: lessons, import, licence, meson, build-debug, build-release, write-up -->
# M2-R1 item 4: source-tree bootstrap

Date: 2026-10-10. Worktree `.claude/worktrees/m2`, branch `m2` (HEAD `4363dff` when built). Spec: `cycle/M2_BRIEF.md` R1 item 4.

**Result.** `src/` now holds the Homeworld 1 engine with no Cataclysm changes, and it builds on Linux as `hwi-engine`: debug (ASan/UBSan) and release, both with **0 errors and 0 warnings under `-Werror`**, each in about 11 s from clean on this 24-thread machine. Both binaries link and start (usage path, no data, no window). A clang build also passes. The minimal-donor import was measured at 14,257 diff hunks and abandoned, so the imported tree is **GoK's whole `src/` at `3af31fb`**. That is a decision for the user (see Decisions).

| | Debug | Release |
|---|---|---|
| Command | `tools/build.sh --debug --clean` | `tools/build.sh --release --clean` |
| meson options | `--buildtype=debug -Db_sanitize=address,undefined` (`-O0 -g`, `HW_BUILD_FOR_DEBUGGING`) | `--buildtype=release -Db_sanitize=none` (`-O3`, `HW_BUILD_FOR_DISTRIBUTION`) [M2-R2 oracle: `--release` is now `-Doptimization=2 -Ddebug=false -Db_sanitize=none` (`-O2`), and every build type adds `-fno-strict-aliasing -ffp-contract=off -fno-fast-math`; docs/m2-r2-oracle.md "flags"] |
| Output | `build/debug/hwi-engine` | `build/release/hwi-engine` |
| Size | **74,870,832 B** (`size`: text 41,178,013, data 23,887,008, bss 2,225,120) | **5,157,976 B** (text 4,193,125, data 232,408, bss 1,685,376) |
| `file` | ELF 64-bit LSB pie, x86-64, dynamically linked, with debug_info, not stripped | ELF 64-bit LSB pie, x86-64, dynamically linked, not stripped |
| sha256 | `2f7c7f1e36dc19a0aabcf2afe0f26ebc2d93202a3fbb4f9e1c580ad57249ab3d` | `0024b0f6816841c6ad8d76abb3abecca84a68a91b6186251ce4fabebad53b297` |
| Build time, clean, incl. `meson setup` | real 11.2 s, user 1 m 46 s | real 10.9 s, user 2 m 02 s |
| No-op rebuild | | real 0.66 s |
| Ninja steps | 292/292 | 292/292 |
| Errors / warnings | 0 / 0 (`-Werror` on) | 0 / 0 (`-Werror` on) |
| Start test | exit 0, usage printed (109 lines), no ASan/UBSan report | exit 0, usage printed (55 lines) |

Cross-check: the release size equals the GoK release build in `docs/hw1-port-lessons.md` B4 byte for byte (5,157,976 B), and the clang build below equals its clang build (4,770,080 B). Both were built from the same donor commit with the same flags, so this is consistent with the import being complete and unaltered. Byte equality of the code was not checked; at least the embedded build-version string differs (`4363dff+` here, `3af31fb` there).

## Decisions (for the user; for integrate to copy into cycle/STATE.md)

1. **GoK's whole `src/` at `3af31fb` was imported, not `ad1bb82` plus hand-picked portability hunks.** Measured first (`tools/bootstrap/hunks.py`, below): Relic's `src/Game` + `src/Ships` against GoK's, 361 files matched case-insensitively, differ in 360 files, in **14,257 diff hunks** (+39,685 / -37,078 lines after normalising CRLF and trailing whitespace). [audit: these "hunks" are zero-context difflib change blocks, as `hunks.py` counts them. GNU `diff` on the same normalised files gives 14,273 hunks with `-U0` and 7,039 with the usual `-U3` (+39,374 / -36,771). The conclusion stands either way.] The original also cannot compile outside MSVC at all: `ad1bb82:src/Game/TYPES.H:11-13` is `#if defined(_WIN32) && defined(_M_IX86)` ... `#error Use of types.h for compiler other than Microsoft C`. And the SDL layer needs GoK's reworked headers. This is far past the brief's "hundreds of hunks" threshold. `ad1bb82` stays the licence and comparison base (diffstat in `src/PROVENANCE.md`).
2. **So "unmodified" means "no Cataclysm changes", not "Relic 2003 behaviour".** The tree carries GoK's non-portability changes. The ones that matter for 1:1 work, verified in `src/`:
   - The universe runs at 64 Hz by default (`Game/Universe.c:75` `UNIVERSE_UPDATE_RATE_SHIFT = 2`). `/vanilla` gives 16 Hz (`SDL/main.c:848,2195`). [M2-R2 oracle: 16 Hz is now the default; GoK's 64 Hz only with the new `/gok64Hz` switch; docs/m2-r2-oracle.md "flags"]
   - Saves use the portable `0x10ad0029` format (`Game/SaveGame.h:16-22`).
   - `turboTimeCompressionFactor = 8` (`Game/Universe.c:102`).
   - The numerics differ: SSE, truncating float-to-int, and a millisecond task tick (`docs/hw1-port-lessons.md` A3, A7).

   **Your call for M2-R2:** keep these and run with `/vanilla`, or revert them to Relic or Cataclysm behaviour as part of the port. Proposed: decide per item when the delta map reaches each subsystem; make 16 Hz the default once Cataclysm timing is ported.
3. **`src/Win32/` is Relic's original Win32 layer, byte-identical to `ad1bb82`, kept as reference and not built** (the brief's "Win32/ as reference"). Relic's original `Game/` and `Ships/` are **not** in the tree. They are the `ad1bb82` versions of the files GoK ported, and any GoK clone reproduces them (`git archive ad1bb82 src/Game src/Ships`). Say if you want them in-tree, for example as `src/reference/hw1-ad1bb82/`. That would add about 10 MB of REULA text.
4. **kas2c lives in `src/tools/kas2c/`**, so all REULA-derived code sits under `src/` with its notice. One build-file line makes GoK's relative include work there (see Local changes).
5. **The build version comes from `git describe` of this repo** (here `4363dff+`). GoK uses that string for its multiplayer compatibility check (`meson_options.txt`). Pin it with `-Dbuild_version=` if two machines must match.

## What was taken from where

Full per-file lists and the diffstat are in `src/PROVENANCE.md`. Summary:

| Destination | Source | Files | Built? |
|---|---|---|---|
| `src/Game/` | GoK `3af31fb` `src/Game` | 303 | yes |
| `src/Ships/` | GoK `src/Ships` | 83 | yes |
| `src/SDL/` | GoK `src/SDL` (GoK's rename of Relic `src/Win32`) | 70 | yes (static lib `hw_sdl`) |
| `src/Missions/{SinglePlayer,Tutorials}/` | GoK `src/Missions` (`.kas`, `.kash`, meson) | 23 | generated C, yes |
| `src/ThirdParty/` | GoK `src/ThirdParty` (CRC32, LZSS/BitIO, stb_image, Titan `ServerStatus.h`) | 14 | yes |
| `src/tools/kas2c/` | GoK `tools/kas2c` (Relic KAS2C 2.05, ported, "2.06noleak") | 6 | yes (build tool) |
| `src/COPYING.REULA` | GoK `COPYING.REULA` | 1 | n/a |
| `src/Win32/` | Relic `ad1bb82` `src/Win32`, text files only | 103 | **no** |
| `src/README.md`, `src/PROVENANCE.md`, `src/meson.build`, `meson.build`, `meson_options.txt`, `tools/build.sh`, `tools/bootstrap/*.py` | written here | | |

Not imported: every `Makefile.am` (autotools); GoK `src/homeworld.c` (an autotools placeholder); GoK `src/obj/` (Relic's prebuilt codec objects, which the build replaces with reverse-engineered C); `kas2c-macOS-compile.sh`; GoK's optional tools (biggie, polycount, monochrome-btg, `x86_64` converters); the emscripten demo subproject. From `ad1bb82`: `Homeworld.ico`, `HomeworldLogo.ico`, `res.geo`, `vc50.pdb` and `vssver.scc` in `src/Win32` (binary). Every imported file is byte-identical to its source except `src/tools/kas2c/meson.build`.

Diffstat of `src/` against `ad1bb82` (normalised lines, code files; details and mapping in PROVENANCE):

| Dir | Files | Identical | Modified | Moved in | New | Only in ad1bb82 | + | - |
|---|---|---|---|---|---|---|---|---|
| Game | 313 | 1 | 280 | 8 | 12 | 12 | 41,412 | 42,273 |
| Ships | 83 | 0 | 80 | 2 | 1 | 0 | 2,855 | 4,321 |
| SDL | 70 | 1 | 54 | 0 | 15 | 0 | 14,109 | 7,924 |
| Win32 (reference) | 93 | 93 | 0 | 0 | 0 | 0 | 0 | 0 |
| Missions (all) | 23 | 1 | 19 | 0 | 3 | 0 | 472 | 849 |
| tools/kas2c | 6 | 0 | 4 | 0 | 2 | 0 | 269 | 38 |
| ThirdParty | 14 | 0 | 0 | 7 | 7 | 0 | 9,934 | 277 |
| **total** | 602 | 96 | 437 | 17 | 40 | 12 | 69,051 | 55,682 |

## Local changes (build files only; no C source edited)

- **`meson.build` (repo root), `meson_options.txt`.** New files, adapted from GoK `meson.build` and `meson_options.txt` at `3af31fb`:
  - Project and executable are named `hwi-engine`. kas2c moved under `src/tools`.
  - `-Werror` is set through meson's `werror` option (default true) instead of a hard-coded flag.
  - GoK's 37 `-Wno-*` suppressions pass through `cc.get_supported_arguments()`. Option `legacy_warnings` (default true) switches the whole list off for measurement.
  - With clang, the 7 clang-only suppressions that GoK uses only for emscripten are added. This is the fix `docs/hw1-port-lessons.md` B4 and C1.14 asked for, and a clang 18 release build now passes with no extra flags. They stay out of GCC builds because `-Wno-uninitialized` and `-Wno-pointer-sign` are valid GCC flags and would hide warnings.
  - Dropped: the emscripten and darwin branches, `-Ddemo`, and the optional tools. The `-Ddemo=false` define is kept as `-DHW_GAME_HOMEWORLD`.
- **`src/meson.build`.** Same content as GoK `src/meson.build`. [audit: not byte-identical. `diff` shows one added first line, the comment `# Same layout as Gardens of Kadesh 3af31fb src/meson.build.`. The build content is unchanged.]
- **`src/tools/kas2c/meson.build`.** GoK's file plus `include_directories: include_directories('..')`. `parser.y:18` does `#include "../../src/Game/Types.h"`, a path that assumes GoK's `tools/kas2c`. From `src/tools`, `../../src/Game` resolves to `src/Game`, so the C source stays unedited. The first build attempt failed without this: `parser.y:18:10: fatal error: ../../src/Game/Types.h: No such file or directory`.

## Package prerequisites

Pop!_OS 24.04 (Ubuntu noble). Everything was already installed. `bison`, `flex` and `libavformat-dev` were added on 2026-10-08 for the reference build (`docs/hw1-port-lessons.md` B2). Versions from `dpkg-query`:

| Package | Version | Why |
|---|---|---|
| `meson` | 1.3.2-1ubuntu1 | build (root `meson.build` needs >= 1.1) |
| `ninja-build` | 1.11.1-2 | backend |
| `gcc` | 4:13.2.0-7ubuntu1 (gcc 13.3.0) | compiler |
| `pkg-config` | 1.8.1-2build1 | dependency lookup |
| `flex`, `bison` | 2.6.4-8.2build1, 2:3.8.2+dfsg-1ubuntu0.24.04.1 | kas2c lexer and parser |
| `libsdl2-dev` | 2.30.0+dfsg-1ubuntu3.1 | platform layer |
| `libgl-dev`, `libx11-dev` | 1.7.0-1build1, 2:1.8.7-1build1 | OpenGL; X11 (meson links `x11` on Linux, `docs/hw1-port-lessons.md` A3) |
| `libavcodec-dev`, `libavformat-dev`, `libavutil-dev`, `libswscale-dev` | 7:6.1.1-3ubuntu5 | Bink movies (`-Dmovies=true`, default) |
| `libasan8`, `libubsan1` | 14.2.0-4ubuntu2~24.04.1 | debug sanitizers |

One line: `sudo apt-get install meson ninja-build gcc pkg-config flex bison libsdl2-dev libgl-dev libx11-dev libavcodec-dev libavformat-dev libavutil-dev libswscale-dev`. Optional: `clang` (18.1.3 here).

## Exact commands run

All were run from the worktree root `/home/evanmedinger/Documents/GameDev/Homeworld-Infection/.claude/worktrees/m2`; `original/` and `reference/` mean the main checkout's directories. Logs are in `original/scratch-m2/bootstrap-logs/`, outside git.

```sh
# 1. base commit ad1bb82: the reference clone is shallow (.git/shallow = 3af31fb), so fetch the archive
mkdir -p original/scratch-m2
curl -sSfL -o original/scratch-m2/gok-ad1bb82.tar.gz \
  "https://gitlab.com/gardens-of-kadesh/gardens-of-kadesh/-/archive/ad1bb82/gardens-of-kadesh-ad1bb82.tar.gz"
tar -xzf original/scratch-m2/gok-ad1bb82.tar.gz -C original/scratch-m2/gok-export
#    pax comment in the archive: ad1bb82ca3c62dcc8be9d35f2e836d0bb7cf7caa
#    (a full clone, git clone https://gitlab.com/gardens-of-kadesh/gardens-of-kadesh.git
#     original/scratch-m2/gok-hist-bootstrap, has origin/master = 3af31fb in .git/packed-refs;
#     the harness refused git commands run against it from this worktree session)

# 2. measure the minimal-donor approach
python3 -I tools/bootstrap/hunks.py original/scratch-m2/gok-export/gardens-of-kadesh-ad1bb82 reference/gardens-of-kadesh
#    -> {'matched': 361, 'identical': 1, 'hunks': 14257, 'added': 39685, 'removed': 37078, 'orig_only': 20, 'gok_only': 21}

# 3. import (copies, never modifies, the sources; writes a manifest)
python3 -I tools/bootstrap/import_tree.py reference/gardens-of-kadesh \
  original/scratch-m2/gok-export/gardens-of-kadesh-ad1bb82 . manifest.json
#    -> copied 603 skipped 15
#    then src/meson.build written and src/tools/kas2c/meson.build edited by hand (Local changes)

# 4. diffstat and provenance
python3 -I tools/bootstrap/diffstat.py <ad1bb82 root> . diffstat.json
python3 -I tools/bootstrap/provenance.py manifest.json diffstat.json <ad1bb82 root> src/PROVENANCE.md "<hunks sentence>"
#    (PROVENANCE.md was then hand-edited in two places: the Decisions pointer and the option defaults row)

# 5. builds
tools/build.sh --debug --clean      # = meson setup --buildtype=debug -Db_sanitize=address,undefined build/debug . && meson compile -C build/debug
tools/build.sh --release --clean    # = meson setup --buildtype=release -Db_sanitize=none build/release . && meson compile -C build/release

# 6. start test: an unknown switch makes ProcessCommandLine print the usage and return -1
#    (SDL/main.c:976-979); main() then returns 0 before any SDL init (SDL/main.c:2189-2192).
#    Dummy SDL drivers as an extra guard; timeout 10; no input
env SDL_VIDEODRIVER=dummy SDL_AUDIODRIVER=dummy HW_Data=/tmp/hwi-nodata-data timeout 10 build/debug/hwi-engine /hwi-usage
env SDL_VIDEODRIVER=dummy SDL_AUDIODRIVER=dummy HW_Data=/tmp/hwi-nodata-data timeout 10 build/release/hwi-engine /hwi-usage

# 7. extra measurements (build dirs deleted afterwards)
meson setup --buildtype=release -Db_sanitize=none -Dwerror=false -Dlegacy_warnings=false build/warnscan . && meson compile -C build/warnscan
python3 -I tools/bootstrap/warntally.py <warnscan.log> warntally.json
CC=clang meson setup --buildtype=release -Db_sanitize=none build/clang . && meson compile -C build/clang
#    -> OK, 0 warnings under -Werror, real 8.3 s, 4,770,080 B
```

The start test prints `Invalid or unrecognised command line option: '/hwi-usage'` and then the option list. The only startup work before argument parsing is `utyOptionsFileRead()`, which `fopen`s `~/.homeworld/Homeworld.cfg` read-only and returns because the file is absent (`SDL/utility.c:866-890`, `Game/StatScript.c:896-918`), and `opKeyboardLoad()`. After the run `~/.homeworld` still does not exist. Neither binary was run against game data. Per the task, that is the data-deltas item's job, using the reference build.

## Warnings

- **With GoK's suppression list (the shipped configuration): 0 warnings** in debug and release, enforced by `-Werror`. The meson configure step prints one `WARNING:` line, the sanitizer notice.
- **What the list hides** (release, `-Dlegacy_warnings=false -Dwerror=false`, `warning_level=3` = `-Wall -Wextra -Wpedantic`): **15,662 unique warning sites** (15,875 lines) in 216 files. The build still links.
  - By location: **10,841 in kas2c-generated mission C** (`Tutorial1.c` 1,305, `Mission12.c` 810, `Mission02.c` 700, ...), 3,971 in `src/Game` (top: `Universe.c` 811, `ETG.c` 443), 477 in `src/Ships`, 359 in `src/SDL`, 9 in kas2c, 4 in ThirdParty.
  - By flag: `-Wunused-value` 7,642; `-Wpedantic` 3,391; `-Wunused-parameter` 1,813; **`-Wint-conversion` 1,462**; `-Wsign-compare` 292; `-Wunused-but-set-variable` 211; **`-Wpointer-to-int-cast` 178, `-Wint-to-pointer-cast` 152**; `-Wcast-function-type` 104; `-Wpointer-arith` 88; `-Wunused-variable` 77; `-Wformat=` 68; `-Wimplicit-fallthrough=` 62; `-Wunused-result` 24; `-Warray-bounds=` 23; `-Wmaybe-uninitialized` 12; 22 smaller groups with 9 or fewer each.
  - The int/pointer conversions (1,792 together) are the 64-bit hazard class from `docs/hw1-port-lessons.md` A5. That is relevant when Cataclysm code lands in the same files.
- `-Werror` stays on. It passes.

## Licence

- `src/COPYING.REULA`: GoK's copy of the REULA, verbatim.
- `src/README.md` starts with the notice: REULA, non-commercial. It quotes GoK's `COPYING` preamble verbatim and summarises §1.1, §2.1, §3.1-3.2, §5.2 and §7.5, with the clause numbers checked against the text. [audit: one clause number was wrong. "No warranty" was filed under §7.5, but it is §4.1. Fixed in `src/README.md`.] It also explains that the MIT grant in GoK's `COPYING.MIT` does not cover this tree.
- Root `README.md` has a new "Licence" paragraph and a `src/` row in the layout table.
- `src/PROVENANCE.md` repeats the notice at its top.

## Open points

- **Fidelity flags.** meson's `release` is `-O3`, and the tree has `TreatAs*` pointer-pun sites without `-fno-strict-aliasing` (`docs/hw1-port-lessons.md` A5b). The determinism work (PLAN risk table) should pin `-O2 -fno-strict-aliasing` and no fast-math before the checksum oracle compares anything. Not changed here, to keep the build equal to GoK's. [M2-R2 oracle: done, see docs/m2-r2-oracle.md "flags".]
- **Not attempted:** a Windows build (meson on Windows or a mingw cross build; no mingw-w64 is installed) and a 32-bit build.
- `tools/bootstrap/` holds the measurement and import scripts so the lists can be regenerated. They read `reference/` and the `ad1bb82` export and write only where told.
- The full clone `original/scratch-m2/gok-hist-bootstrap` (all GoK history) is available to other R1 items. This session could not run git in it: the harness refuses git commands that target paths in the main checkout.
