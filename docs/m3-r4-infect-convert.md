# M3-R4 ad hoc: an infected ship whose Beast form the level never loaded (CAT-FIX infect-convert), 2026-10-26

Landed from the user's terminal session 38027cce between rounds, after the user's Mission07 crash of 19:47.

## The user's words (verbatim)
- "Okay can you look at the crash logs? I think I hit an edge case in mission 7 where an infected missile hit my heavy
  crusier that I salvaged last mission. I think this may have triggered a capture event where my heavy crusier would have
  switched teams. Can you look at what happened?"

## The crash log
`original/play-home/logs/play-20261026-190316.log` (the release build, 19:03 to 19:47). The Mission07 load lists every ship
static it loads: line 52 `shipstatic: loaded RACE_TAIDAN/tHeavyCruiser.shp type=80 ...` is the salvaged cruiser carried in by
the fleet; the Beast statics are Mission07's eleven `IncludeShips RACE_BEAST` lines (`mission07.mission` 65-75: bCruiseMissile,
bInterceptor, bAcolyte, bACV, bHiveFrigate, bProcessor, bRammingFrigate, bRecon, bSentinel, bSwarmer, bWorker), no
bHeavyCruiser. Then 483 `WARNING: ships not capable of attacking told to attack` lines over the whole mission (CommandLayer's
`clAttackThese` safety, not the crash), and `hwi-engine: fatal signal 11` with no message and no backtrace (release build, no
core: apport's core pattern and `ulimit -c 0`).

## What the game does on an infection
- `cat_beastInfectShip` (src/Game/BeastWeapon.c:87, exe 0x004298f0) stuns the ship and sets its conversion time (the user's
  "capture event"); every infection path ends there: the Beast cruise missile's ram (Collision.c:718), the Beast weapons
  (BeastWeapon.c:410, `cat_beastAttackHit`), the script (KASFunc.c:5819) and the Beast salvage (Salvage.c:1309, 1877).
- `cat_univBeastInfectionUpdate` (src/Game/UnivUpdate.c:9066, exe 0x00543860) -> `cat_univConvertShip` (exe 0x00543950) ->
  `univAddShip(beastAttackConvertTo, GetValidRaceForShipType(...), ...)` -> `univCreateShip` (exe 0x00542320), which reads the
  new type's static straight (LOD, mesh, guns, specifics size).
- `race_taidan/tHeavyCruiser.shp`: `beastAttacksRequired 1`, `beastAttackConvertTo bHeavyCruiser`; 37 types are infectable
  (the Somtaaw line to its Beast twins, the Taiidan heavy cruiser and multi-gun corvette, the P1 fighter, ion array frigate
  and missile corvette, the two derelict prison / Ecclesiam ships, the traders' floating city).
- Which statics a level loads: the mission's IncludeShips (`SetInfoNeededForShipAndRelatedStaticInfo`, LevelLoad.c:2230,
  exe 0x004a29f0: the type, its missile and its mine, nothing else) plus the hyperspace fleet's own types (`levelPreInit`'s
  fleet and inside-ships loops); `universeStaticInit` (Universe.c, exe 0x0053d2a0) loads the flagged ones and frees the
  rest. A saved game stores the flags (`universeSaveEverythingNeeded`) and `PreLoadGame` restores them
  (`universeLoadEverythingNeeded`) before the same `universeStaticInit`.
- So the fleet's tHeavyCruiser is loaded, its Beast form bHeavyCruiser is not, and the conversion creates a ship from an
  unloaded static: SIGSEGV. The debug build's own guard says it first (`univCreateShip`, UnivUpdate.c:1940: "RACE_BEAST\
  bHeavyCruiser was not loaded properly - stubbing out", compiled out of the release build, which runs on into the fault).

## Reproduced
HWI-TEST `/catInfectAt <tick>:<ShipType>` (with `/catLoadGameTest`): that many ticks after the load the current player's
first ship of that type is infected by the first Beast-owned ship (`cat_beastInfectShip`, what the cruise missile's hit
does) and `infecttest:` lines report it. On the user's save "07 - Outskirts of Kadiir Nebula" (19:26), 1280x720 offscreen,
`/catInfectAt 200:tHeavyCruiser`:
- before (`m3r4-m7infect-before`): `infecttest: infected tick=345 ship=tHeavyCruiser#253 by bCruiseMissile#0 (player 1)
  convertTo=bHeavyCruiser loaded=0 convertAt=9562.12 now=9552.12`, then the fatal "not loaded properly" at UnivUpdate.c:1940
  = the user's crash site.
- after (`m3r4-m7infect-after`): at the load `universeStaticInit: loading RACE_BEAST\bHeavyCruiser, the Beast's form of
  RACE_TAIDAN\tHeavyCruiser (CAT-FIX infect-convert)`; `infecttest: infected ... loaded=1`; `infecttest: converted tick=624
  old=tHeavyCruiser mine=0 new=bHeavyCruiser mine=0 beast=1` (the player's cruiser is gone, the Beast owns a heavy cruiser);
  `loadgametest: done ticks=2500`, no stop, ASan 0.

## The exe
The same structure, read in `ghidra/exports/cataclysm_gog_r3a/decompiled.c` (a decompile reading; not run on the exe):
`FUN_004a29f0` sets bit 0 of static+0x30 for the type and, for the six missile carriers (0x55, 10, 0x5f, 0x19, 1, 0x22) or
the minelayer (0x54), of the race's missile or mine static, nothing else; `cat_univConvertShip` 0x00543950 ->
`univCreateShip` 0x00542320 reads `GetShipStaticInfo(param_1)` with no loaded check. So cataclysm.exe crashes the same way
when a ship whose Beast form the level lacks is infected. Hence a **CAT-FIX** (a fault the exe shares), not an HWI-FIX.
The campaign's own include lists are closed under the Beast forms in every Beast mission but three (the counterparts of
included infectable types that are not included; Beast = the mission includes a RACE_BEAST type):

