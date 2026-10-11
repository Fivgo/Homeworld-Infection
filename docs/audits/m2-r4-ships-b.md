# Audit: M2-R4 port "ships-b" (the Cataclysm-only ships, the retired HW1 types)

Audited: docs/m2-r4-ships-b.md and the port's tree state (WIP commit 2f1a847 plus the working tree, on top of
d407410; bb1f318 / c0804d4 / e533a40 are the user's harness and workflow commits and were excluded). Method:
clean builds in a separate directory, the header tables compared with `headers.txt` and the exe's raw data,
25 bodies compared with the r6 decompilation, the full diff read for scope / CAT comments / IDENTICAL entries,
and the plain harness re-run with the audit binary. Nothing here was taken from the report without a check.

## 1. Builds (task item 1)

Clean directory `original/scratch-m2/build-audit-ships-b/` (gitignored scratch), `meson setup` exactly as
tools/build.sh: debug `--buildtype=debug -Db_sanitize=address,undefined`, release `-Doptimization=2 -Ddebug=false
-Db_sanitize=none`; `-Werror` is meson's `werror` default (meson.build line 8).

| configuration | rc | warnings | `hwi-engine` size |
|---|---|---|---|
| debug (ASan/UBSan) | 0 | 0 | 80,016,952 B (80,017,216 B after the audit fix below) |
| release (-O2) | 0 | 0 | 4,929,904 B |

`tools/m2_layout32.sh`: OK, 45 ship files, every ILP32 `sizeofShipSpecifics` assert holds.

## 2. Headers and bodies (task item 2)

**`shipHeaders` rows (Universe.c) vs headers.txt:** a script parsed the 145 rows and the 52 exe pointers: 143 rows
equal; `kCloakGenerator` / `tCloakGenerator` are `DefaultShipHeader` where the exe has NULL (the accepted default,
as the report says). Correct.

**The 22 new headers, every slot:** the script also parsed each `CustShipHeader` initialiser (designated fields
mapped to slots 1..34, casts stripped) and compared it with headers.txt; three headers (Acolyte 0x008c3260, Worker
0x008c6b50, Sentinel 0x008c6830) were additionally re-read as raw dwords from the exe with tools/m2_exedata.py and
named through r6 functions.csv, giving the same slot lists as headers.txt. Result: all slots of all 22 headers
match, with the typed stand-ins the port declares (`nullsubShip` / `nullsubStaticInit` / `nullsubAttackPassive`
for the exe's one `nullsub`, `kasfSpecialOffShip` / `Target` for `kasfSpecialOff`, `nullsubReturnFalseTarget`,
`ACVDockCommand` wrapping `ACVMoveCommand` for slot 22, `DefaultShipAttackPassive` whose body equals
`P1MothershipAttackPassive`), except two slots left NULL: Mimic slot 29 (`MimicSlot74Command`, declared in the
report) and **Worker slot 26** (`kasfSpecialOff` in the exe; declared only in Worker.c's comment, not in the
report: finding F1, doc-fixed). `CloakGeneratorHeader` exists in the tree with no exe counterpart (ships-a's, the
NULL default).

**25 bodies vs r6** (tools/m2_fdec.py; constants read with tools/m2_exedata.py; offsets checked against the
`CAT_ASSERT_OFFSET` lines of SpaceObj.h and the `_Static_assert`s of the ship headers):

