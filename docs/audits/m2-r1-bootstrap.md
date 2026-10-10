<!-- done: builds, binaries, start-test, clang, warnings, provenance-full, provenance-commits, citations, licence, git-status, fixes, verdict -->
# Audit: M2-R1 item 4 "bootstrap" (src/ tree and hwi-engine build)

Date: 2026-10-10. Auditor: adversarial audit stage of the M2-R1 workflow. Worktree `.claude/worktrees/m2`, branch `m2`, HEAD `4363dff`.

Producer outputs audited:
- `docs/m2-r1-bootstrap.md`
- `src/PROVENANCE.md`, `src/README.md`, `src/COPYING.REULA`
- `meson.build`, `meson_options.txt`, `src/meson.build`, `src/tools/kas2c/meson.build`
- `tools/build.sh`, `tools/bootstrap/*.py`
- the root `README.md` diff

Audit scratch: `original/scratch-m2/audit-bootstrap/` (logs and the `warnscan/` and `clang/` build dirs, outside git) and `/tmp/hwi-audit-bootstrap/` (the audit scripts `cmpall.py`, `gitcheck.py`, `commitinfo.py` and `hunkcheck.py`, plus `sites.txt`).

Nothing was committed. No game data was read or run, no Ghidra was run, no input was injected, and the reference clones were only read.

## What was checked, and how

### 1. Clean builds, exactly as the doc says

I ran `rm -rf build`, then `tools/build.sh --debug --clean`, then `tools/build.sh --release --clean`, each under `time`. Logs: `original/scratch-m2/audit-bootstrap/{debug,release}.log`.

| Claim | Producer | Audit measurement | Result |
|---|---|---|---|
| Debug builds | OK, 292/292 | rc 0, `[292/292] Linking target hwi-engine` | correct |
| Release builds | OK, 292/292 | rc 0, 292/292 | correct |
| Errors / warnings | 0 / 0 under `-Werror` | 0 `warning:` lines from the compiler. The only `WARNING` is meson's sanitizer notice in the debug configure step. `-Werror` is present in all 248 entries of each `compile_commands.json`. | correct |
| Debug size | 74,870,832 B; text 41,178,013, data 23,887,008, bss 2,225,120 | identical (`stat`, `size`) | correct |
| Release size | 5,157,976 B; text 4,193,125, data 232,408, bss 1,685,376 | identical | correct |
| sha256 | debug `2f7c7f1e…49ab3d`, release `0024b0f6…53b297` | identical: the rebuild is byte-for-byte reproducible | correct |
| `file` output | debug "with debug_info, not stripped"; release "not stripped" | matches | correct |
| Build time | about 11 s each (11.2 / 10.9) | real 11.8 s (debug), 10.9 s (release) | correct |
| Flags | debug `-O0 -g`, `HW_BUILD_FOR_DEBUGGING`; release `-O3`, `HW_BUILD_FOR_DISTRIBUTION`; `-DHW_GAME_HOMEWORLD` | `compile_commands.json`: release has `-O3` in 248 entries; `-DHW_BUILD_FOR_DEBUGGING` / `_DISTRIBUTION` in 245; `-DHW_GAME_HOMEWORLD` in 248 | correct |

### 2. Start test

Command: `env SDL_VIDEODRIVER=dummy SDL_AUDIODRIVER=dummy HW_Data=/tmp/hwi-nodata-data timeout 10 build/<type>/hwi-engine /hwi-usage`. This is the hwi-engine binary, not the GOG game. No data was present.

| Result | Producer | Audit |
|---|---|---|
| Debug | exit 0, 109 lines, no ASan/UBSan report | exit 0, 109 lines, first line `Invalid or unrecognised command line option: '/hwi-usage'`, 0 sanitizer lines |
| Release | exit 0, 55 lines | exit 0, 55 lines |
| `~/.homeworld` | still absent afterwards | absent before and after |

Verdict: correct.

### 3. Clang and the warning scan (re-run in scratch, outside the worktree)

**Clang build.** `CC=clang meson setup --buildtype=release -Db_sanitize=none <scratch>/clang .`, then compile, with clang 18.1.3. Result: rc 0, 0 warnings, `-Werror` on, **4,770,080 B**. Correct. This also matches `docs/hw1-port-lessons.md:518`.

**Warning scan.** `meson setup --buildtype=release -Db_sanitize=none -Dwerror=false -Dlegacy_warnings=false <scratch>/warnscan .`. I tallied the log with my own `grep`/`sed`/`sort -u` on (file:line:col, flag), not with the producer's `warntally.py`. Every number below matches the doc.

| Measure | Value |
|---|---|
| Warning lines | 15,875 |
| Unique sites | 15,662 |
| Files with warnings | 216 |
| Sites in kas2c-generated missions | 10,841 |
| Sites in `src/Game` | 3,971 |
| Sites in `src/Ships` | 477 |
| Sites in `src/SDL` | 359 |

