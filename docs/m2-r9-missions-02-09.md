<!-- done: generator, mission-table, level-load, flows, mission-selftest, runs-streams-reviews -->
# M2-R9 producer 2: stage 6 part B, missions 02-09

Base commit `64f49a9` (branch `m2`, worktree `.claude/worktrees/m2`). Nothing committed. Every engine run went through
`tools/m2_harness.sh` (silent, headless); no Ghidra, no GOG game, no oracle DLL, no input injection, no gamescope.

## 1. Summary

- `tools/m2_kas2c_gen.py` now generates Mission02 .. Mission09 with Mission01 and Tutorial1 (`DEFAULT_SCRIPTS`);
  `--check` reports every output current; **891 bodies verified** against the machine code (callees and 32-bit
  constants equal), **10,026 all-constant calls' arguments equal** (before: Mission01 + Tutorial1, 149 bodies / 1,790
  tuples). 4 OVERRIDES (HAND-EDIT, reasons in the generator). Mission01.c byte-unchanged; Tutorial1.c changed in one
  line (a latent fault, section 2.1).
- `catKasScripts` rows 02 .. 09 filled; the CAT-TODO now names only Mission10 .. Mission17 (producer 3).
- The mission sphere's `"Mine"` (0x004a3f20) and `"Slipgate"` (0x004a1020) readers ported; no
  `CAT-TODO(M2-R9 part B)` left in LevelLoad.c.
- 96 kasf the new scripts call that no audit sampled were compared with the exe: 3 fixed (kasfSpecialOn's return,
  kasfVectorAdd and kasfVectorCopy adding a point on every call) [audit: said "2 fixes"; section 2.4 and the summary
  count 3]. The mission runs found 6 more bugs, fixed (section 2.4) [audit: said section 4, which is the flows].
- `/catMissionTestMission N` (+ harness `--mission N`): **missions 02 .. 09 each PASS with all objectives, each pair of
  runs byte-identical, ASan 0**. Without the switch Mission01's self-test is unchanged: 6/6, stream IDENTICAL to
  producer 1's audited `r9a2-missiontest-a`.
- Guard rails (final binary): headless Mission01 stream IDENTICAL to the audited `r9a2-mission-a`; Armed8, ai, build,
  loadgame IDENTICAL to the audited; LiveFireExercise2 moved (its two Slipgate lines now make two gates: 89 -> 91 ships).
  console `kas lines=6 ok=6 tablerows=486`. fe ASan 0. **sp: ASan 0, but the run now stops in Mission04** on a
  texreg assertion (section 7.3): the screenshot loop, which the gate now lets into Mission02 .. 09, reaches Mission04.

## 2. File by file

### 2.1 Generator and generated code

