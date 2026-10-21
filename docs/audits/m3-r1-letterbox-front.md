<!-- done: build, exe-read, diff, fence, streams, sp -->
# Audit: M3-R1 letterbox-front (producer 1, play-test item 1)

Auditor: adversarial audit of `docs/m3-r1-letterbox-front.md` and `git diff ce4fab4` (worktree m2, branch m2; the
port work is the uncommitted tree on top of 36dfc2b; the commits 18281e2 .. 36dfc2b are the orchestrator's: the
workflow script, cycle/answers, STATE.md, not port work). Every engine run went through `tools/m2_harness.sh`
(silent, headless, offscreen); labels `m3r1-aud-lbf-*` in `original/scratch-m2/m2r2-harness/runs/`, engine
`--bin original/scratch-m2/build-audit-m3r1-letterbox-front/debug/hwi-engine` (my clean build) unless stated.
No GOG game, no oracle DLL, no input, no gamescope, no Ghidra, no GPU measurement (correctness runs only).

## 1. Clean build

`rm -rf original/scratch-m2/build-audit-m3r1-letterbox-front`, then `meson setup` with tools/build.sh's arguments
(debug: `--buildtype=debug -Db_sanitize=address,undefined`; release: `-Doptimization=2 -Ddebug=false
-Db_sanitize=none`) and `meson compile`: both rc 0, **0 warnings** (werror=true, warning_level 3, from
meson-info). Sizes: debug `hwi-engine` 88,712,400 B, release 5,629,440 B. After my comment fix (below) a second
clean pair (`debug-final` / `release-final`): rc 0, 0 warnings, 88,712,408 / 5,629,440 B (the debug +8 B is the
two extra comment lines moving `__LINE__` of later asserts in render.c). `tools/m2_layout32.sh`: OK.

## 2. The exe (tools/m2_fdis.py, the R2 exe.dis, the r6 export)