The per-flag counts also all match: unused-value 7,642, pedantic 3,391, unused-parameter 1,813, int-conversion 1,462, sign-compare 292, unused-but-set-variable 211, pointer-to-int-cast 178, int-to-pointer-cast 152, cast-function-type 104, pointer-arith 88, unused-variable 77, `format=` 68, `implicit-fallthrough=` 62, unused-result 24, `array-bounds=` 23, maybe-uninitialized 12. Correct.

The `warnscan` binary is also 5,157,976 B. The warning flags do not change the code.

**Suppression counts.** The GCC suppression list in `meson.build` has 37 `-Wno-*` entries, and the clang-only block adds 7. Correct.

### 4. Provenance: every file, not only 20

`/tmp/hwi-audit-bootstrap/cmpall.py` byte-compares every file under `src/` with its claimed source:
- the working tree of `reference/gardens-of-kadesh`, for `Game`, `Ships`, `SDL`, `Missions`, `ThirdParty`, `tools/kas2c` and `COPYING.REULA`;
- the `ad1bb82` archive export, for `Win32/`.

Results:
- **499 GoK files byte-identical, plus 103 `Win32/` files byte-identical to `ad1bb82`.** The one mismatch is `tools/kas2c/meson.build`, the documented local change. Its `diff` is 4 added lines: the `include_directories('..')` line and a 3-line comment. Correct: 603 imported, and "every imported file is byte-identical except `tools/kas2c/meson.build`".
- `src/` holds 606 files: the 603 imported, plus `README.md`, `PROVENANCE.md` and `meson.build`. All 606 are text (`file --mime-encoding`: 581 us-ascii, 13 iso-8859-1, 8 unknown-8bit, 4 utf-8). None has a binary or game-data extension. The tree is 18 MB.
- Not-imported lists: GoK `src/` has 19 files absent from `src/`, all of them `Makefile.am`, `homeworld.c`, or `obj/*.o` and `*.obj`. Also absent are kas2c's `Makefile.am` and `kas2c-macOS-compile.sh`, and the five `ad1bb82` `Win32` binaries (`Homeworld.ico`, `HomeworldLogo.ico`, `vc50.pdb`, `res.geo`, `vssver.scc`). These match PROVENANCE. Correct.

### 5. Provenance: are the files really from the commits named? (20-file spot check)

The harness refused git commands aimed at the reference clones and the scratch full clone, both with `git -C` and with `cd` first. So I wrote a read-only git pack reader, `/tmp/hwi-audit-bootstrap/gitcheck.py`. It resolves `<commit>:<path>` to a blob id in the full GoK clone `original/scratch-m2/gok-hist-bootstrap/.git` (pack `f323ddf0…`) and compares that id with the git blob hash (`sha1("blob <len>\0" + data)`) of the file in `src/`.

- **20 of 20 sampled files match their commit's tree blob.**
  - 14 from `3af31fb`: `COPYING.REULA`, `tools/kas2c/parser.y`, `src/ThirdParty/stb/stb_image.h`, `src/Missions/SinglePlayer/Mission05_OEM.kas`, `src/Game/Universe.c`, `src/Game/SaveGame.h`, `src/Game/desktop.ini`, `src/SDL/main.c`, `src/SDL/TitanInterfaceC.stub.c`, `src/Ships/Ships.h`, `src/Ships/P2Swarmer.c`, `src/Game/Types.h`, `src/ThirdParty/LZSS/LZSS.c`, `src/Missions/Tutorials/Tutorial1.kas`.
  - 6 from `ad1bb82`: `src/Win32/{main.c, MAKEFILE, Homeworld.rc, rinit.cpp, glext.h, Debug.depend}`.
  - Control: `tools/kas2c/meson.build` does not match (blob `3d40cb20…` against local `390937e3…`), as expected for the edited file. The `ad1bb82` export's `src/Game/TYPES.H` matches the `ad1bb82` tree.
- Commit identities:
  - `ad1bb82ca3c62dcc8be9d35f2e836d0bb7cf7caa` has no parent line, so it is a root commit. Its message is "Original Relic code, as published in 2003".
  - `3af31fbca2acfb4e791386a041087b340e77892f` is `refs/remotes/origin/master` in the full clone. It is also `refs/heads/master`, `.git/HEAD` and `.git/shallow` in `reference/gardens-of-kadesh` (read as files).
  - The archive `gok-ad1bb82.tar.gz` has pax comment `ad1bb82ca3c62dcc8be9d35f2e836d0bb7cf7caa`.
  - All correct.

### 6. Measurement and diffstat claims

