<!-- done: a-white-mines, b-mine-ai, c-fence, d-report -->
# M3-R3 producer 4: Mission03's mines (the white textures, the mine AI)

Base 96b0137 (the mission02 audit's checkpoint). The user's items 4 and 6 (cycle/answers/m3-r3-user.md, read at the
start; no later answer there changes this producer's scope).

Subsystems: **(a) white-mines** (the cause, the exe's path, the fix, the load log, the captures), **(b) mine-ai** (the
exe's state machine beside the tree's, the per-tick log), **(c) fence** (streams, replays, dumps, self-tests, fe / sp,
builds), **(d) report**.

## Summary

- **Item 4, the white mines: found and fixed (HWI-FIX).** Both kinds (the 199 level mines of race Derelict, the
  Taiidan minelayer's mines) have textures in the data; they were never registered. The tree read a mine's (and every
  missile's) team-colour row through a ship cast: right in the exe's ILP32 layout (+0xF0 for every kind,
  `meshTextureRegisterAllPlayers` 0x004ae910), 4 bytes early in the x86-64 build (`ShipStaticInfo` row at 0xFC,
  `MissileStaticInfo` at 0x100). Fixed in src/Game/Mesh.c `meshStaticTeamColor`. After: every mine material bound and
  loaded (64x64 / 32x64), the mine captures textured, batched byte-equal to `/meshImmediate`. Render only: **no stream,
  replay or dump moved.**
- **Item 6, the mine AI: the exe does the same; nothing ported.** The exe's state machine (`aishipGuideMine` 0x00416410,
  `aishipFlyMineToTarget` 0x00415fa0) is the tree's, line by line. What the user saw is the exe's **faster-away rule**: a
  mine drops a target that flies away from it faster than the mine's top speed (laid mines 600 u/s, level mines 1,000;
  Acolyte 825, Mimic 750, Recon 1,050), brakes to a stop (x0.5 a tick) and searches again once a second. Shown tick by
  tick with the new `/catMineLog` and `/catMineChase`.

## (a) The white mines (item 4)

### The two kinds and their textures

| Kind | Made by | Static | Mesh (LOD 0) | Textures in the mesh | In the archive |
|---|---|---|---|---|---|
| Level ("story") mines: 199 in Mission03 | `cat_scriptSetMineCB` 0x004a3f20 (a mission sphere's `Mine` line; race Derelict, colour scheme 0) | `mineStaticInfos[RACE_Derelict]` (exe 0x00b3b2e8) | `race_derelict\mine\rl0\lod0\mine.peo` (11 materials) | `sidereardisks` (64x64), `botmid` (32x64) | `race_derelict\mine\rl0\lod0..3\*.lif` |
| Laid mines: the minelayer `tMinelayerCorvette` of race Taiidan (`IncludeShips RACE_TAIDAN,tMinelayerCorvette`) | `MinelayerCorvette` through Gun.c (colour scheme of the layer, 5) | `mineStaticInfos[RACE_Taiidan]` | `race_taidan\mine\rl0\lod0\mine.peo` (4 materials) | `page0` (64x64) | `race_taidan\mine\rl0\lod0..4\*.lif` |

Both meshes name their textures and the textures are in `cataclysm.big` (the material tables read with a probe script
over the extracted files in `original/scratch-m2/m3r3-mines/data/`). Neither is a missing file, a path or a palette
fault: before this round **no mine texture was ever loaded** (Mission03's FileLoads.log of M3-R2 / mission02: no
`*/MINE/*.LiF` line; `RACE_BEAST/MISSILE` had 20).

(The Beast mine, `race_beast\mine\rl0\*\mine.peo`, has one material and **no texture at all** in the data: a Beast mine
is drawn untextured by the exe too. No mission's mines are Beast mines: Mission03's are Derelict and Taiidan.)

### The cause: a 64-bit layout fault in the team-colour read (HWI-FIX)

A mesh's textures are registered once per colour scheme the static's **team-colour row** allows
(`meshTextureRegisterAllPlayers`): a material gets one handle per scheme with its flag set; the renderer binds
`handles[object's colour scheme]` (`cat_meshMaterialTextureAndShade`), and a material with nothing registered, or no
handle for the drawn scheme, is drawn **untextured: the lit material colour (diffuse 0xc8c8c8), the pale "white" mesh**.

The exe (`meshTextureRegisterAllPlayers` 0x004ae910) asks `meshNameToStaticInfo` 0x004ae7b0 for the static (a ship's, a
derelict's, `missileStaticInfos[race]` 0x00ba0f40 for `MISSILE`, `mineStaticInfos[race]` 0x00b3abe0 for `MINE`) and
reads the row at **+0xF0 of whatever static it got** (`lea ebx,[eax+0xf0]` at 0x004ae94f; `meshRecolorize` 0x004af3a0
the same at 0x004af4e3 / 0x004af576 / 0x004af5a8). In its ILP32 layout every kind's row is at +0xF0:
`universeDefaultTeamColors` 0x0053c940 writes the ships' rows at 0x00b3b510 + type * 0x458, the missiles' at 0x00ba1030
(= 0x00ba0f40 + 0xF0) and the mines' at 0x00b3acd0 (= 0x00b3abe0 + 0xF0), and tools/m2_layout32.sh pins both
`ShipStaticInfo` and `MissileStaticInfo` to it.

The tree did the same with the ship cast, `((ShipStaticInfo *)info)->shipOrDerelict.teamColor`. **In the x86-64 build the
two rows are not at the same offset**: measured with the build's flags, `ShipStaticInfo.shipOrDerelict.teamColor` is at
**0xFC**, `MissileStaticInfo.teamColor` at **0x100** (`StaticInfoHealthGuidance` contains pointers, so it is padded from
0xFC to 0x100; `StaticInfoShipOrDerelict` repeats its arrays inline and has no padding before its row). The ship cast read
a missile's or a mine's row **4 bytes early**: 4 bytes of padding and the first half of the real row. Mission03's rows
(set correctly by `universeDefaultTeamColors`, checked with a temporary trace at each step of `levelPreInit`: Derelict
`11000000`, Taiidan `00000100`) were read as:

| Static | Its row | Read through the ship cast | Registered | Drawn with | Result |
|---|---|---|---|---|---|
| `mineStaticInfos[RACE_Derelict]` | `11000000` (schemes 0, 1) | `00001100` | schemes 4, 5 | scheme 0 | no handle: untextured |
| `mineStaticInfos[RACE_Taiidan]` | `00000100` (scheme 5) | `00000000` | **nothing** (`bTexturesRegistered` FALSE) | scheme 5 | untextured |

The batched meshes are not involved (the same with `/meshImmediate`), nor the loader or the registry: the textures were
never registered. **The same misread hit every missile static**: Mission03 with `-- /catMeshTexLog MISSILE/RL`
(`m3r3-mines-missile-before` on the before binary / `m3r3-mines-missile-after`):

| Missile static | Its row | Registered before | Registered after |
|---|---|---|---|
| `RACE_SECT/MISSILE` (the player's own) | `11000000` | schemes 4, 5 (read `00001100`) | schemes 0, 1 |
| `RACE_P1/MISSILE` | `10000100` | scheme 4 (read `00001000`) | schemes 0, 5 |
| `RACE_BEAST/MISSILE` | `01000000` | scheme 5 (read `00000100`) | scheme 1 |
| `RACE_KUSHAN/MISSILE` | `00000000` (no Kushan player in Mission03) | none | none (as the exe) |

So missiles were drawn untextured too whenever their colour scheme was not the misread one (no missile was in a captured
frame here: not pixel-checked).

**The fix (HWI-FIX, src/Game/Mesh.c `meshStaticTeamColor`)**: the row of a static from `meshNameToStaticInfo` is the
static's own: `((MissileStaticInfo *)info)->teamColor` for an entry of `missileStaticInfos[]` / `mineStaticInfos[]`
(an address test), else the ship / derelict row. Used at the five reads (`meshTextureRegisterAllPlayers`, the three
passes of `meshRecolorize`, and the HWI-TEST log). It is an HWI-FIX, not a CAT: the exe has no such fault (its +0xF0 is
right for every kind); the fix gives the exe's behaviour back.

### The load log and the draw log (HWI-TEST `/catMeshTexLog <substring>`)

New HWI-TEST switch `/catMeshTexLog <substring>` (src/Game/Mesh.c, src/SDL/render.c, src/SDL/main.c): for the meshes
whose file name has the substring, one `meshtex: register load ...` line per textured material a mesh load registers
(the static found, the row the registration uses, the handle per scheme) and one `meshtex: draw ...` line the first time
an object of the mesh is drawn with a colour scheme (`textures= bound= white=` and per material the scheme's handle state:
`loaded WxH`, `pending`, `free`, `no-handle-for-scheme`, `unregistered`). Read only.

Mission03 self-test, `-- /catMeshTexLog /MINE/RL /catMineView 300:150` (runs below):

| | Level mine (`RACE_DERELICT/mine/Rl0/LOD0/mine.peo`, scheme 0, first drawn tick 300) | Laid mine (`RACE_TAIDAN/mine/rl0/lod0/mine.peo`, scheme 5, first drawn tick 4832 / 14210) |
|---|---|---|
| before (`--bin` the build with only the HWI-FIX neutralised) | `textures=11 bound=0 white=11`, every material `no-handle-for-scheme`; registered `teamColor=00001100 handles=..,485,10485,..` | `textures=4 bound=0 white=4`, every material `unregistered`; `teamColor=00000000 handles=none (nothing registered)` |
| after, batched | `textures=11 bound=11 white=0`: `sidereardisks=loaded 64x64` (3+2 materials), `botmid=loaded 32x64` (6) | `textures=4 bound=4 white=0`: `page0=loaded 64x64` |
| after, `/meshImmediate` | the same | the same |

FileLoads.log: 3,170 file loads before, **3,174 after** (the mine `.LiF` files now read). [audit: the four new loads are `RACE_TAIDAN/MINE/RL0/LOD0..3/PAGE0.LiF` only; the Derelict mine's `sidereardisks` / `botmid` materials (`loaded 64x64` / `32x64` in the draw log) add no file load: before and after, FileLoads.log reads them as `RACE_DERELICT\dJunk2Shipwreck\rl0\Lod0..3\sidereardisks.LiF` and `RACE_DERELICT\dJunk1Strut\rl0\Lod0..1\botmid.LiF` (the registry's shared textures, texreg.c `sharedFrom`), already read for those derelicts; no `RACE_DERELICT/MINE` `.LiF` line in either run (m3r3a-mines-shot-lvl-before / -after)]

### The captures (Mission03, 1024x768, `--shot`)

New HWI-TEST switch `/catMineView <tick>[:dropped][:<distance>]` (src/SDL/render.c): from that universe tick it closes the
sensors manager and snaps the camera to the level mine (or with `:dropped` a laid mine) nearest the player's mothership,
at that camera distance; a new one each 16 ticks once it is gone. A capture run only (it moves the camera).

Region stats with `tools/ppm_region.py` (`colours` = distinct colours in the region; `differ` tol 0):

| Capture | Region | before | after (batched) | after `/meshImmediate` | before vs after | after vs immediate (whole frame) |
|---|---|---|---|---|---|---|
| level mine, tick 400 | 432,304,592,464 | 19 colours, mean 221/176/130 (flat, pale) | 10,657 colours, mean 85/63/41 | 10,657, identical | 25,389 of 25,600 px differ | **0 px** |
| level mine, tick 640 | same | 19 colours | 10,582 colours | identical | 25,401 px | **0 px** |
| laid mine, tick 5000 | 480,350,544,418 | 6 colours (247/189/138 flat) | 970 colours (team-coloured corners) | identical | 1,341 px | **0 px** |
| laid mine, tick 6000 | same | 6 colours | 977 colours | identical | 1,332 px | **0 px** |

Runs (`original/scratch-m2/m2r2-harness/runs/`): `m3r3-mines-shot-lvl-before` / `-lvl-after` / `-lvl-imm`,
`m3r3-mines-shot-drop-before` / `-drop-after` / `-drop-imm` (all `--mode missiontest --mission 3`, mission self-test
done, 5/5, ASan 0); the before binary `original/scratch-m2/m3r3-mines/hwi-engine-before-debug`.

## (b) The mine AI (item 6): the exe does the same; nothing ported

### The exe's state machine beside the tree's

Read from the machine code (tools/m2_fdis.py; the Ghidra export's types for these functions are mislabelled, so the
offsets are read from the instructions: `Missile` +0x1fc owner, +0x200 target, +0x210 missileNumber, +0x220 mineAIState,
+0x224 formation_position, +0x230 FORCE_DROPPED, +0x244 formationinfo; the static +0x28 maxvelocity, +0x100
MINE_STOP_FRICTION, +0x108 MINE_RANGESQR_FORCED, +0x10C maxvelocity_FORCED; an object's +0x58 position, +0x64 velocity,
+0xe0 collPosition). Every row was already ported and reviewed in M2-R7 (oracle-rung-aiship); this round re-read each
against the tree, line by line:

| Step | cataclysm.exe | The tree (src/Game/AIShip.c) |
|---|---|---|
| Dead mine | `aishipGuideMine` 0x00416410: `test ah,0x8` on the flags, return 0 | `SOF_Dead`: return FALSE |
| Target type | target objtype 2, 3, 5, 8 (jump table 0x004167dc): target = 0 | Asteroid, Nebula, Dust, Missile (enum 2, 3, 5, 8): NULL |
| State 0 `MINE_DROP_ATTACK` (a level mine starts here, `cat_scriptSetMineCB` 0x004a3f20) | -> 3 | -> `MINE_ATTACK_RUN` |
| State 1 `MINE_DROP_FORMATION` | no owner, or the owner farther than its `MineClearDistanceSQR` (owner static's custstatinfo +0x68): -> 4 (0x0041646e .. 0x004164b7) | the same |
| State 2 `MINE_SEARCH_AND_STOP` | friction: each velocity / rotation component times `MINE_STOP_FRICTION` (0.5) unless all three lie in [-1.0, 1.0] (0x0072a468 / 0x0072a4c4), then zeroed; on the tick where `univUpdateCounter & 15 == missileNumber & 15` (`MINE_DO_SEARCH_MASK` 0x008ac564 = 15: once a second at 16 Hz) acquire a target if it has none (`aishipmineaquiretarget` 0x004162c0); with a target -> 3 | `aishipslowminewithFriction` + `mine_do_search`: the same (inlined in the exe) |
| State 3 `MINE_ATTACK_RUN` | no target: -> 4 with a formation, else 2; else `aishipFlyMineToTarget`, and on its 0: the same | the same (the M2-R7 CAT line) |
| State 4 `MINE_WALL_FORMATION` | the search tick as state 2 (-> 3 on a target); then fly to `formation_position` until within 10.0 (0x41200000), else the friction | the same |
| Lifetime | `timelived += phystimeelapsed`, expired when above `totallifetime` | the same |
| [audit: added] lost-target lifetime | `univRemoveAllTargetReferencesFromMissiles` 0x00547570 (a ship that dies, leaves or docks): every missile **and mine** targeting it gets target 0 and, when more than 5.0 s of life remain (0x008b5c68), `totallifetime = timelived + 5.0`: a mine whose target was cleared expires 5 s later (deleted through `AddMissileToDeleteMissileList`) | the same (`cat_univClampResourceTimer`, UnivUpdate.c); m3r3a-mines-ailog: the four laid mines `cleared` at 14391 log their last tick at 14470 / 14471 (80 ticks) while ~1,000 units from the command ship; m3r3a-mines-chase-acolyte: mine 66 (`cleared` 1753) at 1833, 5,666 units from Acolyte #10 |
| `aishipFlyMineToTarget` 0x00415fa0: the speed | `FORCE_DROPPED` (+0x230): `maxvelocity_FORCED` (+0x10C), else `maxvelocity` (+0x28) | the same |
| range | `MoveReachedDestinationVariable(mine, target collPosition, MINE_RANGESQR_FORCED)`: the squared value passed as a distance (64,000,000: never fails in practice, as HW1); out of range: target 0, return 0 | the same |
| cloak / disabled | `cat_shipIsCloakedFully` 0x00436110 or flags & 0x800000: target 0, return 0 | the same |
| **faster-away** | `(target - mine) . targetVelocity > 0.0` (0x0072a460) **and** `|targetVelocity|^2 > maxSpeed^2`: target 0, return 0 (0x00416062 .. 0x004160b3) | the same (`//target is flying faster than mine can go so don't follow it!`) |
| chase | the mine moves into the target's blob; `vecNormalizeToLength(target - mine, maxSpeed)`; `aitrackVelocityVectorGuidance` | the same |
| acquire | `aishipmineaquiretarget` 0x004162c0: ships within `mineRange` (forced: `mineRangeForced`), not its player's or an ally's, not KAS-exempt, cloak / Leech / mimic rules, the heaviest (ties on stream 10) | the same (M2-R7 port) |

The static data that sets the speeds (the install's `Mine.shp` files): **level mines (Derelict) 1,000 u/s, laid mines of
the minelayer (Taiidan) 600 u/s** (`maxvelocity` and `maxvelocity_FORCED` both 600), `MINE_STOP_FRICTION` 0.5,
`MINE_RANGESQR` 8,000 ("can't be more or Mission 3 will not work"). The player's strike craft are faster: Acolyte 825,
Mimic 750, Recon 1,050 (their `.shp` files).

### What the user saw: the "faster-away" rule, as designed

A mine chases a target at its top speed while the target comes towards it or is slower. When the target **flies away
from it faster than the mine can go**, `aishipFlyMineToTarget` drops the target (the exe's own comment in HW1's source:
"target is flying faster than mine can go so don't follow it!"); the mine goes to `MINE_SEARCH_AND_STOP`, its speed halves
each tick (stopped within about 0.7 s), and once a second it searches again: it picks the same target up and, while the
target still flies away faster, drops it at once. That is the user's "travel toward their target for about 3 seconds then
stop in place trying to find the target again". A laid mine (600) loses every Somtaaw strike craft that flies away from
it; a level mine (1,000) loses a Recon, catches an Acolyte.

### The per-tick log (HWI-TEST `/catMineLog <ticks>`, `/catMineChase <tick>[:<shiptype>]`)

New HWI-TEST switches (src/Game/AIShip.c, src/Game/OracleDump.c, src/SDL/main.c): `/catMineLog <ticks>` logs every
state change of every mine with its reason (`minelog: change ... state=A->B why=acquired|start|faster-away|range|cloaked|
disabled|cleared`: `cleared` = the target pointer was cleared elsewhere: the ship died, left or docked,
`univRemoveAllTargetReferencesFromMissiles`) and, for the first 8 mines that start an attack run on a target, <ticks>
ticks of `minelog: tick=.. state target dist speed tspeed pos`. `/catMineChase <tick>[:<shiptype>]` orders up to four of
player 0's ships of the type through the command layer (`clWrapMove`) on a straight pass 2,000 units beside the nearest
level mine and 20,000 beyond (a test scene: it changes the game). Both read nothing back into the simulation except the
chase's order.

**Recon (1,050 u/s) past level mine 63** (`m3r3-mines-chase-recon`: `--mode missiontest --mission 3
--mission-test-ticks 1400 -- /catMineLog 480 /catMineChase 400`), mine 63's ticks:

| Tick | State | Target | Distance | Mine speed | Target speed |
|---|---|---|---|---|---|
| 1295 | 2 -> 3 (acquired on its search tick) | sRecon#6 | 9,818 | 0 | 1,050 |
| 1298 | 3 | sRecon#6 | 9,607 | 177 | 1,050 |
| 1310 .. 1366 | 3 (chasing) | sRecon#6 | 8,296 .. 1,247 | 1,000 | 1,050 |
| 1374 | 3 | sRecon#6 | 329 (the Recon passes it) | 1,009 | 1,050 |
| 1376 | **3 -> 2 why=faster-away** | none | | 975 | |
| 1377 .. 1387 | 2 (stopping: x0.5 a tick) | none | | 488, 244, 122, 61, 30, 15, 7.6, 3.8, 1.9, 1.0, 0 | |
| 1391 | 2 -> 3 (its next search tick, 1 s later) | sRecon#6 | | 0 | |
| 1392 | **3 -> 2 why=faster-away** (still flying away) | none | | 0 | |

Mine 62 likewise: acquired at 1294, chased 103 ticks (6.4 s), `faster-away` at 1397 at 1,000 u/s.

**Acolytes (825 u/s) past the same mines** (`m3r3-mines-chase-acolyte`, `/catMineChase 400:sAcolyte`, 2,600 ticks):
mine 63 acquires Acolyte #11 at 1535, chases it at 1,000 (the Acolyte is slower), overshoots at 80 units, turns and
closes again to 38 units at 1751: mine 63's log ends there and the Acolyte is gone (mine 66, on the same Acolyte, logs
`3 -> 2 why=cleared` at 1753 and stops; it re-acquires Acolyte #10 at its search tick 1762 and chases [audit: and expires at 1833, 5 s after the `cleared`, 5,666 units from it: the lost-target lifetime row above]). Mine 62's `faster-away` at 1755 comes one tick
after that blast near its target (logged speed 825 five ticks earlier).

**The Mission03 self-test with the log** (`m3r3-mines-ailog-a`, `-- /catMineLog 480`, 5/5 PASS, 17,825 ticks): the 199
level mines start (`0 -> 3 why=start`, no target, `3 -> 2` at once: no formation); 30 laid mines `1 -> 4` (cleared the
layer); six laid mines (600 u/s) `4 -> 3` on the Mimic (sMimic#19) at 14138 .. 14171, chase it for about 15 s, [audit: two of them (218, 221) reach it at 14389 (45 / 71 units) and their logs end there; the other four] lose it at
14391 (`cleared`: the Mimic's pointer was cleared: it died, left or docked), brake 600 -> 150 -> 37.5 -> 18.8 -> 4.7 and re-acquire the command ship (sMothership#0)
at 14393 .. 14398 on their search ticks [audit: and expire at 14470 / 14471, 5 s after the `cleared` (the lost-target lifetime row above)]; two level mines acquire Recons at 16871 / 16875.

**Before / after**: the tree is the exe here, so no change was made and the log above is both: the user's report
describes the exe's design. The level-mine / laid-mine difference the user may also notice (laid mines give up more
often) is the data: 600 against 1,000 u/s.

## (c) Fence

Builds: `tools/build.sh --debug` and `--release` rc 0, **0 warnings** under -Werror (after every subsystem);
`tools/m2_layout32.sh` OK; the win64 cross build (`ninja -C build/win64`, llvm-mingw) 226/226, no warning (no `_WIN32`
code touched). Every run is the debug build (ASan / UBSan) through tools/m2_harness.sh, labels `m3r3-mines-*` in
`original/scratch-m2/m2r2-harness/runs/`; **every run ASan 0, no unsuppressed leak.** Streams judged with
`python3 -I tools/oracle/hwo_compare.py` (all 21 fields, rng_all included), against M3-R2's exit runs:

| Stream | Run(s) | Against | Result |
|---|---|---|---|
| headless Mission01 (`--mode mission --mission-ticks 6000`) | `m3r3-mines-mission-a` / `-b` (sha256 4616da73824c375b, this build's header; 1,476,370 B) | `m3r1i-mission-a/run.hwo` (1ae66610c3f02410) | **IDENTICAL** both, 6,001 ticks, every field |
| missiontest Mission01 NIS-first | `m3r3-mines-missiontest-a` / `-b` | `m3r2i-e-missiontest-a` (c14c549ceac7b34f) | **IDENTICAL** both (11,426 ticks); 6/6 |
| **Mission03** | `m3r3-mines-m3-a` / `-b` | `m3r2i-e-m3-a` (d737643a58c0f58b) | **IDENTICAL** both (17,794 ticks compared); 5/5 PASS at 17,825; a vs b IDENTICAL |
| Mission03 failed jump (`--fail-jump`) | `m3r3-mines-fj3` | `m3r2i-e-fj3-a` (a3044330e4a63b5b) | **IDENTICAL** (17,669 ticks); 5/5, 17,667 |
| team colours Mission03 into Mission04 (`--mission 3 --teamcolour`) | `m3r3-mines-tc-m3` | `m3r2i-e-tc-m3` (df68de4e18348963) | **IDENTICAL** (17,794 ticks); teamcolour 6 checks, 0 failed (Mission03, Mission04_NIS02, Mission04) |
| Mission06 (minelayers) | `m3r3-mines-m6` | `m3r2i-e-m6-a` (cc2d450412d30d07) | **IDENTICAL** (30,980); 7/7, 31,011 |
| Mission10 (4 level mines) | `m3r3-mines-m10` | `m3r2i-e-m10-a` (a72352eb513d8239) | **IDENTICAL** (21,908); 5/5, 21,939 |
| Mission12 (26 level mines) | `m3r3-mines-m12` | `m3r2i-e-m12-a` (642575577c73c2de) | **IDENTICAL** (10,964); 3/3, 10,995 |
| Mission14 (32 level mines) | `m3r3-mines-m14` | `m3r2i-e-m14-a` (2ecb7d44c32a850d) | **IDENTICAL** (22,307); 4/4, 22,338 |
| skirmish LiveFireExercise2 | `m3r3-mines-lfe2` | `m3r2i-e-skirmish-lfe2-a` (a14774ffd6287143) | **IDENTICAL** (601) |
| skirmish Armed8 | `m3r3-mines-armed8` | `m3r2i-e-skirmish-armed8` (c996a4b4a7314be7) | **IDENTICAL** (601) |
| replay 10-17 (oracle01-r6) | `m3r3-mines-r6` | `m3r2i-e-r17-a` (c3b7d873ae8fb692) | **IDENTICAL** (9,601 ticks, 4,800 packets): the rung stays **2823** |
| replay 10-18 (oracle01-r18) | `m3r3-mines-r18` | `m3r2i-e-r18-a` (bd2c316848aff30c) | **IDENTICAL** (3,527 ticks, 1,763 packets): the rung stays **2871** |

The missions with mines or minelayers, from the levels' files (`Mine` lines, `tMinelayerCorvette`): 03 (210 `Mine` lines,
the minelayer), 06 (the minelayer), 10 (4), 12 (26), 14 (32): all run, all IDENTICAL.

**The four dumps** (first 16 hex of sha256, whole / with `crc=` stripped):

| Dump | This run | M3-R2 (`m3r2i-e-*`) | |
|---|---|---|---|
| `m3r3-mines-motion-noi` | c503c869f0dc9cdb / 3016280e9e0dd3c8 | the same | **EQUAL** |
| `m3r3-mines-motion-on` | c0e411662fa2f527 / 428ef41479eb5749 | the same | **EQUAL** |
| `m3r3-mines-nis-noi` | 86cb155acffc180e / 196bab32d244f614 | the same | **EQUAL** |
| `m3r3-mines-nis-on` | 0307f3be2044c29c / a5de13a44b8d264f | the same | **EQUAL** |

(No mine or missile is drawn in those scenes: the fix moves no pixel there.)

**Self-tests**: render `rendertest: done ... result=PASS` (583 frames); hud `hudtest: done ... result=PASS` (537 frames);
meshes 1,758 files, 0 with bad objects, every mine mesh `bad=0` (Derelict 11 / 8 / 3 / 1 materials, Taiidan 4 / 4 / 4 / 3 / 2,
Kushan 11 / 8 / 3 / 1, Beast 1 / 2 / 1 / 1 with no texture); textures 12,403 listed, 10,661 registered and loaded, 0 not
loaded (the mine `.LiF` among them). **fe** (`m3r3-mines-fe`): Main_game_screen, ASan 0. **sp** (`m3r3-mines-sp`,
`--timeout 1200`): to Mission12 (its `Mission12.btg` loaded) at the timeout, ASan 0, as M3-R2's `m3r2i-e-sp`.

**Every stream and dump before / after: nothing moved.** The change is the texture registration (render state: the
registry's handles and the textures loaded, FileLoads 3,170 -> 3,174 in Mission03) and HWI-TEST log lines; no simulation
field, no random draw. No re-baseline is needed.

Test-only runs (no baselines): `m3r3-mines-diag-a`, `-diag-rows` (the temporary trace, removed), `-shot-*` (a),
`-missile-before` / `-after` (a), `-ailog-a`, `-chase-recon`, `-chase-acolyte` (b).

## Files changed

- **src/Game/Mesh.c**: `meshStaticTeamColor` (HWI-FIX: the static's own colour-scheme row) used in
  `meshTextureRegisterAllPlayers` and the three passes of `meshRecolorize`; HWI-TEST `/catMeshTexLog`
  (`catMeshTexLogSet`, `catMeshTexLogMatches`, the registration log in `meshLoad`, `catMeshTexLogDraw`).
- **src/Game/Mesh.h**: the HWI-TEST prototypes.
- **src/SDL/render.c**: the `catMeshTexLogDraw` call before `cat_meshRenderState` for asteroids / derelicts / missiles
  (HWI-TEST); `/catMineView` (`catMineViewSet`, `catMineViewFrame`, called from `catLodStatsFrameBegin`).
- **src/SDL/render.h**: `catMineViewSet`.
- **src/Game/AIShip.c**: HWI-TEST `/catMineLog` (`catMineLogSet`, `catMineLogStep` at the end of `aishipGuideMine`,
  `catMineLogWhy` set at the exits of `aishipFlyMineToTarget` and state 3) and `/catMineChase` (`catMineChaseSet`,
  `catMineChaseRequested`, `catMineChaseTick`); no behaviour change.
- **src/Game/AIShip.h**: their prototypes. **src/Game/OracleDump.c**: `catMineChaseTick` after the motion test's tick.
- **src/SDL/main.c**: the switches `/catMeshTexLog`, `/catMineView`, `/catMineLog`, `/catMineChase` (HWI-TEST).
- **tools/m2_harness.sh**: header only (the four switches, under the missiontest mode).
- **ghidra/delta_map_review.json** (6 notes), **ghidra/delta_map.json** (`python3 -I tools/delta_map.py merge --done
  m3r3:mines`: records changed 6; IDENTICAL 2,757, MODIFIED 1,580, reviewed 3,582), **docs/m2-r1-function-deltas.md**
  (`tools/delta_map.py report`).
- **docs/m3-r3-mines.md**: this report.

No HWI-FEATURE, no setting, no cfg row, no console row, no settings-line row, no key, no control: the default look is the
exe's (textured mines). No CAT line (the exe's behaviour was already the tree's apart from the 64-bit fault). No CAT-TODO
closed or added; no CAT-TODO or CAT-PROVISIONAL of this producer names M3-R3 (the one left, BTG.c:1151's star size, is the
integrate's).

## Delta-map review entries (decision 55)

Notes appended ("[review: M3-R3 mines] ..."): `meshTextureRegisterAllPlayers` 0x004ae910 (the +0xF0 read and the
x86-64 misread, HWI-FIX), `meshRecolorize` 0x004af3a0 (the three +0xF0 reads), `meshNameToStaticInfo` 0x004ae7b0 (relied
on), `universeDefaultTeamColors` 0x0053c940 (Mission03's rows relied on), `aishipGuideMine` 0x00416410 and
`aishipFlyMineToTarget` 0x00415fa0 (compared again: as the tree). Classes unchanged.

## THE USER'S CHECK (for the play-test brief)

`tools/play.sh` (it rebuilds when the binary is older than the newest src/ commit), play into **Mission03**:

1. **The mines' textures (item 4).** Once the fleet has arrived, look at a minefield up close (the level's mines sit in
   clusters on the way to the CLEE-SAN; select one or zoom the camera to it) and later at the mines the minelayers
   drop. Expected: the level mines are grey-brown metal with vents and panels; the laid mines are square with
   yellow team-coloured corners. Report any mine still flat white, and whether missiles in flight (the player's Acolytes',
   the P1 missile corvettes') now show textures too (they had the same fault).
2. **The mines' behaviour (item 6), unchanged, the exe's.** Fly a group past a minefield: the mines chase while you come
   towards them and **stop and wait when a faster ship flies away from them**, then pick it up again if it turns back.
   The laid mines (600 u/s) give up on every Somtaaw strike craft flying away; the level mines (1,000 u/s) catch Acolytes
   but not Recons. Report a mine that stops while its target is slower and still coming towards it (that would be a
   fault). [audit: a mine whose target is destroyed (or leaves) disappears about 5 s later even while it chases a new
   target: the exe's lost-target lifetime, not a fault.] Optional evidence: `tools/play.sh -- /catMineLog 480` writes the `minelog:` lines into the play log.

## For STATE.md

- Decision (M3-R3 mines): the mines' and missiles' white textures were an x86-64 layout fault, not the exe's:
  HWI-FIX `meshStaticTeamColor` (Mesh.c) gives a missile / mine static its own colour-scheme row (the exe reads +0xF0 of
  every static kind). No setting; the textured look is the default and the exe's.
- Decision (M3-R3 mines): the mine AI is the exe's (`aishipGuideMine` 0x00416410, `aishipFlyMineToTarget` 0x00415fa0):
  the user's "3 s then stop" is the faster-away rule plus the once-a-second search; nothing ported.
- Names chosen: `meshStaticTeamColor`; HWI-TEST switches `/catMeshTexLog <substring>`, `/catMineView
  <tick>[:dropped][:<distance>]`, `/catMineLog <ticks>`, `/catMineChase <tick>[:<shiptype>]` (log prefixes `meshtex:`,
  `mineview:`, `minelog:`, `minechase:`).
- Streams: nothing to re-baseline (every stream, both replays, the four dumps IDENTICAL / EQUAL to M3-R2's).

## What is left

- The Beast mine has no texture in the game data (one untextured material): a Beast mine would look plain in the exe too.
  No mission places one. Owner: none (data, as the exe).
- Missiles' textures: registered for the right schemes now (the log above), but no capture of a missile in flight was
  taken. Owner: the engine-trails producer (item 7, the missile corvettes) can confirm in its captures, else the
  integrate's play-test brief.
- The other x86-64 casts of a non-ship static through `ShipStaticInfo` were not audited beyond the five texture reads
  (the ship-only readers in Salvage.c / UnivUpdate.c / ShipView.c take ships). Owner: M3 oracle physics (layout audits)
  if wanted.
