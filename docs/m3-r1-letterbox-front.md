<!-- done: a-exe-read, b-port, c-setting, d-capture, e-fence -->
# M3-R1 producer 1: the letterbox bars in front of the scene (letterbox-front)

Subsystems: (a) exe-read: the exe's letterbox path, (b) port: the bars in front, (c) setting: `letterboxBehind` on its
surfaces + the toggle self-test, (d) capture: `/catShot` + `--shot` + `tools/ppm_region.py` (+ `--res`, `--cfg`),
(e) fence: PPMs, dumps, streams.

Base ce4fab4. All five subsystems are finished.

## (a) The exe's path (cataclysm.exe 1.01, export cataclysm_gog_r6)

- `rndRenderTask` 0x0057a910: clear; `rndScissorEnabled = glIsEnabled(GL_SCISSOR_TEST) & 0xff` (0x0057aa1a, stored
  0x00a75d84); `regFunctionsDraw` 0x004f0ae0; then `if (nisScissorFade != 0 || (nisFullyScissored &&
  !rndScissorEnabled)) rndDrawScissorBars(rndScissorEnabled)` (0x0057aa43 .. 0x0057aa6f); `glDisable(GL_SCISSOR_TEST)` (0x0057aa77);
  then the subtitles, the sync status, the cursor. Identical to the tree's render.c.
- `glIsEnabled` is the GL DLL's own (`DAT_00b3a5a8 = GetProcAddress(..., "glIsEnabled")`); rGL returns its state
  (`ctx->ScissorTest`, reference/hw_rgl/kgl.c:2020), and the frame ends with `glDisable(GL_SCISSOR_TEST)`, so the flag
  is FALSE every frame in the exe too: the scissor box of `mrRegionDraw` 0x00569260 (0x005693bc / 0x00569eb3) never
  runs there either. The exe does NOT clip the 3D view.
- `rndDrawScissorBars` 0x0057a410: GL_QUADS, black (fade 0) or the grey 0.5 strip then black at alpha = fade; it does
  not touch the depth test: the bars take whatever depth state the regions left.
- The difference is the depth state the main region leaves: `cat_popupMenuDraw` 0x004ee120 (the last depth call of
  `mrRegionDraw`) ends with `push [0x00a57628]; call cat_rndDepthTestEnable` (0x004ee5bd): a .bss word with no other
  reference in the exe (one hit in the whole file, file offset 0xee5bf), so it passes 0 and the depth test stays OFF.
  The tree's port passed TRUE: the late bars were depth-tested (GL_LEQUAL at z = 0, depth 0.5) against the scene, and
  any geometry nearer than about twice the near clip plane (CAMERA_CLIP_NEAR 20) drew over them.
- The user's report reproduced on the M2 tree (ce4fab4's debug engine, nis mode, n06 26-36 s, `/catMotionShot 1374`):
  the sRecon (dump object O9, screen 514,633, depth 39) shows through the bottom bar: 29,254 non-black pixels in rows
  661-767 (bbox x 322-710). The launching session's probes (now removed) logged the state at the late bar draw:
  depth test on, GL_LEQUAL, prim mode on; the depth went back on between `cat_popupMenuDraw` and the end of
  `mrRegionDraw`.
