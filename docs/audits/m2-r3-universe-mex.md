# Audit: M2-R3 port "universe-mex" (universe statics, MEX, DockStaticInfo, Globals; level load to "universe running")

Auditor: adversarial audit, 2026-10-10.

- **Subject:** producer report `docs/m2-r3-universe-mex.md` and the uncommitted diff in the `m2` worktree (18 tracked
  files, +1,104 / -320 before the audit: `Universe.c/.h`, `MEX.c/.h`, `SpaceObj.h`, `StatScript.c/.h`, `Dock.c/.h`,
  `Salvage.c`, `Globals.c/.h`, `MeshAnim.c/.h`, `LevelLoad.c`, `SinglePlayer.c/.h`, `ghidra/delta_map.json`).
- **Not run:** Ghidra and the GOG game. No input was injected.
- **Evidence:** the r5 export (`ghidra/exports/cataclysm_gog_r5/decompiled.c`, `decompiled_index.csv`,
  `call_graph.csv`), `original/game/cataclysm.exe` (constants, strings, `scriptStructEntry` tables and the 60-entry
  table read through its PE section table; `objdump -d` for two short bodies), `original/game_extracted/cataclysm`
  (all 967 `.mex`, the `.shp` files), the tree's headers, the plain harness.
- **Audit helpers** (untracked, `/tmp/um-audit/`): `fn2.py` (r5 body by name, a copy of the producer's, read first),
  `rd.py` (exe floats/dwords/strings and 16-byte script tables via the PE section table), `mexcheck.py` (walks every
  chunk of every `.mex` and checks the fields the port reads), `l32/t.c` (an extra `clang -m32 -fsyntax-only` probe).

## 1. Clean builds (separate dir, as `tools/build.sh` does)

`original/scratch-m2/build-audit-universe-mex/` was removed and recreated, then
`meson setup --buildtype=debug -Db_sanitize=address,undefined debug` / `meson compile` and
`meson setup -Doptimization=2 -Ddebug=false -Db_sanitize=none release` / `meson compile` (`werror=true` comes from
`meson.build`).

| build | rc | compiler `warning` lines | size |
|---|---|---|---|
| debug (audit dir, producer's tree) | 0 | 0 | 76,947,728 B |
| release (audit dir, producer's tree) | 0 | 0 | 4,823,328 B |
| debug (audit dir, after the audit fixes) | 0 | 0 | 76,947,744 B |
| release (audit dir, after the audit fixes) | 0 | 0 | 4,823,328 B |

After the fixes `tools/build.sh --debug` and `--release` also rc 0 with 0 compiler warnings (the only "WARNING" line
is meson's configure note that sanitizers are on); `build/debug/hwi-engine` 76,937,768 B, `build/release/hwi-engine`
4,819,232 B. `tools/m2_layout32.sh`: OK. An extra ILP32 probe (`l32/t.c`) confirms the new pins are evaluated:
`sizeof(DockStaticInfo)` = 0x9C (0x50 header + one 0x4C point), `sizeof(NAVLightStaticInfo)` = 0x4C (4 + one 0x48
static), and a deliberately wrong `sizeof(madstatic) == 0x91` fails while the tree's `== 0x90` holds.

## 2. Checks against the exe (r5 decompile, exe bytes, install data)

40 checks. "OK" = the port matches the evidence; "WRONG" = a factual error in code, comment or report.

| # | check | evidence | result |
|---|---|---|---|
| 1 | `mexSetCollisionInfo`: sphere scale = max of the three box scales | 0x004b7880 (the two-level max to `local_4`) | OK |
| 2 | sphere: original/coll/approx = s*r, avoid = s*r + 3.0, avoidpad untouched, sqr | same; StaticCollInfo offsets 0/4/8/0x14/0x18/0x1c; 3.0 at 0x0072a534 | OK |
| 3 | rect: offset as stored (no slides), length scaling moves the offset by -(new-old)*0.5, 1.1 radius growth | same; 0.5 at 0x0072a4c8, double 1.1 at 0x0072af60 | OK |
| 4 | diagonal `sqrt(up^2 + (right^2 + forward^2))` in both branches | label 0x004b7ab1 | OK |
| 5 | `collrectmid` = (offset + length) * 0.5 per axis, both branches (+0x38/+0x3c/+0x40) | same | OK |
| 6 | `mexGetDockChunk`: Dok/Dock or Sal/SALVAGE, name at +0x30 | 0x004b7190; strings 0x00885de4 "Dok", 0x00885ddc "Dock", 0x00885dd8 "Sal", 0x00885dd0 "SALVAGE" | OK |
| 7 | `mexGetDockStaticInfo` fallback: pos 0, normal (1,0,0), cone `cos(1.5707963705062866)` | 0x004b7560; double at 0x0072af58 = 1.5707963705062866 | OK |
| 8 | `mexSetEngineNozzle`: offset at +0x2B0 + 0xC*index, object byte at +0x2E0 + 4*level + index, -1 if none; "Eng<i>" | 0x004b7c20; 0x008791ec "Eng", 0x008791f0 "Eng0"; LOD level stride 0x38, bindings stride 0x18 | OK |
| 9 | `mexGetNAVLightStaticInfo` takes the static header (mex at +4, LOD at +0), stride 0x48, bytes at +0x40 + level | 0x004b7680; callers in `InitStatShipInfo` (line 431 of its body passes `statinfo`) | OK |
| 10 | NAV colour packing unchanged (chunk +0x1c red, +0x1d blue, +0x1e green) vs `colRGB` | 0x004b7680 `((g|0xffffff00)<<8|b)<<8|r` = `colRGB(red,green,blue)` | OK |
| 11 | `MEXEngineChunk.objectName` +0x30 and `MEXNAVLightChunk.objectName` +0x34 lie inside every chunk, NUL-terminated | `mexcheck.py` over all 967 `.mex`: 463 Eng (all data 0x40), 2,951 Nav (all 0x44), 0 problems | OK |
| 12 | report counts: 85 of 463 Eng and 5 of 2,951 Nav chunks name an object (sleech "LeechHead") | `mexcheck.py` | OK |
| 13 | `cat_dockFindDockStaticPoint`: case-insensitive, returns the point or NULL | 0x00463d00 (count at +0x48, points at +0x50, name at +0x38) | OK |
| 14 | `DockStaticInfo` header: firstOfType +0, numOfType +0x23, n +0x48, center +0x4C, points +0x50 (35 types) | `scriptSetDockStatics` 0x005270c0 tail (0x23-byte reset loop); `ObjTypes.h` has 35 `DPT_*` | OK |
| 15 | `scriptSetDockStatics` index: 0xFF/0 reset, skip type -1, only when a point was read | same (`local_2f4 != 0`) | OK |
| 16 | `InitStatShipInfo` stores the "center" point after `mexGetDockStaticInfo` | body lines 416-417 | OK |
| 17 | guidance thrust orders: precalc `mod*thrust*mass`, ships `mass*thrust*mod`, missiles/mines `thrust*mass*mod` | `precalcStaticInfoGuidance` 0x0053ad70, `InitStatShipInfo`, `InitStatMissileInfo`, `InitStatMineInfo`, `InitStatDerelictInfoByPath` | OK |
| 18 | `MadMaxMadInfoLoad` rows 5-6 `NUMBER_OF_ANIMS` / `ANIM` with the right callbacks | exe table 0x008b2f88 (7 rows; 0x539e40, 0x539f70) | OK |
| 19 | `cat_madNumberOfAnimsSetCB` / `setMadAnim`: 0x90-byte madstatic, +0x84/+0x88/+0x8c, "%i", "%i %s", "%s%s%s" | 0x00539e40, 0x00539f70; strings 0x00887a38, 0x008b5764, 0x007515fc | OK |
| 20 | `madAnimIndexFindByName` returns -1, no fatal | 0x004b6ed0 | OK (header comment stale: finding 3) |
| 21 | 18 `.shp` use NUMBER_OF_ANIMS; none also uses BindAnimations or a Mad* key | grep of the install | OK |
| 22 | `cat_scriptAsteroidPieceParse`: RotX(4th) * RotY(5th) * RotY(5th), 6th unused, params at +0x34 | `objdump` 0x005280a0..0x0052818f: the sscanf outputs are [S+0x3c], [S+0x38], [S+8]; both `matMakeRotAboutY` (0x4ad470) calls load [S+0x38] | OK |
| 23 | `cat_scriptAsteroidBreakInfoLoad`: case-sensitive key, n < 1 stops, n * 0x44 zeroed, first n lines | 0x00528190 | OK |
| 24 | asteroid key table has no shrink/break keys | exe table 0x008b3008 (24 rows, last `preciseSelection` with the ShipStaticInfos base) | OK |
| 25 | mine key `N-LIPS` at missile base + 0x110; values 0.0007 x4, 0.0001 derelict | exe table 0x008b3b88 row 37 (0xba1050 - 0xba0f40); the 5 `mine.shp` | OK |
| 26 | mine `collBoxScale*` rows keep the asteroid base as the exe | exe table 0x008b3b88 (base 0xba1a00) | OK |
| 27 | `universeStaticInit`: 7 mines, derelict mine always loaded, no nebula close, abort skips the doubling | 0x0053d2a0 (loop 0x00b3ac10..0x00b3b444 stride 300, test 0x00b3b318) | OK |
| 28 | health loop: post-load, `maxhealthBase` +0x44C, Sect-only difficulty factor; 0x00894194 = 1.0 | 0x0053d2a0 tail, `cat_univSetShipMaxHealth` 0x0053c4a0 (race at +0x1E4 == 0) | OK |
| 29 | `universeDefaultTeamColors`: all 21 sets decoded from the [7][8] stack table | 0x0053c940 (rows at -0x38 + 8r) | **WRONG** (finding 1) |
| 30 | team-colour copy: ships, missiles and mines of all 7 races, 8 derelicts take row 6 | same (0x00ba0710..0x00ba1030 stride 0x124 = 8) | OK |
| 31 | save/load: 7 mines; `universeNoStaticLoadedOrNeeded` clears mines 0..3 | 0x0053ce80, 0x0053d090, 0x0053dd90 | OK |
| 32 | `universeClosePlayers`: rm/cm/lm close for the current player; upgrade reset left as CAT-TODO | 0x0053def0 | OK |
| 33 | `universeUpdateTask`: repeattimes 9 / 1, NO_PACKET flag on playPackets (0x00a7518c), 3 `univUpdate` sites | 0x0053e460 (decompile lines 109, 170, 175); `globalsInit` order recordPackets 0x00a75188, playPackets 0x00a7518c | OK |
| 34 | `unitCapCreateShip` + `cat_univMothershipModuleTypesGet` lists and counts | 0x0053eab0, 0x0053f680; `objdump` stores at 0x0053f81c | **WRONG** (finding 2) |
| 35 | `cat_univBeastMothershipModuleTypesGet`: none in missions 1-3, else bDockingBay/bEnginesLower | 0x0053f840 | OK |
| 36 | `gameStatsShipDied` / `gameStatsAddShip` (1000 returns, killer < 8 signed, swarmers skipped, ACV/MCV/bACV twice, no totalShipsInGame) | 0x0053f290, 0x0053f220 | OK |
| 37 | `globalsInit`: universeTurbo 0x00a6c5b0 reset, gMessage not cleared, bMessage stride 0x104 x 2 | 0x0048a060 | OK |
| 38 | `GetAppropriateShipTypeForRace`: the 60-entry table | exe 0x00883cf8 read and compared entry by entry with `catAppropriateShipTypeSwap`: 60/60 equal | OK |
| 39 | `scriptSetShipsCB`: override only when `!singlePlayerGame`; mothership types 0x12, 0x35, 0x3E, 0x4C, 0x62 | 0x004a13f0 (body lines 154, 266) | OK (rest of the function: finding 4) |
| 40 | `cat_univFindNearestShip`, `unitCapCanCreateShip` gating (0x200 or single player) | 0x0053f590, 0x0053ece0 | OK |

**Precision:** 37 correct of 40 sampled checks (findings 1-3 are wrong; finding 4 is an unrecorded remainder, not a
wrong claim) = **0.925**.

## 3. Findings

1. **`universeDefaultTeamColors` set the four "missions 8..17" entries for any mission >= 8** (code
   `if (currentMission >= 8)`). The exe's switch has no case above 17 (its default sets nothing beyond Sect[0], Sect[1],
   Beast[1]), and the producer's own comment says "a mission number outside 1..17 adds nothing". Unreachable with
   the 17 shipped missions, but the code contradicted both the exe and its comment. **Fixed:** `>= 8 && <= 17`.
   Every other entry of the 21 sets decodes as the port has it (tutorial, multiplayer, missions 1-17; e.g. mission 7:
   `CONCAT13(1,..)` = Derelict[3] and `CONCAT11(1,..)` = Derelict[1]; mission 15: `local_4` byte 1 = Derelict[5]).
2. **Undocumented divergence in `cat_univMothershipModuleTypesGet` (DemoSect2).** The exe writes 11 types and stores
   the count 12 (`mov dword [eax+0x50],0xc` at 0x0053f81c; entries +0x00..+0x28 only), so `unitCapCreateShip` (and
   `cat_spDeleteLeftBehindShips`, `cat_univCreateMothershipModules`, from `call_graph.csv`) consume a never-written
   12th entry of an uninitialised stack array. The port counts 11 without saying so. **Fixed:** a `CAT-FIX` comment
   at the list; report annotated.
3. **Stale comment:** `madAnimIndexFindByName`'s header still said it "generates an error if not found" after the port
   made it return -1. **Fixed.**
4. **`scriptSetShipsCB` is partly ported and the remainder was not recorded** (delta-map row MODIFIED, `reviewed`
   false, no note). The exe body has no CryoTray player swap and no multiplayer `MG_HarvestinEnabled` /
   `MG_ResearchEnabled` / `MG_CarrierIsMothership` gates (no reference to `tpGameCreated.flag` 0x008860ec), while the
   tree keeps HW1's; in multiplayer they skip `tResourceCollector` (`ResourceCollector` aliases it) and turn
   `sMothership` into `kCarrier`. Off the DONE WHEN path (single player), and the producer did not claim more. **Fixed
   (records):** delta-map review notes on `scriptSetShipsCB` and `GetAppropriateShipTypeForRace`, item 11 in the
   report's "What is left". Owner: M2-R4 (level load / multiplayer).
5. **Noted, not a defect:** the `CAT_HW1_MISSION_SCRIPTS 0` switch in `SinglePlayer.c/.h` is a provisional change
   outside the row-5 files. It is documented (CAT-PROVISIONAL, stage 6, deletion planned) and the alternative stops
   it removes were measured (`KAS.c (265)` and `File.c (1120)` in the report's runs). The integrate step should know
   that the DONE WHEN holds with HW1 mission scripts and the mission-start NIS switched off.

## 4. Diff review (step 3)

- **Scope:** every changed file is in the producer's `files_changed`. Outside row 5 of the plan: `LevelLoad.c`,
  `SinglePlayer.c/.h` (the `levelload` subsystem the DONE WHEN needs), `MeshAnim.c` (the `ANIM` key needs -1),
  `Salvage.c` (now calls the Dock.c function), all documented in the report.
- **IDENTICAL functions edited without a review entry:** none. The 8 IDENTICAL rows the port edits or re-reads
  (`mexGetNAVLightStaticInfo`, `unitCapDeleteShip`, `gameStatsAddShip`, `gameStatsShipDied`,
  `universeSaveEverythingNeeded`, `universeLoadEverythingNeeded`, `universeNoStaticLoadedOrNeeded`, `initGameStats`)
  were reclassified MODIFIED with notes; I confirmed each against r5 (rows 9, 31, 36 above). `globalsClose` stays
  IDENTICAL with a note. 43 rows changed, none added or removed (compared with `HEAD:ghidra/delta_map.json`).
- **CAT comments:** every Cataclysm change in the diff carries a `CAT:` / `CAT-FIX:` / `CAT-TODO` / `CAT-PROVISIONAL`
  comment with the exe function and address (read hunk by hunk); the `%x` -> `%p` in two `dbgMessagef` calls is a
  64-bit format fix, not a Cataclysm change.
- **HW1 aliases:** none left in `Universe.c` code (the remaining mentions are in comments). `LevelLoad.c` still has
  HW1 aliases in `scriptSetShipsCB` (finding 4).

## 5. Harness runs (plain harness, audit debug build, offscreen, no input)

| run | mode | rc / wall | milestone | first stop | ASan / leaks |
|---|---|---|---|---|---|
| `uma-audit-sp` (producer's tree) | sp, `--timeout 340` | 124 / 340 s | **universe running (univUpdateCounter > 0)**, `univUpdateCounter: 1248` (as the producer's `um-final-sp`) | none: running at the timeout | 0 / no unsuppressed leak; 3,715 file loads, 46 `.mex` |
| `uma-audit-statics-init` | statics + `/catShipStaticsStaticInit` | 0 / 10.6 s | ship statics self-test done: 143 loaded, 2 not found, 27 other `.shp` | none | 0 / none; 162 `.mex` loads; 2 `setMadAnim` warnings |
| `uma-audit-oracle` | oracle | 0 / 8.9 s | oracle self-test done | none | 0 / none |
| `uma-audit-fe` | fe | 124 / 60 s | `Main_game_screen` | none | 0 / none; 232 file loads |
| `uma-audit-sp-postfix` (after the audit fixes) | sp, `--timeout 340` | 124 / 340 s | universe running, `univUpdateCounter: 1248` | none: running at the timeout | 0 / no unsuppressed leak; 3,715 file loads |

The sp start plays `Movies/Animatic01.bik` in real time first, so the 340 s timeout is needed (as the report says).
`.mex` coverage: the harness runs load 162 (statics) and 46 (sp) `.mex` under ASan; all 967 `.mex` of the install
pass the structural check of the fields the port reads (`mexcheck.py`, check 11).

**DONE WHEN** (Mission01 level load completes and sp reaches "universe running" with ASan 0 on the plain harness):
**confirmed** on `uma-audit-sp`, with the CAT-PROVISIONAL mission-script switch noted in finding 5.

## 6. Fixes made by the audit (8)

1. `src/Game/Universe.c` `universeDefaultTeamColors`: the shared mission 8..17 entries only for 8..17 (finding 1).
2. `src/Game/Universe.c` `cat_univMothershipModuleTypesGet`: `CAT-FIX` comment for DemoSect2's 12/11 count (finding 2).
3. `src/Game/MeshAnim.c`: `madAnimIndexFindByName` header comment (finding 3).
4. `ghidra/delta_map.json` `scriptSetShipsCB`: review note on the partial port and the open deltas (finding 4).
5. `ghidra/delta_map.json` `GetAppropriateShipTypeForRace`: review note recording the port.
6. - 8. `docs/m2-r2-oracle.md`: the three "4 `univUpdate` call sites" statements annotated (3 since this port).

The producer report `docs/m2-r3-universe-mex.md` is annotated `[audit: ...]` for findings 1, 2 and 4 and for the
oracle-doc item.

Verdict: PASS-WITH-FIXES - 37/40 checks match r5 and the exe (MEX collision/dock/nozzle/NAV parsing, DockStaticInfo 0x50 header, 7-race mine statics, team colours, 8 derelicts, 60/60 swap table), all 967 .mex parse cleanly, sp reaches universe running (1,248 ticks) with ASan 0 on the plain harness; fixed the mission > 17 team-colour case, documented DemoSect2's 12/11 count, one stale comment, two delta-map notes and three oracle-doc counts