- **Hunks.** The producer reports 361 matched files, 360 differing, 14,257 hunks, +39,685 / -37,078. My independent re-measure (`hunkcheck.py`, GNU `diff` on CRLF-normalised and whitespace-stripped copies) gives 361 matched, 360 differing, 14,273 hunks with `-U0` and **7,039 with `-U3`**, +39,374 / -36,771.
  - The producer's number counts difflib zero-context change blocks, not the usual unified-diff hunks. The doc said only "diff hunks", so I added a clarifying annotation.
  - Classed as correct, because the number reproduces under its stated method and the conclusion ("far past hundreds") holds under either definition.
- **Diffstat totals.** The rows add up:
  - files 313+83+70+93+23+6+14 = 602;
  - identical 96, modified 437, moved 17, new 40, deleted 12;
  - lines +69,051 / -55,682;
  - the doc's "Missions (all)" and "tools/kas2c" rows equal the PROVENANCE sub-rows added together.
  - Correct, as an internal-consistency check. I did not recompute the per-file line counts.
- **TYPES.H.** `ad1bb82:src/Game/TYPES.H:11-13` is `#if defined(_WIN32) && defined(_M_IX86)` / `#else` / `#error Use of types.h for compiler other than Microsoft C`. Correct.
- **Size of Relic's `Game/` and `Ships/`.** The doc says "about 10 MB". `du --apparent-size` gives 8.8 MB + 592 KB. Correct.

### 7. Source citations

I read each cited line in `src/`. All correct.

| Citation | What it says |
|---|---|
| `Game/Universe.c:75` | `UNIVERSE_UPDATE_RATE_SHIFT = 2` |
| `Game/Universe.c:102` | `turboTimeCompressionFactor = 8;  // same as Homeworld Cataclysm` |
| `SDL/main.c:848` | the `/vanilla` entry |
| `SDL/main.c:2195` | `if (vanillaMode)` |
| `Game/SaveGame.h:16-22` | `HWSDL_2 0x10ad0029`; `ORIGINAL 0x10ad0027` |
| `SDL/utility.c:778-783` | the three cfg options |
| `Game/SinglePlayer.c:151`, `SDL/utility.c:941` | the two bools set to `FALSE` |
| `SDL/main.c:976-979`, `2189-2192` | usage printed, `return(-1)`, then `return 0` |
| `SDL/utility.c:866-890`, `Game/StatScript.c:896-918` | `utyOptionsFileRead` → `scriptSetFileSystem` → `fopen(..., "rt")`, and return if NULL |
| `tools/kas2c/parser.y:18` | `#include "../../src/Game/Types.h"` |

### 8. Licence notices

| Item | Finding | Result |
|---|---|---|
| `src/COPYING.REULA` | byte-identical to GoK `COPYING.REULA`; tree blob `c47d4463…` at `3af31fb` | correct |
| `src/README.md` opening | REULA notice, non-commercial | present |
| GoK `COPYING` quote | the three paragraphs match the GoK file word for word (reflowed) | correct |
| MIT remark: "only Thibault Lemaire, 2019-2026" | from `COPYING.MIT` | correct |
| MIT remark: "all GoK commits after 0e40bdd0" | from GoK `documentation/contributors/license-agreement.md:34`, not `COPYING.MIT`, but stated accurately | correct |
| REULA clause summaries | §1.1 (a)/(b), the non-commercial definition and RDN-only distribution; §2.1 assignment; §3.1 five years; §3.2 termination; §5.2 copy protection; §7.5 BC law, Vancouver venue | correct |
| "No warranty" | it is §4.1, not §7.5 | **wrong** (fixed) |
| Root `README.md` | new "Licence" paragraph naming REULA and pointing to `src/COPYING.REULA`, plus a `src/` row in the layout table (`git diff README.md`) | present |
| `src/PROVENANCE.md` | starts with the REULA notice | present |

### 9. Git status (worktree)

- `git diff --cached --stat` is empty, so **nothing is staged**.
- `git status --porcelain --untracked-files=all` shows 626 untracked entries plus the modified `README.md`. All are text docs or scripts: `src/` (606), `docs/*.md`, `ghidra/delta_map.json`, `meson.build`, `meson_options.txt`, `tools/build.sh`, `tools/bootstrap/*.py`, and the other R1 items' `tools/*.py`.
- `build/` is ignored (`.gitignore:12`, confirmed with `git check-ignore -v` for both binaries), as are `__pycache__/` dirs.
- There are no build outputs or game data outside `.gitignore`.
- Correct.

### 10. Prerequisites

`dpkg-query` versions match the table for all 15 packages. clang is 18.1.3, gcc 13.3.0, meson 1.3.2. Correct.

## Findings tally