- Where the tree diverged: not GoK and not the scissor flag (both are the exe's), but the M2-R6 port of
  `cat_popupMenuDraw` (src/Game/PopupMenu.c), which ended with `cat_rndDepthTestEnable(TRUE)`. The other depth calls
  of the main region agree with the exe: `cat_moRender` 0x004aafb0 ends with `push [0x008853fc]` (initialised 1, only
  read: TRUE) and returns early while `nisFullyScissored`; `cat_shipPrintDraw` 0x00510e60 ends with `push 1` (0x005114c0);
  `mouseDraw` turns it off (mouse.c, as the exe's 0x00a75928 word).
- The exe does not clip the 3D view to the letterbox in any path (fully scissored, fade in, fade out, the KAS
  letterbox): the bars are drawn after every region, before the subtitles (`subTitlesDraw` for the four subtitle
  regions, the letterbox bar's included), the sync status and the cursor, so those stay on top. The 2D regions drawn
  inside `regFunctionsDraw` (in-game menus) lie under the late bars in the exe as in the tree. The animatics
  (Animatic.c) do not use this path (no NIS_LetterHeight / scissor there).

## (b) The port: the bars in front (src/)

- **src/Game/PopupMenu.c** `cat_popupMenuDraw`: the last call is `cat_rndDepthTestEnable(letterboxBehind)`:
  `CAT: the exe passes 0x00a57628, a .bss word nothing writes (cat_popupMenuDraw 0x004ee120, 0x004ee5bd)`. With the
  default 0 the depth test stays off for the rest of the frame, as in the exe, so `rndDrawScissorBars` (and the black
  fade quads of `mrRegionDraw`) draw over everything of the scene. This one line is the fix of item 1 in every path:
  the NIS fully scissored, its fade in / out, the KAS letterbox (kasfWideScreenIn / Out set the same
  nisScissorFadeIn / Out / nisFullyScissored). Nothing else reads the depth state before the next frame's
  `mouseDraw` turned it off anyway (mouse.c 941), so frames without a letterbox change only where a 2D element drawn
  after the popup menu (the level fade, the white flash, the NIS black fade, the subtitles, the sync text) met
  geometry nearer than the 0.5 depth: the exe's look there too.
- **src/SDL/render.c** `rndDrawScissorBars`: `CAT:` the fading bars blend normally: the exe turns additive blending off
  for the fade branch and back after (0x0057a6ee .. 0x0057a703 and 0x0057a8c8 ..); `cat_moRender` 0x004aafb0 leaves it
  on (it draws while the bars fade, not while they are full), and an additive black at alpha = fade adds nothing, so
  the fading bars stayed grey 0.5 instead of going to black. `CAT-FIX:` the exe's full-bars branch restores a
  wrong value into rndAdditiveBlending at 0x0057a8c8: ebx, loaded at 0x0057a4ba with the bar's GL y (float bits,
  nonzero), so the exe leaves additive blending ON after full bars (glBlendFunc(GL_SRC_ALPHA, GL_ONE) unless flag
  0x00a75ef0 & 2) [audit: the value is set, not uninitialised]; the port restores the state it found. The blend change is under `!letterboxBehind` (the setting at 1 keeps the M2 frames bit for bit).
- **Not ported (stated):** a scissor box or viewport for the 3D view. The task's lead was right that the flag is
  always FALSE, but so it is in the exe (a per-frame state: the GL DLL's own glIsEnabled after the previous frame's
  glDisable), so the exe draws the scene full-height and covers it with the late bars; clipping would be a deviation
  with no pixel of difference once the bars are drawn without the depth test (the fence below: every bar row is the
  bar colour). The dead `if (... && rndScissorEnabled)` blocks of mrRegionDraw stay verbatim (the exe has them).
- **The letter height** stays GoK's `NIS_LetterHeight` (2.39:1 above an aspect of 1.5, else the exe's 1.85:1, at least
  H/15). The exe computes `(H - W*20/37)/2` at every aspect (rndDrawScissorBars 0x0057a437 .., mrRegionDraw): at
  1024x768 both give 107 rows (every harness run), at the user's 3840x2160 the exe's bars are 42 rows and GoK's 277.
  Left for the user (open question), not changed: it is GoK's deliberate widescreen choice and moves the user's look.

## (c) The setting: HWI-FEATURE(letterbox-setting) `letterboxBehind`

The launching session's name is kept: it matches the options file's camelCase rows (objInterpolate, nisInterpolate)
and says what 1 does. Default **0** (the exe's bars in front); **1** = the M2 frames bit for bit (the depth test left on
after the popup menu, the fading bars' blend untouched).

| surface | where | behaviour |
|---|---|---|
| Cataclysm.cfg row | src/SDL/utility.c utyOptionsList, after nisInterpolate: `letterboxBehind 0\|1` (scriptSetUdwordCB) | read by utyOptionsFileRead, written by every utyOptionsFileWrite (the start-up's and each Accept's save) |
| switch | src/SDL/main.c `/letterboxBehind` (next to /noNISInterp) | **run-only**, as /noInterp: sets 1 for the run; utyOptionsFileWrite writes the file's own value (render.c rndLetterboxFileValues around the write, rndLetterboxBehindSwitch / SwitchEnd / SwitchStands / FileValue). Why: the start-up's opOptionsSaveSettings writes the options file, so a non-run-only switch would turn a one-off comparison run into the saved setting |
| console row | src/Game/Console.c, HWI row `CAT_CON_HWI_ROW_ID + 2` (type 5, `letterboxBehind`), CAT_CON_HWI_ROWS 2 -> 3 (the console self-test's shown / set counts follow it) | sets the variable at once; not in the console's config file (HWI rows are left out, as before). The retail console has no opener (Console.c header), so for play the cfg row is the way |
| Options-screen control | none (the task asks for none) | |
| tools/play.sh | header: the three surfaces and the default; `--help` now prints the whole header | |

**Self-test:** the toggle mode (`/catToggleTest`, src/Game/MotionTest.c `mtToggleLetterbox`, run before the done
line; its failures count into `toggletest: done ... fails=N result=`): the start-up's save wrote the options file's
value (1 row); with the switch, the start state is 1 over the harness file's 0; the switch ends at the file's value;
for 1 then 0: Accept's save (opOptionsSaveSettings) writes the row, the variable is scrambled and
catUtyOptionsFileReadBack reads it back; the console row sets 1 and 0; the switch is run-only (a save under it writes
0); the start state and the file row are back at the end. Lines `toggletest: letterbox ...`. Run plain and with
`-- /letterboxBehind`.

## (d) The shared capture tools

- **Engine switches** (src/Game/MotionTest.c, src/SDL/main.c; HWI-TEST): `/catShot <spec>`, `/catShotFile <prefix>`,
  `/catShotLetterbox`. `catShotFrameEnd` (called in rndRenderTask right before the motion test's frame end, after the
  bars, the subtitles and the cursor, before the flip) reads the PHYSICAL framebuffer (`SDL_GL_GetDrawableSize`, not the
  UI size) with glReadPixels and writes `<prefix>.<tick>.<frame>.ppm` (P6, top row first). `<spec>`: comma list of
  `<tick>[:<frame>]` or `<from>-<to>[/<step>][:<frame>]` (universe ticks; `<frame>` = the n-th frame drawn in that tick,
  default 0; up to 64 items, 4,096 files a run). After a level load (the tick counter restarts) the files are
  `<prefix>.s<N>.<tick>.<frame>.ppm`. One log line per file: `catshot: tick=T frame=F segment=N size=WxH ui=WxH
  letterbox=full|fade=F|none letterboxBehind=0|1 file=...`. Nothing runs when no shot is due; the read-back changes
  only GL_PACK_ALIGNMENT (the fence: every stream and dump hash below is unchanged by it).
- `/catNisScissorOut <tick>:<seconds>` (nis mode only; HWI-TEST): starts the letterbox fade-out at that tick as an NIS's
  ScissorOut event does (nisScissorFadeOut, the fade time from 0), so a fade frame with a ship in the bars can be
  captured. A measurement only, never a dump baseline.
- **Harness flags** (tools/m2_harness.sh, documented in its header): `--shot SPEC` (any mode: `/catShot SPEC
  /catShotFile <run>/shot`; the summary gains a `catshot:` line), `--shot-letterbox`, `--res WxH` (appends screenWidth /
  screenHeight to the run's own home Cataclysm.cfg, never the template), `--cfg 'KEY VALUE'` (repeatable, the same
  way; the summary prints the added rows).
- **tools/ppm_region.py** (python3 -I, standard library only): `info`, `stats` (mean / min / max per channel, distinct
  colours, the N most common, with `--colour r,g,b --tol T` the match / other counts), `probe x,y ..`, `diff A B
  [--region] [--tol] [--out mask.ppm]` (count, bbox, largest difference; exit 1 when any differ), `down2 IN OUT` (2x
  box downscale, for producer 5's 4K-to-1080 comparison), `bands` (rows from the top / bottom entirely one colour: the
  bars). Regions are `x0,y0,x1,y1`, end exclusive, rows from the top.

## (e) The fence (debug ASan build unless stated; runs in original/scratch-m2/m2r2-harness/runs/)

### PPM pixels (1024x768: the bars are rows [0, 107) and [661, 768), NIS_LetterHeight 107; bar colour 0,0,0 full)

- **(a) NIS fully scissored, a ship in the bar band.** nis mode, n06 26-36 s, `/catMotionShot 1374` (dump frame 1374,
  tick 584: the sRecon O9 at 514,633, depth 39, crossing the bottom bar). `m3r1-lbf-nis-on` (default): bottom rows
  661-767: 109,568 of 109,568 pixels 0,0,0 (`bands`: bottom_rows=107); top rows 0-106: 109,479 black + 89 (all inside
  the cursor's 16x24 box at 0,0, drawn after the bars as in the exe). `m3r1-lbf-nis-behind` (`--cfg 'letterboxBehind
  1'`): 29,254 non-black pixels in the bottom bar (bbox x 322-710, rows 661-767), the frame IDENTICAL (`ppm_region.py
  diff`: differ=0) to ce4fab4's own (`m3r1-lb-base-nis1374`, the launching session's ce4fab4 debug binary).
- **Mission01's cutscenes**, missiontest NIS-first start, `--shot 0-2400/3 --shot-letterbox` (`m3r1-lbf-m1seg-a` vs
  `m3r1-lbf-m1seg-behind`, final binary; both streams IDENTICAL to r10i-missiontest-a, ASan 0): 1,390 letterbox frames,
  590 of the opening NIS n01 (segment 0) and 800 of the arrival NIS nislet01 that plays when the fleet reaches
  Mission01 (segment 1, `shot.s1.*`). 31 frames differ between the settings: n01 ticks 1128 / 1131 (242 / 220 pixels
  in the top bar in the old look) and nislet01 ticks 738-765, 786-792, 870-879, 1143-1191, 1377, 1599-1605, 1662-1665
  (380 .. 65,115 pixels, e.g. s1 tick 789: 65,115 in the top bar, the hangar hull; s1 tick 747: 51,099 in the bottom
  bar). In every one of the 31 the default's bar rows hold 0 non-black pixels, top and bottom; the other 1,359 frames
  are byte-identical. (An earlier pair, `m3r1-lbf-m1nis-*`, ran before the capture had segments: its nislet01 files
  overwrote n01's of the same tick; superseded.)
- **A fade frame with a ship in the band.** No NIS of the runs fades while geometry is that near, so the nis mode
  forces the fade out at the same moment: `-- /catNisScissorOut 580:1`, `--shot 576-598/2:6` (`m3r1-lbf-nisfade-a` /
  `-behind`). Tick 584 frame 6 (= dump frame 1374), fade 0.75, bars 80 rows: default bottom rows 688-767 all
  32,32,32 (81,920 of 81,920: the grey 0.5 at black alpha 0.75, 128 x 0.25), top rows 0-79 32,32,32 but for the cursor's 90 pixels (x 0-11, y 0-17) [audit: was "79,872 of 79,872" (rows 690-767 counted) and "top rows 10-77 all 32,32,32" (rows 10-17 hold 50 cursor pixels)]; the old look
  differs in 21,873 pixels (bbox x 396-710, rows 688-767: the recon). Tick 582 (fade 0.875, 93 rows): 28,140 pixels
  differ; tick 586 (0.625): 9,256; ticks 588 / 590: 0 (the recon has left the bars).
- **(b) The KAS letterbox (Mission02's letterbox jump):** missiontest `--mission 2 --shot 25500-25936/2
  --shot-letterbox` (`m3r1-lbf-m2-a` / `-behind`, before the fade-blend change): 64 frames, the fade in from tick 25778
  (fade 0.0208) to 25824 (0.9792), full from 25826; all 64 byte-identical between the settings (no geometry nearer than
  the bar depth in the mission camera); at tick 25800 (fade 0.479) the top 51 rows are 66,66,66 (128 x 0.521) but for the
  cursor's 90 pixels. The fade bars blend normally there already (additive blending was off), so the blend change does
  not touch that frame.

### Dumps (the crc= column is the frame's CRC; "nocrc" = the dump hashed with that column stripped)

| dump | R10 | default (this round) | letterboxBehind 1 |
|---|---|---|---|
| motion /noInterp | 183681235a3fdc7b (nocrc 3016280e9e0dd3c8) | `m3r1-lbf-motion-noi` 183681235a3fdc7b EQUAL | `-noi-behind` 183681235a3fdc7b EQUAL |
| motion flags on | 428fff6c765ab81b (nocrc 428ef41479eb5749) | `m3r1-lbf-motion-on2` 428fff6c765ab81b EQUAL | `-on-behind` (switch) 428fff6c765ab81b EQUAL |
| nis /noInterp | 13ca70eea998e5de (nocrc 196bab32d244f614) | `m3r1-lbf-nis-noi` 2a7fec4de8cfe716, nocrc 196bab32d244f614 IDENTICAL; crc moved on 279 frames: 1043-1168, 1268-1420 | `-noi-behind` 13ca70eea998e5de EQUAL |
| nis flags on | 6a4c1850799187ac (nocrc 7ce8472514254f4f) | `m3r1-lbf-nis-on2` 46df1d391a86a404, nocrc 7ce8472514254f4f IDENTICAL; crc moved on 277 frames: 1034-1159, 1262-1412 | `-on-behind` (switch) 6a4c1850799187ac EQUAL |

The moved frames are the two passes of the sRecon through the bottom bar nearer than the bar depth (ticks 546-560 and
572-588); the motion dump (no letterbox) does not move at all, with either setting. Re-baseline the two default nis
dumps to 2a7fec4de8cfe716 / 46df1d391a86a404 (reason: the bars in front, PopupMenu.c).

### Streams (hwo_compare, every field)

| stream | run | result |
|---|---|---|
| headless Mission01 (6,000 ticks, debug) | `m3r1-lbf-mission-fa` (sha256 d9c83eb49837ebf2, 1,476,370 B) / `-fb` | IDENTICAL to r10i-mission-a over 6,001 ticks, every field (rng_all included); the pair IDENTICAL |
| headless Mission01 (release) | `m3r1-lbf-mission-rel` | IDENTICAL to r10i-mission-rel-a |
| missiontest Mission01 (NIS-first) | `m3r1-lbf-missiontest-fa` / `-fb` | 6/6 PASS, done 11,424; IDENTICAL to r10i-missiontest-a (11,426 ticks); the pair IDENTICAL |
| Mission02 self-test (the letterbox jump at 25,776) | `m3r1-lbf-m2-fa` | 6/6 PASS, done 25,936; IDENTICAL to r10i-m2-a (25,905 ticks) |
| Mission02 / Mission01 with `--shot` and either setting | `m3r1-lbf-m2-a`, `-m2-behind`, `m1nis-a`, `-behind`, `m1seg-a`, `-behind` | IDENTICAL to the above (the capture and the setting move no field) |

No stream moved in any field; nothing to re-baseline.

The final binary (after the toggle self-test's fix below) gave `m3r1-lbf-mission-final`: IDENTICAL to r10i-mission-a
(every field, 6,001 ticks; sha256 d9c83eb49837ebf2, 1,476,370 B, ASan 0) and `m3r1-lbf-nis-on-final` 46df1d391a86a404
(as `m3r1-lbf-nis-on2`). The rest of the batch ran on the binary before that fix, which only touched
`mtToggleLetterbox` (it runs only under /catToggleTest).

### The plain-harness runs

| label | mode | milestone | first stop | result |
|---|---|---|---|---|
| m3r1-lbf-fe | fe, 120 s | Main_game_screen | none: still running at the 120 s timeout (as R10) | ASan 0 |
| m3r1-lbf-toggle2 | toggle | toggle self-test done | none (rc=0) | `done start=1/1 fails=0 result=PASS`, the five `toggletest: letterbox` lines, ASan 0 |
| m3r1-lbf-toggle2-sw | toggle `-- /letterboxBehind` | toggle self-test done | none (rc=0) | PASS: start 1 over the file's 0, the saves write 0, the switch stands at the end; ASan 0 |
| m3r1-lbf-hud | hud | hud self-test done | none (rc=0) | `done tick=599 boxed=19 selected=154 frames=537 ... result=PASS`, ASan 0 |
| m3r1-lbf-ui | ui | ui self-test done | none (rc=0) | `done selected=154 rows=10 popupframes=16 commands=26 targetrows=1 statsframes=8 result=PASS`, ASan 0 |
| m3r1-lbf-fescreens | fescreens | fescreens self-test done | none (rc=0) | fescreentest 22 files / 195 screens PASS; optionstest 34/34 PASS; ASan 0 |
| m3r1-lbf-console | console | console self-test done | none (rc=0) | `done selected=154 frames=4 shown=32 set=25 commands=8 keys=32 kas=6 closed=1 failures=0 result=PASS` (shown / set +1: the new row), ASan 0 |
| m3r1-lbf-sp | sp, 1,200 s | universe running | none: still running at the 1,200 s timeout (as R10) | ASan 0, no fatal, 12 animatics skipped (Animatic01 .. 12), last screenshot golden-12-00352 (R10's r10i-sp: golden-12-00128) |
| m3r1-lbf-mission-fa / -fb / -rel / -final | mission 6,000 | oracle mission run done | none (rc=0) | IDENTICAL to R10 (above) |
| m3r1-lbf-missiontest-fa / -fb, m3r1-lbf-m2-fa | missiontest | mission self-test done | none (rc=0) | 6/6 PASS (above) |
| m3r1-lbf-{motion,nis}-{noi,on2,noi-behind,on-behind} | motion / nis | motion self-test done | none (rc=0) | the dump table above; ASan 0 |

`m3r1-lbf-toggle-sw` (the first run with the switch) FAILED 2 checks: the test read letterboxBehind after the Smooth
motion checks, whose `catUtyOptionsFileReadBack` parses the whole options file again (the file's 0 over the switch's 1).
Fixed in the test (the start state taken before those checks); the engine was right. Builds: `tools/build.sh --debug`
and `--release` green, 0 warnings under -Werror, after every subsystem and at the end; `tools/m2_layout32.sh` OK.

## Files changed (git diff ce4fab4)

- src/Game/PopupMenu.c: the depth restore (CAT, 0x004ee120 / 0x004ee5bd) under HWI-FEATURE(letterbox-setting).
- src/SDL/render.c / render.h: `letterboxBehind` and its run-only switch state (rndLetterboxBehindSwitch,
  rndLetterboxSwitchEnd, rndLetterboxSwitchStands, rndLetterboxFileValue, rndLetterboxFileValues);
  rndDrawScissorBars' fade blend (CAT) and additive restore (CAT-FIX); the `catShotFrameEnd` call in rndRenderTask.
- src/SDL/utility.c: the `letterboxBehind` options row; the switch's file values around utyOptionsFileWrite.
- src/SDL/main.c: `/letterboxBehind`, `/catShot`, `/catShotFile`, `/catShotLetterbox`, `/catNisScissorOut`.
- src/Game/Console.c: the console row, CAT_CON_HWI_ROWS 3.
- src/Game/MotionTest.c / .h: the capture (catShotSet, catShotFileSet, catShotLetterboxSet, catShotFrameEnd),
  /catNisScissorOut, the toggle self-test's letterbox checks (mtLetterboxFileRow, mtToggleLetterbox).
- tools/m2_harness.sh: `--shot`, `--shot-letterbox`, `--res`, `--cfg` (header documented). tools/ppm_region.py: new.
  tools/play.sh: the setting documented; `--help` prints the whole header.
- ghidra/delta_map_review.json (+ delta_map.json, docs/m2-r1-function-deltas.md by `merge` / `report`).
- The launching session's uncommitted probes (LBDBG / LBP / LBR dbgMessagef lines in render.c, mainrgn.c, Region.c)
  were removed (a copy: the session scratchpad's launcher-letterbox-probes.patch); mainrgn.c and Region.c end equal
  to ce4fab4.

## Delta-map review entries (sample `port-M3-R1-letterbox-front` / notes `[review: M3-R1 letterbox-front]`)

0x0057a410 rndDrawScissorBars IDENTICAL -> **MODIFIED** (the fade branch's blend handling, GL_QUADS, the inline letter
height); notes on 0x0057a910 rndRenderTask (MODIFIED; the letterbox part compared, as the tree), 0x00569260
mrRegionDraw (MODIFIED; the dead scissor box, the inline letter height, the popup's depth word), and the IDENTICAL
0x004f0ae0 regFunctionsDraw, 0x0056fd10 primModeSetFunction2, 0x0056fd90 primModeClearFunction2 (compared, unchanged).
`python3 -I tools/delta_map.py merge --done m3r1:letterbox-front`: records changed 6; IDENTICAL 2,763 -> 2,762,
MODIFIED 1,574 -> 1,575, REWRITTEN 83; `report` rewritten. cat_popupMenuDraw 0x004ee120 is Cataclysm-new (no delta-map
row).

## CAT-TODOs

None closed, none added, none re-targeted; no CAT-TODO or CAT-PROVISIONAL names M3-R1 (`/bin/grep -rn "M3-R1" src |
/bin/grep -c "CAT-TODO\|CAT-PROVISIONAL"` = 0). The 23 stage-5 sites are untouched.

## THE USER'S CHECK (for the play-test brief)

1. `tools/play.sh` (release; the user's play-home; no switch). New Game, watch the opening cutscene and then the
   arrival cutscene when the fleet comes out of hyperspace in Mission01 ("Hiigara" area, the mothership's arrival
   nislet): about 46-50 s into the arrival cutscene fighters stream past and a large orange hull swings across the
   top of the frame; at about 1:12 and 1:40 ships pass the bottom edge. Look at the black bars: nothing of a ship or
   the hull may cross into them; every ship must disappear at the bar's edge. In the harness's capture of that
   moment (tick 789) the old build drew 65,115 pixels of the hull in the top bar, this one none.
2. The mission letterbox: any in-mission cutscene (e.g. Mission02's jump at the end) fades the bars in from grey to
   black; they must darken smoothly and stay in front.
3. The old look on request: add `letterboxBehind 1` to `original/play-home/.homeworld/Cataclysm.cfg` (or run
   `tools/play.sh -- /letterboxBehind` for one session): the M2 look, near ships over the bars. Remove the row (or set 0)
   for the default.
4. Report: any frame where a ship still shows in a bar with the default (the mission and roughly when), and whether the
   bars' height at 3840x2160 suits you (see the open question).

## For STATE.md

- Setting `letterboxBehind` (HWI-FEATURE(letterbox-setting)): Cataclysm.cfg row `letterboxBehind 0|1`, default 0 (the
  exe's bars in front); switch `/letterboxBehind` run-only (as /noInterp); console row `letterboxBehind` (HWI row id
  0x1002); no Options control.
- The cause: PopupMenu.c `cat_popupMenuDraw` restored the depth test to TRUE where the exe passes the unwritten word
  0x00a57628 (0); the exe never clips the 3D view (rndScissorEnabled is FALSE every frame in the exe too).
- rndDrawScissorBars: the fade bars forced to normal blending (CAT) and the additive state restored (CAT-FIX of the
  exe's wrong ebx: the bar's GL y, nonzero, which leaves additive blending on [audit: was "uninitialised"]); under `!letterboxBehind`.
- Capture: `/catShot <spec>`, `/catShotFile <prefix>`, `/catShotLetterbox`, `/catNisScissorOut <tick>:<s>`; harness
  `--shot SPEC`, `--shot-letterbox`, `--res WxH`, `--cfg 'KEY VALUE'`; `tools/ppm_region.py` (info, stats, probe,
  diff, down2, bands).
- Re-baseline: the default nis dumps /noInterp 2a7fec4de8cfe716 (was 13ca70eea998e5de; 279 crc frames) and flags-on
  46df1d391a86a404 (was 6a4c1850799187ac; 277 crc frames), every other column IDENTICAL; with `letterboxBehind 1` they
  equal R10's. Motion dumps and every stream unchanged.

## What is left (owner)

- The letter height at wide aspects: GoK's 2.39:1 (277 rows at 3840x2160) against the exe's 1.85:1 (42 rows): a user
  decision (open question); if the exe's is chosen, a one-line change of NIS_LetterHeight (owner: M3 play-test fixes,
  next round).
- tools/play.sh's older Smooth motion note still says the console opens with the tilde key and that /noInterp writes
  0 to the cfg; both stale since M2-R6 / M2-R7 (owner: producer 4, motion-jitter, who owns that setting this round).