| # | body / claim | exe evidence | verdict |
|---|---|---|---|
| 1 | `cat_popupMenuDraw` 0x004ee120 ends with `push [0x00a57628]; call cat_rndDepthTestEnable` | 0x004ee5bd `mov edx,[0xa57628]`, 0x004ee5c3 `push edx`, 0x004ee5c4 `call 0x57ae40` (functions.csv: cat_rndDepthTestEnable) | correct |
| 2 | 0x00a57628 is a .bss word nothing writes | one 4-byte occurrence of 0x00a57628 in the whole exe file (offset 0xee5bf); .data VA 0x74a000, raw size 0x196000, so 0xa57628 lies past the file-backed part: zero-filled | correct |
| 3 | `rndRenderTask` 0x0057a910: `glIsEnabled(0xc11)` after the clear (0x0057aa1a, stored 0xa75d84), `regFunctionsDraw` (0x0057aa2f), the bars when `nisScissorFade != 0 \|\| (nisFullyScissored && !flag)` (0x0057aa43 .. 0x0057aa6f), `glDisable(0xc11)` (0x0057aa77), then subtitles (0x00584620 loop), sync, cursor | as read | correct |
| 4 | the flag is the GL DLL's glIsEnabled | decompiled.c 314082 `DAT_00b3a5a8 = GetProcAddress(..., s_glIsEnabled)` | correct |
| 5 | the scissor test is never left on, so the flag is FALSE every frame | every `push 0xc11; call glEnable` (0x45793e, 0x457a69, 0x457d66, 0x51537e, 0x52df44) is paired with a glDisable in the same function; 0x005693bc / 0x00569eb3 (mrRegionDraw) only under the flag itself; 0x0057aa77 disables each frame | correct |
| 6 | `rndDrawScissorBars` 0x0057a410: fade branch turns additive off (0x0057a6ee .. 0x0057a703: `ebx = rndAdditiveBlending`; if set: glBlendFunc(0x302,0x303), flag = 0), glEnable(GL_BLEND), glColor4f(0,0,0,fade); end restores ebx (0x0057a8c8) | as read | correct |
| 7 | "the full-bars branch restores an **uninitialised** ebx (0x0057a4ba)" | 0x0057a4ba `mov ebx,[esp+0x14]`, which 0x0057a48e filled with the top bar's GL y (fstp): ebx is SET, to float bits that are nonzero, so 0x0057a8c8 stores them in rndAdditiveBlending and (flag 0x00a75ef0 & 2 clear) calls glBlendFunc(GL_SRC_ALPHA, GL_ONE): the exe leaves additive blending ON after full bars | **wrong** (wording; fixed, F1) |
| 8 | `cat_moRender` 0x004aafb0 ends with `push [0x008853fc]` (initialised 1) and returns early while nisFullyScissored | 0x004ab48e; the word in .data = 1 (read from the file); 0x004ab01e `test eax,eax` (eax = [0xa4cb58], nisFullyScissored) `jne` to the epilogue | correct |
| 9 | `cat_shipPrintDraw` 0x00510e60 ends with `push 1` | 0x005114c0 `push 0x1; call 0x57ae40` | correct |
| 10 | `mouseDraw` 0x0056ec50 passes the unwritten word 0x00a75928 | 0x0056f250 `mov ecx,[0xa75928]` .. `call 0x57ae40`; tree mouse.c:941 | correct |
| 11 | `cat_rndDepthTestEnable` 0x0057ae40 = the tree's render.c:2793 | glIsEnabled(0xb71) returned; enable: glEnable + the 0x9003 depth-mask branch; disable: glDisable (+ the mask branch) | correct |
| 12 | `regFunctionsDraw` 0x004f0ae0 IDENTICAL | the region list from the last entry down, counter 0xa57640++: Region.c:1088 | correct |
| 13 | `primModeSetFunction2` 0x0056fd10 / `primModeClearFunction2` 0x0056fd90 IDENTICAL | flat / alpha-test, blend off / identity matrices / lighting, texture off / glDisable(0xb71); clear: smooth, glEnable(0xb71), on, pops: prim2d.c:41 / 67 | correct |
| 14 | `mrRegionDraw` 0x00569260: the scissor box under nisIsRunning && nisFullyScissored && flag (0x005693a4 .. 0x005693bc), letter height inline `(H - W*20/37)/2` | 0x005693c7 .. (the 0xdd67c8a7 / sar 5 divide by 37): 107 rows at 1024x768, 42 at 3840x2160; GoK's NIS_LetterHeight (src/Game/NIS.h:73) gives 107 and 277 | correct |
| 15 | `rndAdditiveBlends` 0x0057af90 (relied on, delta map IDENTICAL) | the exe's has `if (bAdditive && !(0x00a75ef0 & 2))` before the additive glBlendFunc; the tree's is GoK's without it (no reference to 0x00a75ef0 anywhere in src/). Pre-existing, outside this port's edits; GoK's GL backend is decision 28; no review row added | observation |

Bodies compared with the exe: 14 (cat_popupMenuDraw, rndDrawScissorBars, rndRenderTask, cat_moRender,
cat_shipPrintDraw, mouseDraw, cat_rndDepthTestEnable, regFunctionsDraw, primModeSetFunction2,
primModeClearFunction2, mrRegionDraw, rndAdditiveBlends, utyOptionsFileWrite (only HWI lines added; MODIFIED in the map),
the commandOptions / console tables (HWI rows only)). The producer's reading stands: the exe never clips the 3D view;
the M2-R6 port's `cat_rndDepthTestEnable(TRUE)` at the end of `cat_popupMenuDraw` was the only cause; the one-line
port covers the full, fading and KAS letterbox (the same nisScissorFade / nisFullyScissored variables).

## 3. The diff (git diff ce4fab4, port files only)

- Scope: src/Game/PopupMenu.c, src/SDL/render.c / .h, src/SDL/utility.c, src/SDL/main.c, src/Game/Console.c,
  src/Game/MotionTest.c / .h, tools/m2_harness.sh, tools/play.sh, tools/ppm_region.py (new),
  ghidra/delta_map_review.json, ghidra/delta_map.json, docs/m2-r1-function-deltas.md, the report. Equal to the
  producer's list; nothing outside the scope; mainrgn.c / Region.c equal to ce4fab4 (the launcher's probes are gone).