| # | Claim | Verdict |
|---|---|---|
| 1 | debug build succeeds via `tools/build.sh --debug --clean`, 292 steps | correct |
| 2 | release build succeeds, 292 steps | correct |
| 3 | 0 errors / 0 warnings under `-Werror`, both | correct |
| 4 | debug size, `size` sections, sha256 | correct |
| 5 | release size, `size` sections, sha256 | correct |
| 6 | `file` descriptions | correct |
| 7 | about 11 s clean build times | correct |
| 8 | optimisation, `HW_BUILD_*` and `HW_GAME_HOMEWORLD` flags | correct |
| 9 | start test: exit codes, 109/55 lines, no sanitizer output, no `~/.homeworld` | correct |
| 10 | release = GoK release size; clang = GoK clang size (`hw1-port-lessons` B4) | correct |
| 11 | clang 18 release build passes, 4,770,080 B | correct |
| 12 | warning scan numbers (sites, lines, files, by dir, by flag) | correct |
| 13 | 37 GoK suppressions, 7 clang-only | correct |
| 14 | 603 imported files byte-identical to source except kas2c `meson.build` | correct |
| 15 | 20/20 spot-checked files are blobs of the stated commits | correct |
| 16 | commit identities (`ad1bb82` root and message, `3af31fb` = master, tarball pax comment) | correct |
| 17 | not-imported lists | correct |
| 18 | diffstat totals consistent | correct |
| 19 | 14,257 "hunks" (difflib blocks; GNU `-U3` gives 7,039) | correct, annotated |
| 20 | `TYPES.H:11-13` `#error` | correct |
| 21 | behavioural citations (Universe, main, SaveGame, utility, SinglePlayer) | correct |
| 22 | start-up cfg-read citations | correct |
| 23 | `COPYING.REULA` verbatim; GoK `COPYING` quote verbatim | correct |
| 24 | MIT remark (Lemaire; `0e40bdd0`) | correct |
| 25 | REULA §1.1, §2.1, §3.1-3.2, §5.2, §7.5 (law) summaries | correct |
| 26 | root `README.md` licence paragraph and `src/` row; PROVENANCE notice | correct |
| 27 | git status clean of build outputs and game data; nothing staged | correct |
| 28 | package versions | correct |
| 29 | Relic `Game/` + `Ships/` about 10 MB | correct |
| W1 | `src/README.md`: "§7.5: ... No warranty." The no-warranty clause is §4.1 (`COPYING.REULA:144-151`), and §7.5 is governing law only. The doc's "clause numbers checked" is therefore not fully true. | **wrong**, fixed |
| W2 | `docs/m2-r1-bootstrap.md` and `src/PROVENANCE.md`: `src/meson.build` has "same content as GoK `src/meson.build`". `diff` shows one added first-line comment, so the file is not byte-identical. The build content is the same. | **wrong** (minor), fixed |

**Precision** = correct / (correct + wrong) = 29 / 31 = **0.935**.

## Fixes made in the producer's documents (each marked inline with `[audit: ...]`)

1. `src/README.md`: the REULA bullet now reads "§7.5: British Columbia law. §4.1: no warranty." (W1)
2. `docs/m2-r1-bootstrap.md`, Licence section: a note that one clause number was wrong and is fixed in `src/README.md`. (W1)
3. `docs/m2-r1-bootstrap.md`, Local changes: `src/meson.build` noted as GoK's file plus one leading comment line. (W2)
4. `src/PROVENANCE.md`, Local changes table: the same note. (W2)
5. Clarifications, not error fixes: in `docs/m2-r1-bootstrap.md` (Decision 1) and `src/PROVENANCE.md` (Decision section), the 14,257 "hunks" are now labelled as difflib zero-context blocks, with the GNU `diff -U3` (7,039) and `-U0` (14,273) figures added.

Distinct factual errors fixed: 2. Annotations: 5 edits across 3 files. No code, build file or script was changed.

## Unverifiable or not settled

- **Git verification route.** The harness refused git commands against the reference clone and the scratch full clone (`git -C` and `cd` first). Commit membership was therefore checked with a self-written read-only pack reader, not with `git show`/`ls-tree`. It reproduces git blob ids exactly, as the matches and the expected kas2c mismatch show.
- **Byte equality of the generated code** against a GoK build of `3af31fb` was not checked, as the producer itself states. Only sizes are equal.
- **Per-file diffstat line counts** (for example `Game/AIMoves.c` +5,552 / -22) were not recomputed. The table totals are consistent, and the aggregate line counts are close to an independent GNU `diff` measure; they differ slightly because difflib and GNU diff use different algorithms.
- **No Windows or 32-bit build** was attempted, by the producer or here.

Verdict: PASS-WITH-FIXES - both clean builds via tools/build.sh succeed with 0 warnings and binaries byte-identical to the documented sha256; all 603 imported files are byte-identical to the stated sources (20/20 confirmed as blobs of the stated commits); two minor documentation errors fixed (REULA clause number, src/meson.build "same content").
