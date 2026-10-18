<!-- done: nis, camera, fow, beam-clamp, pre-droid pair, madanim-droids, post-droid pair -->
# M2-R6 nis-fow-camera: NIS, animatic, mesh animation + repair droids, fog of war, camera

Status: DONE (producer 5 of M2-R6; base commit `a6e72c9`; nothing committed by the producer, the auditor
checkpoints).

Subsystems (in this order, so that the Mission01 stream moves once): `nis` (NIS.c, Animatic.c, the 27 `cat_nis*` script
callbacks; `/catNisParseTest`, harness mode `nisparse`), `camera` (Camera.c, CameraCommand.c, the `cat_cc*` scripted
focus / zoom / chase, the `cat_mc*` motion cues), `fow` (the fog-of-war grids, cubes and boundary, the stealth
detection, the waiting sites, the FOW saves), `beam-clamp` (the beam's sixth argument, `cat_clampUpdateAttachedObjects`),
then the pre-droid pair, then `madanim-droids` (MeshAnim.c / MadLinkIn.c with `madAnimationStart`'s third argument, the
repair droids), then the post-droid pair.

## Mission01 stream before

- `r6nfc-base-mission`: the base binary (a6e72c9, debug, copied to the session scratchpad), 6,000 ticks: oracle mission
  run done, rc 0, ASan 0, 1,476,370 B, sha256 `24253cbae125485f`. `hwo_compare.py` against the managers audit's
  `aud-r6mg-mission-a`: `IDENTICAL: 6001 ticks and 0 packets compared (ticks 0-6000), no difference` (header build
  identity c3e7c1f2 vs a6e72c9d only). Against R5's `r5-integrate-mission-a` (`645f94c69d68319e`): `FIRST DIVERGENT TICK
  1`, 2 fields, `rng6` (CAND 10 draws ahead) and `rng_all`: the managers port's stream-6 draws of the computer player's
  per-ship timer (docs/m2-r6-managers.md "Mission01 stream after"), nothing else.

## nis (done)

NIS.c, Animatic.c and GoK's movie player (avi.c), with three small helpers elsewhere. The NIS code does not run in the
Mission01 oracle run (no NIS plays: the NIS-first start of a mission is stage 6), so nothing here touches the stream.

- `src/Game/NIS.h`: the **Cataclysm opcode enum** (the exe's dispatch table `nisEventDispatch` 0x0088888c has 75
  entries; the `*Set` callbacks store the codes, e.g. `nisDamageLevelSet` 0x004d4770 0x40): `NEO_Repair` 0x05 after
  Attack, `NEO_CameraClipNear` / `Far` 0x16 / 0x17 after CameraFOV, `NEO_OpenSlipgate` / `CloseSlipgate` 0x36 / 0x37
  after SMPTEOff, and nine appended after MinimumLOD (`NEO_LightAmbient` 0x42 .. `NEO_Subtitle` 0x4a), so every HW1
  code from HaltAttack on moves. **The event is 0x28 bytes** (six parameters; the `*Set` callbacks realloc count *
  0x28 and `cat_nisFogSet` 0x004d3490 writes +0x10..+0x24): `nisevent.param[6]`. ILP32 asserts: nisevent 0x28,
  nisheader 0x84, spaceobjpath 0x38, objectmotion 0x24, cameramotion 0x1C, nisplaying 0x54 (iCurrentEvent +0x50). New
  globals: the NIS's module copies (`catNisModuleShips` 0x00a4ca38, 30 slots, count 0x00a4cb80), the fadeSet flag
  (`catNisBlackFadeFlag` 0x00a4cb4c) and `catNisKeepScissored` (0x00a4cacc, set by `singlePlayerGameUpdate`
  0x005197d0, stage 6).
- `src/Game/NIS.c`, the MODIFIED bodies (all 16 unread ones and the reviewed `nisBlackFadeSet`):
  - `nisUpdateTask` 0x004cc600: the frame-step flag `catUnivFrameStep` (0x00a6ed74) cleared during the scissor
    fade-in; at the NIS's end `catNisKeepScissored` keeps the letterbox instead of clearing `nisFullyScissored`; the
    KAS letterbox finish (`cat_kasWideScreenActive` / `cat_kasWideScreenInFinish` 0x004973d0) is a
    `CAT-TODO(stage 6 KAS)`.
  - `nisStartup` 0x004cc8f0: `cat_taskTimeCompressionReset` (the exe's `taskStart` takes the name as a fourth
    argument, GoK's has none).
  - `nisNewObjectCreate` 0x004cccb0: ship races 0..6 (all seven Cataclysm races, HW1's R1..P3/Traders retired);
    `univCreateShip(type, race, pos, player, 0, 0, 1)` = new `cat_univAddShipModules(..., 0, TRUE)` (placed, upgrades
    applied; the 7th argument: no automatic sMothership modules outside mission 1, 0x00542bba); an NIS sMothership
    gets copies of the modules in the hyperspace ether (new `cat_spGetHyperspaceModuleTypes` 0x0051c0c0 in
    SinglePlayer.c), the support module on the player's support module's dock point (`dockReserveDockPoint`, turned
    180 degrees about X by the point parity), clamped; mission 1 adds a unit-capped docking bay; the generic-object
    case calls an empty stub and leaves the pointer undefined in the exe (`CAT-FIX`: NULL); after the one-keyframe
    placement `cat_clampUpdateAttachedObjects`.
  - `nisStart` 0x004cd2b0: `catSmHideViewport` (0x00a5b228) cleared, `cat_rndLensFlareNISSourceAdd`,
    `cat_rndFogLayerOff`, the fadeSet flag cleared; the RGL hook (0x00b3a6d8 / 0x00b3a610) is GoK-backend ("Decisions"
    28).
  - `nisStop` 0x004cdd60: the NIS lens flare source removed, fog texture unloaded / off / faded to 0,
    `soundEventStopSFX(0.5)`, the module copies deleted, races 0..6, a mothership's clamped modules apply physics again
    (`CAT-FIX` guards: the exe reads the clamp info unchecked), the effect pointer cleared, the NIS music always stops
    (no fast-forward test), `subTitlesUpdate` + `subReset`, `mrCamera = &defaultCamera` (0x00ba1e00) at the end.
  - `nisCamPathUpdate` 0x004ce600: `univUpdateRenderList` + `univUpdateMinorRenderList` after the camera moves.
  - `nisUpdate` 0x004ce780: waits while a movie plays (`binkDonePlaying` 0x008b6d94 = GoK's `aviIsPlaying`, as
    SpeechEvent.c's Bink term); a ship moves through `cat_univSetShipPosition`, then `cat_clampUpdateAttachedObjects`
    and the light / lens flare source ships (`nisLightSource1` / `2` / `nisLensflareSource1`) set
    `cat_lightSetPosition(0 / 1)` / `cat_rndLensFlareNISSourceSetPosition` (w = 0).
  - `nisAttack` 0x004cf2c0 (a bMothership "attacks" with `clSpecial`), `nisObjectShow` 0x004cf470 / `nisObjectHide`
    0x004cf5d0 (the clamped parts follow, the NIS lens flare toggled, `soundEventShipRemove`), `nisRemainAtEnd`
    0x004d0050 and `nisAttackKasSelectionSet` 0x004d45a0 (the exe hashes the KAS label, `cat_kasHashString`:
    `CAT-TODO(stage 6 KAS)`, this tree's KAS names by string), `nisNewEnvironment` 0x004d0e30 / `nisNewNISSet`
    0x004d4380 (`memStringDupe`, no NV pool), `nisBlackFadeSet` 0x004d0770 (the second parameter's byte to 0x00a4cb4c),
    `nisMeshAnimationStart` 0x004d0800 (the third argument: subsystem `madanim-droids`).
  - **IDENTICAL compared, 9 found modified**: `nisSpeechEvent` 0x004cf840 (the variation argument is the ship's type),
    `nisAnimaticSpeechEvent` 0x004cf8f0 (timeout 0.0, HW1 5.0), `nisCameraFOVSet` 0x004d2290 (an event of no object,
    `sscanf "%f"`; GoK's tree had `scanf "%d"`, i.e. read stdin), `nisFadeSetSet` 0x004d2fa0 (the ", n" second
    parameter), `nisRaceSwap` 0x004d2470 (swaps the Sect and the Beast unless the player is race 2, the Kushan),
    `nisFindObjectByName` 0x004ced60 and `nisLoad` 0x004d4a00 (the seven ship races), `nisDelete` 0x004d4fe0 (NULLs
    the string block, `CloseStatInfo`; no effect, not ported), and `nisCurrentTimeSet` 0x004d1380 has no range check
    (the tree's debug check stopped the parse at nislet01's frame 3209 = its length; removed). The other 83 IDENTICAL
    bodies are the exe's.
  - **The 27 `cat_nis*` functions**: the 13 event functions `cat_nisRepair` 0x004cf370 (a live Worker repairs through
    `cat_clSpecialTarget`, others through `clSpecial`), `cat_nisSubtitle` 0x004cf740 (index 0 strNIS01, 2 strNISLET1,
    3..11 strNIS02, strNIS04..11; 1 and > 11 show nothing; `"#r%x#t%x%s"` 3, 0xf to `subTitleAdd(0x70, ...)` for
    param[1] ms), `cat_nisLightAmbient` / `Diffuse` / `GlobalAmbient` / `NumberLights` 0x004cf920..0x004cfa10,
    `cat_nisOpenSlipgate` / `Close` 0x004d00b0 / 0x004d00d0, `cat_nisCameraClipNear` / `Far` 0x004d0110 / 0x004d0140
    (0 = the default plane), `cat_nisEbolts` 0x004d0850 (the Beast-stun state 1 for 2.5 s, spin -1), `cat_nisBeastEffect`
    0x004d08b0 (conversion in 4 s), `cat_nisFog` 0x004d08f0 (`catRndFogOn`, `cat_rndFogLayerFadeTo`), `cat_nisMimic`
    0x004d5fa0 (sMimic unmimics, any other type `cat_mimicBecomeShipType(ship, type, 1)`); and the 14 `*Set` callbacks
    with the exe's 81-row script table (0x00888a60). `cat_nisFogSet`'s `sscanf` order was read from the disassembly
    (the decompile drops the out-arguments): on, time, colour 1, colour 2, speed, density x 10,000,000.
- `src/Game/Animatic.c` (`animBinkPlay` 0x00421860, ported into GoK's avi path `animAviPlay` / `animAviDecode` /
  `animAviEnd`): `universe.realtimeelapsed` saved, zeroed and restored with the total time; **the decode converts
  frames by the movie's frame rate** (the exe's decode callback 0x004217d0 divides by Bink's rate 0x00a70f1c and sets
  the real time too). `src/SDL/avi.c`: `aviFrameRate` from the stream (`avg_frame_rate`, else `r_frame_rate`) and the
  playback paced by it. **Cataclysm's 18 animatics are 30 fps** (their Bink headers; sierra.bik is 15): GoK's player
  assumed HW1's 15 fps, so every animatic played at half speed (Animatic01: 3,689 frames, 246 s instead of 123 s).
- Helpers: `cat_clampUpdateAttachedObjects` 0x00434b50 in Clamp.c (**located**: it sits after
  `cat_updateClampedObjectNoPhysics` in the exe's Clamp.c block; its only callers are `nisNewObjectCreate` and
  `nisUpdate`): the clamped objects put at their offset in the host's frame, without physics; `cat_lightSetPosition` /
  `Ambient` / `Diffuse` / `GlobalAmbient` 0x0055f2a0..0x0055f360 in Light.c (the light layout is HW1's: currentLight
  0x00a74c28, currentLight1 0x00a74cb0, the global ambient 0x008b83f0); `CAMERA_CLIP_NEAR` exported from Camera.h.
- **Self-test `/catNisParseTest <listfile>`, harness mode `nisparse`** (tools/m2_harness.sh; the motion-interpolation
  producer takes mode `nis` for its playback measurement): the harness lists `nis\*.nis`, `nis\*.script` and
  `movies\*.script` of the cataclysm.big TOC; after the start-up every .nis is loaded with the .script of the same
  name through `nisLoad` and every animatic script through `animLoadNISScript`; the header (identifier, version
  0x101), object types, event opcodes (below `NEO_LastNEO` with a dispatch function), object references and the
  attack / repair targets are checked, events counted per opcode, and the script keywords the exe's table does not
  know listed (the exe ignores them). "nisparsetest:" lines, "nisparsetest: done ... result=PASS|FAIL"; quits.

Runs (plain harness):

| run | mode | milestone | first stop | result |
|---|---|---|---|---|
| `r6nfc-nisparse-1` | nisparse | Main_game_screen | `Fatal error - Invalid object race: 6` (nisLoad still had HW1's six races) | fixed: races 0..6 |
| `r6nfc-nisparse-2` | nisparse | Main_game_screen | `Fatal error - tried to set time to frame 3209 ...` (the tree's debug range check; the exe has none) | fixed |
| `r6nfc-nisparse-3`, `-4` | nisparse | Main_game_screen | ASan SEGV in the test (an implicit declaration truncated the animatic header pointer: NIS.c now includes Animatic.h) | fixed |
| `r6nfc-nisparse-5` | nisparse | nisparse self-test done | none (rc 0) | FAIL: the animatic events carry object -1 (the exe stores the parse's current object too); the check now skips animatics |
| `r6nfc-nisparse-6` | nisparse | **nisparse self-test done** | none (rc 0) | **PASS**, ASan 0, no leak: `nis=14 nisScripts=14 animatics=18 objects=593 events=1488 cataclysmEvents=31 unknownKeywordLines=47 failed=0`; ignored keywords (not in the exe's table): `motionCueScale` x12, `minimumLODSet` x32, `stretch`, `closeslipgate` (lower case), `widescreenOut` |

The Cataclysm opcodes in the data: repair 1, openSlipgate 1, lightAmbient 2, lightDiffuse 2, globalAmbient 2,
numberLights 1, mimic 3, fog 4, ebolts 2, beastEffect 1, subtitle 12.

## camera (done)

Camera.c, CameraCommand.c, the `cat_cc*` scripted calls and the `cat_mc*` motion cues. Nothing here changed the
Mission01 stream (`r6nfc-cam-mission`, 1,000 ticks: IDENTICAL to the base over ticks 0-1000); the motion cues' draw
call is wired with the stream-moving step (see `madanim-droids`), because their reset scatters the points with random
stream 0.

- **Layout: the exe's Camera is 0x5C bytes** (cameraInit 0x0042b7d0 clears +0x55 and +0x58; nisStart allocates 0x5C;
  ccInit's stack entry is 0x78): new `Camera.catSnapToFocus` (+0x55, set by the scripted focus / zoom calls) and
  `Camera.catScriptedDistance` (+0x58, the distance the snap applies); names chosen here. So the CameraCommand is 0x94
  (`ccMode` at +0x8C = universe+0x98, the `cat_ccIsPilotingShip` test; `RenderList` at universe+0xA0, `ccFocusOnFleet`)
  and **HW1's `universe.dontUpdateRenderList` is universe+0xB8 itself**: the M2-R4 layout had kept HW1's word at +0xB4
  and named +0xB8 `catSensorsHoldRenderList`; the two are one field again (Universe.h, UnivUpdate.c). ILP32 asserts:
  Camera 0x5C, CameraStackEntry 0x78, CameraCommand 0x94 / ccMode +0x8C, universe ccMode 0x98 and RenderList 0xA0;
  `tools/m2_layout32.sh` OK, Universe still 0x3D9BC. (Reading the camera decompile needs this: Ghidra's CameraCommand
  type is HW1's, so every field name after `actualcamera` is the next field's: Ghidra "action" is `transition`,
  "transTimeStamp" is `action`, ..., "dontFocusOnMe" is `ccMode`.)
- Camera.c: `cameraInit` clears the two new fields; HW1's `PilotOffsets` / `PilotOffsetsR2` tables (the two
  `CAT-PROVISIONAL` markers of this file) and `cameraSetEyePositionBasedOnShip` are retired: Cataclysm's pilot view is
  `cat_ccChaseCamFromShip` 0x0042cc80 with each type's `pilotViewOffsetX/Y/Z` from the .shp. `cameraControl` 0x0042bab0
  compared again: the exe's (GoK's joystick path kept, inactive without a joystick).
- CameraCommand.c, the 7 unread MODIFIED bodies and the rest:
  - `ccFocusIsLegalToFocusOn` 0x0042cdd0: a focus ship counts when `cat_ccShipIsLegalToFocusOn` 0x0042cd40 allows it
    (inlined in the exe), within `MaxCamDistSquared` (225,000,000).
  - `NewSetFocusPoint` 0x0042cf60: the collision centres read directly, a mimicking ship zooms to
    `cat_mimicGetCameraRadius`, a minimum zoom distance of at least 0.1 replaces the collision radius, single-precision
    eye normal and float sums as the exe.
  - `CameraChase` 0x0042d510: pilot mode through `cat_ccChaseCamFromShip` (no `pilotView` switch; the mode ends without
    a one-ship focus), the up vector always reset, the **snap** after a scripted call (the trackers restart on the
    target with the position tracker's log base 0, which lands each value on its target, the scripted distance
    becomes the focus distance, then the render lists are updated), `CAMERA_SET` asks the motion cues to reset.
  - `ccFocusOnPlayersMothership` 0x0042dc30 (`spFindCameraAttitude` only outside single player; single-player mission
    2 starts on the other side), `ccFocusGeneral` 0x0042deb0 (no `proxInFocusSelection`: an empty stub in the exe),
    `ccCopyCamera` 0x0042e310 (the angle computation inlined without the cosine clamp), `ccFocusOnFleetSmooth`
    0x0042e6d0 (a toggle like `ccViewToggleMissionSphere`); `transTimeStamp` is `cat_taskRealTimeElapsed` in
    `ccFocusGeneral`, `ccFocusFar`, `ccCancelFocus`, `ccForwardFocus`, `ccFocusOnFleetSmooth` (the exe's unscaled clock;
    HW1 `taskTimeElapsed`).
  - **IDENTICAL compared, 3 found modified**: `ccFocusFar` 0x0042e230 and `ccForwardFocus` 0x0042e4c0 (the real-time
    stamp), `ccCancelFocus` 0x0042e3d0 (no early return at the base entry `dontFocusOnMe`; plus the stamp). The other
    IDENTICAL bodies of the two files are the exe's (ccInit's field order checked against the exe's offsets).
  - New: `cat_ccChaseCamFromShip` 0x0042cc80, `cat_ccFocusCloseScripted` / `cat_ccFocusScripted` 0x0042e140 /
    0x0042e160, `cat_ccSetDistanceScripted` 0x0042e180, `cat_ccZoomScripted` 0x0042e1a0, `cat_ccSetDistance` 0x0042e210
    (their callers are the KAS functions: stage 6).
  - **The motion cues** (`CatMotionCues catMotionCueState`, the exe's 0x00c49400, layout asserted: mode +0x74, points
    +0x80, count +0x8D20, fade +0x8D24): `cat_mcSetMode` 0x0042c090, `cat_mcResetRequest` 0x0042c0b0, `cat_mcpReset`
    0x0042c0c0 (100 points in a +-4500 box around the eye in mode 1, 1500 in +-15000 in mode 0, scattered with random
    stream 0), `cat_mcFocusAverageVelocity` 0x0042c340 (none if a focus ship has a non-move command or moves slower
    than 130), `cat_mcNormalScalar` 0x0042c450 (the streak vector -0.1 x the focus velocity per second x 0.2,
    EvalCubic-smoothed; zero within 0.35 s of a focus change), `cat_mcRender` 0x0042c630 (the points wrapped into the
    box with floor, blended grey 0.7 streaks, alpha min(speed x 0.03, 0.9), drawn once the streak is 0.5 long),
    `cat_mcDrawCues` 0x0042ca10 (mode 0's screen-space streaks); `cat_hmatMultiplyHMatByVec` 0x004acdf0 in Matrix.c.
    The GL calls were identified from the exe's glcompat pointer table (0x00b3a514 glPushAttrib, 0x00b3a47c glEnable,
    0x00b3a4cc glBlendFunc, 0x00b3a530 glBegin, 0x00b3a51c glColor4f, 0x00b3a604 glVertex3f, 0x00b3a440 glEnd,
    0x00b3a474 glPopAttrib, 0x00b3a5d4 glMatrixMode, 0x00b3a508 glPushMatrix, 0x00b3a5c0 glLoadIdentity, 0x00b3a4a4
    glPopMatrix) and the matrices by `smViewportRender`'s `glGetFloatv` (0x00b377c0 = rndCameraMatrix, 0x00b37840 =
    rndProjectionMatrix). The reset requests are wired at the exe's four sites with `catMotionCues` on: `universeReset`
    (the M2-R4 `CAT-TODO(stage 3)` there misread it as a recorded-game camera mode), `utyNewGameStart`,
    `smSensorsCloseForGood` (the two `CAT-TODO(M2-R6 NIS/camera)` markers); `kasfSetMotionCues` 0x00498550 is a KAS
    function (stage 6).

## fow (done)

The fog of war: `src/Game/FOW.c` / `FOW.h` (new, the exe's block 0x0047e7f0-0x00480d50 with no HW1 sibling) and the
sites that use it. **Nothing here changed the Mission01 stream**: `r6nfc-fow1-mission` (6,000 ticks, the grids, cubes,
waiting sites, the universe margin) and `r6nfc-fow2-mission` (plus `univFindNearestResource`'s fourth argument): rc 0,
ASan 0, sha256 `24253cbae125485f` both, the base's hash.

- **Who gets a grid: the local player only.** `cat_fowCreatePlayerGrids` 0x004a38c0 compares each index with the word
  at 0x00a43cae, which is `curPlayer` (the Globals.c block: gameIsRunning 0x00a43c90, multiPlayerGame 0x94,
  multiPlayerGameUnderWay 0x98, singlePlayerGame 0x9c, tutorial 0xa0, ..., numPlayers 0xac, curPlayer 0xae; the
  disassembly `mov ax, ds:0xa43cae; cmp edi, eax` at 0x004a38e4) and makes a grid for that player alone; an AI player
  has none and sees every ship that is not fully cloaked and undetected (`cat_fowCanPlayerSeeShip`'s NULL branch). So
  the DONE WHEN's "`Player.catFOWGrid` non-NULL for every player" is the exe's "for the local player": the Mission01
  run prints `oracle: mission fow players=2 curPlayer=0 grid0=set grid1=NULL` (a new stdout line after "mission run
  done", not in the stream).
- `FOW.c`: the grid (`CatFOWGrid` 0x20 bytes ILP32, `CatFOWCell` 0x20: usage, visited, centre, three dirty flags; the
  10,000-unit cells, 0x0087f774), `fowCreate` 0x0047e9a0, `fowDestroy` 0x0047ed30, `fowSelect` 0x0047ed70, `fowClear`
  0x0047ed80, `fowActivate` / `fowActivateCube` / `W` 0x0047edf0..0x0047efb0, `fowDeactivateCube` / `W` 0x0047f070 /
  0x0047f1d0, `fowGetUsageCount` / `W`, `fowCubeHasUnvisited` 0x0047f400, `fowGetVisited` / `W`, `fowWorldToGrid`
  0x0047f6b0, `fowIsValidW` 0x0047f750, `cat_fowGridSize` 0x0047e7f0; the boundary renderer (the sensors manager's
  explored / occupied outline): `fowRender` 0x0047f810, `cat_fowBoundaryCreate` 0x0047f850 / `Destroy` 0x0047fb00,
  `fowCellMarkDirty` 0x0047fb80, `cat_fowBoundaryUpdate` 0x0047fcf0, `cat_fowBoundaryRebuildFaces` 0x0047fd80 (the 12
  edges per cell against the three cells across each edge, the table 0x0072a878), `cat_fowCellCornerGet` 0x004801a0,
  `cat_fowBoundaryDraw` 0x004802d0 (16-step colour ramps 0x0087f778..0x0087f7bc, the corner triangles at 0.2 of a
  cell), `cat_fowBoundaryDrawList` 0x00480650 (distance-coloured GL_LINES, restarted every 64 lines),
  `cat_fowLineAdd` 0x00480a90, `cat_fowCornerLinesAdd` 0x00480be0; `cat_fowCanPlayerSeeShip` 0x0047e810 (selects the
  player's grid and leaves it selected, as the exe), `cat_shipAffectsFOW` 0x00480ce0 (not a component, not a swarmer).
  CAT-FIX guards: a NULL grid tested before the range test (`fowActivate`, `fowGetUsageCount`), `fowRender` with no
  grid, the colour index of a one-line batch (the exe's `__ftol` of a NaN).
- `LevelLoad.c`: `cat_fowCreatePlayerGrids` (the box +-smUniverseSize; the local player's ships light their cubes;
  `CAT-FIX`: a grid made again by `singlePlayerLoadNewLevel` frees the previous level's first, the exe leaks it), called
  in `levelInit` 0x004a3a10 after the sensors parameters; **the universe box grows by 5,000 per axis** in
  `SetSMMissionTweaks` (`levelInit` and `levelStartNext` 0x004a3c80 inline it: `smUniverseSize* += 5000.0`, 0x0072a618,
  before `smUpdateParameters`; HW1 had no margin). `SinglePlayer.c`: `singlePlayerLoadNewLevel` calls it (0x0051b1f4).
- `UnivUpdate.c`: `cat_univShipFOWActivate` 0x00542240 / `Deactivate` 0x005422b0 (the cube at `cat_shipGetFOWRadius(ship,
  0)`), `cat_univShipFOWUpdate` 0x00544b10 (the cube follows the ship's cell; on entering unvisited cells, at most every
  10 s of real time, the overlay's "enemy spotted" / "slipgate spotted" notice for the first non-allied ship there:
  strings 0x00bb4e30 = `strEnemySpottedNotification`, 0x00bb4e68 = `strSlipgateSpottedNotification`, by the string
  table's order from 0x00bb4e18 = `strWorkerNeedsCrystalTech`; `CAT-FIX`: the exe restores the selected grid from an
  unset local when the player has no grid); the cube in `cat_univAddShipModules` (univCreateShip's add path, 0x00542b5a),
  `univWipeShipOutOfExistence` 0x005477d0, **`univRemoveShipFromOutside`'s second argument** 0x00547990 (1 from
  `cat_dockShipDocksAtShip` x3 and `UpdateHyperspacingShip`, 0 from the inlined `InstantlyHyperspaceOut` copies: read
  from the pushes at each call site), `cat_univSetShipPosition` 0x0054d180, the stealth detection in
  `cat_univShipDeferredUpdate` (0x00544a8a: catTypeFlags 0x20000, `(univUpdateCounter & 0xf) == (shipNumber & 0xf)`),
  and **`univFindNearestResource`'s fourth argument** 0x00549c50 (the fog of war for a player without an AI from
  `cat_clHarvestNearestResource` 0x004497a0; plus the exe's attribute 0x200 test, the mimic-asteroid test with its
  stream-0 draw, and its clip test, which compares the unmodified first argument of `pieMovePointClipToLimits` and so
  drops nothing, where HW1 tested `univObjectOutsideWorld`).
- `ShipSelect.c`: `cat_shipStealthDetectionUpdate` 0x0053fd90 (allies detect, except player 1 in single-player mission
  14; a detector ship, catTypeFlags 0x10000 with upgrade 0x800000, within its FOW radius; the flags set aloud, and for
  the leech also cleared). Mission01 has no stealth type (`stealthUpdates=0` in the mission run); the command self-test's
  scripted mimics are, and its log now prints `commandtest: fow grid=... stealthUpdates=N`.
- `Physics.c` `physUpdateObjPosVelShip` 0x004e4950 (the cube follows the move), `Universe.c` (`universeReset`'s
  `fowDestroy`, `universeSwitchToPlayer`'s `fowSelect` 0x0053ea10), `Dock.c` (`dockPutShipOutside`'s
  `cat_univShipFOWActivate`), `SinglePlayer.c` (`PutShipOutsideAt` 0x005180a0), `ResearchAPI.c` (`cat_rmTechSetAdd` /
  `Init`) and `Alliance.c` (`allianceSetAlliance`: the cubes off and on around the change).
- The waiting sites of `cat_fowCanPlayerSeeShip`: `ACVHouseKeep` 0x005ae8c0, `HiveFrigateHouseKeep` 0x005b5b30,
  `RammingFrigateHouseKeep` 0x005c0090 (a target the owner cannot see cancels the special order, `clWrapSpecial` with
  no targets), `cat_ccShipIsLegalToFocusOn` 0x0042cd40 (CameraCommand.c), and **`processAttackToDo` 0x00441c10's
  prelude** (not in the tree before: with no target seen a multiplayer game halts, a single-player one treats the
  order as having no target left unless the leader is `SPECIAL_2_CatExempt20`; the recorder term is a
  `CAT-TODO(stage 5 recorder)`). `selShipUnderMouse` 0x00506550's call (the cursor's enemy test behind 0x00a5b1c8) is
  not ported: its body is still HW1's (no review entry; the select owner's).
- The picking helpers' grid: `cat_selPositionHiddenByFOW` 0x00509400 and `cat_selShipSelectableInFOW` 0x0050d3a0
  (Select.c) on the player's grid; `cat_uiFindNearestSpecialTarget` 0x00563e20 returns NULL without a grid (the
  `&& FALSE` stand-in removed), `cat_mrEnterSlipgate` 0x005665a0 takes the slipgates whose cell was visited
  (`fowGetVisitedW == 1` after `fowSelect`, 0x00566674); `pingListDraw` 0x004e8850's test (a ping other than an
  anomaly ping, `TOMask == PTOM_Anomaly` = ping+0x54 == 4, only where the fog does not hide it); the sensors scene draws
  `fowRender` (`cat_smViewportRenderScene` 0x0050b4b0). `catFowMode` (0x008b8848) has no writer in the exe (no store in
  the r6 decompile): it stays 2.
- The saves: `cat_saveSavePlayerFOW` 0x004ff6b0 / `cat_saveLoadPlayerFOW` 0x004ff7e0 in SaveGame.c (the local player's
  grid as the exe's two BASIC chunks, sub-types 0x2000 (the 0x20-byte grid) and 0x3000 (the cells); loading rebuilds
  the boundary with every cell dirty; `CAT-FIX` for a NULL grid on save; the recorder gates 0x00a578f8 / 0x00a75194
  are a `CAT-TODO(stage 5 recorder)`).

## beam-clamp (done)

- **The beam's sixth argument** (`collCheckBeamCollWithTargetsInBlob` 0x0043c570, Collision.c): the exe pushes six
  pointers and a seventh value (read from the pushes at 0x0043ca74..0x0043ca91 and 0x0043cb64..0x0043cb80): the sixth
  is the Sentinel-field triangle out-parameter (initialised -1 in `collCheckBeamColl` 0x0043c900), the seventh the beam
  length, which is `collCheckBeamColl`'s own third argument (`collCheckBulletTargetColl` 0x0043cd90 pushes Bullet+0x100,
  `traveldist`, at 0x0043d2e5), so the tree keeps reading `bullet->traveldist`. New in the blob function: a beam of
  another player (Bullet+0x14C vs Ship+0x32C) that crosses an active sSentinel / bSentinel field
  (`cat_collLineSentinelFieldIntersect` 0x0043abf0) nearer than the nearest hit stops there (side 0, the triangle out)
  and that Sentinel is not tested further; every other hit sets the triangle to -1. `collCheckBeamColl` then hits the
  field with the triangle (`cat_sentinelFieldBulletHit`, 0x0043cbb5, no damage) before its old field-active test
  (0x0043cc08). Found on the way, also the exe's: **the beam length test is `v*v - dsqr < length*length`** in all
  three places (the blob function 0x0043c6eb..0x0043c6fc, the missile target 0x0043c9e4, the blob loop 0x0043cb4a)
  where HW1 (ad1bb82 collision.c:1827) and the tree tested `v - sqrt(dsqr) < length`; computed in double as the exe's
  x87 (53-bit precision control); the square root moves after `collCheckRectLine` as in the exe.
- `cat_clampUpdateAttachedObjects` 0x00434b50: located and ported with the NIS (subsystem `nis`).

## Mission01 stream: the pre-droid pair

`tools/oracle/mission_test.sh --ticks 6000 --label r6nfc-predroid --no-perturb-run` (the tree after `nis`, `camera`,
`fow`, `beam-clamp`; debug, ASan): `r6nfc-predroid-a` / `-b` rc 0, ASan 0, **byte-identical, 1,476,370 B, sha256
`24253cbae125485f`** (`cmp` and `hwo_compare.py` IDENTICAL; the flipped-copy check PASS). Against the base run
`r6nfc-base-mission` (a6e72c9): `IDENTICAL: 6001 ticks and 0 packets compared`. Against R5's baseline
`r5-integrate-mission-a` (`645f94c69d68319e`): first divergent tick 1 in `rng6` (CAND 10 draws ahead) and `rng_all`
only, the move the managers port made before this work (docs/m2-r6-managers.md "Mission01 stream after"); with those
two fields left out (`--fields` all the others) `IDENTICAL: 6001 ticks ... no difference`. So nothing of this
producer's pre-droid work moved the stream; the R5 pair cannot be byte-IDENTICAL any more because of producer 4.

## madanim-droids (done)

### Mesh animation (MeshAnim.c, MadLinkIn.c)

- **The exe's madanim** (MeshAnim.h; `cat_madAnimClear` 0x004b69d0 clears 0x38 bytes): +0x08 `flags` is now the
  **mode** madAnimationStart's third argument sets (`MAD_ModeLoop` 1, `MAD_ModeBackToPrevious` 2, 0 =
  `MAD_ModePauseAtEnd`), +0x0C `nCurrentAnim` holds the animation **number** (madstatic.animIndexes maps it when the .shp
  numbers its animations, `cat_madAnimIndexOfNumber`), two new words: +0x10 `catPreviousAnim`, +0x18
  `catBindingSwapPending`; ILP32 asserts (+0x18, curves +0x28, saveBindings +0x2C, bindings +0x30).
- `madAnimationStart(ship, number, mode)` 0x004b6b90: a playing animation becomes the previous one (unpaused first), the
  bindings swap is deferred to the next update, the participation bit is the number's (taken before the lookup), no
  fatal for a missing start keyframe, the mode stored last. `CAT-FIX`: an invalid number or no madBindings returns.
- `madAnimationUpdate` 0x004b6ce0: the deferred swap (saving the gun bindings once); **the per-type animation sound** at
  the first frame (`cat_madAnimSoundEvent`, the table SoundEvent.c has since M2-R5 sound); at the end the mode decides:
  loop, **pause on the last frame and stay current** (mode 0: `madAnimationPause(ship, TRUE)`, TRUE returned), or end as
  madAnimationStop (mode 2). The header's MAF_Loop is not read.
- `madAnimationStop` 0x004b6e70 (unpause, restore the bindings only if swapped and forget them, current -> previous),
  `cat_madAnimationFinished` 0x004b7090 (the number mapped; `CAT-FIX` for a type without numbers),
  `madAnimBindingsDupe` 0x004b6a10 (current -> previous), `madAnimBindingUpdate` 0x004b6720 (IDENTICAL in the map,
  found MODIFIED: returns with no animation, maps the number, loops by the mode word). `cat_madAnimationStartNumber`
  (the M2-R4 stand-in) is gone: its 25 callers pass the exe's mode, each read from the push before the call (`push 0x2`
  etc.; the register-held ones resolved: `AcolyteHouseKeep` 0x005ad871 (0, 0) / 0x005ad935 (1, 2), `HiveFrigateHouseKeep`
  (1, 0) (0, 0) (1, 0), `cat_workerRepairTarget` (0, 0) x2, `MothershipInit` 0x005bb99b (0, 1), `RammingFrigateHouseKeep`
  0x005c0375 (2, 0)): Acolyte 2, Carrier 3, Dock.c 7 (link, unlink, salvage), HiveFrigate 4, Leech 4, ModuleShip 1
  (loop), Mothership 1 (loop: the `CAT-PROVISIONAL` closed), NIS 1, RammingFrigate 4, Salvage 2, Sentinel 2, Worker 4.
- `univCreateShip`: `cat_madAnimClear`, and the start state by `built & 1` (0x00542a1a: built or placed; HW1's NIS value 2
  and its fatal error gone). `univMeshBindingsDelete`: `CAT-FIX` (the exe restores saveBindings unchecked, NULL while a
  started animation waits for its swap, and so leaks the gun bindings).
- MadLinkIn.c: `madLinkInUpdateMeshAnimations` 0x004a51a0 (a cued animation starts with mode 0 and plays its sound; no HW1
  Mothership door upkeep or door "Ka-chunk"), `madLinkInSetUpInitialBuiltMadState` 0x004a5420 / `Placed` 0x004a54c0
  (only for a type without numbered animations; the Kushan kHeavyDefender starts with closed guns; HW1's R1/R2 cases and
  the `Mothership.h` include retired). **IDENTICAL compared, 8 found modified**: `madOpenGunsShip` 0x004a5560,
  `madLinkInCloseGunsShip` 0x004a56b0, `madLinkInPostDockingShip` 0x004a5800, `madLinkInPreDockingShip` 0x004a5910,
  `madLinkInOpenDoor` 0x004a5a20, `madLinkInCloseDoor` 0x004a5ae0, `madLinkInOpenSpecialShip` 0x004a5ba0,
  `madLinkInCloseSpecialShip` 0x004a5cb0 store sound 0x405 for every opening and 0x406 for every closing (HW1:
  ShipCmn_AnimOpening / Closing / DamagedOpen / DamagedClose = 0x406..0x409; `CAT_SHIPCMN_ANIM_OPEN` / `CLOSE`, names
  chosen here); their random picks (`ranRandomFn(10) % n`) are HW1's `randombetween`. `madLinkInCloseGuns`,
  `madLinkInPostDocking`, `madAnimationPause`, `Fix_madBindings`: the exe's.
- RammingFrigate.c's `stage 3` tag: `cat_rammingFrigateLatchTarget` gives up in single player while the NIS scissors are
  fully in (`nisFullyScissored == 1`, 0x005c121b).
- **The dock self-test's harvesting Worker** now holds the paused end of its jack animation (number 0, mode 0) and
  harvests: the test logs `mad=<number>/<paused>/<at end>` on each harvest line and `jackPausedTick=` on its done line.

### The repair droids (RepairDroid.c / .h)

- `droidCreateArray` 0x005c3070 (the 0x18-byte header, 0x2C per droid, one object block of `sizeof(Ship)` per droid,
  each an sRepairDroid object off every list; no array for the Beast; `CAT-FIX`: the block is cleared),
  `cat_droidFreeArray` 0x005c32b0, `droidUpdateArray` 0x005c32f0 (the seven states: inside, launching from the next
  type-17 dock point out along its cone by the fly-away distance, landing on the nearest hull vertex
  (`cat_meshObjectNearestVertex` 0x004b4ef0, new in Mesh.c), walking for 2..20 s, welding (the `EGT_WELDING_EFFECT`
  special-purpose effect, 0x00c41d10 = table index 20, every 0.5..2.5 s), one step between welds, returning along the
  hull (`cat_droidWalkTowardPoint` 0x005c2b80) until `droidIsValidPredockPos` 0x005c2fd0 lets it fly into the point,
  docking; velocity = 0.7 x the gap; random stream 0 for every time and walk step), `cat_droidRandomWalkVertex`
  0x005c29e0; the entry fields named (state, target, vertex, polygon, previousVertex, stateTime, nextWeldTime) and the
  header's (dockPoint, nextLaunchTime, lastHealth). The x87 sums are computed in double (53-bit precision control);
  the weld distance through `fsqrt` ("Decisions" 23).
- Wired: `univCreateShip` 0x00542659 (a Sect type with nRepairDroids), `FixShip` 0x00500950 (made anew after a load),
  `univFreeShipContents` 0x0054b550 and `univDeleteDeadShip` 0x00547d00 (`cat_droidFreeArray`),
  `cat_univShipSelfRepair` 0x00543490 (`droidUpdateArray` with `enableRepairbots`, for a Sect static); the renderer's
  test of the state byte (`cat_droidRenderArray` draws the droids outside the ship).
- **`/catRenderTest`**: turns `enableRepairbots` on; Mission01's Sect fleet is in the hyperspace ether at tick 16 (no
  ship in the universe has droids: `shipsWithDroids=0`), so the test adds a player sProcessor (5 droids) at
  (10000, 10000, 0), damages it to half health with upgrade 0x200000 and focuses the camera on it; new lines
  `rendertest: droids ...` and `rendertest: counts droids=N droidUpdates=N`, and **droids > 0 is part of PASS**.

## Mission01 stream: the post-droid pair

`tools/oracle/mission_test.sh --ticks 6000 --label r6nfc-postdroid --no-perturb-run` (the tree with the mesh animation
and the droids): `r6nfc-postdroid-a` / `-b` rc 0, ASan 0, **byte-identical, sha256 `24253cbae125485f`**, and
`hwo_compare.py` against the pre-droid pair `r6nfc-predroid-a`: `IDENTICAL: 6001 ticks ... no difference`. **The stream
did not move**: the run's new stdout line says `repairbots=1 droidUpdates=0`: no Sect ship with droids is damaged
in the universe during the 6,000 ticks (the player's Sect fleet waits in the hyperspace ether, the 154 ships in play
are player 1's), and the mesh-animation change touches no hashed field in this run (`r6nfc-mad1-mission`, the
madanim step alone: also `24253cbae125485f`). So the change "confined to the stream-0 fields" is empty here; the
baseline stays `24253cbae125485f` (the managers' move against R5's `645f94c69d68319e`: `rng6` and `rng_all` only).

## Build results

`tools/build.sh --debug` and `--release`: green, 0 warnings under -Werror after every subsystem (the final builds after
the last edit). `tools/m2_layout32.sh`: OK; it now also includes NIS.h (nisevent 0x28, nisheader 0x84, nisplaying 0x54,
...) and FOW.h (CatFOWGrid 0x20), and through SpaceObj.h the new madanim offsets.

## Plain-harness runs (final binary unless marked)

| run | mode | milestone | first stop | result |
|---|---|---|---|---|
| `r6nfc-base-mission` | mission 6,000 | oracle mission run done | none (rc 0) | base a6e72c9: `24253cbae125485f`, ASan 0 |
| `r6nfc-fow1-mission`, `-fow2-mission` | mission 6,000 | oracle mission run done | none | after `fow`: `24253cbae125485f`, ASan 0, grid0=set grid1=NULL |
| `r6nfc-predroid-a` / `-b` | mission 6,000 (mission_test) | oracle mission run done | none | **pre-droid pair** byte-identical `24253cbae125485f`, ASan 0 |
| `r6nfc-mad1-mission` | mission 6,000 | oracle mission run done | none | madanim alone: `24253cbae125485f` |
| `r6nfc-postdroid-a` / `-b`, `r6nfc-postdroid2-a` / `-b` | mission 6,000 | oracle mission run done | none | **post-droid pair** byte-identical `24253cbae125485f` (without / with the motion-cue draw) |
| `r6nfc-final-a` / `-b` | mission 6,000 | oracle mission run done | none | final debug pair: `24253cbae125485f`, ASan 0, `stealthUpdates=0 repairbots=1 droidUpdates=0` |
| `r6nfc-final-rel-a` / `-b` | mission 6,000, release | oracle mission run done | none | byte-identical `cddd8cf978fe2897`; `hwo_compare` against the debug stream IDENTICAL (build identity only) |
| `r6nfc-final-render` | render | render self-test done | none | PASS, ASan 0: `droids=2500 droidUpdates=583` (sProcessor#154, 5 droids), effects 1241 |
| `r6nfc-final-dock` | dock | dock self-test done | none | PASS, ASan 0: `jackPausedTick=1024`, the harvester at `mad=0/1/1` |
| `r6nfc-final-command` | command | command self-test done | none | PASS, ASan 0: `fow grid=set stealthUpdates=387` |
| `r6nfc-final-nisparse` | nisparse | nisparse self-test done | none | PASS, ASan 0, no leak (14 NIS, 14 scripts, 18 animatics) |
| `r6nfc-final-hud` | hud | hud self-test done | none | PASS, ASan 0 (the sensors frames draw through the grid) |
| `r6nfc-final-fe` | fe | Main_game_screen | still running at 120 s | ASan 0, no leak |
| `r6nfc-sp-final2` | sp, 360 s | universe running (univUpdateCounter 3,712) | still running at the 360 s timeout | ASan 0, no leak (R5: 1,248 ticks); `r6nfc-sp-final` (same binary, background) also reached 3,712 with 0 ASan reports but its summary was not written |

Earlier runs of this session (superseded): `r6nfc-dock-1`, `-2`, `r6nfc-command-1`, `r6nfc-hud-1`, `r6nfc-nisparse-7`,
`r6nfc-render-1` .. `-4` (`-3` stopped at `Fatal error - ... sCarrier was not loaded`: Mission01 does not load the
sCarrier; the test uses an sProcessor), `r6nfc-sp-1` (univUpdateCounter 3,712 at 360 s, ASan 0; overlapped rebuilds),
`r6nfc-fe-1`.

## New self-test lines and harness changes

- `/catNisParseTest`, harness mode `nisparse` (subsystem `nis`).
- `/oracleMission`: a stdout-only line after "mission run done": `oracle: mission fow players=.. curPlayer=.. gridN=set|NULL
  stealthUpdates=.. repairbots=.. droidUpdates=..` (not in the stream).
- `/catRenderTest`: the repair droids (above); droids > 0 joins PASS.
- `/catDockTest`: `mad=` on the harvest lines, `jackPausedTick=` on the done line.
- `/catCommandTest`: `commandtest: fow grid=... stealthUpdates=N` after its done line.
- tools/m2_harness.sh header documents all of these; tools/m2_layout32.sh includes NIS.h and FOW.h.

## Delta map

`ghidra/delta_map_review.json`: 183 entries with sample `port-R6-nis-fow-camera` (138 from the `nis` and `camera`
subsystems, 45 from this session's `fow`, `beam-clamp`, `madanim-droids`; an existing entry keeps its note with this
review appended after " | "), merged with `tools/delta_map.py merge --done m2r6:nis-fow-camera`: counts now IDENTICAL
2,942, MODIFIED 1,394, REWRITTEN 84, reviewed 2,797. **IDENTICAL bodies found modified this session** (class changed
to MODIFIED): `levelStartNext` 0x004a3c80 (the universe margin), `madAnimBindingUpdate` 0x004b6720, `madOpenGunsShip`
0x004a5560, `madLinkInCloseGuns` 0x004a5660 (the ion-array frigates skipped), `madLinkInCloseGunsShip` 0x004a56b0,
`madLinkInPostDockingShip` 0x004a5800, `madLinkInPreDockingShip` 0x004a5910, `madLinkInOpenDoor` 0x004a5a20,
`madLinkInCloseDoor` 0x004a5ae0, `madLinkInOpenSpecialShip` 0x004a5ba0, `madLinkInCloseSpecialShip` 0x004a5cb0 (11; with
the `nis` / `camera` ones, 9 + 3, **23 in all**); [audit: the 23 class changes in the map are these 11, 8 of the 9 NIS ones (`nisCurrentTimeSet` stays IDENTICAL: the removed check was HW1's `NIS_ERROR_CHECKING` debug check) and 4 camera ones (`cameraInit`, IDENTICAL in the base map, is MODIFIED for its two new fields, besides the 3 `cc*` named in "camera"); the merge also synced 91 entries of the map (82 of them the managers port's) with the review file committed at a6e72c9 (notes, change text and confidence only, no class change)] compared and the exe's: `madAnimationPause`, `Fix_madBindings`,
`madLinkInPostDocking`. Every MODIFIED / IDENTICAL function of NIS.c, Animatic.c, MeshAnim.c, MadLinkIn.c, Camera.c
and CameraCommand.c now has a review entry (0 unread). FOW.c and RepairDroid.c's runtime are cat-new (no HW1 sibling).
Partial entries say so in their note (`SaveUniverse`, `LoadUniverse`, `PutShipOutsideAt`, `UpdateHyperspacingShip`,
`FixShip`: only the FOW / droid parts compared).

## CAT-TODOs

Closed (75 tagged lines removed in src/): every `CAT-TODO(M2-R6 FOW)`, `CAT-TODO(stage 3 FOW)`, `CAT-TODO(M2-R6 ships(, the
repair droids))`, `CAT-TODO(M2-R6 mesh animation ...)`, `CAT-TODO(M2-R6 NIS/camera)` and the `CAT-TODO(stage 3)`
madAnimationStart / FOW / letterbox tags in the files above (ResearchAPI.c, CameraCommand.c, UnivUpdate.c, Dock.c,
Universe.c, Ping.c, Physics.c, Sensors.c, CommandLayer.c, ResCollect.c, Select.c, Alliance.c, mainrgn.c, HiveFrigate.c,
ACV.c, RammingFrigate.c, RepairDroid.h, MeshAnim.c, SoundEvent.c, Salvage.c, Mothership.c (the `CAT-PROVISIONAL` loop),
Leech.c, Worker.c, Sentinel.c, ModuleShip.c, Carrier.c, Acolyte.c, render.c, NIS.c, utility.c). New, re-targeted:
`CAT-TODO(stage 5 recorder)` x2 (the recorder terms of `processAttackToDo` and of the FOW saves) and, from `nis`,
`CAT-TODO(stage 6 KAS)` x3 (the letterbox finish, the KAS-hashed names). `grep -rn "CAT-TODO\|CAT-PROVISIONAL" src/ |
grep -i -E "M2-R6 (FOW|ships|mesh|NIS|camera)|stage 3 FOW"`: nothing.

## What is left (owner)

- `selShipUnderMouse` 0x00506550: its body is still HW1's (no review entry); the exe's cursor enemy test calls
  `cat_fowCanPlayerSeeShip(curPlayer, target, 0)` behind 0x00a5b1c8 (the select / main-region owner: stage 3 integrate
  or the next Select.c reader).
- `SelectionSwitchPlayerOwner`'s FOW part and `kasfDeleteShips` / `kasfHasShipBeenDiscovered` (stage 6 KAS, with their
  bodies); the recorder gates (stage 5 recorder).
- Mission01's oracle run never draws the droids or the motion cues (no damaged Sect ship in play, the main view not drawn
  in the headless mission run), so the oracle stream does not cover them; the reference recording (oracle mode A) will.
- The NIS-first start of a mission and the NIS playback measurement (stage 6; the motion-interpolation producer's mode `nis`).