**`tools/m2_kas2c_gen.py`** (stage 6 part B rules; every rule is grounded in the exe's bytes):
- `DEFAULT_SCRIPTS = ["Mission01", "Tutorial1", "Mission02", .. "Mission09"]`; usage text updated.
- aisTeams garble generalised: Ghidra's `aisTeams[I].field[J] + k` forms (regex `AIS_ELEM`) are resolved to the
  absolute address `aisTeams 0x008dfd60 + I*0x9D10 + J*0xFB4 + 8` (+k bytes, or +k*4 through a pointer) and then to the
  hash / string they really are (the old `calibrate()` replaced).
- MSVC division / modulo magic (`div_magic`, Hacker's Delight): `/ d` and `% d` count the multiplier constants the
  compiler emitted; strength-reduced `* N` products are excluded from the C-only constant set; decimal constants
  >= 0x10000 are counted.
- Raw Ghidra literals (`RAWSTR_i`): resolved by their bytes in the exe (`ghidra_bytes()` decoder; exact candidates,
  explicit numbers / `DAT_` symbols excluded with a fallback; a `<` bound uses the cmp constant + 1; tuple-context
  disambiguation; positional mapping by the asm address order).
- Switch data (`switchdataD_*`) and in-function data are not constants of the body; the tail-jumped
  `kasfTeamHyperspaceOut` body rule; `lp*` / `lParam` symbols; `ubyte` / `ubyte *` locals;
  `(ShipPtr)(TEAM+0x1c)` -> `(ShipPtr)&CurrentTeamP->shipList`.
- Guards: the generator stops on `CurrentTeamP +/-` arithmetic and on unread `*(char **)` dereferences; the localised
  table form `*(char **)(&DAT_x + strCurLanguage * 4)` (and its folded byte-offset form) becomes
  `(&PTR_lstr_x)[strCurLanguage]` / `LSTRING_x[strCurLanguage]`.
- OVERRIDES (HAND-EDIT, 4):
  - `0x005eb0b0` Watch_Mission05_FleetIntel_WaitMessage: `msg = universe.players[3].cat_unknown_02EC + 0x516d;` ->
    `msg = (ubyte *)0xb7f8e9;` (Ghidra took the message hash 0x0b7f8e9 for a universe address).
  - `0x005ebd50` Watch_Mission05_Objectives: the same hash in the switch compare (`cmp eax,0xb7f8e9` at 0x005ebdab).
  - `0x00604c90` Watch_Mission08_Frigates_Guard: the dead-code `kasJump(0x4c9664, Init_Mission08_Frigates_Guard,
    Watch_Mission08_Frigates_Guard)` behind `(kasfShipsOrderAttributes(..) == 0) & 2` that Ghidra dropped
    (0x00604cb4 .. 0x00604ccc).
  - `0x006071e0` Watch_Mission09_Resourcers_Nothing: `kasfAssert(kasfShipsCount(pGVar1) <= 3);` on the ICF-folded stub
    0x0049ae30 (`setle dl; push edx; call 0x0049ae30`, 0x006072b5 .. 0x006072c3).
- `meson_text()` writes `src/Missions/meson.build`.

**Generated** (only through the generator): new `src/Missions/SinglePlayer/Mission02.c` .. `Mission09.c`; regenerated
`src/Missions/CatMissions.h`, `src/Missions/KasShared.c` (functions shared by ICF folding across missions),
`src/Missions/meson.build`. `Mission01.c` unchanged. `Tutorial1.c`: one line, Lesson12's
`kasfObjectiveCreate("L12Tactical", (char *)(*(char **)(0x8cd960 + strCurLanguage * 4)), ..)` (a dereference of the
exe's address 0x008cd960, which faults in this process the first time Lesson12 runs) is now
`LSTRING_008cd960[strCurLanguage]` from the new localised-table rule: the exe's own strings, a justified deviation
from "Tutorial1 unchanged" (the earlier output was a latent fault, not the exe's behaviour).

Rerun: `python3 -I tools/m2_kas2c_gen.py` (writes the default scripts), `python3 -I tools/m2_kas2c_gen.py --check`
(rc 0, "current" for every output, then the verification line), `--scripts MissionNN` for one script.

### 2.2 Mission table, readers, converters

- **`src/Game/SinglePlayer.c`**: rows `mission02` .. `mission09` of `catKasScripts` filled with
  `Init_MissionNN, Watch_MissionNN, MissionNN_Functions, &MissionNN_FunctionCount, CAT_MISSIONNN_WATCH_EXE` (watch
  addresses 0x005e0bb0, 0x005e50d0, 0x005ea950, 0x005f4420, 0x005fb6d0, 0x00602280, 0x006056a0, 0x0060db20 as
  before); the CAT-TODO retargeted to one line: `CAT-TODO(M2-R9 part B): the generated code of Mission10 ..
  Mission17 (..; producer 3)`. HWI-TEST hook in `singlePlayerInit` (0x00518c50) after Warp.script is read: under
  `HWI_ORACLE`, `/catMissionTestMission N` sets `catSpWarpEnabled` / `catSpWarpLevelTo` (section 5).
- `cat_spKasScriptGenerated` readers: **`src/SDL/render.c`** debugScreenshots gate comment (the gate reads the table:
  the screenshot loop now runs on through Mission02 .. 09). The KAS save converters (`cat_spKasFunctions`, save
  offset base) read the same table: missions 02 .. 09 now convert (no code change needed there).

### 2.3 Level load (`src/Game/LevelLoad.c`)

Table 0x00883a48 now has `{ "Mine", cat_scriptSetMineCB }` (0x00883ab8) and `{ "Slipgate", cat_scriptSetSlipgateCB }`
(0x00883ac8) between "MeteorStorm" and "Derelict".
- `cat_scriptSetMineCB` 0x004a3f20: `"%f %f %f %f"` (0x0088456c); a mine of `mineStaticInfos[RACE_Derelict]`
  (0x00b3b2e8) owned by players[1] in single player / players[7] in multiplayer, race 6 (also at +0x214), maxtraveldist
  = maxvelocity * 0.0625, REALlyBig camera distance / lifetime, `GS_MineLauncher`, hit effect
  `etgGunEventTable[race][GS_MissleLauncher][EGT_GunHit]`, damage 5000; ID table only in multiplayer; placed at the
  scanned point as the exe does (univInitSpaceObjPosRot at 0x004a41e8 takes the untransformed position);
  isMoving / haventCalculatedDist 1; blob, SpaceObj / Impactable / Missile lists, mesh state.
- `cat_scriptSetSlipgateCB` 0x004a1020: `"%f %f %f %d/%d %f %s"` (0x0088423c); the exe's fatals ("Bad Slipgate point
  definition: %s" 0x00884218, "Bad shiptype in %s" 0x00884204); transformed unless missionman; `univAddShip(type,
  RACE_Derelict, ..)`, gameStatsAddShip, yaw, pairing through `catSlipgatePending` (0x00c362dc,
  cat_slipgateSetPartner 0x005c71b0 both ways), `(~SOF_Targetable) | SOF_NotBumpable`, `cat_shipSetDisableReason(gate,
  1, 6)`.
- `levelPreInit` 0x004a3230: `catSlipgatePending = NULL` (0x004a3245).

### 2.4 Bugs the flows found (all fixed at the exe's body or marked)

| file | function (exe) | fix |
|---|---|---|
| `src/Game/KASFunc.c` | kasfSpecialOn 0x00497d30 | returns 1 after issuing the order (`mov eax,1` at 0x00497da4; 0 at 0x00497dab) |
| `src/Game/KASFunc.c` | kasfVectorAdd 0x00498dd0 / kasfVectorCopy 0x00498e30 | an existing labelled point is only overwritten (the exe returns at 0x00498dfe / 0x00498e5d); the port added a new point on every call (unbounded growth in Watch loops) |
| `src/Game/Tweak.c` | the tweak table (exe strings 0x004ae058 ..) | HWI-FIX: the 24 `twRepairRate[CLASS_*]` / `twSalvageDamageLimit[CLASS_*]` keys written out: `makeEntry` stringified the macro-expanded index ("twRepairRate[3]"), so no Tweak.script line matched; every repair rate and salvage limit was 0 (no Worker / Processor repair, Mission02's repair objective impossible) |
| `src/Game/TradeMgr.c` | AllowPlayerToResearch 0x0052e9a0 | CAT-FIX: a name that is no technology (-1: Mission09's "TECH_ADVANCED_SYSTEMS") is skipped; the exe writes the word below its table (0x00ba467e, read by nothing) |
| `src/Game/Blobs.c` | bobUpdateExtraCollBobInfo 0x00426160 | HWI: GoK's refactor assertion "no missile in a blob" removed (neither the exe nor HW1 assume it; Mission03's minelayer mines stopped every debug run) |
| `src/Game/BeastWeapon.c` | cat_beastGetAttackStaticInfo 0x00429b70 | HWI-FIX (64-bit only): ShipType is 8 bytes there, so the overlay struct read padding as the convert-to type (Mission07's infected convoy ship stopped the render in GetShipStaticInfo); a static copy is returned, ILP32 keeps the exe's pointer |
| `src/Game/Debug.c` | dbgFatal 0x00462a90 | HWI-FIX: an ASan build prints the stack trace before the fatal exit (debug aid; harness runs use /nodebugInt) |

kasf review: 96 kasf called by Mission02 .. 09 and sampled by no audit were compared side by side (decompile and
disassembly where unclear): 93 same as ported, the 3 above fixed (kasfSetCurrentMimicItem's default case differs only
for indexes the scripts never pass; kasfTeamHealthLowest's rounding is the exe's 2^52+2^51 trick = lrint).

### 2.5 Self-test (section 5) and tooling

`src/Game/MissionSelfTest.c` / `.h`, `src/SDL/main.c` (`/catMissionTestMission`), `src/Game/KASFunc.c` (HWI-TEST hooks
under `HWI_ORACLE` in kasfObjectiveCreate 0x00494e70, kasfObjectiveCreateSecondary 0x00494ed0, kasfObjectiveSet
0x00494f10, kasfObjectiveDestroy 0x00494f60, kasfObjectiveDestroyAll 0x00494f70; bodies unchanged),
`tools/m2_harness.sh` (`--mission N`, documented in its header).

### 2.6 Delta map

`ghidra/delta_map_review.json`: 50 rows got a `[review: M2-R9 missions-02-09]` note (levelPreInit,
AllowPlayerToResearch, bobUpdateExtraCollBobInfo, the five kasfObjective* hooks, singlePlayerInit, dbgFatal, and the
40 compared kasf that have HW1 rows: "compared again with the exe (Mission02..09 call it): same as ported"); the
other 56 compared kasf (kasfSpecialOn and kasfVectorAdd / Copy among them) are Cataclysm-only (no delta-map row).
`python3 -I tools/delta_map.py merge --done m2r9:missions-02-09`: "records changed 50", counts IDENTICAL 2762 /
MODIFIED 1575 / REWRITTEN 83 / reviewed 3346 (no class changed). `python3 -I tools/m2_stage_counts.py`: total
IDENTICAL 2762, MODIFIED 1575, REWRITTEN 83, catnew 3789, mission 2085. Not reviewed: scriptSetTweakableGlobals
0x00527820 (unreviewed row; only its table's keys were compared).

## 3. Builds

`tools/build.sh --debug` and `--release`: rc 0, 0 warnings under `-Werror` (every incremental build of the round).
Final binaries: debug `c20bc9e144f4ee31` (rebuilt from the final tree: same hash), release `5605f350fe299aa2`.
`tools/m2_layout32.sh`: OK (all ILP32 layout asserts hold, 45 ship-specific sizes). `python3 -I
tools/m2_save32_gen.py --check`: current. `python3 -I tools/m2_kasf_table_gen.py --dispatch --check`:
KASFuncTable.c and KASFuncDispatch.c current. `python3 -I tools/m2_kas2c_gen.py --check`: all current, "verified 891
bodies: callees and constants equal to the machine code's, 10026 all-constant calls' arguments equal".

## 4. Flows exercised

Every flow below was observed in the self-test logs of the final pairs (section 6):
- **arrival**: `G_ShipsHaveArrived` in every mission: the at-once missions (catSpMissionArrivesAtOnce 4 .. 8) at
  tick 35, Mission02 / 03 / 09 after the fleet's hyperspace roll-in at tick 212-213.
- **roll call**: Mission03, 04, 06, 08 jump with the roll call up and `GoNowHyperspace` clicked (its callback
  through feFunctionExecute; ships still out: remaining 22, 5, 39 and 55 [audit: said "38-55"; the roll-call lines of the final pairs]); Mission09 under fire, where the roll call offers
  `EmergencyHyperspace`, which is clicked.
- **letterbox jump** (catSpMissionNextLevel 2, 5, 7): Mission02 -> Mission03, Mission05 -> Mission06_NIS04,
  Mission07 -> Mission08 with no roll call.
- **NIS staging levels / animatic**: Mission03 -> Mission04_NIS02 (+ nis/n02.nis), Mission05 -> Mission06_NIS04,
  Mission06 -> Mission07_NIS05 (+ nis/n05.nis), Mission09 -> Mission10_NIS06; the NIS starts at the next level.
- **per-mission screens**: the objectives (`kasfObjectiveCreate` texts), the roll call screen, the hyperspace
  button; the screenshot loop (sp) rendered Mission01 .. Mission03 and entered Mission04.
- **failed jump return**: not reached by a PASS run; the mission-loss judgement (PLAYER_DEAD) was hit while the
  Mission08 player was being developed (`r9m2-p13-m8`: "missiontest: FAIL tick=9696 the player lost the mission")
  and is part of every judgement (failed=0 in all final runs).

## 5. The mission self-test

Switches: `/catMissionTest <ticks> /catMissionTestMission N` (HWI-TEST, `src/SDL/main.c`), harness
`tools/m2_harness.sh <label> --mode missiontest --mission N [--mission-test-ticks T]`. Without `--mission`
(`/catMissionTest <ticks>` alone) the run is Mission01's and unchanged (stream IDENTICAL to the audited; only the done
line gained `mission=1`).

**Start**: mission N starts through the exe's **warp start** (cat_spWarpToLevel 0x0051b9c0 with Warp.script's level N
record: Relic's fleet, RUs and technologies for that mission), as a copy of Warp.script with `WarpEnabled 1` /
`LevelToWarpTo N` would. Chosen over chaining because each test then depends only on its own mission (a chained
Mission09 would replay 02 .. 08), and because it is the exe's own entry for exactly this purpose.

**Judge**: the KAS objective calls report to the test (hooks above): an objective is complete when its script
destroys it by name or sets it complete; `ObjectivesDestroyAll` (the Hyperspace message's clean-up) completes only
an objective the table marks *held* (Mission04 Destroy, Mission09 Protect: their scripts have no other end, verified by
grep). PASS = level loaded, KAS started, fleet arrived, objectives created in the table's order (completion order free),
each completed after creation, the hyperspace jump begun, the next level loaded, no mission loss. The run ends 32
ticks after the next level loads. Lines: `missiontest: <objective> tick=N completed`, `missiontest: done mission=N
ticks=N objectives=N/M result=PASS|FAIL` (ticks = the mission level's ticks + the next level's), rc 0, run.hwo via
`/oracleDump`.

**The player's part** (no input injection; the command layer, the construction / research managers and ShipsDamage):
- M02: recon to the destroyer; sMothershipMicro; TECH_REPAIR / TECH_SALVAGE and the workers' upgrade; repair special
  (cat_uiSpecialTargetCommand) on the destroyer; salvage special on the pod; ShipsDamage on the raiders.
- M03: hangar module; the mimic disguise (cat_mrMimicTarget); ShipsDamage on the mines and around Cleesan; mothership
  moves.
- M04: docking bay; guard (protect) the ACVs / acolytes; TECH_RESEARCH_CLEESAN_ATTACK; defence around the mothership.
- M05: recon to the Bentusi; defence around the Bentusi / Caal-Shto; ShipsDamage on the Beast; harvest;
  sMothershipSpecial.
- M06: recons to the station and nav points Nav1 .. Nav4; the mothership guards the Caal-Shto, defence 16000 around
  her (the heavy cruiser group spared until discovered); mines; the cruiser group to 45% hull (the Leech scene), then
  destroyed; TECH_PHASED_TELEMETRY.
- M07: recon to the convoy; defence after the first infection; ShipsDamage on the infected.
- M08: two workers; harvest; ShipsDamage on the spy's pursuers; captures: salvage-upgraded worker pairs on three
  collectors and one frigate (damaged to 40%), recons scuttled for support units (the warp fleet uses 219 of 220),
  the capture escort (enemies within 5000 of the targets, sparing collectors, the frigate target and the spy);
  then NoSurvivors.
- M09: transports to the vector, the sensor, the engagement, defence of the transports.

## 6. Runs (final binary `c20bc9e144f4ee31`, plain harness)

### 6.1 Per mission (pairs `r9m2-f-mN-a` / `-b`, `--mission N --mission-test-ticks 90000 --timeout 3600`)

| mission | objectives | completions (tick) | jump / roll call -> next level | done ticks | run.hwo sha256 | pair | ASan |
|---|---|---|---|---|---|---|---|
| 02 | 6/6 | FindDestroyer 2352, Build 2352, RepairDestroyer 13152, DestroyPirates 21808, SalvagePod 24448 | 24976 letterbox -> Mission03 | 25136 | `724546f07da17829` | byte-identical | 0 |
| 03 | 5/5 | BuildHangarModule 1744, Find 7168, Mimic 8624, Lure 17184 | 17472 GoNow -> Mission04_NIS02 | 17825 | `9e9c9e9460dbd582` | byte-identical | 0 |
| 04 | 5/5 | Replace 1712, Escort 7424, Research 14592, Destroy 20400 (held) | 20464 GoNow -> Mission05 | 20816 | `cdbd36b35a03bc05` | byte-identical | 0 |
| 05 | 6/6 | InterceptBentusi 1536, SaveBentusi 11968, DestroyBeast 12480, ProtectCaalShto 12480, BuildEngineering 15520 | 16224 letterbox -> Mission06_NIS04 | 16384 | `fa71938ce90ba5a7` | byte-identical | 0 |
| 06 | 7/7 | Explore 2016, Scout 3520, Mines 4656, HvCruiser 27648, Leech 28176, Escort 30496 | 30576 GoNow -> Mission07_NIS05 | 30931 | `b20c11c04e77f9e5` | byte-identical | 0 |
| 07 | 4/4 | Mayday 2304, Protect 16368, Destroy 16368 | 16432 letterbox -> Mission08 | 16592 | `66b6ec46c98c6e55` | byte-identical | 0 |
| 08 | 6/6 | Resource 4032, Protect 4992, CaptureFrigate 18800, CaptureResourcers 28720, NoSurvivors 28736 | 29008 GoNow -> Mission09 | 29363 | `a405cd7344b40d48` | byte-identical | 0 |
| 09 | 6/6 | MoveTransports 2928, DestroySensor 3952, Wait 9728, Engage 16320, Protect 20160 (held) | 20224 Emergency -> Mission10_NIS06 | 20579 | `89efb68ab6f952ee` | byte-identical | 0 |

(Hyperspace is the last objective of each table, reached by the jump.) Every run: rc 0, milestone "mission self-test
done", first stop none, no unsuppressed leak; wall 78 .. 155 s.

### 6.2 Guard rails

| run | mode | rc / wall | milestone | first stop | result |
|---|---|---|---|---|---|
| `r9m2-f-mission-a`, `-b` (`mission_test.sh --ticks 6000 --no-perturb-run`) | mission | 0 / 27 s | oracle mission run done | none | ASan 0; a = b `be3e5030cebbea0b`; MISSION TEST PASS (flipped copy caught at 2000 / ships_health) |
| `r9m2-f-missiontest-a`, `-b` | missiontest (Mission01) | 0 / 63-69 s | mission self-test done | none | ASan 0; `done mission=1 ticks=11424 objectives=6/6 result=PASS`; a = b `aac59b117769f451` |
| `r9m2-f-console` | console | 0 / 16 s | console self-test done | none | ASan 0; `consoletest: kas lines=6 ok=6 tablerows=486` |
| `r9m2-f-skirmish-lfe2-a`, `-b` | skirmish LiveFireExercise2 | 0 / 17-19 s | skirmish self-test done | none | ASan 0; ticks=600 ships=91 PASS; a = b `4f97795dd286d44d` |
| `r9m2-f-skirmish-armed8` | skirmish Armed8 | 0 / 17 s | skirmish self-test done | none | ASan 0; ships=54 PASS; `3ac685f989ce36a0` |
| `r9m2-f-ai-a`, `-b` | ai 400 | 0 / 16 s | ai self-test done | none | ASan 0; checks=16/16 PASS; a = b `b9bdad1e3cae9349` |
| `r9m2-f-build-a`, `-b` | build | 0 / 24 s | build self-test done | none | ASan 0; PASS; a = b `3c29833a5ef6e20d` |
| `r9m2-f-loadgame-a`, `-b` | loadgame "01 - Hiigara" 3600 | 0 / 21-22 s | save-load self-test done | none | ASan 0; PASS; a = b `5e88987d393b6f7d` |
| `r9m2-f-fe` | fe 120 s | 124 / 120 s | Main_game_screen | none: running at the timeout | ASan 0, no leak |
| `r9m2-f-sp` | sp `--timeout 340` | 1 / 338 s | universe running | `texreg.c (569): Assertion of (reg->nPalettes < 8) failed` in Mission04 | **ASan 0**; 40 bytes leaked after the fatal exit; levels Mission01_NIS01, 01, 02, 03, 04_NIS02, 04 (section 7.3) |

## 7. Streams before and after

Fields compared: `python3 -I tools/oracle/hwo_compare.py REF CAND --fields
rng10,rng6,ships_n,ships_misc,ships_pos,ships_vel,ships_health,missiles,bullets,derelicts,resources,univcheck,dt,total,real`
(per-field first ticks also over rng0, rng_all, ships_rot, statcheck, players, fpcw).

### 7.1 Before (base `64f49a9`, before any src change)

`r9m2-before-mission6k-a` = `-b` `be3e5030cebbea0b`, `r9m2-before-mt` `aac59b117769f451`, `r9m2-before-lfe2`
`aae4dd0336966be4`, `r9m2-before-armed8` `3ac685f989ce36a0`: each IDENTICAL to producer 1's audited `r9a2-mission-a`
(`1028375d20f15d75`), `r9a2-missiontest-a` (`929138a2d6baac97`), `r9a2-skirmish-lfe2-a` (`2eb7b79363150f2d`),
`r9a2-skirmish-armed8` (`462a2414ccffa6e5`) (hashes differ by the header's build identity only).

### 7.2 After (final)

| stream | vs audited | moved fields (first tick) |
|---|---|---|
| headless Mission01 6000 (`r9m2-f-mission-a`) | `r9a2-mission-a`: **IDENTICAL**, 6,001 ticks | none |
| Mission01 self-test (`r9m2-f-missiontest-a`) | `r9a2-missiontest-a`: IDENTICAL, 11,426 ticks | none |
| Armed8 | `r9a2-skirmish-armed8`: IDENTICAL, 601 ticks | none |
| ai 400 | `r9a2-ai-a`: IDENTICAL, 401 ticks | none |
| build | `r9a2-build-a`: IDENTICAL, 7,001 ticks | none |
| loadgame | `r9a2-loadgame-a`: IDENTICAL, ticks 1984-5586 | none |
| LiveFireExercise2 (`r9m2-f-skirmish-lfe2-a`) | `r9a2-skirmish-lfe2-a`: FIRST DIVERGENT TICK 0 | ships_n, ships_misc, ships_pos, ships_vel, ships_rot, ships_health, univcheck @0; rng_all @1; rng6 @10; every other field IDENTICAL |

LiveFireExercise2's move is the Slipgate reader: `multiplayer/livefireexercise2/resource.missphere` has two lines
`Slipgate ,-28974.0,19407.7,0.0,1/2,0.0,dSlipgateLarge` and `.. ,45787.7,-30123.8,514.0,2/2,0.0,dSlipgateLarge`; the
exe makes two dSlipgateLarge ships of them, the port now does too (ships 89 -> 91). It moved with the readers
(`r9m2-g1-lfe2`) and nothing after: `r9m2-g1-lfe2` vs `r9m2-f-skirmish-lfe2-a` IDENTICAL. The Tweak.c, kasf and
other fixes of the round moved none of these streams.

### 7.3 The sp stop (found, not fixed: owner M2-R10 render)

Opening the screenshot gate to the generated missions lets `/debugScreenshots` run on: Mission01 -> 02 -> 03 ->
Mission04_NIS02 (n02) -> Mission04, where the render registers a ninth colour scheme for
`RACE_P1/P1IONARRAYFRIGATE/RL0/LOD0/PAGE0` (flags 0x36, loaded, not shared, 8 palettes) and `trColorsIndexAlloc`'s
HW1 assertion stops the debug run (diagnostic runs `r9m2-diag-sp`, `r9m2-diag2-sp`, with a temporary print, since
removed). The exe's trTextureRegister 0x00585760 appends the colour without a bound (0x005858ef ..) into a 64-byte
colour block (memAllocFunction(0x40) at registration): past 8 it writes beyond the block. The 8 existing colour infos
read back as non-colours (e.g. `81d4ebe0/818181f1`), so this texture's colour block is suspect (palette sharing by
CRC, `reg->palettes = otherReg->palettes`, places it at palettes + 0x400 * nPalettes of another texture's buffer).
ASan sees nothing (the game's own heap). Neither missiontest (headless) nor Mission01 meets it.

## 8. CAT-TODOs

- Closed: `CAT-TODO(M2-R9 part B)` in LevelLoad.c (the Mine / Slipgate readers).
- Retargeted: SinglePlayer.c `CAT-TODO(M2-R9 part B): the generated code of Mission10 .. Mission17 (..; producer 3)`.
- New: none.

## 9. For STATE.md

- `/catMissionTestMission N` (HWI-TEST, with `/catMissionTest <ticks>`; default 1 = Mission01 unchanged); harness
  `--mode missiontest --mission N`. Start = the warp start (Warp.script level N record). Tables / players in
  `MissionSelfTest.c` `catMtMissions[]` (02 .. 09; 10 .. 17 NULL).
- Lines: `missiontest: start mission=N ..`, `missiontest: <objective> tick=N created|completed`,
  `missiontest: next level tick=0 <level>`, `missiontest: end mission=N .. jump=T rollcall=<button> ..`,
  `missiontest: done mission=N ticks=N objectives=N/M result=PASS|FAIL`.
- `DEFAULT_SCRIPTS` = Mission01, Tutorial1, Mission02 .. 09; 891 bodies / 10,026 tuples; 4 OVERRIDES.
- Stream refs: Mission01 6000 `be3e5030cebbea0b` (content = audited); missiontest `aac59b117769f451`; LFE2
  `4f97795dd286d44d` (91 ships); Armed8 `3ac685f989ce36a0`; missions 02 .. 09 in section 6.1.
- delta map done label `m2r9:missions-02-09`.

## 10. What is left (owner)

- Mission10 .. Mission17 generation, rows and self-test tables: M2-R9 part B producer 3.
- The texreg colour-block stop in Mission04's render (section 7.3): M2-R10 render.
- scriptSetTweakableGlobals 0x00527820 delta-map row unreviewed (only its keys compared here): M2-R10 review.
- The failed-jump return flow has no PASS-path exercise (a deliberate loss test would be a separate switch): M2-R10
  mission tests.