- Tags: the Cataclysm changes carry `CAT:` with function and address (PopupMenu.c 0x004ee120 / 0x004ee5bd;
  render.c 0x0057a410 with 0x0057a6ee .. 0x0057a703 / 0x0057a8c8); the deviation is `CAT-FIX` with its reason;
  every setting line (cfg row, console row, switch, render.c state and helpers, header) carries
  `HWI-FEATURE(letterbox-setting)` (counts: render.c 11, render.h 7, main.c 7, utility.c 4, MotionTest.c 4,
  Console.c 2, PopupMenu.c 1); the helpers' bodies are tagged at the signature and the block comment, as
  MotionInterp.c's miOptionsFileValues; the capture is `HWI-TEST` (precedent: AISelfTest.c, Animatic.c ...).
- CAT-TODO: none added; `/bin/grep -rn "M3-R1" src | /bin/grep -c "CAT-TODO\|CAT-PROVISIONAL"` = 0; the 23
  stage-5 sites untouched.
- Delta map: the review rows (rndDrawScissorBars IDENTICAL -> MODIFIED; notes on rndRenderTask, mrRegionDraw,
  regFunctionsDraw, primModeSet/ClearFunction2) are in delta_map_review.json (decision 55). Generator check: ce4fab4's
  delta_map.json + the producer's review file + ce4fab4's report in a scratch tree, `delta_map.py merge --done
  m3r1:letterbox-front` ("records changed 6", IDENTICAL 2,762, MODIFIED 1,575, REWRITTEN 83) and `report`:
  **byte-identical** to the producer's delta_map.json and docs/m2-r1-function-deltas.md. After my note fix (F1),
  `merge` (records changed 1) and `report` were rerun in the worktree.
- `rndDrawScissorBars`: `oldAdditive` restore is unconditional but a no-op under letterboxBehind 1 (rndAdditiveBlends
  only calls glBlendFunc on a change), so the setting at 1 keeps the M2 frames (confirmed by the dumps below).
- Capture code (MotionTest.c catShotFrameEnd): reads the back buffer before the flip after the cursor, writes P6 top
  row first, `.s<N>` after a tick restart; note (not a defect at the sizes in use): it leaves GL_PACK_ALIGNMENT at 1
  for the rest of the run, so a run that combines `--shot` with the motion test's CRC read-back at a width whose
  `3*W` is not a multiple of 4 would change that CRC; every width of the fence and the planned 1920 / 2560 / 3840 has
  `3*W % 4 == 0` (the report states the alignment change).

## 4. The fence, rerun from my clean build

### PPM pixels (tools/ppm_region.py and an independent 10-line Python reader of the P6 bytes)

| run | frame | ppm_region.py | independent reader |
|---|---|---|---|
| m3r1-aud-lbf-nis-shot (default) | nis n06 26-36, `--shot 584:6 -- /catMotionShot 1374` | `bands`: bottom_rows=107; bottom rows 661-767 `match=109568 other=0`; top rows 0-106 `match=109479 other=89`; `shot.584.6.ppm` vs `nis.dump.1374.ppm` differ=0 (the any-mode capture = the motion capture) | top non-black 89, bbox (0,0)-(11,17) = the cursor; bottom non-black 0; row 660 (scene above the bar) 1,015 non-black |
| m3r1-aud-lbf-nis-shot-behind (`--cfg 'letterboxBehind 1'`) | same | bottom `match=80314 other=29254`; IDENTICAL (differ=0) to ce4fab4's `m3r1-lb-base-nis1374` frame | bottom non-black 29,254, bbox x 322-710, rows 661-767 |
| m3r1-aud-lbf-nisfade-a / -behind | `--shot 576-598/2:6 -- /catNisScissorOut 580:1` | tick 584 (fade 0.75, 80 rows): default bottom rows 688-767 `match=81920 other=0` at 32,32,32; top rows 0-79 81,830 at 32,32,32 + 90 cursor pixels; default vs behind differ 21,873 (bbox x 396-710, rows 688-767); tick 582 28,140; 586 9,256; 588 / 590 0 | top rows 0-79 non-32: 90, bbox (0,0)-(11,17) |
| m3r1-aud-lbf-m2-shot / -behind (Mission02 KAS letterbox jump) | `--shot 25770-25840/2 --shot-letterbox` | 32 files, fade in 25778 (0.0208) .. 25824 (0.9792), full from 25826; 0 of 32 differ between the settings; tick 25800 (0.4792) top 51 rows 52,134 at 66,66,66 + 90 cursor, bottom 51 rows all 66,66,66; 25840 `bands` 107 / 107; tick 25800 equal to the producer's | - |
| m3r1-aud-lbf-m1seg-a / -behind (Mission01 NIS-first, `--shot 0-2400/3 --shot-letterbox`) | 1,390 files | 31 differ: n01 1128, 1131 (242 / 220 px) and nislet01 (s1) 738-765, 786-792, 870-879, 1143-1191 (6), 1377, 1599-1605, 1662-1665; s1 789 65,115 (bbox 0,0-709,106: the top bar), s1 747 51,099 (bottom bar) | in all 31 the default's 214 bar rows hold 0 non-black pixels; viewed s1 789 side by side (old look: the orange hull in the top bar; default: black) |

