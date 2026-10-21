<!-- done: build, sites, exe, captures, click-fit, streams, diff, fixes, final-runs -->
# Audit: M3-R1 producer 5, ui-scale (the UI scale at 4K / 1440p and the 4K window)

Auditor's scope: the producer's report docs/m3-r1-ui-scale.md and `git diff fdf35ee` in the m2 worktree (no commit
after fdf35ee: the whole port is the working tree, 23 tracked files changed and 6 new). Runs on the plain harness
(tools/m2_harness.sh, silent and headless, `--bin` pointing at the audit's own clean builds); labels `m3r1-aus-*`
(the producer's tree), `m3r1-aus-z-*` (the audit's first fixed build), `m3r1-aus-y-*` (the audit's final build),
`m3r1-aus-proof-*` / `m3r1-aus-fix-*` (the fix proofs). No GPU measurement was made (correctness runs only), no
gamescope, no input injected, no Ghidra.

## 1. Builds (clean, separate directory)

`original/scratch-m2/build-audit-m3r1-ui-scale/`, meson set up exactly as tools/build.sh does (debug:
`--buildtype=debug -Db_sanitize=address,undefined`; release: `-Doptimization=2 -Ddebug=false -Db_sanitize=none`),
`-Werror` in build.ninja:

| build | tree | warnings | hwi-engine bytes |
|---|---|---|---|
| debug/, release/ | the producer's | 0 / 0 | 89,555,400 / 5,695,736 |
| final2-debug/, final2-release/ (clean) | after the audit's fixes | 0 / 0 | 89,588,336 / 5,699,832 |

The last two edits after final2 (a comment in uiscale.h, the play.sh header text) were compiled incrementally into
final2-debug / final2-release with 0 warnings (section 9).

## 2. The logical / physical table against the tree

`/bin/grep` of glViewport, glScissor, glReadPixels, SDL_GetWindowSize, SDL_GL_GetDrawableSize, glCopyTex*,
glPointSize, glLineWidth, glPixelZoom, glLineStipple and every MAIN_WindowWidth / Height (31 files) and
MAIN_WindowWidthActual / HeightActual site in src/Game and src/SDL (src/Win32 is not built: src/meson.build has no
Win32 subdir).

* Correct as classified: ShipView.c's viewport (GoK's factor) and its scissor, TradeMgr / ConsMgr / mainrgn scissors
  through `uiScaleScissor`, MotionTest.c's read-back and PPM, catShotFrameEnd and screenshot.c (drawable / Actual),
  Clipper.c's two logical conversions, render.c's star point sizes and BTG.c (Actual), rndAspectRatio, the prim2d
  widths, PiePlate's width and stipple, the 2D projection sites (render.c text, plug, lens flares CAT_LF_X), Options.c /
  main.c request sites, utility.c's rows, /catLodStats.
* **Finding F2 (missing, a defect):** main.c `mainStartupGL` / `mainStartupParticularRGL` -> `mainRescaleMainWindow`
  -> `utyChangeResolution(MAIN_Window*)` runs BEFORE the new window exists, i.e. at the REQUEST, on every mode switch
  (the Video screen's resolution list via opOptionsAcceptHelper, mainRestoreRender). Nothing laid the front end out
  again at the logical size afterwards. Proof (`m3r1-aus-proof-us-4k`, the producer's code with the audit's check,
  a real `mainLoadGL` from 3840x2160 to 2560x1440): `modeswitch ... logical=1707x960 scale=150 main=2560,1440 options
  button=1178,766` and `FAIL mode switch`: the main region stayed 2560x1440 and the main menu sat off-centre (it belongs
  at 751,526). The producer's own user check (item 5) sends the user down this path ("pick a smaller resolution in
  Options -> Video"). Fixed (section 7).
* Not listed (not built, n/a): utility.c utyChangeResolution's Emscripten glViewport, render.c's HW_ENABLE_GLES
  SDL_CreateWindow. Noted in the report.

## 3. The exe (cataclysm_gog_r6 via tools/m2_fdec.py / m2_fdis.py / m2_exedata.py), 12 bodies

| function | what was checked | result |
|---|---|---|
| mousePositionSet 0x0056e8d0 | utyClientRectGet, clamp 0 .. w-1 / h-1, SetCursorPos | as the report; = HW1 Win32/mouse.c:457 (MOUSE_CURSOR_CLAMP) |
| feAllScreensReposition 0x00474ae0 | `(MAIN_WindowWidth - 0x280) / 2`, `(MAIN_WindowHeight - 0x1e0) / 2`, then all screens deleted and Main_game_screen started when no game runs | as the tree |
| cat_moSelectedDraw 0x004a6bb0 | `x = MAIN_WindowWidth - DAT_0072ace4`, 0x0072ace4 = 150 (m2_exedata) | as the report |
| mouseDraw 0x0056ec50 | the 0.325 pixel centre (0x0072a858 = 0x3ea66666) | as the report (decision 29) |
| btgConvertStar 0x00428c20 | half size = width * double 0x0072a480 = 0.5, no resolution term | as the report; = HW1 Game/btg.c:776 |
| primLineThick2 0x00571440 | glPushAttrib(GL_LINE_BIT), glLineWidth(thickness), (x + 0.325) / MAIN * 2 - 1 | as the report |
| primModeSetFunction2 0x0056fd10 / primModeClearFunction2 0x0056fd90 | no glLineWidth in the exe | as the report (the width is HWI-FEATURE) |
| rndBackgroundRender 0x00574b90 | `0x400 < MAIN_WindowWidth` -> 1.2; the disassembly's switch 0x280 / 0x320 / 0x400 / 0x500 -> 2.1 / 2.5 / 2.65 / 2.8 (0x40066666 ...) | IDENTICAL stands (the decompile hides the switch; m2_fdis shows it) |
| WinMain 0x005631f0 | fullScreen clears showBorder; 0x80000000 / 0x06cf0008; W + 2 SM_CXFRAME (0x20), H + SM_CYCAPTION (4) + 2 SM_CYFRAME (0x21); CreateWindowExA at 0,0 | as the report |
| tmNumberRUsDraw 0x0052ded0, cmNumberRUsDraw 0x004578e0 | glScissor(x0, MAIN_WindowHeight - y1, w, h), depth clear | as the tree (MAIN units: the exe's only size) |
| svShipViewRender 0x00515010 | MAIN_WindowWidth - 1, MAIN_WindowHeight - y1 boxes | as the tree |
| setupPixelFormat 0x00574550 | the Win32 PIXELFORMATDESCRIPTOR only (the window is WinMain's) | IDENTICAL stands (GoK's SDL creation) |

* **Finding F3 (wrong delta-map rows):** the producer reclassified mousePositionSet, btgConvertStar, mainSaveRender
  0x005604e0 and mainSetupSoftware 0x00560550 IDENTICAL -> MODIFIED. The delta map compares the exe with HW1's
  source (docs/m2-r1-function-deltas.md), and `tools/delta_map.py show` puts each exe body next to an equal HW1 body
  (Win32/mouse.c:457 with the clamp; Game/btg.c:776 width / 2.0; Win32/main.c:2354 RGLtype + names; Win32/main.c:2365
  rgl.dll / sw / 640x480x16). The differences are GoK's tree. Reverted to IDENTICAL with the reason in the notes.
* **Finding F4 (edited IDENTICAL bodies without a note):** cmNumberRUsDraw 0x004578e0, tmNumberRUsDraw 0x0052ded0
  (the report even lists it as "no row"), primRoundRectOutline 0x005718b0 and setupPixelFormat 0x00574550 were edited
  by the port and carried no ui-scale review note. Notes added (decision 55: in delta_map_review.json, then
  `tools/delta_map.py merge` and `report`).

## 4. The captures and the comparison tool (the audit's builds)

| comparison | runs | tool | independent check |
|---|---|---|---|
| Main_game_screen 3840x2160 (200%) box-downscaled 2x vs 1920x1080, frames 400 / 800, tol 0 | aus-fe-4k / aus-fe-1080 (and z-) | every region 1.0000, mean 0, max 0 (buttons 47,632 px, title 77,355, version 4,278, panel 307,200, cursor 576) | my own P6 reader + 2x2 mean over fe-buttons: exact 1.0000, mean 0.000, max 0, 86% of the region lit |
| HUD 4K vs 1080p, ticks 100 / 300 / 450, area, tol 24 | aus-hud-4k / -1080 | PASS: info 0.9996 (mean 0.04 / 0.09), lists 0.9999, support 1.0000, cursor 1.0000 | hud-lists mean 0.050 max 30, info panel mean 0.044 max 53 (the tool's 0.05 / 0.04) |
| negative control: 4K at uiScale 100 vs 1080p | aus-fe-4k-100 / aus-fe-1080 | | fe-buttons exact 0.066, mean 88.3: the comparison does see a small UI |
| 2560x1440 (150%) vs 1707x960 | aus-fe-1440 / -960 | centre-sampled tol 0: every region 1.0000; area tol 24: 0.9340 / 0.9558 / 0.9294 / 0.9743 / 0.9288 (as stated) | |
| HUD 1440 vs 960, centre | aus-hud-1440 / -960 | 100 / 300: 0.9998 / 1.0000 / 1.0000 / 1.0000; 450: 0.9954 / 0.9961 / 1.0000 / 0.9844 (as stated) | |
| 1024x768 vs fdf35ee's own captures (m3r1-ui-base-fe / -hud, binary hwi-engine-debug-fdf35ee) | aus- and z-fe-1024, -hud-1024 | | ppm_region diff: fe differ=211 (the build string 905..1021, 756..765) at frames 400 and 800; hud differ=0 at 100 / 300 / 450 |
| 4K uiScale 100 vs fdf35ee at 4K | aus-fe-4k-100 / m3r1-ui-base-fe-4k | | differ=211 (3721..3837, 2148..2157), the build string only |

The logged lines: `uiscale: physical 2560x1440 window 2560x1440 scale 150% (auto, uiScale 0) logical 1707x960
factor 1.49971 x 1.50000`, 4K `scale 200% (auto, uiScale 0) logical 1920x1080`, 1024x768 / 1080p / 1707x960 100%.

## 5. The scripted click, the edges, the window fit, the overrides

* 4K (y-us-4k): `click miss window=2129,1205 -> logical=1064,602 button=858,586-1064,618 screen=Main_game_screen`
  (mouse.h mouseInRect: x1 exclusive, so 1064 is outside), `click hit window=1923,1205 -> logical=961,602 ...
  screen=Audio_Options`; 1440: miss 1435,813 -> 957 (the edge column), hit 1281,813 -> 854,542. As the report.
* Edges: 4K `window=1920,2159 -> logical=960,1079`, `3839,2159 -> 1919,1079`, 50 rows below -> 1079, right of the
  window -> 1919; 1440 `1280,1439 -> 853,959`, `2559,1439 -> 1706,959`.
* Window fit: `-- /testUsableBounds 3840x1960@3840x2160` logs `window: requested 3840x2160, usable 3840x1960 (display 0:
  3840x2160), created 1024x768 (fullscreen-desktop): display size -> fullscreen desktop`; `3840x2160@3840x2160` the same
  with usable 3840x2160 (the offscreen driver's own mode is 1024x768: the decision and flags are the check); the
  9-case table and the display choice in every uiscale run. Read myself: render.c setupPixelFormat calls
  uiScaleWindowFit before SDL_CreateWindow with the fitted size / flags / display position; uiScaleWindowFit only
  fits with a real desktop (not offscreen / dummy) or the test bounds; mouse.c clamps mousePoll's result and
  mousePositionSet to 0 .. MAIN_Window* - 1. **Judged against the user's cfg** (play-home Cataclysm.cfg, read only:
  screenWidth 3840, screenHeight 2160, fullScreen 0; the play log's `Current display mode: 3840x2160 24`): the request
  equals the display, so the rule gives a borderless fullscreen-desktop window of 3840x2160 on that display: the title
  bar and panel can no longer hide the bottom rows, the physical size (and the 200% auto scale) is what the user asked
  for. Sound. The exe's own windowed mode (WinMain) has the same problem on a desktop this size; the fit is an HWI-FIX.
* Overrides at 4K: `--cfg 'uiScale 100'` -> `scale 100% (cfg, uiScale 100) logical 3840x2160`; `-- /uiScale 150` ->
  `scale 150% (switch, uiScale 150) logical 2560x1440`; the control on both screens, the row read back, the switch
  run-only, the console row: the self-test, PASS at every size.
* **Finding F1 (a defect):** the Video screens' UI SCALE atoms were placed at their 640x480 coordinates
  (`atom->x = atom->loadedX = 500`), after feScreensImageLoad had centred the screen's own atoms
  (`x = feResRepositionCentredX(loadedX)`). At any logical size above 640x480 the control sat outside the panel:
  `control 150% button=500,325-546,341 (640x480 500,325)` at 1920x1080 logical (it belongs at 1140,625), and the same
  523,333 click point at every size in the producer's runs. The self-test clicked the region wherever it was, so it
  passed. Fixed (section 7). An atom dump of both screens (a throwaway build) shows the positions overlap no control,
  only the panels and the right column's cut-out region (Video_Options: under SMOOTH MOTION 500,268, below the
  Keyboard tab 502,201..233; Video: above EFFECTS 373,119).

## 6. The diff

* Scope: every changed file is in the producer's list; no orchestrator or user commit after fdf35ee.
* Tags: `HWI-FEATURE(ui-scale)` 290 lines in src (as stated, 0 at fdf35ee); untagged added lines are only inside
  blocks under a CAT: / HWI-TEST comment (the mousePositionSet clamp, TestUsableBoundsSet, the self-test dispatch) or
  split GoK lines. **Finding F5:** `/noBorder` -> SDL_WINDOW_BORDERLESS is the exe's WS_POPUP window (a Cataclysm
  behaviour) inside an HWI-FIX function without a `CAT:` tag: tagged.
* CAT-TODO: 36 -> 37 lines, owner tags 27 -> 28 (`/bin/grep`); the new one `CAT-TODO(M3-R2)` in BTG.c is one line,
  owner first, M3-R2 = "the fix round" of the M3 plan (docs/m2-r10-summary.md); `/bin/grep -rn M3-R1 src | grep
  CAT-TODO|CAT-PROVISIONAL` = 0; the 23 stage-5 sites untouched.
* Generated files: `tools/delta_map.py merge` + `report` into scratch copies reproduce ghidra/delta_map.json and
  docs/m2-r1-function-deltas.md byte for byte, before and after the audit's review edits.
* **Finding F6 (comment):** uiscale.h called `uiScaleUpdate` "per frame"; it runs only at a front-end Accept (and in
  the self-test). Fixed. **Finding F7 (doc):** tools/play.sh's header gave the log line as `usable WxH (display WxH)`;
  the engine prints `(display N: WxH)`. Fixed.
* The user's check: `tools/play.sh --rebuild` exists (play.sh:77), the setting / control / switch / log lines exist as
  named, New Game -> Mission01 is a real path; it names launcher, mission and moment, and what to report.
* Simulation: no field of any stream moved (section 8).

## 7. Fixes (9)

1. **src/Game/Options.c opUiScaleAddToScreen** (F1): the copies take the screen's centring (the copied radio button's
   `x - loadedX`, `y - loadedY`) while loadedX / loadedY keep the 640x480 positions, so a later
   feAllScreensReposition still works. `[audit: ...]` comment.
2. **src/SDL/uiscale.c uiScaleWindowCreated** (F2): when the window replaces an earlier one (a mode switch) and the
   main region is not the logical size, `utyChangeResolution(MAIN_Window*)` lays the front end out again at the
   logical size (logs `uiscale: relaid out at logical WxH after the mode switch`). Proof: the same real `mainLoadGL`
   run on the fixed build (`m3r1-aus-fix-us-4k`): `modeswitch ... logical=1707x960 main=1707,960 options
   button=751,526`, the check PASS. (That real mode switch also showed a pre-existing GoK fault: main.c
   mainDestroyWindow clears mainActuallyQuit, so the next SDL_QUIT is swallowed; the run ended rc 1 after "Quit game,
   baby!" without the normal shutdown and LeakSanitizer listed the task allocations. Not this port's; noted in the
   report's user check, owner M3-R2.)
3. **src/Game/UiScaleTest.c** (proof of 1 and 2, and a stronger Accept): + "the UI SCALE buttons at the exe's centring
   of the screen"; the 200% Accept is now a click on Video_Options' own OP_Options_Accept button inside the region
   processing (its callback opOptionsAccept -> opOptionsAcceptHelper: the real order, applied at once, ASan 0) instead
   of the producer's call-by-call copy, + "the ACCEPT button's click closes Video_Options"; + 7. utModeSwitch: the
   mode switch's order on the same window (uiScaleWindowRequest, mainRescaleMainWindow, uiScaleWindowCreated; a real
   mainLoadGL makes a second window and trips the GoK quit fault above, so the permanent check replays the steps)
   must leave the main region and the menu at the logical size. 42 -> 45 checks.
4. **src/SDL/uiscale.c** (F5): `CAT: /noBorder: the exe's WS_POPUP window (WinMain 0x005631f0)`.
5. **src/SDL/uiscale.h** (F6): the uiScaleUpdate comment.
6. **tools/play.sh** (F7): the log line's text.
7. **ghidra/delta_map_review.json** (F3): mousePositionSet, btgConvertStar, mainSaveRender, mainSetupSoftware back to
   IDENTICAL (`[audit: ...]` reasons); merged: IDENTICAL 2,762, MODIFIED 1,575, REWRITTEN 83 (as at fdf35ee).
8. **ghidra/delta_map_review.json** (F4): notes on cmNumberRUsDraw, tmNumberRUsDraw, primRoundRectOutline,
   setupPixelFormat; `merge` (8 records changed) and `report` regenerated delta_map.json and
   docs/m2-r1-function-deltas.md.
9. **docs/m3-r1-ui-scale.md**: `[audit: ...]` notes on the control's positions, the Accept proof, the check count,
   the missing table rows (the mode-switch layout, the two unbuilt sites), the delta-map rows, the user check's
   resolution-change path.

## 8. Streams and dumps (hwo_compare, every field; dumps whole and crc-stripped)

| stream / dump | run(s), build | sha256 (16) | against | result |
|---|---|---|---|---|
| headless Mission01, 6,000 ticks, debug pair | aus-mission-a / -b (producer's tree), z-mission-a / -b, **y-mission-a / -b (final)** | 787e98865eb42c38, 1,476,370 B, 6,001 T | r10i-mission-a (5bd51234405ea0d8...) | **IDENTICAL in every field, ticks 0-6000, rng_all included**; a / b IDENTICAL |
| the same, release | aus-, z-, **y-mission-rel** | 034e6f849e4488f4 | r10i-mission-a and r10i-mission-rel-a | IDENTICAL |
| missiontest Mission01 pair | aus-m1-a / -b, z-m1-a / -b | 5dd566a0a893f371, 3,246,588 B | m3r1-amj-m1-a (producer 4's audited), r10i-missiontest-a | IDENTICAL (ticks 0-11425); `done mission=1 ticks=11424 objectives=6/6 result=PASS` |
| Tutorial1 leg | aus-tutorial, z-tutorial | fec66f9a4d801958, 2,569,841 B | m3r1-fd3-tutorial, r10i-tutorial-a | IDENTICAL (ticks 0-10445); 12/12 lessons, PASS |
| motion /noInterp | aus-, z-motion-noi | c503c869f0dc9cdb / crc-stripped 3016280e9e0dd3c8 | m3r1-amj-motion-noi (producer 4's audited hashes) | EQUAL (cmp), both forms |
| motion flags on | aus-, z-motion-on | c0e411662fa2f527 / 428ef41479eb5749 | m3r1-amj-motion-on | EQUAL |
| nis /noInterp | aus-, z-nis-noi | 86cb155acffc180e / 196bab32d244f614 | m3r1-amj-nis-noi | EQUAL |
| nis flags on | aus-, z-nis-on | 0307f3be2044c29c / a5de13a44b8d264f | m3r1-amj-nis-on | EQUAL |

No stream and no dump moved in any field or column; nothing to re-baseline. The z- runs used final-debug (the audit's
fixes 1, 2 with the producer's Accept copy in the self-test); the final build differs from it only in UiScaleTest.c
(run by `--mode uiscale` alone), so the y- mission pair and the y- uiscale runs complete the final build's fence.

## 9. Self-tests and the plain-harness runs (ASan / LeakSanitizer from each summary)

| label(s) | mode | result |
|---|---|---|
| y-us-{4k,1440,1080,1024,fit1960,fit2160,4k-cfg100,4k-sw150} (final) | uiscale | `checks=45 fails=0 result=PASS` each, rc 0, ASan 0, no unsuppressed leak |
| aus-us-* (the producer's tree) | uiscale | 42/42 PASS each, ASan 0 |
| aus-proof-us-4k / aus-fix-us-4k (real mainLoadGL) | uiscale | proof: 2 FAILs (F1, F2); fix: 44/44 PASS; both rc 1 + leak list (the GoK quit fault, section 7) |
| aus-, z-fe-{1024,1080,4k}, aus-fe-{1440,960,4k-100} | fe, 120 s | Main_game_screen, running at the timeout, ASan 0 (the 4K fe included) |
| aus-hud-{1080,4k,1440,960,1024}, z-hud-1024 | hud | `done tick=599 boxed=19 selected=154 frames=537 statusbars=6840 infopanels=537 pie=537 result=PASS`, ASan 0 |
| aus-, z-ui | ui | `selected=154 rows=10 popupframes=16 commands=26 targetrows=1 statsframes=8 result=PASS`, ASan 0 |
| aus-, z-fescreens | fescreens | `optionstest: done options=35 startdefaults=35 readback=35 result=PASS`; `files=22 loaded=22 screens=195 pushed=195 popped=195 wrong=0 ... result=PASS`, ASan 0 |
| aus-, z-console | console | `shown=34 set=27 commands=8 keys=32 kas=6 closed=1 failures=0 result=PASS`, ASan 0 |
| aus-, z-keys / -strings / -toggle | keys, strings, toggle | `rows=197 ... failures=0 PASS`; `ids=1705 ... PASS`; `start=1/1 fails=0 PASS`; ASan 0 |
| aus-sp (producer's tree), 1,200 s | sp | running at the timeout, **ASan 0**, 12 animatics skipped, last screenshot golden-12-00464 (as the producer's) |
| aus-sp-4k (producer's tree), 3840x2160, 1,200 s | sp | running at the timeout, **ASan 0**, 11 animatics skipped, golden-11-01136, `scale 200% ... logical 1920x1080` |
| z-sp (the audit's fixed build), 1,200 s | sp | running at the timeout, **ASan 0**, no unsuppressed leak, 12 animatics skipped, golden-12-00464 |
| z-sp-4k (the audit's fixed build), 3840x2160, 1,200 s | sp | running at the timeout, **ASan 0**, no unsuppressed leak, 11 animatics skipped, golden-11-01248 |

## 10. Precision

Sampled checks of the producer's claims: 49. Correct 42: the two clean builds; the mission pair and the release run;
the missiontest pair; the Tutorial1 leg; the four dumps; the 4K / 1080p front end (tol 0, confirmed independently)
and HUD numbers; the 1440p centre / area numbers and the HUD at 1440p; the 1024x768 frames and the 4K uiScale-100
frame against fdf35ee's; the cfg / switch log lines; the 4K and 1440 click hit / miss; the edge events; the forced
usable-bounds lines; the fescreens / optionstest, console, keys, strings, ui, toggle results; the sp and 4K sp ASan
0; 9 exe bodies as described (feAllScreensReposition, cat_moSelectedDraw, mouseDraw, mousePositionSet's clamp,
btgConvertStar's term, primLineThick2, primModeSet / Clear2, rndBackgroundRender, WinMain); the tag count 290; the
CAT-TODO count and form; the generators reproduce; the user's check (launcher, setting and log line exist); the fit
rule for the user's cfg. Wrong 7: F1 (the control's placement "under SMOOTH MOTION" was outside the panel), F2 (the
site table missed the mode switch's request layout, a defect), F3 (four IDENTICAL -> MODIFIED rows), F4 (four edited
IDENTICAL bodies without the claimed notes), F5 (a Cataclysm behaviour without CAT:), F6 (the "per frame" comment),
F7 (the play.sh log-line text). **Precision 42 / 49 = 0.857.**

## 11. For the next port (reference hashes, the final build)

Mission01 6,000 ticks debug 787e98865eb42c38 (1,476,370 B) / release 034e6f849e4488f4, IDENTICAL to r10i-mission-a;
missiontest Mission01 5dd566a0a893f371; Tutorial1 fec66f9a4d801958; dumps motion-noi c503c869f0dc9cdb (crc-stripped
3016280e9e0dd3c8), motion-on c0e411662fa2f527 (428ef41479eb5749), nis-noi 86cb155acffc180e (196bab32d244f614), nis-on
0307f3be2044c29c (a5de13a44b8d264f); captures (game-derived, scratch only): z-fe-1024 frame 800 49231f92c939ba59,
z-fe-1080 1d3517156a3c1748, z-fe-4k fa9e4d22751277ca, z-hud-1024 tick 300 27fe8273e44644ac (a .hwo / PPM carries the
build identity: compare with hwo_compare / ppm_region, not by hash across builds).

Verdict: PASS-WITH-FIXES - UI scale 200/150/100 by height with cfg row, switch, console row and Video control verified (4K->1080p frames equal to the bit, 1440p centre-sampled equal, click hit/miss, edges, fit rule); audit fixed the control's off-panel placement and the mode-switch layout at the request (both proven by failing then passing runs), 4 wrong delta-map reclassifications, 4 missing review notes, a CAT tag and 2 text errors; no stream or dump moved (Mission01 787e98865eb42c38 IDENTICAL to R10)
