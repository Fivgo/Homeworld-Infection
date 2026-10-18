<!-- done: events, orders, moves, managers, aitest, gates -->
# M2-R7 ai-orders-moves: AI orders, moves, handlers, events, the attack and defense managers, the ai self-test

Status: DONE (M2-R7 producer 2; base commit `a3cc7f4`; nothing committed by the producer, the auditor checkpoints).

Subsystems: **events** (AIEvents.c / .h, AIHandler.c / .h), **orders** (AIOrders.c / .h, `cat_statsRandomAttackShipStatic`
in Stats.c), **moves** (AIMoves.c / .h, the move enum, parameters and layout asserts of AITeam.h, the P2 swarm code of
AITeam.c and AIUtilities.c), **managers** (AIAttackMan.c, AIDefenseMan.c), **aitest** (`/catAiTest`, harness mode `ai`,
new `src/Game/AISelfTest.c` / `.h`), **gates** (builds, mission stream, replay, fe, sp, dock, build).

## Headline

* The 14 HW1-type stand-ins of M2-R4 ships-b in my files are gone: `grep -c "CAT-TODO(stage 4" src/Game/AIOrders.c
  src/Game/AIHandler.c` is 0 and 0 (were 7 and 2; the 5 left in AIResourceMan.c (4) and AIFleetMan.c (1) sit in the
  managers' own bodies, producer 3's: no order of mine needs them). Every `aioCreate*` asks for Cataclysm's ships by
  the computer player's race with the exe's alternative tables (types and equivalents read from the exe's stack
  stores), the Beast rows that need research falling back as the exe does (`catShipTypeEnabled`).