### Dumps (whole sha256/16, and with the ` crc=` column stripped)

| dump | R10 (whole / nocrc) | default (whole / nocrc) | letterboxBehind 1 |
|---|---|---|---|
| motion /noInterp | 183681235a3fdc7b / 3016280e9e0dd3c8 | m3r1-aud-lbf-motion-noi 183681235a3fdc7b / 3016280e9e0dd3c8 EQUAL | -noi-behind (cfg) 183681235a3fdc7b EQUAL |
| motion on | 428fff6c765ab81b / 428ef41479eb5749 | m3r1-aud-lbf-motion-on 428fff6c765ab81b EQUAL | -on-behind (switch) 428fff6c765ab81b EQUAL |
| nis /noInterp | 13ca70eea998e5de / 196bab32d244f614 | m3r1-aud-lbf-nis-noi **2a7fec4de8cfe716** / 196bab32d244f614 (279 frame lines differ, F 1043 .. 1420, crc only) | -noi-behind (cfg) 13ca70eea998e5de EQUAL |
| nis on | 6a4c1850799187ac / 7ce8472514254f4f | m3r1-aud-lbf-nis-on **46df1d391a86a404** / 7ce8472514254f4f (277, F 1034 .. 1412, crc only) | -on-behind (switch) 6a4c1850799187ac EQUAL |

As the producer stated: every non-crc column IDENTICAL to R10; the two default nis dumps move in crc only (reason:
the bars in front, PopupMenu.c); with the setting on all four equal R10's.

### Streams (hwo_compare.py, every field)

| stream | run (sha256/16, size) | result |
|---|---|---|
| headless Mission01, 6,000 ticks, debug | m3r1-aud-lbf-mission-a / -b (d9c83eb49837ebf2, 1,476,370 B, both; `cmp` byte-identical) | IDENTICAL to r10i-mission-a (5bd51234..., still present): 6,001 ticks, 0 packets, no difference (rng_all included); the pair IDENTICAL |
| headless Mission01, release | m3r1-aud-lbf-mission-rel (99d9998c881dae7d, 1,476,370 B; release binary of the clean build) | IDENTICAL to r10i-mission-rel-a and to r10i-mission-a |
| missiontest Mission01 (NIS-first) | m3r1-aud-lbf-missiontest-a / -b (7b79ad092284ba69, 3,246,588 B) | 6/6 PASS, done 11,424; IDENTICAL to r10i-missiontest-a and to the producer's m3r1-lbf-missiontest-fa (11,426 ticks) |
| Mission02 self-test | m3r1-aud-lbf-m2 (cafab2552fdb85eb, 6,381,116 B) | 6/6 PASS, done 25,936; IDENTICAL to r10i-m2-a and to m3r1-lbf-m2-fa (25,905 ticks) |
| with captures / setting | m3r1-aud-lbf-m2-shot, -m2-shot-behind, -m1seg-a, -m1seg-behind | IDENTICAL to r10i-m2-a / r10i-missiontest-a (capture and setting move no field) |

No simulation field moved in any stream; no rng_all move.

### Self-tests and modes (ASan 0 in every one)