| mission | Beast | missing Beast forms of its own includes |
|---|---|---|
| 01, 02, 03, 06, 08, 09, 12, 13 | no | (9 to 12 each; nothing can infect there) |
| 04 | yes | bMothershipEnginesLower |
| 05, 07, 10, 11, 14, 17 | yes | none |
| 15 | yes | bACV bMultiBeamFrigate bProcessor bRammingFrigate bSentinel bWorker |
| 16 | yes | bMultiBeamFrigate (with bCruiseMissile included: a latent exe crash on a player's multi-beam frigate) |

The hole is the fleet the player brings in: Mission07 is closed for its own types and never expected a Taiidan heavy
cruiser (Mission06, the salvage mission, includes tHeavyCruiser and no Beast at all).

## The fix (CAT-FIX (M3-R4 infect-convert))
`src/Game/Universe.c`: `cat_universeFlagBeastForms` and a second load pass in `universeStaticInit` right after the ship loop.
When the level loads any Beast ship type, every loaded infectable type (`beastAttacksRequired >= 0`) gets its
`beastAttackConvertTo` flagged needed, in that type's own race (`GetValidRaceForShipType`; with the carrier's missile, as
`SetInfoNeededForShipAndRelatedStaticInfo` does), and the pass loads it like the first pass did; the post-load and health
passes then treat it as any other. It lives in `universeStaticInit` because the .shp fields exist only once a static is
loaded (a never-loaded static holds zeroes) and because both the level entry (`levelPreInit`) and a save
(`universeLoadEverythingNeeded`) go through it, so the user's existing Mission07 save is covered too. A level without Beast
ships gets nothing. Cost in the Beast missions: Mission04 loads bMothershipEnginesLower (53 file loads), 15 six types, 16
one; the others nothing beyond what the fleet brings. One `universeStaticInit: loading ...` line per addition in the log.
HWI-TEST: `/catInfectAt` (MissionSelfTest.c `catInfectTick`, main.c).

## Fence (debug builds of the worktree, ASan 0 in every run, both builds 0 warnings)
| run | result |
|---|---|
| `m3r4-m7infect-before` / `-after` | the reproduction above: the fatal stop before, the conversion after |
| `m3r4-ic-mission` | the Mission01 stream IDENTICAL to `m3r3i-e-mission-a` (6,001 ticks), the file list identical |
| `m3r4-ic-m7` | Mission07 self-test `objectives=4/4 result=PASS` at 16,592; IDENTICAL to `m3r3i-e-m7-a` (16,561 ticks); no addition (closed) |
| `m3r4-ic-m6` | Mission06 self-test `objectives=7/7 result=PASS` at 31,011; IDENTICAL to `m3r3i-e-m6-a` (30,980 ticks); no addition (no Beast) |
| `m3r4-ic-m4-before` / `-after` (the unfixed binary of this worktree vs this one) | Mission04 `5/5 PASS` at 20,818 both; the streams IDENTICAL (20,787 ticks); the after run's file list +53 lines, all bMothershipEnginesLower (the closure's one Mission04 addition) |
| `m3r4-ic-load07-before` / `-after` | the user's save loaded for 8,000 ticks with no infection switch (the unfixed binary vs this one): `loadgametest: done ticks=8000 ... ships=98` both; the streams IDENTICAL (8,003 ticks); the after run's file list +100 lines, 97 bHeavyCruiser and the three `bcarrier ... spiny.LiF` pages its materials share; `universeStaticInit: loading RACE_BEAST\bHeavyCruiser ...` once |

Tags: `CAT-FIX` 198 -> 201 lines, `HWI-TEST` 505 -> 511 lines. Pre-existing, not this change's: the UBSan misaligned `ushort`
load in `btgGetTexture` (src/Game/BTG.c:389) in every run that loads a save or changes level; and since 0c0d634 (the mine
recolour) the Mission03 / 06 / 07 file lists load six derelict junk `.LiF` textures (dJunk1Strut botmid, dJunk2Shipwreck
sidereardisks) a second time, right after the Taiidan mine's pages (`m3r4-mr-m6-a` shows the same twelve lines; the M3-R3
baselines six) — the registration the recolour triggers; harmless, a follow-up.

## For the user
The crash was the game's own edge case: a salvaged ship of a type the next mission never expected the Beast to convert
(the original cataclysm.exe has the same hole). With this build the Beast's form of every ship you bring into a Beast
mission is loaded with the level, so an infection converts the ship (you lose it to the Beast, as the game intends) instead
of crashing. Your "07 - Outskirts of Kadiir Nebula" save is covered: loading it loads the Beast heavy cruiser too (the log
says `universeStaticInit: loading RACE_BEAST\bHeavyCruiser, the Beast's form of RACE_TAIDAN\tHeavyCruiser`). The next
`tools/play.sh` rebuilds the release binary.