* **The ai self-test PASSES**: `/catAiTest 400` (harness mode `ai`), two runs byte-identical (`run.hwo` 98,768 B, sha256
  `2f86c13e83803037`), ASan 0, no leak. The computer player (player 1 of Mission01, Beast, AI_ADVANCED, update rate 15)
  forms its attack, guard and recon teams at **tick 17**, gives the attack, defence and recon orders at **17**, issues
  the attack moves at **33** (team 2, 11 ships, move-team; first command-layer order: order 1, a move, to bMultiGunCorvette#286), the
  recon moves at **49** (move-to, bCloakedFighter#240) and the defence moves at **65** (the fast roving team's patrol:
  move-team-index, 9 ships, bHeavyCorvette#251): one AI update (16 ticks) a stage.
* **Mission01 stream: unchanged** by this producer: `7c224543c9ad751a` before (base binary) and after (final binary), both
  IDENTICAL over 6,001 ticks to the baseline `r6-integrate-mission-a` (49240abcf9f0ed97) but for the header's build
  identity; no field moved. Mission01's computer player has no fleet control (`giveComputerFleetControl` is set by the
  missions' KAS call, off under `CAT_HW1_MISSION_SCRIPTS 0`), so none of the ported managers, orders or moves runs;
  the stream-6 draws of `aiaInit` keep their count (the kamikaze pair is only reordered).
* **Replay of oracle01: unchanged**: byte-identical in every field to producer 1's final replay (`IDENTICAL: 9601 ticks
  and 4800 packets`), so IDENTICAL to the GOG recording in every simulation field through **tick 176** (first divergence
  177 `ships_vel`, producer 1's), the replay's computer player being skipped under playback as expected.

## 1. events (AIEvents.c / .h, AIHandler.c / .h)

| function | exe | change |
|---|---|---|
| `aieExecute` | 0x00403a70 | no shipDied check (HW1's was always FALSE) and no fuelLow / fuelHigh checks; the rest (teamDied first, then getting rocked, enemy (not) nearby, firing, disengage, health low / high, numbers low / high, interrupt) in the exe's order |
| `aieHandlersClear` | 0x00403eb0 | 12 slots |
| `AIEvents` struct | AITeamMove +0xCC..+0x154 | fuelLow / fuelHigh removed; ILP32 asserts: 0x88 bytes, numbersLow +0x48, teamDied +0x70, interrupt +0x74 (the offsets aieExecute reads) |
| `aieHandlerSetFuelLow` / `High`, `aieCheckFuelLow` / `High`, `aihGenericEmptyFuelHandler`, `aihSwarmerEmptyFuelHandler`, `aihGenericFuelLowHandler` | (none) | removed: no fuel in Cataclysm, none of them in the exe |
| handler table | 0x0074a3a4 (`aieHandlerToNum` 0x00405ce0) | the exe's 20 entries: no fuel handlers; the linker folded `aihReconShipTeamDiedHandler` into `aihReconaissanceTeamDiedHandler` (0x00406e10 twice) and `aihRemoveTeamDiedHandler` into `aihPatrolTeamDiedHandler` (0x00406e80 twice): same bodies, the tree keeps both names |
| `aihHarassNumbersLowHandler` | 0x00405d40 | Sect: 16 sAcolyte (sACV, sMimic); Beast: 16 bHeavyCorvette (bInterceptor, bAttackBomber, bAcolyte / bACV once researched); none for another race (CAT-FIX: the exe re-adds the move-team node) |
| `aihFastDefenseNumbersLowHandler` | 0x00406070 | empty team: Sect 12 sAcolyte (sRecon, sACV, sMimic), Beast 12 bInterceptor (bHeavyCorvette, bRecon, bAttackBomber, bACV) |
| `aihSlowDefenseNumbersLowHandler` | 0x004062d0 | empty team: Sect 3 sMCV (sACV, sMCV, sMultiBeamFrigate, sHiveFrigate), Beast 3 bMultiGunCorvette (7 alternatives) |
| `aihFastDefenseDistressHandler` / `aihSlowDefenseDistressHandler` | 0x00406880 / 0x00406af0 | the rescuing support type by the team ship's race (`cat_aihRescueSupportType`: k/tAdvanceSupportFrigate, s/bProcessor, 0x004068bd / 0x00406b39); the attack keeps a single-player script team's tactics (team +0x74, `aioCatAttackTactics`; 0x00406a00 / 0x00406c9c); the resourcer to rescue by the AI's race (tResourceCollector, sWorker, bWorker; 0x004069b2) |
| `aihKamikazeHealthLowHandler` | 0x00405fd0 | IDENTICAL in the map, found modified: no kamikaze order when no ship qualifies |
| `aihGravWellEnemyNearbyHandler` | 0x004066d0 | found modified: the cooperating team's attack is Aggressive (0x0040677c / 0x004067f6 push 2; HW1 Neutral) |

The other 14 AIHandler and 11 AIEvents bodies compared: identical (aieCheckHealthLow / High at 0x00404090 / 0x00404150,
the setters, the fix functions, the team-died handlers, getting rocked, patrol enemy nearby, grav well not nearby).
[audit: not the fix functions: `aiePreFixAIEvents` 0x00404210 and `aieFixAIEvents` 0x00404310 convert 12 slots in the
exe (no fuelLow / fuelHigh), so they are MODIFIED against HW1; this port edited them (the fuel lines removed) without a
review entry; the audit added both entries (sample `audit-R7-ai-orders-moves`) and merged them]

## 2. orders (AIOrders.c / .h)

All 22 exe orders (0x0040e160..0x00410920) rewritten from the exe; helper macros in AIOrders.h: `aioCatEnabledOr(type,
fallback)` (the inline `catShipTypeEnabled` read of every Beast row) and `aioCatAttackTactics(team)` (script team in
single player: `kasTactics`, else Aggressive); AIOrders.c's `aioCatTeamTactics(team, default)`.

| order | exe | Cataclysm (Sect / Beast unless stated) |
|---|---|---|
| `aioCreateGuardShips` | 0x0040e160 | sAcolyte + 6 alternatives / bInterceptor + 9; Kushan kHeavyDefender, Taiidan tHeavyDefender with no alternatives (CAT-FIX: uninitialised in the exe) |
| `aioCreateReconaissance` | 0x0040e3a0 | mothership recon: 1 sAcolyte + 6 / 1 bInterceptor + 8; active recons: 1 sAcolyte (sMimic) / bInterceptor (bCloakedFighter) |
| `aioCreateHarass` | 0x0040e740 | 15 sAcolyte + 3 / bInterceptor + 4 |
| `aioCreateDefendMothership` | 0x0040e950 | no fuel handler |
| `aioCreateFastRovingDefense` / `SlowRovingDefense` | 0x0040e970 / 0x0040eb40 | 12 sAcolyte + 6 / bInterceptor + 8; 3 sMultiBeamFrigate + 8 / bMultiGunCorvette + 10 |
| `aioCreateActiveSupport` | 0x0040ed80 | empty team: sCarrier / bCarrier (strike craft), sProcessor / bProcessor (resource) by the team ship's race (CAT-FIX: the AI's race for an empty team; the exe reads ShipPtr[0] of an empty selection) |
| `aioCreateFighterStrike` / `CorvetteStrike` / `FrigateStrike` | 0x0040ee70 / 0x0040f110 / 0x0040f3a0 | 13 sAcolyte + 9 / 15 bAttackBomber + 12; 9 sACV + 7 / 9 bHeavyCorvette + 12; 5 sHiveFrigate + 1 / 5 bIonArrayFrigate + 3; script-team tactics |
| `aioCreateCapture` / `aioCreateMine` | 0x0040f550 / 0x0040f5d0 | k/tSalCapCorvette 3..7 (Kushan / Taiidan only); tMinelayerCorvette 2..5 (Taiidan only) |
| `aioCreateSpecialDefense` | 0x0040f620 | found modified: either race's grav well generator gets the enemy-nearby handler |
| `aioCreateTakeoutMothershipFast` / `Big` / `Huge` / `Guard` | 0x0040f6b0 / 0x0040f9c0 / 0x0040fbc0 / 0x0040ff90 | the exe's tables by race and, for Big and Guard, by the player's mothership type (the six Cataclysm motherships and carriers; Big: CAT-FIX, no order for another race) |
| `aioCreateTakeoutTarget` / `FancyTakeoutTarget` | 0x00410700 / 0x00410920 | Sect / Beast targets only (plain); the reserve test adds a SOF_Dead or Traders mothership; constraints by class; the guess sMultiBeamFrigate / bIonArrayFrigate / tStandardFrigate / p1MissileCorvette; fancy: danger radius 16,000 (0x00410a48), secondary team from **`cat_statsRandomAttackShipStatic` 0x00524d70** (new, Stats.c: one stream-6 draw over the exe's per-race tables 0x0089700c..0x00897074), redrawn once above 20 (0x00410a96, 0x00410ad1) |

No order keeps a fuel-low handler. Removed (no exe counterpart, never called): `aioCreatePatrol`, `aioCreateReconHarass`,
`aioCreateReinforcements`, the five P2 orders.

## 3. moves (AIMoves.c / .h, AITeam.h / .c)

* **Move types**: the exe has no P2 swarm moves: MOVE_SWARMATTACK / SWARMDEFENSE / SWARMPOD removed from the enum, so
  fancy get-ships is 0x16, dock 0x17, kamikaze 0x2B, delete-team 0x2D as the exe's creators store them; the swarm
  parameters, moves (`aimProcessSwarm*`, `aimCreateSwarm*`, their callbacks and fix / save / load), the AITeam.c swarm
  helpers (`aitMoveSwarm*`, `aitFindNewPod`, `aitMoveSwarmersToNewPod`, `aitAllDefenseSwarmersFull`; their only callers
  were the swarm moves) and the AIUtilities.c helpers (`aiuSwarmDock`, `aiuFindSwarmerTarget`, `aiuFindSwarmTargets`,
  `aiuShipsCloseToEnemyMothership` with its caller `aiaProcessHarassTeams`) removed; the save table rows went with them.
* **AITeamMove layout**: ILP32 asserts: 0x170 bytes, type +0x10, events +0xCC, processFunction +0x154, processing +0x164,
  tactics +0x16C (m2_layout32 OK).
* **Default tactics**: the exe's creators store 2 (Aggressive) where HW1 had Neutral, for 26 creators (guard ships, get
  ships, var wait / dec / set / destroy, move team, hyperspace, count ships, flank, harass, dock, defend mothership,
  active patrol, mothership move, active capture, active mine, mine volume, special defense, delete team, guard coop,
  launch, formation, move done, attack, fancy get ships); control resources and active resource keep Neutral (1). The
  creators with no exe counterpart (var inc, move team index / split, move to, capture, resource volume) keep HW1's.
* **Process functions** (MODIFIED in the map): flank (0x004083f0), harass (0x00408d30), defend mothership (0x004094c0),
  temp guard (0x00409c50: Aggressive invader attack, race rescue type), active recon (0x0040a440: **no random tactics
  draw**), active capture (0x0040b3f0: `cat_aiuWrapCapture(ships, &onesel, 1)`, whose second argument is the dontIssue
  flag, so no capture order is given; CAT-FIX: the target selection freed): ported; get ships (0x004071b0): HW1's body
  (CAT-FIX: the dropped move allocation not made); advanced attack and dock: producer 1's.
* **IDENTICAL found modified**: hyperspace 0x00407dd0 (multiplayer clamp to +-smUniverseSize), active patrol 0x00409af0
  (script-team tactics), resource volume 0x0040ad00 (no harvesting in multiplayer ends the move; Ship words
  catResourcing +0x1D8 / catResourceFinished +0x1D4), resource-volume resource died 0x00407ac0, mothership move
  0x0040b000 (s/bMothership, s/bCarrier; Aggressive), special defense 0x0040b890 (k/tCloakGenerator), armada 0x0040a910
  (numShips tests on the target selection).
* The other process bodies compared (guard ships, get ships, var*, move, move split, intercept, move to, move attack,
  patrol, reinforce, support, ship recon, control resources, active resource, active mine, mine volume, guard coop,
  launch, formation, attack, special, kamikaze, fancy get ships): identical (aimProcessSpecial's and aimProcessIntercept's
  returns checked in the disassembly).

## 4. managers (AIAttackMan.c, AIDefenseMan.c)

| function | exe | change |
|---|---|---|
| `aiaSendRecon` | 0x00401000 | no fuel handler |
| `aiaArmada` | 0x00401120 | found modified: returns without enough armada teams; refinds a target when the target selection is empty; one recon team while none is out |
| `aiaPriorityShipsConstraints` | 0x004011d0 | CLASS_MonsterShip dangerous; non-dangerous frigates: sProcessor, bDFGFrigate, bProcessor, k/tAdvanceSupportFrigate, tDFGFrigate |
| `aiaGenerateAttackType` | 0x004012a0 | found modified: huge by the mothership's class (0x00401389); cloak / grav well generators by race, TRUE with no order for another race (0x00401638..) |
| `aiaProcessAttackTeams` | 0x004016f0 | found modified: attack type r % (singlePlayerGame ? 15 : 11) (0x0040176a); enemies within 30,000 (0x004017f0) |
| `aiaDivideNewShips` | 0x00401b50 | Cataclysm's pairs, Taiidan pairs and generators in single player only |
| `aiaAttackManager` | 0x00401df0 | found modified: the cleanup skips an empty slot (0x00401e10) |
| `aiaShipDied` | 0x00401fa0 | found modified: an emptied armada selection stays |
| `aiaInit` | 0x00402010 | found modified: features over both races' types; resourcing by class; the kamikaze threshold drawn before the roll (explicit order) |
| `aidSetupResourceDefenseTeams` | 0x00402aa0 | guards s/bWorker, s/bProcessor, k/tAdvanceSupportFrigate, tResourceCollector |
| `aidDefenseManager` | 0x004035a0 | the inlined defendable count over Cataclysm types (0x00403683) |
| `aidAddAllShips` | 0x00403070 | no fuel handler; mothership / carrier excluded by class |
| `aidCheckSphereOfInfluence` | 0x004028d0 | found modified: seconds rounded to nearest (`cat_aiuRoundToUdword`, now exported) |
| `aidCloakDefense` | 0x00402e00 | found modified: k/tProximitySensor by race; rounded seconds for the request, truncated for the alert reset (0x00402f4f / 0x00402fc3) |
| `aidMothershipDefense` | 0x00403130 | found modified: a lone fighter never counts (the stream-6 draw at 0x004031a4 made and dropped); no fuel handlers; the defense-team slot refilled only when empty, count decremented (0x00403248) |

Other bodies compared: identical (aiaProcessSpecialTeams, aiaTimeAttack, aiaTeamDied, aiaClose, aidSendDistressSignal,
aidClearDistressSignal, aidPositionProximitySensorGuard, aidTeamDied, aidShipDied, aidInit, aidClose).

## 5. aitest: `/catAiTest <ticks>` (harness mode `ai`)

New `src/Game/AISelfTest.c` / `.h` (HWI-TEST, REULA header), hooked in `OracleDump.c` (after each universe tick, as the
other mission-run self-tests) and `src/SDL/main.c` (`/catAiTest`); `tools/m2_harness.sh --mode ai [--ai-ticks N]`
(header documented; args `/catAiTest N /oracleDump <run>/run.hwo`; milestone `ai self-test done`; summary line
`aitest:`). Scenario: tick 16, player 1 gets its race's mothership as factory at the grid point farthest from the level's
ships (10000, -10000, 0), 30,000 RUs and a scripted fleet of its race (156 ships) handed to the AI as new ships; player
0, which has no mothership in Mission01 under `CAT_HW1_MISSION_SCRIPTS 0`, gets an sMothership 25,000 away (the AI aims
fleet orders, patrol paths and the sphere of influence at it); fleet control on as the missions' KAS call does. The
checks are one table (`catAiTestChecks`) over the AI's own structures; producer 3 adds rows (build, research, harvest).

| check | first tick | what held |
|---|---:|---|
| teams | 17 | attack 7, guard 2, recon 1 teams (teamsUsed 48) |
| attackorder | 17 | attack team 0's move list has a temp-guard move (6 moves) |
| defenceorder | 17 | guard team 0: active patrol |
| reconorder | 17 | recon team 0: active recon |
| attackmove | 33 | attack team 2 (11 ships) past its get-ships move, current move move-team |
| attackcommand | 33 | bMultiGunCorvette#286 has a move order |
| reconmove | 49 | recon team (1 ship) on its move-to |
| reconcommand | 49 | bCloakedFighter#240 has a move order |
| defencemove | 65 | guard team 0 (9 ships) on its patrol's move-team-index |
| defencecommand | 65 | bHeavyCorvette#251 has a move order |

`aitest: done tick=65 fleet=156 ... checks=10/10 result=PASS`. **N = 400**: the checks hold by tick 65 (setup 16, then one
AI update every 16 ticks per stage: orders 17, attack 33, recon 49, defence 65); 400 leaves six times that margin and runs
in about 15 s under ASan. The ships the AI asks to build are not delivered: building is the fleet manager's
(producer 3); the probes showed that without a large enough reserve the guard teams wait on their get-ships move, and
that without an enemy mothership the patrol path is empty.

## 6. gates (final tree, plain harness; binaries copied to original/scratch-m2/m2r7-orders/)

Builds: `tools/build.sh --debug` and `--release` rc 0, 0 warnings / errors under -Werror; the AI files, Stats.c,
AISelfTest.c, OracleDump.c and main.c compiled once more with `-Werror=implicit-function-declaration`: clean.
`tools/m2_layout32.sh`: OK (with the new AITeamMove / AIEvents asserts).

| run | result |
|---|---|
| `r7b-base-mission-a/-b` (`mission_test.sh --ticks 6000 --no-perturb-run`, base binary) | MISSION TEST PASS, `7c224543c9ad751a`, ASan 0; vs r6-integrate-mission-a IDENTICAL (build identity only) |
| `r7b-final-mission-a/-b` (final debug) | MISSION TEST PASS, byte-identical, **`7c224543c9ad751a`**, ASan 0; vs r6-integrate-mission-a IDENTICAL, 6,001 ticks |
| `r7b-final-mission-rel-a/-b` (release) | MISSION TEST PASS, `118e1ff1238338d6`, IDENTICAL to the debug stream (build flags only) |
| `r7b-final-ai-a`, `-b` | ai self-test done, result=PASS, ASan 0, no leak, `cmp` equal (`2f86c13e83803037`), hwo_compare IDENTICAL 401 ticks; UBSan: the known ETG / BTG kinds only |
| `r7b-final-replay-a` | oracle replay done ticks=9601 packets=4800, ASan 0, rc 0; IDENTICAL to `r7a-final-replay-a` (9601 ticks, 4800 packets) |
| `r7b-final-fe` | Main_game_screen, ASan 0, 0 UBSan reports |
| `r7b-final-sp` (340 s) | universe running, univUpdateCounter 3,264, ASan 0, no leak |
| `r7b-final-dock` | docktest done result=PASS (the R6 line), ASan 0 |
| `r7b-final-build` | buildtest done result=PASS, ASan 0; stream IDENTICAL to the base binary's `r7b-base-build` (7,001 ticks; it differs from R6's at tick 29 `ships_health` by producer 1's x87 rounding fix) |

Replay against the GOG recording (`docs/m2-r6-oracle-mode-a.md` arguments):

```
python3 -I tools/oracle/hwo_compare.py original/oracle/oracle01-rec-20261017-140428/oracle01-rec.hwo \
  <runs>/r7b-final-replay-a/run.hwo --from-tick 1 --to-tick 9600 \
  --fields rng10,rng6,ships_n,ships_misc,ships_pos,ships_vel,ships_health,missiles,bullets,derelicts,resources,univcheck,dt,total,real \
  --json <runs>/r7b-final-replay-a/compare.json
  -> FIRST DIVERGENT TICK 161 (rng6, excluded, decision 47)
  (the same without rng6, compare-sim.json) -> FIRST DIVERGENT TICK 177 (last equal 176), ships_vel; packet 46 blobcheck (stock)
```

Mission01 stream hash: before `7c224543c9ad751a`, after `7c224543c9ad751a`; no field moved (the task expected rng6 and
the computer player's ships; Mission01's computer player has no fleet control, see the headline).

## Delta-map review entries

90 entries `[review: M2-R7 ai-orders-moves]` in `ghidra/delta_map_review.json` (72 new with sample
`port-R7-ai-orders-moves`, 18 existing with the note appended), merged with `python3 -I tools/delta_map.py merge --done
m2r7:ai-orders-moves` (90 records changed, nothing else); `docs/m2-r1-function-deltas.md` regenerated. Totals: IDENTICAL
2,922 -> 2,874, MODIFIED 1,414 -> 1,462, reviewed 2,921 -> 2,993; stage 4 now 318 / 159 / 2. [audit: with the audit's two entries for the event fix functions: 92 entries, IDENTICAL 2,872, MODIFIED 1,464,
reviewed 2,995; stage 4 316 / 161 / 2] Not in the map:
`cat_statsRandomAttackShipStatic` 0x00524d70 (cat-new).

## CAT-TODOs

Closed: the 9 `CAT-TODO(stage 4 AI)` of AIOrders.c (7) and AIHandler.c (2); producer 1's `CAT-TODO(M2-R7 AI orders)` on
the swarm helpers and on `aiuShipsCloseToEnemyMothership` (code removed). Re-targeted: `aiuFindArmadaTarget`'s caller-
owned buffer -> `CAT-TODO(M2-R7 AI player)` (the ArmadaType MaxSelection at AIPlayer +0xCF8 belongs with the AIPlayer
layout and its save chunk; the behaviour already tests numShips as the exe). `CAT-TODO(stage 4 ...)` in src/: 20 -> 11,
all producer 3's (AIResourceMan 4, AIFleetMan 1, AIPlayer 1, utility.c 2, Dock.c, Salvage.c, UnivUpdate.c). CAT-FIX
added: aioCreateGuardShips (Kushan / Taiidan alternatives), aioCreateActiveSupport (empty team race),
aioCreateTakeoutMothershipBig (other races), the three numbers-low handlers (other races), aimCreateGetShipsNoAdd (leak),
aimProcessActiveCapture (leak).

## Files changed

src/Game: AIEvents.c / .h, AIHandler.c / .h, AIOrders.c / .h, AIMoves.c / .h, AIAttackMan.c, AIDefenseMan.c, AITeam.c /
.h, AIUtilities.c / .h, Stats.c / .h, OracleDump.c, meson.build, **AISelfTest.c / .h (new)**; src/SDL/main.c;
tools/m2_harness.sh; ghidra/delta_map_review.json, ghidra/delta_map.json, docs/m2-r1-function-deltas.md; this report.

## What is left (owners)

* **M2-R7 producer 3**: the build path (the AI's ship requests reach no finished ship in the ai test: RUs 30,000 ->
  27,600 and nothing delivered): add build / research / harvest rows to `catAiTestChecks`; the AIPlayer layout
  (ArmadaType) and AI save chunks; the 11 stage-4 tags; `airAddNewShip` / `aifAssignNewShips` still use HW1 aliases
  (Mothership, Carrier, ResourceCollector, AdvanceSupportFrigate, ProximitySensor); AITeam's `aitFindGoodCoopTeam` and
  layout (0x280).
* **Unknown in the exe**: the tactics default of the creators the exe does not contain (var inc, move team index / split,
  move to, capture, resource volume) is left at HW1's.
