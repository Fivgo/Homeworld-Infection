<!-- done: a-exe-base, b-instrumentation, c-assembly, d-fences, e-docs -->
# M3-R2 port 3: the hyperspace arrival of a modular ship (hyperspace-assembly)

Base commit 4b799cb (the nis-flicker audit). Work = `git diff 4b799cb` (9 files: src/Game/HS.c, HS.h,
SinglePlayer.c, src/SDL/render.c, main.c, tools/play.sh, ghidra/delta_map_review.json, ghidra/delta_map.json,
docs/m2-r1-function-deltas.md). Not committed (the auditor checkpoints). The user's answers file
(cycle/answers/m3-r2-user.md) was read first and again before this report: nothing in it changes this scope (its
later entries are the GPU, the M3-R3 pointer and the overlay key, queued for M3-R3).

Subsystems: (a) the exe's gate and clip as the base (CAT), (b) the instrumentation (HWI-TEST log lines),
(c) HWI-FEATURE(hyperspace-assembly), (d) the fences, (e) docs (play.sh header, delta-map review).

## 1. Which arrival the user saw
The 10-21 logs load only Mission01_NIS01 and Mission01. n01.nis has no hyperspace event (its sMothership is shown at
frame 0, `show` / `remainAtEnd`). The arrival is **Mission01's**: after nislet01 (the NIS-first start's segment s1,
ticks 2..1713) the fleet arrives by hyperspace under the letterbox Mission01 keeps (SinglePlayer.c
HYPERSPACE_LOADDONE: catSmHideViewport / catNisKeepScissored for mission 1), the camera on the mothership
(ccFocusOnPlayersMothership at the GONE step): the GONE step at tick 1729, the Kuun-Lan's hull out at **1749**, its six
modules at 1759..1770, the hull's exit over at 1917, the modules' at 1927..1938 (the `hsarrive:` / `hsgate:` /
`hsfinish:` lines). The captures (m3r2-hsa-shot01-base, the base build) show the camera close on the Kuun-Lan for the
whole arrival: the user's "close up of the Kuun-Lan". The missions with an arrival flight (not
catSpMissionArrivesAtOnce) are 1, 2, 3, 9, 11, 13, 16; Mission02's is the same fleet (hull at tick 22).