| body | exe | result |
|---|---|---|
| AcolyteStaticInit | 0x005ad3d0 | match (flags 0x211/0x212/0x213 = 0/1/0 after GenericInterceptorStaticInit) |
| MimicStaticInit, MCVStaticInit, LeechStaticInit | 0x005b8330, 0x005b80f0, 0x005b7ad0 | match (flag triples 1/1/1, 1/1/1, 1/1/0; the MCV side-step keys) |
| CrystalStaticInit, CrystalDied | 0x005affd0, 0x005b0090 | match (per-type statics, `resourceValue` key, the stripped assert; blast type 3 unless `noBlast` at Ship+0x63C, killer at +0x60C) |
| SwarmerStaticInit, RepairDroidStaticInit, SentinelStaticInit | 0x005c7930, 0x005c29b0, 0x005c5d10 | match (race-selected side-step blocks, the Sentinel's 0x211) |
| MultiBeamFrigateStaticInit | 0x005bd730 | match (MultiBeamRangeRatio at +0, side-step keys at +4) |
| RKOStaticInit, dBeastMothershipStaticInit | 0x005c3df0, 0x005b00d0 | match |
| ModuleShipStaticInit | 0x005af5e0 | match: all 19 type cases and their delays (0x3f800000 .. 0x40a80000 = 1.0 .. 5.25) at the tree's ids |
| ModuleShipInit | 0x005af720 | match: the Beast stun (0x0072a4c4 = 1.0, 0x0072a5b0 = 2.5 read from the exe), the sMothershipCommand animation; spin flag at +0x20 asserted |
| CruiseMissileAttack | 0x005aff20 | match (0x0072a618 = 5000.0; kamikaze bits 0x6) |
| SlipgateStaticInit | 0x005c71c0 | match (types 0x70/0x71/0x72/0x88, default = statinfo itself) |
| HiveFrigateStaticInit | 0x005b54b0 | match (15 s, 7000, gunRange = bulletRange x 0.8 (0x0072a5d4), tooClose = minBulletRange x 0.7 (0x0072aaf4 = 0x3f333333); bulletRange 0x1A4 / minBulletRange 0x1B0 asserted) |
| ACVStaticInit | 0x005ae360 | match (EMP_Max_Combine_Distance 0x438 / EMP_Range 0x434 asserted; 2400.0; side-step at +0x10 asserted) |
| RammingFrigateStaticInit | 0x005bfad0 | match (the five statics at 0x418 .. 0x428, all asserted) |
| DreadnoughtStaticInit | 0x005b0e50 | match (gunRange = bulletRange, tooClose = minBulletRange x 0.9 (0x0072a5b4)) |
| WorkerStaticInit | 0x005c9580 | match (sWorker / bWorker blocks else dbgFatalf, 6.0, Grinder / gun0 points, the gun orientation from the first object, flags, the salvage and repair-point inits) |
| NomadMoonInit | 0x005bdbb0 | match (0x008c56d8 = 3925.0; two dNomadFieldEmitter (0x84) with the moon as creator) |
| kasfSwarmMoveTo | 0x00492610 | match (one selection, the pod branches dead) |
| singlePlayerNisStoppedCB | 0x0051b430 | match for the retire delta (race 0 -> type 0 sRecon, race 1 -> 0x21 bRecon); the rest of the body is stage 6, as the review note says |
| soundEventBurstStop | 0x00521710 | **mismatch** (finding F2): the exe also stores 0 to Ship+0x48C = `soundevent.gun` (SOUNDEVENT at +0x448, `gun` at +0x44) after the stop; neither HW1 nor the port did. Fixed in code |

**Retirement:** ShipDefs.h no longer has `HW1LEGACY_FIRST_SHIP` .. `P3Megaship` (ids 200-221); `Ships/DefenseFighter.c`
and `.h` do not exist; `RepairCorvette.h` declares only `refuelRepairShips` / `stopRepairEffect`; a whole-word grep
over src/Game, src/Ships, src/SDL, src/Missions for the 29 dropped identifiers and `RepairCorvetteHeader` /
`DefenseFighterHeader` finds no code reader (only comments, the `str*` string ids and the `RepairCorvette.h`
includes). `src/PROVENANCE.md` still lists the deleted files as imported in M2-R1, which is history, not a reader.

**Harness (audit binary, plain harness, silent):**
- `r4sb-audit-statics` (statics + `/catShipStaticsStaticInit`): rc 0, ASan 0, no unsuppressed leak, 143 of 145
  types (the two disabled cloak-generator .shp not found), 27 other .shp = 170 of 173, 3,015 file loads,
  **108 `staticinit=next` / 35 `staticinit=none`**, as the report.
- `r4sb-audit-sp` (sp, 340 s): milestone "universe running", `univUpdateCounter` 1,248 at the timeout (rc 124),
  ASan 0, no unsuppressed leak, 3,721 file loads, 33 ship statics, as the report (its 3,723 / 3,725 loads vary by
  run).
- After the F2 fix (rebuilt debug and release, 0 warnings): `r4sb-audit-fixed-statics` rc 0, ASan 0, 143 of 145,
  108 StaticInits, 3,015 file loads; `r4sb-audit-fixed-sp` (340 s): universe running, `univUpdateCounter` 1,248, ASan 0,
  no unsuppressed leak, 3,723 file loads.

## 3. The diff (task item 3)

`git diff d407410` over src/: 165 tracked files of the port (47 added, 28 deleted, 87 modified in src/, plus the
report, the delta map and tools/m2_layout32.sh), +14,153 / -8,166 lines; 589 added src lines carry `CAT`. The
report's "169 files, +14,036 / -8,147" counts the four user files of c0804d4 / e533a40 and predates the last
edits; not a defect.

- **Scope.** Beyond the Ships files and the Game/SDL files the report names, the WIP commit edited the eleven
  stage-4 AI files, `Dock.c` (-2,139 lines), `CommandLayer.c`, `ConsMgr.c`'s tables, `InfoOverlay.c`,
  `Bounties.c`, `MadLinkIn.c`, `Sensors.c`, `ResearchGUI.c`, `mainrgn.c`. Every one of these hunks was read: each
  removes code keyed on a dropped type (whole functions such as `aiaProcessSwarm`, `aifP2FleetCommand`,
  `FindAnotherResearchShiptoDockWith`, `R1/R2ResearchShipDocksAtResearchShip`, `ShipDocksAtRepairCorvette`,
  `ShipDocksAtResController`, `ShipDocksAtFuelPod`, `DroneDocksAtDDDF`, the P2 mothership dock init; table rows;
  single tests), so the edits are forced by the deletion the scope asked for, and the AI's substituted ship
  requests carry `CAT-TODO(stage 4 AI)`. The report's section 5 did not mention them (finding F3, doc-fixed).
- **CAT comments.** 120 hunks in pre-existing files have no `CAT` text. Those in the resumed run's 22 files are
  includes, blank context or lines whose comment sits on the neighbouring line; the WIP's whole-function and
  table-row deletions (Dock.c, the AI files, ConsMgr.c, InfoOverlay.c, CommandLayer.c `selectionHasSwarmers`) have
  none at all, against the convention (finding F4, recorded in the report; not back-filled).
- **IDENTICAL functions edited without a review entry.** Each hunk's enclosing function was intersected with the
  delta map: 23 hits, 3 of them artefacts of a deleted neighbour (`aiaAttackManager`, `aifFindAllies`,
  `clSetResearch`); the other 20 are real edits of functions still classed IDENTICAL (finding F5, fixed in the
  delta map: 20 review notes, class MODIFIED, counts recomputed to IDENTICAL 3,066 / MODIFIED 1,277 / reviewed
  1,532). The r6 bodies were read for the type tests: e.g. `aiaPriorityShipsConstraints` 0x004011d0 excludes
  0x10 sProcessor, 0x2d, 0x33 bProcessor, 0x3c, 0x4a, 0x4e (both race variants and the Processors);
  `isCapitalShipStaticOrBig` 0x00481130 tests statinfo+0x2a8 then 0x3f/0x4d/0x40/0x4f; `formGroups` tests
  0x43/0x51; ten of the twenty have no ship-type test left at all.
- The producer's own 5 reclassifications and 24 review notes were checked against the bodies above where they
  belong to this audit's sample (kasfSwarmMoveTo, soundEventBurstStop, singlePlayerNisStoppedCB): consistent,
  except the missed store in F2.

## 4. DONE WHEN (task item 4)

Re-checked on the plain harness with the audit binary (section 2): every Mission01 ship type and all 143 loadable
types use the exe's header row (the two cloak generators excepted by decision); the StaticInits ran under ASan
(108) with 0 errors; no HW1-only ship type or file is left; statics 143/143 as the probe and 170/173 .shp; sp
reaches the running universe, 1,248 ticks, ASan 0. **Met.**

## Findings

| # | finding | evidence | fix |
|---|---|---|---|
| F1 | `WorkerHeader` slot 26 is NULL where the exe has `kasfSpecialOff`; the report claims every header is the exe's and names only the Mimic slot 29 exception | hdrcheck script, exe dwords at 0x008c6b50+0x68; Worker.c:999 comment | doc: `[audit: ...]` under DONE WHEN |
| F2 | `soundEventBurstStop` misses the exe's `soundevent.gun = NULL` (Ship+0x48C), though the review entry says "whole body read, ported" | r6 0x00521710 line `*(ship + 0x48c) = 0`; SOUNDEVENT layout (SoundStructs.h) at Ship+0x448 | code: the store added with a CAT comment; rebuilt (0 warnings), statics and sp re-run |
| F3 | Section 5 lists only the resumed run's files; the WIP's edits to the AI files, Dock.c, CommandLayer.c, ConsMgr.c, InfoOverlay.c and six more are undocumented | `git show --stat 2f1a847` | doc: paragraph added to section 5 |
| F4 | About 100 whole-function / table-row deletions of the retire step carry no CAT comment | diffcheck script (120 hunks without `CAT`) | doc: recorded in the same paragraph |
| F5 | 20 functions edited by the retire step were left IDENTICAL in the delta map with no review entry | diffcheck script; r6 bodies | delta map: 20 entries reclassified MODIFIED with review notes, counts recomputed; doc: section 6 note |

## Precision

Sampled checks: 63. Correct: 58 (2 builds, the 145 rows, 21 of the 22 headers, 24 of the 25 bodies, the 3
retirement checks, statics / StaticInit counts, sp, layout32, the Mission01 headers, the two typed-stand-in
equivalences). Wrong: 5 (F1 .. F5). Precision 58 / 63 = **0.921**.

Fixes made in place: 1 code (F2), 1 delta map (F5, 20 entries), 3 report edits (F1, F3+F4, F5).

Post-fix runs: statics rc 0 / 143 of 145 / 108 StaticInits / ASan 0; sp 1,248 ticks, ASan 0 (section 2).

Verdict: PASS-WITH-FIXES - 58/63 checks hold (all 22 headers slot for slot, 24/25 bodies, retirement complete, statics 143/145 with 108 StaticInits, sp 1,248 ticks ASan 0); fixed the missed soundevent.gun store, 20 unreviewed IDENTICAL edits in the delta map, and the report's silence on the WIP's AI/Dock/CommandLayer retire edits and Worker slot 26.