| run | result |
|---|---|
| m3r1-aud-lbf-toggle | `toggletest: done start=1/1 fails=0 result=PASS`; letterbox start 0 / accept-1 row 1 read back 1 / accept-0 row 0 read back 0 / switch run-only row 0 / end 0; console rows `'letterboxBehind 1' -> 1`, `'0' -> 0`; the run's Cataclysm.cfg holds `letterboxBehind    0` |
| m3r1-aud-lbf-toggle-sw (`-- /letterboxBehind`) | PASS: start 1 over the file's 0, the saves write 0, the switch stands at the end |
| m3r1-aud-lbf-hud | `done tick=599 boxed=19 selected=154 frames=537 ... result=PASS` |
| m3r1-aud-lbf-ui | `done selected=154 rows=10 popupframes=16 commands=26 targetrows=1 statsframes=8 result=PASS` |
| m3r1-aud-lbf-fescreens | fescreentest 22 / 195 PASS; optionstest 34/34/34 PASS (as R10: the exe's options; the HWI row is not one) |
| m3r1-aud-lbf-console | `shown=32 set=25 ... failures=0 result=PASS` (R10 31 / 24: the new row) |
| m3r1-aud-lbf-fe | Main_game_screen, still running at the 120 s timeout (as R10) |
| m3r1-aud-lbf-res (`--mode mission --mission-ticks 40 --res 1600x900 --shot 30`) | `catshot: 1 files (tick=30 ... size=1600x900 ui=1600x900 letterbox=none ...)`, `options rows added: screenWidth 1600;screenHeight 900`; the PPM 1600x900; home-template still only TutorialNeeded / PlayIntros |
| m3r1-aud-lbf-sp (1,200 s) | universe running, still running at the 1,200 s timeout (rc 124, as R10); ASan 0, no unsuppressed leak, 0 "fatal" lines; 12 animatics skipped (Animatic01 .. 12); last screenshot golden-12-00352 (as the producer's m3r1-lbf-sp) |

### The setting's surfaces and the docs

- cfg row: utility.c utyOptionsList after nisInterpolate, `letterboxBehind` scriptSetUdwordCB, default FALSE
  (render.c); written by every utyOptionsFileWrite (the toggle runs' files).
- switch: main.c `/letterboxBehind`, run-only through rndLetterboxFileValues around utyOptionsFileWrite (toggle-sw).
- console row: Console.c `CAT_CON_HWI_ROW_ID + 2`, CAT_CON_HWI_ROWS 3 (console self-test +1 / +1).
- tools/play.sh header: the three surfaces and the default; `--help` now prints the header to `set -euo` (checked by
  running its sed expression on the file, not the launcher).
- tools/m2_harness.sh header: `--shot`, `--shot-letterbox`, `--res`, `--cfg` documented and working as written
  (above); `tools/ppm_region.py` info / stats / probe / diff / bands used above and agree with my own reader.
- THE USER'S CHECK: `tools/play.sh` exists (release, play-home), New Game -> the opening NIS n01 and the arrival
  nislet01 in Mission01 exist (segments 0 and 1 of the capture); the moment (nislet01 ~46-50 s = ticks 738-792 at 16
  Hz; ~1:12 = 1143-1191; ~1:40 = 1599-1605) matches the captures; the hull in the top bar at s1 789 is what the
  old look shows; the setting row and `-- /letterboxBehind` exist. Accurate.

## Findings

| id | severity | finding | evidence | action |
|---|---|---|---|---|
| F1 | low (doc / comment) | The CAT-FIX reason says the exe's full-bars branch restores an "uninitialised" ebx. ebx is set at 0x0057a4ba to the top bar's GL y (float bits, nonzero), so the exe deterministically leaves additive blending ON after full bars (glBlendFunc(GL_SRC_ALPHA, GL_ONE) unless 0x00a75ef0 & 2); the subtitles and cursor drawn after the bars then blend additively in the exe | exe 0x0057a48e / 0x0057a4ba / 0x0057a8c8 .. 0x0057a8ea | fixed: render.c comment, report (2 places), delta_map_review.json note (merge + report rerun), each marked [audit] |
| F2 | low (doc) | Report's fade frame: "bottom rows 688-767 all 32,32,32 (79,872 of 79,872)": rows 688-767 are 81,920 pixels (79,872 = rows 690-767) | ppm_region.py stats on the producer's and my shot.584.6.ppm | fixed [audit] |
| F3 | low (doc) | Report's fade frame: "top rows 10-77 all 32,32,32": rows 10-17 hold 50 cursor pixels (x 0-11) | same | fixed [audit]: top rows 0-79 32,32,32 but the cursor's 90 |
| O1 | open question (integrate) | The CAT-FIX itself: the exe's look under full bars has additive blending on for the subtitles / cursor drawn after them; the port keeps the state it found. A deviation with a visible consequence (subtitle text over the scene), justified as a state leak of a wrong value; the integrate / user may want the exe's state instead | F1 | stated, not changed |
| O2 | note | rndAdditiveBlends: the exe's guard `0x00a75ef0 & 2` is absent from the tree (GoK's body, decision 28); the delta map says IDENTICAL (HW1 vs exe), the port only relies on it | exe 0x0057afa8 | none |
| O3 | note | catShotFrameEnd leaves GL_PACK_ALIGNMENT 1 for the run (stated in the report); harmless while `3*W % 4 == 0` | MotionTest.c | for producer 5 (resolution work) |
| O4 | open (producer's) | the letter height at 3840x2160: GoK 277 rows vs the exe's 42 (confirmed by the exe's inline divide) | exe 0x005693c7, NIS.h:73 | the producer's open question stands |

## Precision

Sampled checks of the producer's claims: 37 (the 14 exe rows of section 2 that are claims, the scope / tags /
CAT-TODO / generator checks (4), the PPM statements (9: default and behind bars, the cursor 89, behind = ce4fab4,
fade bottom count, fade top, fade diffs, the 31 Mission01 frames and their counts, the KAS frames), the 8 dumps and
the crc frame counts (3 statements), the 5 stream statements, the 7 self-test / mode statements, the surfaces and
docs (3), the user's check (1)) minus the 2 observations = 37 judged; wrong: 3 (F1, F2, F3); correct: 34.
**Precision 34 / 37 = 0.919.**

## Fixes (3, all doc / comment; no code behaviour changed)

1. src/SDL/render.c rndDrawScissorBars' CAT-FIX comment: the exe's ebx is the bar's GL y, not uninitialised; states
   the exe's additive-on consequence.
2. docs/m3-r1-letterbox-front.md: the same wording in (b) and "For STATE.md"; the fade frame's pixel counts.
3. ghidra/delta_map_review.json note of 0x0057a410 (+ `delta_map.py merge` / `report`: delta_map.json,
   docs/m2-r1-function-deltas.md).

Builds after the fixes: debug and release green, 0 warnings (section 1).

## Reference for the next port (this audit's runs; judge with hwo_compare, not sha256 across builds)

- Mission01 headless: m3r1-aud-lbf-mission-a / -b d9c83eb49837ebf2 (1,476,370 B, 6,001 T), IDENTICAL to r10i-mission-a
  in every field; release m3r1-aud-lbf-mission-rel 99d9998c881dae7d.
- missiontest Mission01: m3r1-aud-lbf-missiontest-a 7b79ad092284ba69 (6/6, 11,424); Mission02: m3r1-aud-lbf-m2
  cafab2552fdb85eb (6/6, 25,936).
- Dumps: motion noi 183681235a3fdc7b, motion on 428fff6c765ab81b, nis noi 2a7fec4de8cfe716 (nocrc 196bab32d244f614),
  nis on 46df1d391a86a404 (nocrc 7ce8472514254f4f); with letterboxBehind 1: R10's 13ca70eea998e5de / 6a4c1850799187ac.
- Captures: m3r1-aud-lbf-nis-shot/shot.584.6.ppm (default), -nis-shot-behind (old look), -nisfade-a, -m1seg-a.

Verdict: PASS-WITH-FIXES - the exe reading (one unwritten .bss word restores the depth test OFF after the popup menu; no 3D clip in the exe) and the one-line port hold; bars are bar-coloured over the ship in the full, fading and KAS letterbox with the default and the old look returns bit for bit with letterboxBehind 1; no stream moved in any field, dumps move in crc only as stated; 3 doc / comment errors fixed (the "uninitialised" ebx, two fade pixel counts)