## 2. What the exe does (read first; decompile + disassembly) and what the tree did
| exe function | behaviour | tree before |
|---|---|---|
| hsStart 0x0048f1c0 | effect right scale = `hyperspaceBoxScale * rightlength * 1.08` (static +0x3D0) | no boxScale |
| hsOrientEffect 0x0048f070 | tumble[0] / [1] = `boxScale * uplength / rightlength * 1.08` | no boxScale |
| hsEnd 0x0048fd10 | the three hsRectangle calls take `uplength * boxScale`, `boxScale * rightlength` (0x0048fe79..0x0048fe90); hsLine keeps rightlength (0x0048ff0e); tests displayEffect ([esp+0x94]) and hsGateState | no boxScale |
| hsGetEquation 0x0048f320 | a POPUP_INTO case: (-1, +forwardlength) | no case: default plane z >= 0 |
| hsContinue 0x0048f400 | clamped path: if the host's hsState is 0, `glDisable(GL_CLIP_PLANE0)` and return (0x0048f4a6) | clipped by the host's (default) plane anyway |
| hsShouldDisplayEffect 0x0048f1a0 | FALSE for a clamped ship with a host: a module never draws a gate of its own | same |
| UpdateArrivingShip 0x00518170 | GONE: outside, SOF_Hide (0x8000); wait 2 x BASE (mothership, self-hosted part) or 3 x BASE + RANDOM x ranRandomFn(10)/2^32; DECELERATE: SOF_Hide cleared, hsStart(ship, rate, 0, hsShouldDisplayEffect) | same (the tree's reading confirmed) |
| cat_rndRenderObjectList 0x005776f0 | hsContinue / hsEnd only while the ship's own hsState is 1..6 | same |

The sMothership's .shp: `hyperspaceBoxScale 6.0 ; JS - Make the hyperspace around the spine encompass all the
components` (race_sect/smothership.shp:99; bmothership 2.5, dbeastmothership 6.0). The tree loaded the key
(Universe.c:1087, default 1.0) but never read it. hyperwindow.ebg only does `setXYZScale(xscale, yscale, zscale)` with
hsStart's arguments (no counts, no random draws): the boxScale in hsStart cannot move a stream.

### The user's two items are both GoK deviations from the exe
* **Item 2 (gate "frigate sized")**: the tree's gate was the Kuun-Lan's spine box, 339.2 x 293.7; the exe's is 6 x:
  2035.1 x 1762.1. Captures: Mission02 (m3r2-hsa-shot02-base vs -new, ticks 30..200): the base gate is a sliver at the
  hull's front; the new one spans the ship and sweeps back over it. Mission01 likewise (shot01, 1786..1914).
* **Item 3 (back parts "flickered into existence after the hyperspace was complete")**: the hull's own exit ends first
  (Mission01 hsfinish 1917: the host's COLLAPSE_OUTOF does not wait for the modules: ShipCanMakeHyperspace needs the
  range flag; the exe's fleet test in UpdateArrivingShip is the same: cat_spShipCanHyperspace, static +0x2a8 or class 10, specialFlags & 0x80); from then GoK clipped every module still in its own exit by the
  host's default plane z >= 0 (the back half) until the module's own exit ended (1927..1938). Base frames: the back
  modules visible at 1914, gone 1918..1934, back at 1938. Pixels (|base - new| > 40 per frame, base m3r2-hsa-shot01-base
  vs new m3r2-hsaf-shot01-fnew): 1918 7,601 / 1922 7,590 / 1926 7,574 / 1930 6,056 / 1934 6,067 (region x 545..638,
  y 325..512: the back modules, present in the new build, missing in the base) / 1938 0 / 1942 0. Full-frame changes
  frame to frame: base 7,980 at 1918 and 6,888 at 1938, new 2,511 / 856 (no jump). [audit: these counts do not
  reproduce: `tools/ppm_region.py diff --tol 40 --region 545,325,638,512` on the same files gives 8,659 / 8,647 /
  8,625 / 6,773 / 6,756 at 1918..1934 and 0 at 1938 / 1942 (the audit's own reverted build m3r2-hsaa-shot01-rev vs
  m3r2-hsaa-shot01-new the same, byte-equal to base / new), and the full-frame frame-to-frame changes (--tol 40) are
  base 9,493 (1914 -> 1918) and 8,393 (1934 -> 1938), new 3,442 / 1,415; no per-pixel metric tried (largest channel,
  channel sum, channel mean, luma > 40) gives the stated figures. The finding stands: the back modules are missing in
  the base build from 1918 to 1934 and present in the new one]
  **The departure** had the same fault through the missing POPUP_INTO plane: Mission03's jump (hull hsStart into
  17579, modules 17591..17599): in the base the EnginesLower module vanished at 17592..17594 (the hull still popping up)
  and came back at 17596 (frame changes 726 / 714 px vs ~130 around them); the new build keeps it
  (m3r2-hsa-png/dep03-crop2.png).

## 3. Changes, file by file
**src/Game/HS.c**
* CAT (a): `hsGateShapeGet`: the gate's lengths `uplength * boxScale`, `boxScale * rightlength` and the sweep
  +-forwardlength (the exe's). Used by hsOrientEffect (tumble), hsGetEquation, hsContinue (planes + the effect's
  position), hsEnd (rectangle + origin). hsStart's right scale times boxScale. hsGetEquation: the POPUP_INTO case.
  hsContinue: the host-inactive `glDisable(GL_CLIP_PLANE0)`. (7 `CAT:` / `CAT (` lines with the addresses.)
* HWI-FEATURE(hyperspace-assembly) (c): `hsAssemblyBoxGet` (the union of the host's collision box and its live
  SOF_CatShipPart parts' boxes, each part's 8 corners by clampOffset + clampCoordsys as updateClampedObject places
  it); on top of the exe's shape the sweep is extended to the union's forward extent and the rectangle grown to the
  bounding rectangle of the exe's and the union's (kept bit for bit when the exe's already covers the union);
  `hsAssemblyPartPending(part)`: a SOF_CatShipPart part still SOF_Hide in SHIPHYPERSPACE_DECELERATE whose host is
  shown and SHIPHYPERSPACE_DONE; `hsAssemblyEnabled` (the switch).
* HWI-TEST (b): `hsAssemblyLogStart` ("hsgate:" at hsStart of a host of parts or a part: core box, the tree's / exe's /
  now gate, the sweep, the union), "hsfinish:" in hsFinish.

**src/SDL/render.c** (HWI-FEATURE(hyperspace-assembly))
* rndMainViewRenderFunction: after the view test [audit: after the level-of-detail loop; the producer's build added them
  before it, see 4], each visible host's pending parts join the frame's LOCAL `visible`
  list (never universe.RenderList); the HWI-TEST "hsassembly:" line (first frame of each tick: parts drawn, the host's
  hsState and clipt).
* cat_rndRenderObjectList: hsContinue / hsEnd also for a pending part (the host's clip through hsContinue's clamped
  path, exactly as a part in its own exit); a pending part sets no `renderedLODs` bit, runs no nav lights, no cloud
  lightning countdown, no edge effect (see 4).

**src/Game/SinglePlayer.c**: HWI-TEST "hsarrive:" lines at the GONE and DECELERATE steps of a host of parts or a part.
**src/Game/HS.h**: the declarations. **src/SDL/main.c**: `/noHsAssembly` (run-only). **tools/play.sh**: header block
"Hyperspace of a modular ship". **ghidra/**: section 8.

## 4. Render-only proof (every changed read and write)
* The simulation is untouched: no write to SOF_Hide, the hyperspace states, the timers, the waits or gamerand()
  (UpdateArrivingShip / UpdateHyperspacingShip only gained dbgMessagef lines).
* SOF_Hide's readers (104 lines in 27 files: selection, targeting, collision, sensors, KAS, the save,
  ShipIDtoShip ...) are unchanged; the new reads are in HS.c / render.c only.
* universe.RenderList membership (read by Gun.c's fire effects, UnivUpdate.c's damage effects, SoundEvent.c) is
  untouched: the parts go into the main view's local list through `obj.catVisibleLink` (read only by render.c).
* A pending part gets no view test (rndShipVisibleUsingCoordSys writes the selection circle; the host's already sets
  its parts' magnitudeSquared) and no LOD pass (lodLevelGet writes currentLOD, read by Gun.c / Damage.c) and is drawn
  at its current level (LOD 0 under FULL DETAIL); its cameraDistanceSquared (Battle.c / ETG.c readers) is not written.
  [audit: NOT so in the producer's build: the parts joined `visible` before the level-of-detail loop, which runs
  lodLevelGet on every object of that list, so a hidden part's currentLOD (distance-table hysteresis), cameraDistanceVector
  and cameraDistanceSquared (read by UnivUpdate.c's render-list insertion, Gun.c, Damage.c, Clouds.c) were written every
  frame, and under /lodBands its time joined the frame budget's passes; the audit moved the block after the loop
  (render.c), so these statements now hold. Pixels unchanged: the audit's Mission01 frames are byte-equal to
  m3r2-hsag-shot01-new]
* Drawn paths with simulation-visible state are skipped for a pending part: `renderedLODs` (kasfRenderedShips,
  AITeam.c aitCheckAmIBeingWatched), the cloud lightning countdown, the edge effect (the Beast-stun bolts draw random
  stream 0), the nav lights (their blink state advances when drawn: skipping it keeps the exe's phase; with the lights
  drawn the first A/B differed by the blink only).
* hsGateShapeGet is also reached from hsStart (simulation) through hsOrientEffect: it writes only the particle's
  tumble (its size); the effect's position is rewritten each frame by hsContinue / hsEnd as before.
* cat_lodTimingStart/Stop sample the wall clock per level (first 4 samples; used only by /lodBands' frame-time passes):
  already non-deterministic, not a simulation input of the streams.
* Result: every stream IDENTICAL in every field incl. rng_all (section 6).

## 5. HWI-FEATURE(hyperspace-assembly): surfaces and default
Default ON; no options-file row, no console row, no key, no Options control (a fix of the user's rule, not a
preference). Surface: the run-only switch `/noHsAssembly` (main.c; the exe's look for an A/B), documented in
tools/play.sh. 50 HWI-FEATURE(hyperspace-assembly) lines (HS.c 27, HS.h 3, render.c 12, main.c 8). [audit: 55 after the
audit tagged five untagged feature lines in render.c (17 there)]

What it does in the shipped data (all 17 self-tests' hsgate lines): the union of every modular ship that hyperspaces
lies inside the exe's gate and sweep: Kuun-Lan union x -868.1..566.3, y -518.4..518.4, z -1998.1..1990.0 vs the exe's
2035.1 x 1762.1 and +-2214.5; beast bMothership (Missions 10, 14, 16; 2 parts; boxScale 2.5) union x -1022.1..655.5,
y -876.7..876.7, z -2196.0..2713.1 vs 2170.1 x 2592.0 and +-3220.0. So the gate and sweep equal the exe's for both; the
layer's visible part is the parts drawn under the hull's clip from the hull's first exit frame (the user's rule:
"every part present before the exit animation starts, then one clip reveal"). In Mission01 and 02 the modules' waits
(1.8..2.6 s vs the hull's 1.2 s, the tweak file's BASE 0.6 / RANDOM 0.8) end during the hull's POPUP_OUTOF or first
slicing ticks, so the pending parts are fully clipped (z >= +forwardlength) when drawn: Mission01 A/B
(m3r2-hsag-shot01-new vs -noasm, the final binary) **0 of 104 frames differ**; the layer guarantees the rule for any
wait without a visible cost here.

Gate numbers (hsgate:, Mission01 tick 1749 = Mission02 tick 22 = every sMothership jump):
| | up | right | sweep (model z) |
|---|---|---|---|
| core hull box | 339.2 | 293.7 | fwd 2214.5 |
| tree's gate (before) | 339.2 | 293.7 | +2214.5 .. -2214.5 |
| exe's gate (CAT, boxScale 6.0) | 2035.1 | 1762.1 | +2214.5 .. -2214.5 |
| assembly union (6 parts) | x 1434.4 | y 1036.8 | z -1998.1 .. 1990.0 |
| now | 2035.1 | 1762.1 | +2214.5 .. -2214.5 |

## 6. The fences
Binaries: debug sha256 4633878e2f3dcee7, release 94ca64d96bd7d1d7 (frozen in original/scratch-m2/m3r2-hsa-final2;
the full set ran on m3r2-hsa-final, the binary before the last render-only change (the rectangle kept bit for bit),
and the arrival/departure set was re-run on final2). Builds: tools/build.sh --debug and --release rc 0, 0 warnings
(-Werror); the win64 cross build (build/win64, ninja) links hwi-engine.exe, no warning; no _WIN32 code touched.
tools/m2_layout32.sh OK. GPU: no measurement in this port (no frame time, no 4K); claude-gpu-done not needed.

**Streams** (hwo_compare.py; every one IDENTICAL in every field incl. rng_all, vs M3-R1's m3r1i-*):
| stream | run (final2 / final) | sha256 (16) | result |
|---|---|---|---|
| headless Mission01 6,000 | m3r2-hsag-mission-a / -b | 8285a210f60e4110 both (1,476,370 B) | IDENTICAL to m3r1i-mission-a/run.hwo, ticks 0-6000 |
| same, release | m3r2-hsag-mission-rel-a | efacfabd75e6b220 | IDENTICAL to m3r1i-mission-rel-a |
| missiontest 01 (NIS-first, the arrival) | m3r2-hsag-missiontest-a / -b | 0826eebadcbfbf76 | 6/6 PASS done 11,424; IDENTICAL 0-11425 |
| 02 | m3r2-hsag-m2-a | 5c10254447c28a4d | 6/6 PASS 25,936; IDENTICAL |
| 03 | m3r2-hsag-m3-a | 329b6be24259b285 | 5/5 PASS 17,825; IDENTICAL |
| 04 | m3r2-hsaf-m4-a | d0de0b38dc668963 | 5/5 PASS 20,818; IDENTICAL |
| 05 | m3r2-hsaf-m5-a | 443cea5202a3c1c8 | 6/6 PASS 16,384; IDENTICAL |
| 06 | m3r2-hsaf-m6-a | 8b6c4d1c011dfbab | 7/7 PASS 31,011; IDENTICAL |
| 07 | m3r2-hsaf-m7-a | 5d0bffd0f7fb5eee | 4/4 PASS 16,592; IDENTICAL |
| 08 | m3r2-hsaf-m8-a | 3cc9cbf0a3d38df8 | 6/6 PASS 29,361; IDENTICAL |
| 09 | m3r2-hsag-m9-a | 8b404b637819f726 | 6/6 PASS 20,617; IDENTICAL |
| 10 | m3r2-hsag-m10-a | 1620a89f51e3d96e | 5/5 PASS 21,939; IDENTICAL |
| 11 | m3r2-hsaf-m11-a | e9d9bdac27d12d05 | 5/5 PASS 8,275; IDENTICAL |
| 12 | m3r2-hsaf-m12-a | c4b85620c5aaf1b8 | 3/3 PASS 10,995; IDENTICAL |
| 13 | m3r2-hsag-m13-a | c78510056cd1e34c | 3/3 PASS 19,283; IDENTICAL |
| 14 | m3r2-hsag-m14-a | 1f7b6f55d4176942 | 4/4 PASS 22,338; IDENTICAL |
| 15 | m3r2-hsaf-m15-a | 5ea4b480661cd6db | 3/3 PASS 15,424; IDENTICAL |
| 16 | m3r2-hsag-m16-a | a35da4ac77ca9364 | 6/6 PASS 21,523; IDENTICAL |
| 17 | m3r2-hsaf-m17-a | 54d6a7d09d111d4c | 4/4 PASS, the game's end 15,584; IDENTICAL |
| Mission01 -> 02 (--teamcolour, /catTeamColourTicks 300, shots) | m3r2-hsag-tc02 | - | IDENTICAL to m3r1i-tc-user to tick 2600 |
| failed jump (Mission03 --fail-jump) | m3r2-hsag-fj3-a | 19f38ca54774375a | armed 17472, return 17635 PASS; IDENTICAL |
| Tutorial1 leg | m3r2-hsaf-tutorial-a | c8b84dbfa67fd28f | IDENTICAL (0-10445) |
| warp start level 10 | m3r2-hsaf-warp10-a | 75bdd2c42a4e771c | IDENTICAL |
| lesson load (LESSON 11, --save-src the tutorial run's save) | m3r2-hsaf-tload-d | 78543d2449fe5c3c | PASS; IDENTICAL (8360-11962) |
| loadgame "01 - Hiigara" | m3r2-hsaf-loadgame-a | edff00638db7d1f3 | IDENTICAL |
| replay 10-17 (oracle01-r6, 1..9600) | m3r2-hsag-r17-a | - | IDENTICAL to m3r1i-r17-a (9601 ticks, 4800 packets): rung 2823 |
| replay 10-18 (oracle01-r18, 1..3526) | m3r2-hsag-r18-a | - | IDENTICAL to m3r1i-r18-a (3527 ticks, 1763 packets): rung 2871 |
Every run: ASan 0, no LeakSanitizer report. Moved: nothing; nothing to re-baseline.

**Dumps** (whole / crc-stripped): motion /noInterp c503c869f0dc9cdb / 3016280e9e0dd3c8 (1,431); nis /noInterp
86cb155acffc180e / 196bab32d244f614 (1,440); motion on c0e411662fa2f527 / 428ef41479eb5749 (1,431, final2 too); nis
on 0307f3be2044c29c / a5de13a44b8d264f (1,440, final2 too); opt-outs 183681235a3fdc7b / 13ca70eea998e5de: all EQUAL
whole to M3-R1's (m3r2-hsaf-motion-*, -nis-*). No crc moved (no hyperspace in their scenes).

**fe** (m3r2-hsaf-fe): Main_game_screen, ASan 0, rc 124 at 120 s. **sp** (m3r2-hsaf-sp, --timeout 1200): the
universe through Mission12 (Mission12.btg loaded), ASan 0, no leak report, rc 124 at the timeout (as M3-R1); the
stb_image_write UBSan shift note at the first screenshot is M3-R1's too. Its hsgate lines show every jump (Mission01's
arrival at tick 837 of the sp loop's own start, the later arrivals at tick 22 and departures at 1277..1378); the
loop's screenshots (golden-NN-ticks, every 112 ticks) do not fall on the 1-2 s of an arrival: the arrival frames are
the fence captures below.

**Captures** (original/scratch-m2/m2r2-harness/runs/<label>/shot.*.ppm; sheets in original/scratch-m2/m3r2-hsa-png):
| capture | base build (4b799cb) | new |
|---|---|---|
| Mission01 arrival s1 1745..1990 | m3r2-hsa-shot01-base | m3r2-hsaf-shot01-fnew, m3r2-hsag-shot01-new (/noHsAssembly: -noasm) |
| Mission02 arrival s2 20..260 | m3r2-hsa-shot02-base | m3r2-hsa-shot02-new, m3r2-hsag-tc02 |
| Mission03 departure 17570..17720 (+ arrival 20..200) | m3r2-hsa-shot03-base | m3r2-hsa-shot03-new |
| failed jump 17470..17680 | m3r2-hsa-fj3-base | m3r2-hsa-fj3-new |
The `hsassembly:` lines (fence a): Mission01 pending parts drawn under the hull's clip from tick 1750 (the first frame
after the hull's hsStart at 1749), 6 parts while the hull is in POPUP_OUTOF (clipt 0.9375 .. 0.4375), then 5, 4, 3, 1
as each part's own exit takes over (1759..1770). Mission02: from tick 29 (the hull's exit at 22 is outside the
camera's view until then, so no part needed drawing). The departure is one collapse over the assembly: the hull's
gate (2035 x 1762) sweeps the whole Kuun-Lan; the modules clip by its plane and finish together (Mission03 hsfinish
17767, the hull 17784; the failed jump all at 17634).

## 7. KAS / NIS / multiplayer arrivals
* KAS team hyperspace (kasfTeamHyperspaceIn): the beast's bMothership (2 parts) arrives / leaves by it in Missions 10,
  14 and 16 (hsgate lines, part=0 with `assembly parts=2`); its gate is the exe's (above); its parts (if hidden by
  UpdateArrivingShip's path) are covered by the same pending rule. sCarriers (Missions 13, 14, 16) have no
  SOF_CatShipPart parts: unaffected.
* An NIS sMothership (nisNewObjectCreate with cat_spGetHyperspaceModuleTypes): no `hsgate:` line came from an NIS ship in any of the 17 self-tests or the sp loop (NIS ships do not go through
  hsStart); unaffected.
* Multiplayer: the same HS.c paths (hsShouldDisplayEffect, the clamped clip); the exe's base port applies there too;
  not run (no multiplayer harness for a jump).

## 8. Delta-map review entries (ghidra/delta_map_review.json; `python3 -I tools/delta_map.py merge --done m3r2:hyperspace-assembly`: records changed 7; IDENTICAL now 2,757, MODIFIED 1,580, reviewed 3,578; docs/m2-r1-function-deltas.md regenerated)
IDENTICAL found modified -> MODIFIED: hsOrientEffect 0x0048f070, hsStart 0x0048f1c0, hsGetEquation 0x0048f320,
hsContinue 0x0048f400, hsEnd 0x0048fd10. Relied on, compared again (notes): hsShouldDisplayEffect 0x0048f1a0,
UpdateArrivingShip 0x00518170.

## 9. CAT-TODOs
None added, none closed. The only one naming M3-R2 is M3-R1's BTG.c:1151 (the star size), left for the integrate to
re-target as the task says.

## 10. THE USER'S CHECK (for the play-test brief)
`tools/play.sh` (the release build, 3840x2160, as on 10-21). New game, watch Mission01's opening: after the nislet,
the Kuun-Lan arrives in the white flash with the camera close on it.
1. The gate: a large blue-white rectangle that spans the whole Kuun-Lan (not a small frigate-sized one) and slides from
   its bow to its stern as the ship comes out. Report whether it matches the vessel's size.
2. The back modules (engines, lower spine, ore-can armour, at the stern): they must be there as the gate passes them
   and must NOT vanish and pop back about a second after the gate is gone. Report any part that appears late or
   flickers.
3. Optional, Mission02's start (the same arrival from further out) and any jump out (Mission03's end): one gate over
   the whole ship, the modules never cut in half while the ship enters the gate.
4. Optional A/B: `tools/play.sh -- /noHsAssembly` shows the exe's look (identical in these missions; it differs only
   if a module's wait outlasted the gate's opening).

## 11. For STATE.md
* M3-R2 hyperspace-assembly: the exe's gate (hyperspaceBoxScale on the gate rectangle and the effect, hsGetEquation's
  POPUP_INTO plane, hsContinue's host-inactive glDisable) ported as the base: items 2 and 3 were GoK deviations.
* HWI-FEATURE(hyperspace-assembly) default ON, switch `/noHsAssembly` (run-only), no cfg row / console row / key /
  control; names: hsGateShapeGet, hsAssemblyBoxGet, hsAssemblyPartPending, hsAssemblyEnabled; HWI-TEST log lines
  `hsgate:`, `hsarrive:`, `hsassembly:`, `hsfinish:`.
* No stream, replay or dump moved; nothing re-baselined.

## 12. What is left (owner)
* The user's play-test check above (the integrate's brief).
* The exe's hsStart tests glCapFastFeature(GL_BLEND) where the tree tests etgEffectsEnabled (left: a simulation-side
  creation call; owner M3 oracle physics if ever a stream needs it).
* Multiplayer arrival of a modular mothership not exercised (owner: stage 5 multiplayer, with the LAN work).
