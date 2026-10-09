# Cataclysm mission code map (KAS)

Recovered 2026-10-08 from `original/game/cataclysm.exe` (GOG build 12948, retail 1.01; image base 0x400000), working only from
the Ghidra exports in `ghidra/exports/cataclysm_gog/` plus a read-only capstone disassembly of the exe. Machine-readable names
for the runtime and the kasf library are in `ghidra/proposals/missions.json` (538 entries). The 2,085 generated mission functions
are **not** in that file because Ghidra has not created functions in the mission range yet (see section 1); their kas2c names are in
sections 7 and 8 here.

HW1 references are to `reference/HomeworldSDL` (`src/Game/KAS.c`, `src/Game/KASFunc.c`, `tools/kas2c/kas2c.c`, `parser.y`).
Background: `docs/hw1-engine-gameplay.md` section 14 (mission system) and `docs/big-archive-contents.md` section 5 (mission data).

## 1. Summary

| What | Where | Count |
|---|---|---:|
| KAS runtime (`KAS.c` and Cataclysm helpers) | `0x004901b0`-`0x00491c60` | 33 HW1 functions (30 `KAS.c`, 3 `HS.c`) + 10 Cataclysm-only helpers; 11 `Timer.c` callees at `0x0052bfe0`-`0x0052c860` |
| kasf library (`KASFunc.c` plus Cataclysm additions) | `0x00491c70`-`0x0049ad80` | 475 addresses named (291 HW1 + 184 Cataclysm-only); the table has 485 distinct C names (299 HW1, 186 Cataclysm-only) |
| ICF-folded empty stubs used as kasf | `0x0049ae30` (`ret`), `0x006fe9a0` (`xor eax,eax; ret`) | 9 kasf names |
| kas2c function table, embedded in `.data` | `0x00755048`-`0x00878137`, 487 entries of 0x990 bytes (1.19 MB) | 486 + `"*"` sentinel |
| Generated mission code (17 missions + Tutorial1) | `0x005d6fe0`-`0x00670a10` (629296 bytes) | 2085 functions |
| FSM definitions / state definitions | (per mission, section 8) | 322 / 1256 |
| `kasJump` / `kasFSMCreate` call sites in mission code | | 2507 / 898 |

Key facts:

- **Ghidra has no functions in `0x5d6fe0`-`0x670a10`.** `functions.csv` jumps from `0x005d6fc0` to `0x00670a10`; the range only has
  837 auto labels (switch tables). The mission functions are reached only through immediates pushed to `kasMissionStart`,
  `kasFSMCreate` and `kasJump`, so auto-analysis never made them functions. A function-creation pass over the address list in
  section 8 is the next Ghidra step. A recursive-descent walk from the 18 `kasMissionStart` roots covers the range completely:
  2085 functions, no gaps that are not `nop`/`int3` padding, and no overlaps.
- **Only the 17 missions and Tutorial1 have KAS code.** `singlePlayerGameUpdate` (`0x005197d0`) calls `kasMissionStart` for
  `"mission01"`..`"mission17"` and `tutInitTutorial` (`0x0052f690`) calls it for `"tutorial1"`. There are no other call sites, so the 11
  `MissionNN_NISmm` staging levels run no KAS script. Their numbering comes from the `.mission` folders (big-archive-contents section 5.1).
- **Cataclysm compiled every KAS name to a hash.** FSM, state, team, timer, variable and message names are 28-bit hashes
  (`cat_kasHashString`, `0x00491860`), not strings. Names below are recovered by hashing candidate words (section 6).
- **The exe carries its own kas2c table.** `cat_kasFunctionTable` (`0x00755048`) holds the KAS name, return flag, C name and typed
  parameters of all 486 kasf functions. A dev-console `KAS` command uses it (`cat_consoleKasCommand`, `0x0045f270`), and
  `cat_kasCallFunctionByName` (`0x0045b9a0`) dispatches on the name hash. Hashing every table name and matching it to the 486 dispatcher
  branches gives the address of every kasf with no guessing.

## 2. How it was found (reproducible)

1. **Table.** Parse the `FunctionCall` records at `0x755048` with a stride of 0x990: name[129] at +0, returnsNumber at +132, realName[129] at +136, numParams at +268,
   then params of 136 bytes from +272 (name[129], type at +132). Types use the HW1 kas2c `ParamType` order; Cataclysm passes names as
   `UDWORD` hashes where HW1 passed `CHARPTR`.
2. **Dispatcher.** Every branch of `0x45b9a0` has the shape `if (hash == K) return kasfX(params...)`. Each K equals `cat_kasHashString(table name)`.
   All 486 table entries matched a branch, with no extra branches. Two KAS names, `ShipsSetColor` and `ShipsSetColour`, share one realName.
3. **Check against HW1.** HW1 kasf functions sit in `KASFunc.c` source order: 290 of 291 HW1-named addresses are monotonic in
   source line. The one exception is `kasfTutShowBackButton`, in the tutorial-button group where Cataclysm added its own Prev/Quit button
   functions. Of the 184 Cataclysm-only kasf, 63 are inserted inside the HW1 run next to related HW1 functions, and 121 form a trailing block `0x497d30`-`0x49ad80`.
4. **Runtime.** The `KAS.c` functions were matched by decompiled body, by call order (`kasSave`/`kasLoad` start with
   `SaveHyperspaceGates`/`LoadHyperspaceGates` and end with the timer save/load, as in HW1 `KAS.c:1215,1274,1282,1382`) and by callers in
   SinglePlayer/AITeam/AIPlayer code. See the evidence field of each entry in `missions.json`.
5. **Mission functions.** Recursive descent from the 18 `(Init, Watch)` pairs passed to `kasMissionStart`. Every pushed immediate in range
   whose target starts with the kas2c scope prologue (`mov [0x00c3749c], scope`) was accepted as a function. Jump tables were followed
   through MSVC `movzx`+`jmp [table+4*idx]` pairs. 13 pushed immediates in range were rejected because they are hash constants that happen
   to be valid addresses (for example `0x0062fa97` = hash of `Flanking`).
6. **Hierarchy.** `kasFSMCreate(fsmHash, Init, Watch, kasAITeamPtr(teamHash))` and `kasJump(stateHash, Init, Watch)` call sites give
   mission, then FSM, then state. All 2,085 functions got a context. The scope constant in each prologue (0 = mission, 1 = FSM, 2 = state) matches
   its context in every case (scope counts: 36 / 505 / 1544).

## 3. KAS runtime

| Address | Name | HW1 equivalent | Notes |
|---|---|---|---|
| `0x00417e10` | `aitMsgSend` | AITeam.c:aitMsgSend | (fromTeam,toTeam,msgHash): lazily inits toTeam msg queue (+0x50), stores sender+msg, 64-entry wrap; sole callee of kasfMsgSend; mission dispatch requeues with it. (medium) |
| `0x00417ec0` | `cat_aitMsgQueueRemoveAt` | (Cataclysm-only) | (team,index): returns msg hash at index of team msg queue and swap-removes it; called by Cataclysm kas2c message-dispatch loops in 98 mission functions. (medium) |
| `0x004203a0` | `aivarDestroyAll` | AIVar.c:aivarDestroyAll | Frees every AIVar (0x8df668 array) and zeroes counts; called by kasLabelledEntitiesDestroy where HW1 KAS.c calls aivarDestroyAll(). (medium) |
| `0x0045b9a0` | `cat_kasCallFunctionByName` | (Cataclysm-only) | Hash-switch over cat_kasHashString(name): 486 branches, each calls one kasf* (the map used to name every kasf); only caller cat_consoleKasCommand. (high) |
| `0x0045f270` | `cat_consoleKasCommand` | (Cataclysm-only) | Dev console: strtoks "Name args", matches KAS name in table 0x755048 (stride 0x990), converts args by kas2c param type, calls 0x45b9a0, prints "KAS Ok"/"KAS Returned %d". (high) |
| `0x004901b0` | `hsStaticInit` | HS.c:hsStaticInit | Called by kasMissionStart exactly where HW1 KAS.c:239 calls hsStaticInit; frees static gate list 0xa43e84 and zeroes count 0xa43e80 (Cataclysm takes no argument). (medium) |
| `0x00490470` | `SaveHyperspaceGates` | HS.c:SaveHyperspaceGates | First call of kasSave (HW1 kasSave starts with SaveHyperspaceGates); saves hsNoGate flag 0x8818f4 and 0x30-byte static gate records. (high) |
| `0x00490510` | `LoadHyperspaceGates` | HS.c:LoadHyperspaceGates | First call of kasLoad; reads hsNoGate flag and 0x30-byte static gate records back (mirror of 0x490470). (high) |
| `0x004905f0` | `kasClose` | KAS.c:kasClose | Zeroes kasUnpausedTeam (0xa43eec) and subMessageReturnedFalseTime (0xa43f20); called from SinglePlayer code 0x5193f0. HW1 KAS.c:108. (high) |
| `0x00490610` | `kasInit` | KAS.c:kasInit | Only zeroes subMessageReturnedFalseTime (0xa43f20); called from SinglePlayer init 0x518c50. HW1 KAS.c:114. (medium) |
| `0x00490620` | `kasTeamDied` | KAS.c:kasTeamDied | if (CurrentTeamP(0xa43ee8)==team) CurrentTeamP=0; called from AITeam destroy 0x416b90. HW1 KAS.c:119. (high) |
| `0x00490640` | `kasJump` | KAS.c:kasJump | Stores state-name hash at team+0x60, watch fn at team+0x68, tail-jumps to init; 2507 call sites in mission code with (hash,Init_*,Watch_*). HW1 KAS.c:133. (high) |
| `0x00490660` | `kasFSMCreate` | KAS.c:kasFSMCreate | Sets team type 3 (ScriptTeam), FSM hash +0x5c, FSM watch +0x64, saves/restores CurrentTeamP+scope, runs init; 898 mission call sites. HW1 KAS.c:152. (high) |
| `0x004906d0` | `kasExecute` | KAS.c:kasExecute | Runs CurrentMissionWatchFunction then FSM/state watch of every ScriptTeam (or only kasUnpausedTeam); called from AIPlayer 0x411240 and universeUpdateTask. HW1 KAS.c:192. (high) |
| `0x00490770` | `kasMissionStart` | KAS.c:kasMissionStart | Called by singlePlayerGameUpdate with "mission01".."mission17" + Init/Watch and by tutInitTutorial with "tutorial1"; copies name, stores watch, calls hsStaticInit, runs init. (high) |
| `0x004907a0` | `kasAITeamPtr` | KAS.c:kasAITeamPtr | Scans aiCurrentAIPlayer teams for teamType 3 with label hash (+0x58); console TEAM_ prefix path; 2667 mission call sites. HW1 KAS.c:247. (high) |
| `0x004907e0` | `kasAITeamShipsPtr` | KAS.c:kasAITeamShipsPtr | Same scan as kasAITeamPtr, returns &team->shipList (+0x1c); console TEAMSHIPS_ prefix; 1164 mission call sites. HW1 KAS.c:274. (high) |
| `0x00490820` | `kasShipsVectorPtr` | KAS.c:kasShipsVectorPtr | Looks up labelled selection, returns static hvector of selection centre (FUN_00508be0) or static errorPos; console SHIPSPOINT_ prefix. HW1 KAS.c:301. (high) |
| `0x004908c0` | `kasTeamsVectorPtr` | KAS.c:kasTeamsVectorPtr | Team lookup by label hash then centre of team ships into static hvector; console TEAMSPOINT_ prefix. HW1 KAS.c:332. (high) |
| `0x00490960` | `kasVolumeVectorPtr` | KAS.c:kasVolumeVectorPtr | Labelled-volume lookup (0xa43f14) then volume centre (FUN_00551140) into static hvector; console VOLUMEPOINT_ prefix. HW1 KAS.c:363. (high) |
| `0x004909f0` | `kasThisTeamsVectorPtr` | KAS.c:kasThisTeamsVectorPtr | No args; centre of CurrentTeamP->shipList into static hvector, else errorPos; 505 mission call sites (THISTEAMSPOINT). HW1 KAS.c:384. (high) |
| `0x00490a60` | `kasPathPtrNoErrorChecking` | KAS.c:kasPathPtrNoErrorChecking | Labelled-path lookup (0xa43efc) returning NULL silently; console PATH_ prefix uses it. HW1 KAS.c:416. (high) |
| `0x00490aa0` | `kasPathPtr` | KAS.c:kasPathPtr | Same path lookup but calls the debug stub before returning NULL (dbgFatalf compiled out); called by mission code (PATH_x). HW1 KAS.c:431. (high) |
| `0x00490af0` | `kasVolumePtr` | KAS.c:kasVolumePtr | Labelled-volume lookup (0xa43f14/0xa43f18); console VOLUME_ prefix; 93 mission call sites. HW1 KAS.c:448. (high) |
| `0x00490b30` | `kasVectorPtr` | KAS.c:kasVectorPtr | Labelled-vector lookup (0xa43f08); console POINT_ prefix; 324 mission sites. kasVectorPtrIfExists has identical release code, likely ICF-folded here. HW1 KAS.c:480. (high) |
| `0x00490b70` | `kasGetGrowSelectionPtrIfExists` | KAS.c:kasGetGrowSelectionPtrIfExists | Selections (0xa43ef0) lookup returning &entry->shipList or NULL without creating; console SHIPS_ prefix. HW1 KAS.c:494. (high) |
| `0x00490bb0` | `kasGrowSelectionPtr` | KAS.c:kasGrowSelectionPtr | Find-or-create labelled selection (alloc 0x40/grow 0x10, growSelectInit); 7303 mission call sites (SHIPS_x). HW1 KAS.c:512. (high) |
| `0x00490ca0` | `kasGrowSelectionClear` | KAS.c:kasGrowSelectionClear | if (ships && ships->selection) ships->selection->numShips = 0; 19 callers. HW1 KAS.c:543. (high) |
| `0x00490cc0` | `kasLabelledPathAdd` | KAS.c:kasLabelledPathAdd | Grows LabelledPaths (0xa43efc), path = FUN_0041a480(numPoints,closed) (aiuCreatePathStruct), stores label hash. HW1 KAS.c:554. (high) |
| `0x00490d90` | `kasLabelledVolumeAdd` | KAS.c:kasLabelledVolumeAdd | Grows LabelledVolumes (0xa43f14), allocs 0x1c-byte Volume, stores label hash. HW1 KAS.c:579. (high) |
| `0x00490e60` | `kasLabelledVectorAdd` | KAS.c:kasLabelledVectorAdd | Grows LabelledVectors (0xa43f08), allocs hvector and stores x,y,z,w + label hash. HW1 KAS.c:604. (high) |
| `0x00490f90` | `kasLabelledEntitiesDestroy` | KAS.c:kasLabelledEntitiesDestroy | CurrentTeamP=0; frees selections, vectors, volumes, paths; then aivarDestroyAll, KAS var store, timTimerDestroyAll; caller SinglePlayer 0x519510. HW1 KAS.c:633. (high) |
| `0x00491120` | `kasAddShipToTeam` | KAS.c:kasAddShipToTeam | Finds ScriptTeam by label hash or aitCreate(3); bumps kasOrigShipsCount, stores shiptype (+0x2b0), aitAddShip, newships--. HW1 KAS.c:715. (high) |
| `0x004911a0` | `kasShipDied` | KAS.c:kasShipDied | Removes ship from every KAS selection (clRemoveShipFromSelection); sole caller 0x411970 in AIPlayer.c range, as HW1 AIPlayer.c:1367. HW1 KAS.c:745. (high) |
| `0x004911e0` | `cat_kasShipRemovedFromSelections` | (Cataclysm-only) | Byte-identical twin of kasShipDied (not folded); called from univUpdate and 0x548f40/0x5490d0/0x549270/0x549880; no HW1 counterpart. (medium) |
| `0x00491220` | `kasSave` | KAS.c:kasSave | Called by SaveGame; SaveHyperspaceGates, scope, scope name, CurrentMissionName(0x30), WatchFunctionToMissionEnum, team index, selections/paths/vectors/volumes, timTimerSave. HW1 KAS.c:1210. (high) |
| `0x00491450` | `kasLoad` | KAS.c:kasLoad | Called by LoadGame 0x4fe0e0; mirrors kasSave: LoadHyperspaceGates, MissionEnumToWatchFunction, team index, labelled lists. HW1 KAS.c:1277. (high) |
| `0x00491800` | `kasConvertFuncPtrToOffset` | KAS.c:kasConvertFuncPtrToOffset | Returns -1 for NULL else func - MissionEnumToWatchFunction(currentMission-1); caller AITeam save 0x418040 (HW1 AITeam.c:2510). HW1 KAS.c:1385. (high) |
| `0x00491830` | `kasConvertOffsetToFuncPtr` | KAS.c:kasConvertOffsetToFuncPtr | Inverse of kasConvertFuncPtrToOffset (-1 -> NULL); caller AITeam load 0x418270 (HW1 AITeam.c:2554). HW1 KAS.c:1415. (high) |
| `0x00491860` | `cat_kasHashString` | (Cataclysm-only) | ELF/PJW hash of (c & 0xDF) chars; hash("mission01")=0x087e6ef1 is the mission scope name in Init/Watch_Mission01; used by console KAS dispatcher and 15 others. (high) |
| `0x004918a0` | `cat_kasVarStoreInit` | (Cataclysm-only) | Zeroes the hashed KAS var store (0xa43f24/28/2c) and sets 0x882078=0x6aac470 (var hash kasLoad later sets to 1); called from 0x410fa0. (medium) |
| `0x004918d0` | `cat_kasVarFindOrCreate` | (Cataclysm-only) | Finds {scopeHash,nameHash,value} entry or appends one (grow by 0x20); used by kasfVarCreateSet etc. Replaces HW1 string AIVars for KAS. (medium) |
| `0x004919d0` | `cat_kasVarDestroyEntry` | (Cataclysm-only) | Removes the given 12-byte entry pointer from the KAS var store by shifting the array down. (medium) |
| `0x00491a50` | `cat_kasVarDestroy` | (Cataclysm-only) | Removes entry matching (scopeHash,nameHash) from the KAS var store by shifting the array down. (medium) |
| `0x00491ae0` | `cat_kasVarDestroyAll` | (Cataclysm-only) | Frees the KAS var store array and zeroes count/alloc; called from kasLabelledEntitiesDestroy and via thunk 0x4918c0. (medium) |
| `0x00491b20` | `cat_kasVarFind` | (Cataclysm-only) | Lookup of (scopeHash,nameHash) returning entry or NULL; 12 callers incl. kasfVarValueGet (scope 0 when name has bit31 / mission scope). (medium) |
| `0x00491b80` | `cat_kasVarSave` | (Cataclysm-only) | Writes count, alloc, 0x882078 then each 12-byte KAS var entry with the save-number/structure writers. (medium) |
| `0x00491be0` | `cat_kasVarLoad` | (Cataclysm-only) | Reads back what cat_kasVarSave wrote (count, alloc, 0x882078, entries). (medium) |
| `0x0051b230` | `WatchFunctionToMissionEnum` | SinglePlayer.c:WatchFunctionToMissionEnum | Loops mission index 0..0x13 comparing arg to the 18 Watch_MissionNN/Watch_Tutorial1 addresses; result saved by kasSave. HW1 SinglePlayer.c:3030. (high) |
| `0x0051b340` | `MissionEnumToWatchFunction` | SinglePlayer.c:MissionEnumToWatchFunction | switch(index) returning Watch_Mission01..17 and Watch_Tutorial1 (0x6708a0); used by kasLoad and both kasConvert* functions. HW1 SinglePlayer.c:3044. (high) |
| `0x0052bfe0` | `timTimerStart` | Timer.c:timTimerStart | Sole engine callee of kasfTimerStart (scope hash, name hash); Timer.c address order matches HW1 Timer.c source order. (high) |
| `0x0052c060` | `timTimerStop` | Timer.c:timTimerStop | Sole engine callee of kasfTimerStop (scope hash, name hash); Timer.c address order matches HW1 Timer.c source order. (high) |
| `0x0052c190` | `timTimerExpired` | Timer.c:timTimerExpired | Sole engine callee of kasfTimerExpired (scope hash, name hash); Timer.c address order matches HW1 Timer.c source order. (high) |
| `0x0052c290` | `timTimerExpiredDestroy` | Timer.c:timTimerExpiredDestroy | Sole engine callee of kasfTimerExpiredDestroy (scope hash, name hash); Timer.c address order matches HW1 Timer.c source order. (high) |
| `0x0052c3d0` | `timTimerRemaining` | Timer.c:timTimerRemaining | Sole engine callee of kasfTimerRemaining (scope hash, name hash); Timer.c address order matches HW1 Timer.c source order. (high) |
| `0x0052c480` | `timTimerSet` | Timer.c:timTimerSet | Sole engine callee of kasfTimerSet (scope hash, name hash); Timer.c address order matches HW1 Timer.c source order. (high) |
| `0x0052c4d0` | `timTimerDestroy` | Timer.c:timTimerDestroy | Sole engine callee of kasfTimerDestroy (scope hash, name hash); Timer.c address order matches HW1 Timer.c source order. (high) |
| `0x0052c550` | `timTimerDestroyAll` | Timer.c:timTimerDestroyAll | Frees timer array 0xa6b298, zeroes count/alloc; called by kasLabelledEntitiesDestroy as in HW1 KAS.c; between timTimerDestroy and timTimerCreate as in Timer.c:147. (high) |
| `0x0052c580` | `timTimerCreate` | Timer.c:timTimerCreate | Sole engine callee of kasfTimerCreate (scope hash, name hash); Timer.c address order matches HW1 Timer.c source order. (high) |
| `0x0052c660` | `timTimerCreateSetStart` | Timer.c:timTimerCreateSetStart | Sole engine callee of kasfTimerCreateSetStart (scope hash, name hash); Timer.c address order matches HW1 Timer.c source order. (high) |
| `0x0052c860` | `timTimerSave` | Timer.c:timTimerSave | Last call of kasSave (HW1 kasSave ends with timTimerSave); writes timer count/alloc and count*0x18 bytes. (medium) |
| `0x0052f690` | `tutInitTutorial` | Tutor.c:tutInitTutorial | levelInit(dir,level); clears tutLastLessonName/tutCurrLessonName; if tutorial==1 kasMissionStart("tutorial1",0x6704e0,0x6708a0). HW1 Tutor.c:524. (high) |
| `0x00755048` | `cat_kasFunctionTable` | tools/kas2c/kas2c.c:functions | kas2c FunctionCall[487] table (2448-byte entries: name, returnsNumber, realName, numParams, 16 params) embedded in .data; ends with "*" sentinel. (high) |

Runtime globals used by generated code: `CurrentTeamP` `0x00a43ee8`, `CurrentMissionScope` `0x00c3749c` (0/1/2),
`CurrentMissionScopeName` `0x00c374a0` (now a hash), `CurrentMissionWatchFunction` `0x00a43ee4`, `CurrentMissionName` `0x00c374c0`
(copied by `kasMissionStart`, `0x2f` bytes), `kasUnpausedTeam` `0x00a43eec`. AITeam KAS fields: label hash `+0x58`, FSM hash `+0x5c`,
state hash `+0x60`, FSM watch `+0x64`, state watch `+0x68`, message queue `+0x50`.

## 4. The Cataclysm kas2c dialect

Compared with HW1 kas2c (`tools/kas2c/kas2c.c`, `parser.y`), Cataclysm's generated code differs in these ways. A regenerated
`.kas` has to target this dialect:

1. **Names are hashes.** `cat_kasHashString(s)`: `h = (c & 0xDF) + (h << 4)`, then if the top nibble `g` is non-zero, `h ^= g >> 24; h &= ~g`
   (ELF/PJW hash, case-folded). Scripts pass the hash as an immediate:
   - `kasFSMCreate(hash("FSM"), Init, Watch, kasAITeamPtr(hash("team")))`
   - `kasJump(hash("State"), Init, Watch)` followed by `return`
   - `kasGrowSelectionPtr(hash("label"))`, and likewise for the other `kas*Ptr` lookups
   - every Timer/Var/Msg kasf takes a `UDWORD` hash
   The FSM create site is unchanged from HW1 apart from the hash.
2. **Scope prologue.** Each generated function starts with `CurrentMissionScope = 0|1|2`. Mission-level functions then set
   `CurrentMissionScopeName = hash("missionNN")` (or `hash("tutorial1")`), the lowercase name passed to `kasMissionStart`.
   This holds for all 18 scripts; for example `hash("mission01") = 0x087e6ef1`. FSM and state functions copy `CurrentTeamP->labelHash`
   (HW1: `strcpy(CurrentMissionScopeName, kasThisTeamPtr->kasLabel)`, kas2c.c:1489-1501).
3. **Variables.** KAS vars live in a hashed store `{scopeHash, nameHash, value}` (`cat_kasVar*`, `0x004918a0`-`0x00491be0`), not in
   HW1 AIVars. A name with bit 31 set is global (scope 0). kas2c sets that bit for `G_` names and hashes the full `G_` name, so
   `0x858db5e4` = `0x80000000 | hash("G_RecentlyLoaded")`. Other names are scoped to the current team label hash, or to 0 at mission
   scope. This is HW1 `kasfScopeName` (KASFunc.c:151-163) with hashes in place of string concatenation.
4. **Engine-to-script contract through global vars.** The engine reads and writes KAS globals by hashing these exe literals: `G_RecentlyLoaded`
   (kasLoad), `G_AutoSave` and `G_TimesGameLoaded` (`0x00497230`/`0x004972d0`), `G_ShipsHaveArrived` and `G_PlayNislet` (singlePlayerGameUpdate),
   `G_CrystalsHarvested` (`0x004f98b0`), `G_ResourcersRetired` (`0x004667f0`), `G_StopShipsFiringOnMission15` (`0x0048c2e0`),
   `G_TRIGGER_FirstShipInfected` (`0x004298f0`) and the `G_CLICK_` prefix (`0x004d66f0`). A rewrite must keep these names.
5. **`IFONCE`** becomes `if (!VarValueGet(h) && cond) { VarCreateSet(h, 1); ... }` with a hashed `_VIFONCEnnn` name. The format string is
   in kas2c, not in the game.
6. **Message switch (Cataclysm-only construct).** 98 functions walk the team message queue backwards. Each step calls
   `cat_aitMsgQueueRemoveAt(team, i)` (`0x00417ec0`), switches on the message hash (MSVC compiles this as a binary `cmp/ja` tree), runs the
   matching case, and on `default:` re-queues the message to itself with `aitMsgSend(CurrentTeamP, CurrentTeamP, msg)` (`0x00417e10`).
   HW1 kas2c has no such construct (HW1 scripts poll `MsgReceived`), so Cataclysm's parser gained a message-case statement.
7. **`LSTRING_x[strCurLanguage]`** is kept, but with **3** languages, indexed by `strCurLanguage` `0x00a6b00c`: English plus the
   placeholders `"French version goes here"` and `"German version goes here"`. HW1 uses `LSTRING_COUNT_REQUIRED 5` (kas2c.h:27).
   Example: `ObjectiveCreate("DestroyBombers", LSTRING[...]...)` at `0x005d73f7`.
8. **Other string literals** stay as `char*`: ship type names (`"sMothership"`), tech names (`"TECH_SALVAGE"`), race names, objective
   labels and tutorial FE region names. 389 distinct literal addresses are pushed from mission code.
9. **Linker effects.** MSVC `/OPT:ICF` merged byte-identical generated functions across FSMs and missions (section 7). Empty
   bodies merge the most, because the scope name comes from `CurrentTeamP` rather than a per-function constant. `kasfDock` and
   `kasfDockSupport` (identical bodies in HW1 KASFunc.c:1732,1738) fold to `0x00494570`. Nine debug or unused kasf fold into the two empty stubs.
   Ghidra FID mislabels those stubs `seekdir`/`telldir`; they are not CRT functions.
10. **Save games** store mission function pointers as `func - Watch_MissionNN` (`kasConvertFuncPtrToOffset`, `0x00491800`, via
    `MissionEnumToWatchFunction`, `0x0051b340`). Original `.SAV` files therefore encode **this exact layout**; see section 9.

## 5. kasf library

Every kasf address, signature and HW1 equivalent is in `missions.json` (class `hw1:KASFunc.c` = 291, `cat-new` kasf = 184; the other 14 `cat-new` entries are runtime helpers).
Ten C names have no entry of their own: the nine on the stubs below, and `kasfDockSupport`, which shares `0x00494570` with `kasfDock`.
HW1 kas2c names that are **missing** from Cataclysm's table: `kasfForceTaskbar`, `kasfTargetDrop`, `kasfTeamGiveToPlayer`.

Folded into empty stubs, so these are no-ops in retail: `0x0049ae30` (`ret`) = `kasfLog`, `kasfTeamSkillSet`, `kasfGateMoveToNearest`,
`kasfToggleActor`, `kasfGameEnd`, `kasfSetPlayerResourceCap`*, `kasfAssert`*. `0x006fe9a0` (`return 0`) = `kasfTeamSkillGet`,
`kasfGateShipsOutNearest`. Names marked * are Cataclysm-only. Mission code calls `0x49ae30` 15 times.

### 5.1 Cataclysm-only kasf (cat-new, not in HW1 kas2c)

| Address | C name | KAS name | Params | Mission call sites |
|---|---|---|---|---:|
| `0x00492740` | `kasfShipsSetFormation` | `ShipsSetFormation` | Ships:GROWSELECTIONPTR, Formation:SDWORD | 10 |
| `0x004928a0` | `kasfFormationParade` | `FormationParade` | pShips:GROWSELECTIONPTR | 1 |
| `0x004929a0` | `kasfShipsGuardShips` | `ShipsGuardShips` | Guard:GROWSELECTIONPTR, Ships:GROWSELECTIONPTR | 1 |
| `0x00492d60` | `kasfFindMeteors` | `FindMeteors` | meteors:GROWSELECTIONPTR | 1 |
| `0x00493060` | `kasfFindMinesNearTeam` | `FindMinesNearTeam` | ships:GROWSELECTIONPTR, radius:SDWORD -> num | 9 |
| `0x00493130` | `kasfFindMinesInside` | `FindMinesInside` | volume:VOLUMEPTR, ships:GROWSELECTIONPTR -> num | 6 |
| `0x00493400` | `kasfShipUnderConstruction` | `ShipUnderConstruction` | ships:GROWSELECTIONPTR -> num | 6 |
| `0x00493420` | `kasfShipsParalyze` | `ShipsParalyze` | ships:GROWSELECTIONPTR, paralyze:SDWORD | 1 |
| `0x00493460` | `kasfTimesHitByEMP` | `TimesHitByEMP` | ships:GROWSELECTIONPTR -> num | 3 |
| `0x00493550` | `kasfTeamAttributesBitTest` | `TeamAttributesBitTest` | attributes:SDWORD -> num | 3 |
| `0x00493a10` | `kasfShipsSetRepairFactor` | `ShipsRepairModifierSet` | ships:GROWSELECTIONPTR, repairModifier:SDWORD | 6 |
| `0x00493a60` | `kasfShipsClearRepairFactor` | `ShipsRepairModifierClear` | ships:GROWSELECTIONPTR | 0 |
| `0x00493af0` | `kasfShipsSetPercentDamaged` | `ShipsHealthSet` | ships:GROWSELECTIONPTR, percentHealth:SDWORD | 28 |
| `0x00493c90` | `kasfReinforceTeamWithNewShips` | `ReinforceTeamWithNewShips` | teamtoreinforce:AITEAMPTR, shipstoadd:GROWSELECTIONPTR | 0 |
| `0x00494250` | `kasfSetTactics` | `SetTactics` | Tactics:SDWORD | 0 |
| `0x00494830` | `kasfShipsLaunch` | `ShipsLaunch` | ships:GROWSELECTIONPTR, host:GROWSELECTIONPTR | 2 |
| `0x004948f0` | `kasfShipsFinishedLaunching` | `ShipsFinishedLaunching` | Ships:GROWSELECTIONPTR -> num | 1 |
| `0x00494a20` | `kasfMsgQueueFlush` | `MsgQueueFlush` | team:AITEAMPTR | 41 |
| `0x00494f50` | `kasfObjectiveGetAny` | `ObjectivesGetAny` | -> num | 3 |
| `0x004950d0` | `kasfShipsSelectGuarding` | `ShipsSelectGuarding` | NewShips:GROWSELECTIONPTR, OldShips:GROWSELECTIONPTR, Guarding:GROWSELECTIONPTR -> num | 4 |
| `0x00495140` | `kasfShipsSelectIdle` | `ShipsSelectIdle` | NewShips:GROWSELECTIONPTR, OldShips:GROWSELECTIONPTR -> num | 0 |
| `0x00495160` | `kasfShipsSelectNotIdle` | `ShipsSelectNotIdle` | NewShips:GROWSELECTIONPTR, OldShips:GROWSELECTIONPTR -> num | 1 |
| `0x00495180` | `kasfShipsSelectClamped` | `ShipsSelectClamped` | NewShips:GROWSELECTIONPTR, OldShips:GROWSELECTIONPTR, Target:GROWSELECTIONPTR -> num | 6 |
| `0x00495200` | `kasfShipsGetClamped` | `ShipsGetClamped` | pShips:GROWSELECTIONPTR, pTarget:GROWSELECTIONPTR -> num | 3 |
| `0x00495260` | `kasfShipsSelectNumber` | `ShipsSelectNumber` | NewShips:GROWSELECTIONPTR, OldShips:GROWSELECTIONPTR, Number:SDWORD -> num | 17 |
| `0x004952b0` | `kasfShipsSelectPlayer` | `ShipsSelectPlayer` | NewShips:GROWSELECTIONPTR, OldShips:GROWSELECTIONPTR -> num | 23 |
| `0x004952f0` | `kasfShipsSelectSalvaging` | `ShipsSelectSalvaging` | NewShips:GROWSELECTIONPTR, OldShips:GROWSELECTIONPTR | 0 |
| `0x00495390` | `kasfShipsSelectSalvagable` | `ShipsSelectSalvagable` | NewShips:GROWSELECTIONPTR, OldShips:GROWSELECTIONPTR, Targets:GROWSELECTIONPTR -> num | 6 |
| `0x00495410` | `kasfShipsSelectShips` | `ShipsSelectShips` | newships:GROWSELECTIONPTR, oldships:GROWSELECTIONPTR, compareships:GROWSELECTIONPTR -> num | 53 |
| `0x00495460` | `kasfShipsSelectInside` | `ShipsSelectInside` | newships:GROWSELECTIONPTR, oldships:GROWSELECTIONPTR, volume:VOLUMEPTR -> num | 12 |
| `0x004954a0` | `kasfShipsSelectUpgradable` | `ShipsSelectUpgradable` | newShips:GROWSELECTIONPTR, originalShips:GROWSELECTIONPTR -> num | 2 |
| `0x004954d0` | `kasfShipsSelectEnemyNoStealthy` | `ShipsSelectEnemyNoStealthy` | NewShips:GROWSELECTIONPTR, OriginalShips:GROWSELECTIONPTR -> num | 33 |
| `0x00495750` | `kasfShipsSelectDocked` | `ShipsSelectDocked` | NewShips:GROWSELECTIONPTR, OldShips:GROWSELECTIONPTR -> num | 1 |
| `0x00495840` | `kasfShipsSelectMoreThanDamaged` | `ShipsSelectMoreThanDamaged` | newShips:GROWSELECTIONPTR, originalShips:GROWSELECTIONPTR, maxHealthPercent:SDWORD -> num | 10 |
| `0x004959a0` | `kasfShipsSelectFarther` | `ShipsSelectFarther` | newships:GROWSELECTIONPTR, oldships:GROWSELECTIONPTR, point:VECTORPTR, distance:SDWORD | 0 |
| `0x00495ee0` | `kasfShipsStop` | `ShipsStop` | ship:GROWSELECTIONPTR | 84 |
| `0x00496080` | `kasfShipsSpecialToggle` | `ShipsSpecialToggle` | ships:GROWSELECTIONPTR | 0 |
| `0x00496160` | `kasfResearchCloseIfOpen` | `ResearchCloseIfOpen` |  | 2 |
| `0x004961b0` | `kasfCameraSetAngleDeg` | `CameraSetAngleDeg` | angle:SDWORD | 98 |
| `0x00496210` | `kasfCameraSetDeclinationDeg` | `CameraSetDeclinationDeg` | declination:SDWORD | 99 |
| `0x00496260` | `kasfCameraSetDistance` | `CameraSetDistance` | distance:SDWORD | 89 |
| `0x004963c0` | `kasfTutSetObjPointerTargetShip` | `TutSetObjPointerTargetShip` | name:CHARPTR, pShipList:GROWSELECTIONPTR | 3 |
| `0x004963e0` | `kasfTutSetObjPointerTargetShipSelection` | `TutSetObjPointerTargetShipSelection` | name:CHARPTR, pShipList:GROWSELECTIONPTR | 6 |
| `0x004964b0` | `kasfTutSetObjPointerTargetAIVolume` | `TutSetObjPointerTargetAIVolume` | name:CHARPTR, volume:VOLUMEPTR | 1 |
| `0x004964d0` | `kasfTutSetPointerTargetObj` | `TutSetPointerTargetObj` | name:CHARPTR | 10 |
| `0x00496610` | `kasfTutShowNextTextButton` | `TutShowNextTextButton` |  | 0 |
| `0x00496620` | `kasfTutHideNextTextButton` | `TutHideNextTextButton` |  | 0 |
| `0x00496630` | `kasfTutNextTextButtonClicked` | `TutNextTextButtonClicked` | -> num | 0 |
| `0x00496650` | `kasfTutHidePrevButton` | `TutHidePrevButton` |  | 0 |
| `0x00496660` | `kasfTutPrevButtonClicked` | `TutPrevButtonClicked` | -> num | 1 |
| `0x00496670` | `kasfTutShowQuitButton` | `TutShowQuitButton` |  | 1 |
| `0x00496680` | `kasfTutHideQuitButton` | `TutHideQuitButton` |  | 0 |
| `0x00496690` | `kasfTutQuitButtonClicked` | `TutQuitButtonClicked` | -> num | 0 |
| `0x00496870` | `kasfCameraFocus` | `CameraFocus` | ships:GROWSELECTIONPTR | 51 |
| `0x00496910` | `kasfCameraZoom` | `CameraZoom` | zoom:SDWORD | 0 |
| `0x00496930` | `kasfCameraDistance` | `CameraDistance` | distance:SDWORD | 5 |
| `0x00496950` | `kasfCameraFocusClose` | `CameraFocusClose` | ships:GROWSELECTIONPTR | 38 |
| `0x00496cc0` | `kasfResearchIsOpen` | `ResearchIsOpen` | flags:SDWORD -> num | 6 |
| `0x00496cd0` | `kasfBuildIsOpen` | `BuildIsOpen` | flags:SDWORD -> num | 6 |
| `0x00496f00` | `kasfSpeechEventLink` | `SpeechEventLink` | event:SDWORD, variable:SDWORD, bFirstLink:SDWORD | 0 |
| `0x00496f80` | `kasfSpeechEventNoLink` | `SpeechEventNoLink` | event:SDWORD, variable:SDWORD | 0 |
| `0x00497580` | `kasfWideScreenInDontCloseSensors` | `wideScreenInDontCloseSensors` | frames:SDWORD | 38 |
| `0x004976f0` | `kasfSubtitleReset` | `SubtitleReset` |  | 54 |
| `0x00497d30` | `kasfSpecialOn` | `SpecialOn` | ships:GROWSELECTIONPTR -> num | 28 |
| `0x00497db0` | `kasfSpecialOff` | `SpecialOff` | -> num | 0 |
| `0x00497dc0` | `kasfShipsSpecialOn` | `ShipsSpecialOn` | srcShips:GROWSELECTIONPTR, destShips:GROWSELECTIONPTR -> num | 0 |
| `0x00497e40` | `kasfShipsSpecialOff` | `ShipsSpecialOff` | ships:GROWSELECTIONPTR -> num | 0 |
| `0x00497e70` | `kasfShipsSelectLowFuel` | `ShipsSelectLowFuel` | newShips:GROWSELECTIONPTR, originalShips:GROWSELECTIONPTR, percentFuel:SDWORD -> num | 0 |
| `0x00497ed0` | `kasfDeleteShipsWhenMissionEnds` | `DeleteShipsWhenMissionEnds` | shipstodelete:GROWSELECTIONPTR | 12 |
| `0x00497f10` | `kasfSetCurrentMimicItem` | `SetCurrentMimicItem` | itemType:SDWORD, shipName:CHARPTR | 227 |
| `0x00497f70` | `kasfSetMCVItemIndex` | `SetMCVItemIndex` | itemType:SDWORD, index:SDWORD | 32 |
| `0x00497fd0` | `kasfMimic` | `Mimic` | ships:GROWSELECTIONPTR, shipName:CHARPTR, playerIndex:SDWORD | 6 |
| `0x00498080` | `kasfUnmimic` | `Unmimic` | ships:GROWSELECTIONPTR | 6 |
| `0x004980c0` | `kasfEnableShipConstruction` | `EnableShipConstruction` | shipType:CHARPTR, shipRace:CHARPTR, bEnabled:SDWORD | 66 |
| `0x00498100` | `kasfShipsAttackCapable` | `ShipsAttackCapable` | pShip:GROWSELECTIONPTR, bCapable:SDWORD | 0 |
| `0x00498150` | `kasfGetShipsClampedTo` | `GetShipsClampedTo` | pShipsAttached:GROWSELECTIONPTR, pHostShips:GROWSELECTIONPTR -> num | 7 |
| `0x00498230` | `kasfStartLevelByHyperspacingShips` | `StartLevelByHyperspacingShips` |  | 17 |
| `0x00498250` | `kasfLockoutMainRegion` | `LockoutMainRegion` | lock:SDWORD | 1 |
| `0x00498270` | `kasfShipsScuttle` | `ShipsScuttle` | ships:GROWSELECTIONPTR | 37 |
| `0x00498290` | `kasfMakeShipCamera` | `MakeShipCamera` | ship:GROWSELECTIONPTR | 0 |
| `0x00498300` | `kasfSetShipPositionXYZ` | `SetShipPositionXYZ` | ship:GROWSELECTIONPTR, x:SDWORD, y:SDWORD, z:SDWORD | 1 |
| `0x00498360` | `kasfSetShipPosition` | `SetShipPosition` | ship:GROWSELECTIONPTR, destination:VECTORPTR | 0 |
| `0x004983b0` | `kasfSetShipPositionNear` | `SetShipPositionNear` | ship:GROWSELECTIONPTR, detinstation:VECTORPTR, distance:SDWORD | 0 |
| `0x00498490` | `kasfShowMinesInSensors` | `ShowMinesInSensors` | showMines:SDWORD | 1 |
| `0x004984a0` | `kasfChaseMode` | `ChaseMode` | -> num | 0 |
| `0x004984b0` | `kasfToggleChaseMode` | `ToggleChaseMode` |  | 0 |
| `0x004984c0` | `kasfShipsSetInvincible` | `ShipsSetInvincible` | ships:GROWSELECTIONPTR, invincible:SDWORD | 103 |
| `0x00498510` | `kasfShipsSetDerelict` | `ShipsSetDerelict` | ships:GROWSELECTIONPTR, bDerelict:SDWORD | 6 |
| `0x00498550` | `kasfSetMotionCues` | `SetMotionCues` | cue:SDWORD | 0 |
| `0x00498590` | `kasfHaltAllPlayerShips` | `HaltAllPlayerShips` |  | 1 |
| `0x004985f0` | `kasfShipsSelectRace` | `ShipsSelectRace` | newShips:GROWSELECTIONPTR, originalShips:GROWSELECTIONPTR, shipType:SDWORD -> num | 15 |
| `0x00498630` | `kasfShipsUnderAttackElsewhere` | `ShipsUnderAttackElsewhere` | ships:GROWSELECTIONPTR, attackers:GROWSELECTIONPTR -> num | 50 |
| `0x004986f0` | `kasfShipsBeastify` | `ShipsBeastify` | ships:GROWSELECTIONPTR, originator:GROWSELECTIONPTR | 3 |
| `0x00498790` | `kasfGetPlayerMothership` | `GetPlayerMothership` | ships:GROWSELECTIONPTR | 14 |
| `0x004987e0` | `kasfFindEnemyShipsInsideWithoutStealthy` | `FindEnemyShipsInsideWithoutStealthy` | volume:VOLUMEPTR, ships:GROWSELECTIONPTR -> num | 11 |
| `0x00498870` | `kasfFindEnemiesNearbyWithoutStealthy` | `FindEnemiesNearbyWithoutStealthy` | ships:GROWSELECTIONPTR, radius:SDWORD -> num | 0 |
| `0x00498970` | `kasfFindEnemiesNearTeamWithoutStealthy` | `FindEnemiesNearTeamWithoutStealthy` | ships:GROWSELECTIONPTR, radius:SDWORD -> num | 0 |
| `0x00498a60` | `kasfShipsSetBeastInfectionUnlimited` | `ShipsSetBeastInfectionUnlimited` | ships:GROWSELECTIONPTR, enable:SDWORD | 15 |
| `0x00498ae0` | `kasfShipsSelectBeastified` | `ShipsSelectBeastified` | newShips:GROWSELECTIONPTR, originalShips:GROWSELECTIONPTR -> num | 11 |
| `0x00498b30` | `kasfShipsSelectSalvaged` | `ShipsSelectSalvaged` | newShips:GROWSELECTIONPTR, originalShips:GROWSELECTIONPTR -> num | 3 |
| `0x00498b80` | `kasfPingMultipleAddShips` | `PingMultipleAddShips` | ships:GROWSELECTIONPTR, label:CHARPTR | 16 |
| `0x00498c10` | `kasfGiveTechToAI` | `GiveTechToAI` | szTechName:CHARPTR | 65 |
| `0x00498c70` | `kasfShipsUpgrade` | `UpgradeShips` | ships:GROWSELECTIONPTR | 212 |
| `0x00498cb0` | `kasfGetShipCountSalvaged` | `GetShipCountSalvaged` | shipType:CHARPTR, player:SDWORD -> num | 5 |
| `0x00498cd0` | `kasfGetShipCountBeastified` | `GetShipCountBeastified` | shipType:CHARPTR, player:SDWORD -> num | 0 |
| `0x00498cf0` | `kasfMakeShipsControllable` | `MakeShipsControllable` | ships:GROWSELECTIONPTR, controllable:SDWORD | 13 |
| `0x00498d50` | `kasfGetNumberShipsPlayerKilled` | `GetNumberShipsPlayerKilled` | playerIndex:SDWORD, shipClass:SDWORD -> num | 0 |
| `0x00498d70` | `kasfGetNumberShipsPlayerLost` | `GetNumberShipsPlayerLost` | playerIndex:SDWORD, shipClass:SDWORD -> num | 0 |
| `0x00498d90` | `kasfFirstShip` | `FirstShip` | pDst:GROWSELECTIONPTR, pSrc:GROWSELECTIONPTR | 6 |
| `0x00498dd0` | `kasfVectorAdd` | `PointAdd` | label:UDWORD, x:SDWORD, y:SDWORD, z:SDWORD | 80 |
| `0x00498e30` | `kasfVectorCopy` | `PointCopy` | label:UDWORD, vec:VECTORPTR | 19 |
| `0x00498e80` | `kasfVectorCrossProduct` | `PointCrossProduct` | Result:VECTORPTR, One:VECTORPTR, Two:VECTORPTR | 3 |
| `0x00498ef0` | `kasfVectorDotProduct` | `PointDotProduct` | One:VECTORPTR, Two:VECTORPTR -> num | 0 |
| `0x00498f30` | `kasfVectorParametric` | `PointParametric` | result:VECTORPTR, start:VECTORPTR, end:VECTORPTR, t:SDWORD, maxt:SDWORD | 8 |
| `0x00499020` | `kasfVectorAddTo` | `PointAddTo` | result:VECTORPTR, add:VECTORPTR | 7 |
| `0x004990e0` | `kasfVectorSubFrom` | `PointSubFrom` | result:VECTORPTR, sub:VECTORPTR | 4 |
| `0x004991a0` | `kasfVectorScalarMult` | `PointScalarMult` | result:VECTORPTR, numerator:SDWORD, denominator:SDWORD | 4 |
| `0x004991d0` | `kasfVectorNear` | `PointNear` | result:VECTORPTR, near:VECTORPTR, distance:SDWORD | 45 |
| `0x00499330` | `kasfSalvage` | `Salvage` | ships:GROWSELECTIONPTR | 7 |
| `0x00499460` | `kasfRepair` | `Repair` | ships:GROWSELECTIONPTR | 5 |
| `0x00499570` | `kasfHasShipBeenDiscovered` | `HasShipBeenDiscovered` | ship:GROWSELECTIONPTR -> num | 38 |
| `0x004995e0` | `kasfFindSlipgate` | `FindSlipgate` | volume:VOLUMEPTR, slipgates:GROWSELECTIONPTR | 11 |
| `0x00499670` | `kasfShipsDockInstant` | `ShipsDockInstant` | shipSel:GROWSELECTIONPTR, hostSel:GROWSELECTIONPTR | 12 |
| `0x004996e0` | `kasfSlipgateDockInstant` | `SlipgateDockInstant` | slipgate:GROWSELECTIONPTR | 1 |
| `0x00499760` | `kasfSlipgateDock` | `SlipgateDock` | slipgate:GROWSELECTIONPTR | 3 |
| `0x00499790` | `kasfStopInstant` | `StopInstant` |  | 4 |
| `0x00499820` | `kasfEnteringSlipgate` | `EnteringSlipgate` | -> num | 3 |
| `0x00499870` | `kasfIsIdle` | `IsIdle` | ships:GROWSELECTIONPTR -> num | 212 |
| `0x004998b0` | `kasfIsTeamIdle` | `IsTeamIdle` | team:AITEAMPTR -> num | 0 |
| `0x004998c0` | `kasfThisTeamIdle` | `ThisTeamIdle` | -> num | 5 |
| `0x004998d0` | `kasfShipsInVolume` | `ShipsInVolume` | volume:VOLUMEPTR, shipsToFind:GROWSELECTIONPTR -> num | 0 |
| `0x004999c0` | `kasfInfectionMissileAttack` | `InfectionMissileAttack` | targetSel:GROWSELECTIONPTR | 0 |
| `0x00499b80` | `kasfGetBuildTime` | `GetBuildTime` | ShipType:CHARPTR -> num | 106 |
| `0x00499bb0` | `kasfGetBuildCost` | `GetBuildCost` | ShipType:CHARPTR -> num | 564 |
| `0x00499bd0` | `kasfShipsCreate` | `ShipsCreate` | createdShips:GROWSELECTIONPTR, shiptype:CHARPTR, count:SDWORD, owner:SDWORD | 31 |
| `0x00499c70` | `kasfShipsCreateLaunch` | `ShipsCreateLaunch` | createdShips:GROWSELECTIONPTR, shiptype:CHARPTR, count:SDWORD, host:GROWSELECTIONPTR, owner:SDWORD | 143 |
| `0x00499e40` | `kasfShipsCreateHyperspace` | `ShipsCreateHyperspace` | createdShips:GROWSELECTIONPTR, shiptype:CHARPTR, count:SDWORD, dest:VECTORPTR, owner:SDWORD | 0 |
| `0x00499f90` | `kasfPlayerNearShips` | `PlayerNearShips` | ships:GROWSELECTIONPTR, distance:SDWORD -> num | 0 |
| `0x0049a030` | `kasfShipsSetBulimia` | `ShipsSetBulimia` | ships:GROWSELECTIONPTR, bulimia:SDWORD | 1 |
| `0x0049a080` | `kasfShipsEffectedByMines` | `ShipsEffectedByMines` | ships:GROWSELECTIONPTR, effectedByMines:SDWORD | 4 |
| `0x0049a0c0` | `kasfShipInfectionTerminal` | `ShipInfectionTerminal` | ships:GROWSELECTIONPTR -> num | 0 |
| `0x0049a100` | `kasfShipsSort` | `ShipsSort` | ships:GROWSELECTIONPTR, origin:VECTORPTR | 232 |
| `0x0049a220` | `kasfEnableNaggarok` | `EnableNaggarok` | pNaggarok:GROWSELECTIONPTR, bEnable:SDWORD | 9 |
| `0x0049a250` | `kasfForceCataclysmDefaultMode` | `ForceCataclysmDefaultMode` |  | 1 |
| `0x0049a2a0` | `kasfSetSensorsLookAtPoint` | `SetSensorsLookAtPoint` | point:VECTORPTR | 45 |
| `0x0049a2d0` | `kasfShipsSetHelpless` | `ShipsSetHelpless` | ships:GROWSELECTIONPTR, helpless:SDWORD | 11 |
| `0x0049a350` | `kasfTutSelectShips` | `TutSelectShips` | pShips:GROWSELECTIONPTR | 1 |
| `0x0049a360` | `kasfNomadMoonEmitterCount` | `NomadMoonEmitterCount` | NomadMoon:GROWSELECTIONPTR -> num | 1 |
| `0x0049a380` | `kasfNomadMoonCanFire` | `NomadMoonCanFire` | NomadMoon:GROWSELECTIONPTR -> num | 1 |
| `0x0049a3a0` | `kasfNomadMoonFire` | `NomadMoonFire` | NomadMoon:GROWSELECTIONPTR -> num | 2 |
| `0x0049a3c0` | `kasfGetNaggarokMissiles` | `GetNaggarokMissiles` | Naggarok:GROWSELECTIONPTR -> num | 2 |
| `0x0049a3e0` | `kasfDecNaggarokMissiles` | `DecNaggarokMissiles` | Naggarok:GROWSELECTIONPTR | 1 |
| `0x0049a400` | `kasfEnableBeastWeaponAutoAttack` | `EnableBeastWeaponAutoAttack` | pBeastShips:GROWSELECTIONPTR, bEnable:SDWORD | 99 |
| `0x0049a460` | `kasfSetAIDepotShip` | `SetAIDepotShip` | pShip:GROWSELECTIONPTR | 3 |
| `0x0049a480` | `kasfMission13DeleteEngineeringModuleHack` | `Mission13DeleteEngineeringModuleHack` |  | 0 |
| `0x0049a490` | `kasfSlipgateExplode` | `SlipgateExplode` | center:VECTORPTR | 1 |
| `0x0049a4d0` | `kasfShipsHyperspaceOut` | `ShipsHyperspaceOut` | ships:GROWSELECTIONPTR | 11 |
| `0x0049a530` | `kasfShipsHyperspaceIn` | `ShipsHyperspaceIn` | ships:GROWSELECTIONPTR, point:VECTORPTR | 8 |
| `0x0049a560` | `kasfShipsHyperspaceInDefault` | `ShipsHyperspaceInDefault` | ships:GROWSELECTIONPTR | 18 |
| `0x0049a5f0` | `kasfForceParadeFormation` | `ForceParadeFormation` | mothership:GROWSELECTIONPTR, fleet:GROWSELECTIONPTR | 20 |
| `0x0049a6a0` | `kasfIsForceFieldActive` | `IsForceFieldActive` | Ships:GROWSELECTIONPTR -> num | 0 |
| `0x0049a6f0` | `kasfIsEMPReady` | `IsEMPReady` | Ships:GROWSELECTIONPTR -> num | 3 |
| `0x0049a740` | `kasfIsBeastReady` | `IsBeastReady` | Ships:GROWSELECTIONPTR -> num | 8 |
| `0x0049a790` | `kasfIsGuarding` | `IsGuarding` | Ships:GROWSELECTIONPTR, Guarded:GROWSELECTIONPTR -> num | 41 |
| `0x0049a850` | `kasfSetStayMimicked` | `SetStayMimicked` | pShips:GROWSELECTIONPTR, bStayMimicked:SDWORD | 2 |
| `0x0049a8b0` | `kasfGetDockHost` | `GetDockHost` | Ships:GROWSELECTIONPTR, Host:GROWSELECTIONPTR, PossibleHost:GROWSELECTIONPTR | 27 |
| `0x0049a950` | `kasfGetTargets` | `GetTargets` | Targets:GROWSELECTIONPTR, Ships:GROWSELECTIONPTR -> num | 6 |
| `0x0049aa00` | `kasfRKOBigGunPos` | `RKOBigGunPos` | RKO:GROWSELECTIONPTR, Bullet:VECTORPTR | 0 |
| `0x0049aa40` | `kasfRKOIsSafe` | `RKOIsSafe` | RKO:GROWSELECTIONPTR -> num | 2 |
| `0x0049aa60` | `kasfRKOAvoidLocation` | `RKOAvoidLocation` | RKO:GROWSELECTIONPTR, Avoid:VECTORPTR, Safe:VECTORPTR -> num | 1 |
| `0x0049aac0` | `kasfRKOBeware` | `RKOBeware` | RKO:GROWSELECTIONPTR -> num | 3 |
| `0x0049aae0` | `kasfShipsSetColor` | `ShipsSetColor / ShipsSetColour` | Ships:GROWSELECTIONPTR, Color:SDWORD | 336 |
| `0x0049ab20` | `kasfIsSwarmReady` | `IsSwarmReady` | Ships:GROWSELECTIONPTR -> num | 1 |
| `0x0049ab70` | `kasfShipsHealthAverage` | `ShipsHealthAverage` | Ships:GROWSELECTIONPTR -> num | 4 |
| `0x0049abd0` | `kasfShipsSetInvincibleInWidescreen` | `ShipsSetInvincibleInWidescreen` | pShips:GROWSELECTIONPTR, bInvincibleInWidescreen:SDWORD | 12 |
| `0x0049ac30` | `kasfDisableBigGunFiring` | `DisableBigGun` |  | 6 |
| `0x0049ac40` | `kasfShipsShuffle` | `ShipsShuffle` | pShips:GROWSELECTIONPTR | 1 |
| `0x0049ac90` | `kasfShipsStopForResearchDocking` | `ShipsStopForResearchDocking` | pShips:GROWSELECTIONPTR, bStop:SDWORD | 2 |
| `0x0049ace0` | `kasfGetVectorX` | `GetPointX` | vector:VECTORPTR -> num | 3 |
| `0x0049ad00` | `kasfGetVectorY` | `GetPointY` | vector:VECTORPTR -> num | 3 |
| `0x0049ad20` | `kasfGetVectorZ` | `GetPointZ` | vector:VECTORPTR -> num | 3 |
| `0x0049ad40` | `kasfGetVectorW` | `GetPointW` | vector:VECTORPTR -> num | 0 |
| `0x0049ad60` | `kasfCameraSetChaseMode` | `CameraSetChaseMode` | chaseMode:SDWORD | 2 |
| `0x0049ad80` | `kasfShipsInsideMe` | `ShipsInsideMe` | InsideMe:GROWSELECTIONPTR, Me:GROWSELECTIONPTR -> num | 18 |

43 Cataclysm-only kasf are never called from mission code (console-only or unused): `kasfCameraZoom`, `kasfChaseMode`, `kasfFindEnemiesNearTeamWithoutStealthy`, `kasfFindEnemiesNearbyWithoutStealthy`, `kasfGetNumberShipsPlayerKilled`, `kasfGetNumberShipsPlayerLost`, `kasfGetShipCountBeastified`, `kasfGetVectorW`, `kasfInfectionMissileAttack`, `kasfIsForceFieldActive`, `kasfIsTeamIdle`, `kasfMakeShipCamera`, `kasfMission13DeleteEngineeringModuleHack`, `kasfPlayerNearShips`, `kasfRKOBigGunPos`, `kasfReinforceTeamWithNewShips`, `kasfSetMotionCues`, `kasfSetShipPosition`, `kasfSetShipPositionNear`, `kasfSetTactics`, `kasfShipInfectionTerminal`, `kasfShipsAttackCapable`, `kasfShipsClearRepairFactor`, `kasfShipsCreateHyperspace`, `kasfShipsInVolume`, `kasfShipsSelectFarther`, `kasfShipsSelectIdle`, `kasfShipsSelectLowFuel`, `kasfShipsSelectSalvaging`, `kasfShipsSpecialOff`, `kasfShipsSpecialOn`, `kasfShipsSpecialToggle`, `kasfSpecialOff`, `kasfSpeechEventLink`, `kasfSpeechEventNoLink`, `kasfToggleChaseMode`, `kasfTutHideNextTextButton`, `kasfTutHidePrevButton`, `kasfTutHideQuitButton`, `kasfTutNextTextButtonClicked`, `kasfTutQuitButtonClicked`, `kasfTutShowNextTextButton`, `kasfVectorDotProduct`.

Overall, mission code makes 23872 kasf call sites to 310 distinct kasf addresses (15 of them to the empty stub `0x49ae30`), plus these runtime calls: `kasGrowSelectionPtr` 7,303,
`kasAITeamPtr` 2,667, `kasJump` 2,507, `kasAITeamShipsPtr` 1,164, `kasFSMCreate` 898, `kasThisTeamsVectorPtr` 505, `kasVectorPtr` 324,
`cat_aitMsgQueueRemoveAt` 114, `kasShipsVectorPtr` 111, `aitMsgSend` 107, `kasTeamsVectorPtr` 96, `kasVolumePtr` 93, `kasPathPtr` 9,
`kasVolumeVectorPtr` 8. Mission code calls nothing outside the KAS runtime, the kasf library and the two `aitMsg*` helpers.

## 6. Name recovery

Names are case-insensitive 28-bit hashes, so a recovered name is only a string with the right hash. Capitalisation is a guess. Sources, best first:

- *(no mark)* **dictionary**: exact hit among 42,323 identifiers from HW1 `.kas` sources, Cataclysm `.mission` layouts (team, point,
  path and volume labels), exe strings and HW1 headers. At this dictionary size a random collision is expected about 0.07 times over the
  whole set.
- **† team stem**: an FSM named after its own team labels, or a numbered sibling of a recovered name. Examples: `TaiidanPunishmentFrigate`
  for teams `TaiidanPunishmentFrigate1..3`, `kCarriers` for `kCarrier1..2`, `WaitMessage10`. Each hash was tested against a few hundred
  candidates, so these are reliable.
- **\* combination**: two vocabulary words joined (for example `AttackCarrier`, `Lesson10`). The earlier pass used a 4,000-word vocabulary,
  where random hits are expected, and kept only plausible results by hand. The second pass used the 826 words of recovered names, with about
  0.2 expected false hits. Treat \* names as likely, not proven.
- **?** unresolved: shown as `FSM_x<hash>` / `S_x<hash>`.

| | dictionary | † stem | \* combination | ? unresolved | total distinct hashes |
|---|---:|---:|---:|---:|---:|
| FSM names | 116 | 26 | 41 | 28 | 211 |
| State names | 97 | 3 | 80 | 23 | 203 |
| Team labels | 657 | | | 0 | 657 |

Team labels all resolve exactly because they come from each mission's `.mission` file. FSM names are often the team label (146 of
898 create sites pass the same hash twice), as in HW1 practice.

## 7. Mission index and shared functions

Sizes are code bytes from the entry to the end of the last instruction. Inline jump tables and alignment padding are excluded; the
block column includes them. "Save offset" for any function = address − Watch address of its mission.

| Script | `kasMissionStart` name (scope hash) | Init | Watch | Block | Bytes | Functions in block | FSM defs | States | kasf sites | cat-new kasf used |
|---|---|---|---|---|---:|---:|---:|---:|---:|---:|
| Mission01 | `"mission01"` (`87e6ef1`) | `0x005db0f0`(86) | `0x005db150`(325) | `0x005d6fe0`-`0x005db2a0` | 17088 | 85 | 20 | 44 | 636 | 23 |
| Mission02 | `"mission02"` (`87e6ef2`) | `0x005e05a0`(1532) | `0x005e0bb0`(1446) | `0x005db2a0`-`0x005e1160` | 24256 | 100 | 15 | 62 | 947 | 45 |
| Mission03 | `"mission03"` (`87e6ef3`) | `0x005e4c60`(1109) | `0x005e50d0`(1247) | `0x005e1160`-`0x005e55b0` | 17488 | 51 | 8 | 28 | 685 | 34 |
| Mission04 | `"mission04"` (`87e6ef4`) | `0x005ea340`(1548) | `0x005ea950`(890) | `0x005e55b0`-`0x005eacd0` | 22304 | 74 | 14 | 40 | 813 | 42 |
| Mission05 | `"mission05"` (`87e6ef5`) | `0x005f3ba0`(2172) | `0x005f4420`(571) | `0x005eacd0`-`0x005f4660` | 39312 | 134 | 21 | 80 | 1493 | 50 |
| Mission06 | `"mission06"` (`87e6ef6`) | `0x005fb080`(1603) | `0x005fb6d0`(1572) | `0x005f4660`-`0x005fbd00` | 30368 | 112 | 16 | 58 | 1107 | 49 |
| Mission07 | `"mission07"` (`87e6ef7`) | `0x00601440`(3633) | `0x00602280`(1195) | `0x005fbd00`-`0x00602730` | 27184 | 74 | 13 | 41 | 911 | 35 |
| Mission08 | `"mission08"` (`87e6ef8`) | `0x006052f0`(933) | `0x006056a0`(3202) | `0x00602730`-`0x00606330` | 15360 | 47 | 10 | 28 | 597 | 36 |
| Mission09 | `"mission09"` (`87e6ef9`) | `0x0060d2d0`(2099) | `0x0060db20`(1398) | `0x00606330`-`0x0060e0b0` | 32128 | 141 | 16 | 79 | 1146 | 39 |
| Mission10 | `"mission10"` (`87e6ec0`) | `0x00612ac0`(2207) | `0x00613360`(661) | `0x0060e0b0`-`0x00613600` | 21840 | 92 | 18 | 46 | 797 | 48 |
| Mission11 | `"mission11"` (`87e6ec1`) | `0x0061f690`(2990) | `0x00620240`(542) | `0x00613600`-`0x00620460` | 52832 | 169 | 24 | 108 | 2085 | 42 |
| Mission12 | `"mission12"` (`87e6ec2`) | `0x00623690`(2034) | `0x00623e90`(631) | `0x00620460`-`0x00624110` | 15536 | 73 | 14 | 45 | 554 | 36 |
| Mission13 | `"mission13"` (`87e6ec3`) | `0x00629a90`(3171) | `0x0062a720`(2235) | `0x00624110`-`0x0062aff0` | 28384 | 125 | 24 | 81 | 990 | 32 |
| Mission14 | `"mission14"` (`87e6ec4`) | `0x00636d30`(2979) | `0x006378e0`(677) | `0x0062aff0`-`0x00637b90` | 52128 | 173 | 27 | 107 | 1977 | 53 |
| Mission15 | `"mission15"` (`87e6ec5`) | `0x00639ce0`(1215) | `0x0063a1a0`(3121) | `0x00637b90`-`0x0063adf0` | 12896 | 31 | 6 | 18 | 493 | 31 |
| Mission16 | `"mission16"` (`87e6ec6`) | `0x00647c90`(2194) | `0x00648530`(651) | `0x0063adf0`-`0x006487c0` | 55760 | 162 | 24 | 110 | 2214 | 46 |
| Mission17 | `"mission17"` (`87e6ec7`) | `0x00666510`(10741) | `0x00668f20`(1540) | `0x006487c0`-`0x00669530` | 134512 | 383 | 48 | 246 | 5117 | 85 |
| Tutorial1 | `"tutorial1"` (`9468c71`) | `0x006704e0`(946) | `0x006708a0`(368) | `0x00669530`-`0x00670a10` | 29920 | 59 | 4 | 35 | 1310 | 28 |

Each script's FSM and state functions come first in its block, and its mission-level `Init_`/`Watch_` pair comes last. This
matches kas2c's emission order: FSMs, then the trailing `INIT`/`WATCH`. The order of the blocks is the mission order, Tutorial1 last.

### 7.1 Shared (ICF-folded) functions

91 functions are reached from more than one context, 46 of them from more than one mission. In the per-mission tables they are marked
‡. The two most common have short names: **E1** = `0x0062ca80`, the empty FSM-scope body (only the prologue). **E2** = `0x00652ef0`, the
empty state-scope body. The "home" kas2c name is the context inside the block where the code lives.

| Address | Size | Scope | Contexts | Missions | Home kas2c name | Body |
|---|---:|---|---:|---:|---|---|
| `0x00652ef0` | 25 | state | 483 | 18 | `Watch_Mission17_HeavyCruisers_Null` | (prologue only) |
| `0x0065f6f0` | 40 | state | 165 | 13 | `Init_Mission17_tCarriers_Build` | VarValueSet |
| `0x0062ca80` | 25 | FSM | 126 | 18 | `Watch_Mission14_BeastMothership` | (prologue only) |
| `0x0061eb40` | 40 | state | 74 | 6 | `Init_Mission11_DFGs_Building` | VarValueSet |
| `0x005f4730` | 29 | state | 26 | 17 | `Init_Mission06_FleetIntel_WaitIntel` | IntelEventNotEnded(tail) |
| `0x0065b260` | 40 | state | 18 | 5 | `Init_Mission17_kHeavyCruisers_Hyperspace` | VarCreateSet |
| `0x00613940` | 40 | state | 17 | 17 | `Init_Mission11_FleetIntel_WaitMessage` | VarValueSet |
| `0x0060c010` | 45 | state | 13 | 5 | `Init_Mission09_AttackSupport_Null` | Stop, VarValueSet |
| `0x00608010` | 29 | state | 11 | 10 | `Init_Mission09_FighterPatrol_Launch` | Launch(tail) |
| `0x00617a10` | 29 | state | 9 | 6 | `Init_Mission11_Harvest_Harvest` | Harvest(tail) |
| `0x006151c0` | 42 | state | 8 | 4 | `Init_Mission11_BEASTCarrier_InHS` | TeamAttributesBitSet, TeamHyperspaceOut(tail) |
| `0x005db0b0` | 53 | state | 7 | 7 | `Init_Mission01_FleetIntel_Fail` | kasGrowSelectionPtr, ShipsCount, MissionFailed(tail) |
| `0x00615ad0` | 49 | state | 7 | 7 | `Init_Mission11_Infected_Attack` | TacticsAggressive, kasGrowSelectionPtr, Attack |
| `0x006063f0` | 81 | state | 6 | 6 | `Watch_Mission09_FleetIntel_Failed` | IntelEventEnded, kasAITeamPtr, MsgSend, kasJump |
| `0x00613880` | 71 | state | 6 | 6 | `Init_Mission11_FleetIntel_WaitTimer` | VarValueGet, TimerSet, TimerStart, VarValueSet |
| `0x0061a930` | 354 | state | 6 | 4 | `Init_Mission11_MultibeamFrigates_Attack` | kasThisTeamsVectorPtr, kasGrowSelectionPtr, kasGrowSelectionPtr, ShipsSelectNearby, kasGrowSelectionPtr, kasGrowSelectionPtr ... |
| `0x005eea30` | 163 | state | 5 | 5 | `Init_Mission05_Harvest_Create` | TimerDestroy, VarValueGet, TimerCreateSetStart, GetBuildTime, VarValueGet, TimerCreateSetStart ... |
| `0x005eff10` | 163 | state | 5 | 5 | `Init_Mission05_IonArrayFrigates_Create` | TimerDestroy, VarValueGet, TimerCreateSetStart, GetBuildTime, VarValueGet, TimerCreateSetStart ... |
| `0x005f0df0` | 163 | state | 5 | 5 | `Init_Mission05_Corvettes_Create` | TimerDestroy, VarValueGet, TimerCreateSetStart, GetBuildTime, VarValueGet, TimerCreateSetStart ... |
| `0x005f2300` | 163 | state | 5 | 5 | `Init_Mission05_Fighters_Create` | TimerDestroy, VarValueGet, TimerCreateSetStart, GetBuildTime, VarValueGet, TimerCreateSetStart ... |
| `0x005f3470` | 163 | state | 5 | 5 | `Init_Mission05_RammingFrigates_Create` | TimerDestroy, VarValueGet, TimerCreateSetStart, GetBuildTime, VarValueGet, TimerCreateSetStart ... |
| `0x006229d0` | 80 | FSM | 5 | 1 | `Watch_Mission12_PirateSalvage` | MsgReceived, DeleteShips, kasJump |
| `0x00631dd0` | 195 | state | 5 | 5 | `Init_Mission14_IonArrayFrigates_Move` | kasGrowSelectionPtr, kasGrowSelectionPtr, ShipsSelectCapital, kasThisTeamsVectorPtr, kasGrowSelectionPtr, ShipsSort ... |
| `0x0063cc00` | 91 | FSM | 5 | 5 | `Watch_Mission16_Infected` | kasGrowSelectionPtr, kasGrowSelectionPtr, ShipsSelectBeastified, kasGrowSelectionPtr, ReinforceTeamWithShips |
| `0x00658c90` | 179 | state | 5 | 2 | `Init_Mission17_Recons_Dock` | VarValueSet, VarValueSet, TimerDestroy, kasThisTeamsVectorPtr, kasGrowSelectionPtr, ShipsSort ... |
| `0x005e3410` | 29 | state | 4 | 4 | `Init_Mission03_PatrolPirates_Stop` | Stop(tail) |
| `0x00616130` | 187 | state | 4 | 4 | `Init_Mission11_Sentinels_Create` | VarValueGet, Log, TimerDestroy, VarValueGet, TimerCreateSetStart, GetBuildTime ... |
| `0x006199d0` | 200 | state | 4 | 4 | `Init_Mission11_CloakedFighters_Move` | kasGrowSelectionPtr, kasGrowSelectionPtr, ShipsSelectType, kasThisTeamsVectorPtr, kasGrowSelectionPtr, ShipsSort ... |
| `0x0063ece0` | 261 | state | 4 | 4 | `Init_Mission16_Sentinels_Wait` | kasGrowSelectionPtr, kasGrowSelectionPtr, ShipsSelectCapital, kasGrowSelectionPtr, kasGrowSelectionPtr, ShipsSelectNotIdle ... |
| `0x00645bb0` | 410 | state | 4 | 4 | `Init_Mission16_RammingFrigates_Attack` | kasGrowSelectionPtr, kasGrowSelectionPtr, ShipsAdd, kasGrowSelectionPtr, kasGrowSelectionPtr, ShipsSelectType ... |
| `0x00646540` | 179 | state | 4 | 1 | `Init_Mission16_DFGs_Dock` | VarValueSet, VarValueSet, TimerDestroy, kasThisTeamsVectorPtr, kasGrowSelectionPtr, ShipsSort ... |
| `0x0064e740` | 163 | state | 4 | 4 | `Init_Mission17_HiveFrigates_Create` | TimerDestroy, VarValueGet, TimerCreateSetStart, GetBuildTime, VarValueGet, TimerCreateSetStart ... |
| `0x0064e980` | 352 | state | 4 | 4 | `Init_Mission17_HiveFrigates_Attack` | kasThisTeamsVectorPtr, kasGrowSelectionPtr, kasGrowSelectionPtr, ShipsSelectNearby, VarCreateSet, kasGrowSelectionPtr ... |
| `0x00651d10` | 163 | state | 4 | 4 | `Init_Mission17_CloakedFighters_Create` | TimerDestroy, VarValueGet, TimerCreateSetStart, GetBuildTime, VarValueGet, TimerCreateSetStart ... |
| `0x00652c50` | 163 | state | 4 | 4 | `Init_Mission17_MultibeamFrigates_Create` | TimerDestroy, VarValueGet, TimerCreateSetStart, GetBuildTime, VarValueGet, TimerCreateSetStart ... |
| `0x00659f50` | 163 | state | 4 | 3 | `Init_Mission17_DFGs_Create` | TimerDestroy, VarValueGet, TimerCreateSetStart, GetBuildTime, VarValueGet, TimerCreateSetStart ... |
| `0x0065de70` | 241 | state | 4 | 1 | `Init_Mission17_kIonCannonFrigates_Attack` | kasThisTeamsVectorPtr, kasGrowSelectionPtr, kasGrowSelectionPtr, ShipsSelectNearby, kasGrowSelectionPtr, kasGrowSelectionPtr ... |
| `0x005de270` | 111 | state | 3 | 1 | `Init_Mission02_Harass_Kamikaze` | HasShipBeenDiscovered, ShipsScuttle, kasAITeamPtr, ReinforceTeamWithShips, kasJump |
| `0x005e4510` | 40 | state | 3 | 1 | `Init_Mission03_BigGuard_Null` | VarCreateSet |
| `0x005ef2c0` | 163 | state | 3 | 3 | `Init_Mission05_Workers_Create` | TimerDestroy, VarValueGet, TimerCreateSetStart, GetBuildTime, VarValueGet, TimerCreateSetStart ... |
| `0x005fecb0` | 102 | FSM | 3 | 1 | `Watch_Mission07_Wave` | cat_aitMsgQueueRemoveAt, aitMsgSend |
| `0x0060bc70` | 45 | state | 3 | 1 | `Init_Mission09_AttackSupport_Hyperspace` | FormationWall, TimerCreateSetStart |
| `0x0060bd20` | 44 | state | 3 | 1 | `Init_Mission09_AttackSupport_Hold` | TimerCreateSetStart, FormationWall(tail) |
| `0x006104f0` | 44 | state | 3 | 2 | `Watch_Mission10_DefendBigGun_Attack` | kasGrowSelectionPtr, Attack |
| `0x0061ef20` | 179 | state | 3 | 1 | `Init_Mission11_DFGs_Dock` | VarValueSet, VarValueSet, TimerDestroy, kasThisTeamsVectorPtr, kasAITeamShipsPtr, ShipsSort ... |
| `0x0062ce40` | 40 | state | 3 | 1 | `Init_Mission14_BeastMothership_Null` | TimerCreateSetStart |
| `0x0062ce70` | 58 | state | 3 | 1 | `Watch_Mission14_FSM_x3a6d090_Null` | TimerExpiredDestroy, DeleteShips |
| `0x0063d2b0` | 163 | state | 3 | 3 | `Init_Mission16_ACVs_Create` | TimerDestroy, VarValueGet, TimerCreateSetStart, GetBuildTime, VarValueGet, TimerCreateSetStart ... |
| `0x0064d110` | 63 | state | 3 | 3 | `Init_Mission17_ACVs_Wait` | Stop, FormationDelta3D, TacticsAggressive, Random, TimerCreateSetStart |
| `0x00658650` | 163 | state | 3 | 3 | `Init_Mission17_Recons_Create` | TimerDestroy, VarValueGet, TimerCreateSetStart, GetBuildTime, VarValueGet, TimerCreateSetStart ... |
| `0x0065a190` | 493 | state | 3 | 3 | `Init_Mission17_DFGs_Guard` | kasGrowSelectionPtr, kasGrowSelectionPtr, ShipsSelectCapital, kasGrowSelectionPtr, kasGrowSelectionPtr, ShipsSelectType ... |
| `0x006610b0` | 598 | state | 3 | 1 | `Init_Mission17_tStandardDestroyers_Attack` | kasGrowSelectionPtr, GrowSelectionClear, kasGrowSelectionPtr, kasGrowSelectionPtr, ShipsAdd, kasGrowSelectionPtr ... |
| `0x005d85c0` | 35 | FSM | 2 | 1 | `Init_Mission01_TaiidanCarrier` | ShipsUpgrade |
| `0x005d9310` | 98 | state | 2 | 1 | `Init_Mission01_TaiidanBattle_Create` | Stop, kasGrowSelectionPtr, GrowSelectionClear, kasAITeamShipsPtr, TeamCountOriginal, kasGrowSelectionPtr ... |
| `0x005ddc60` | 56 | state | 2 | 1 | `Init_Mission02_Strike_Dock` | VarValueSet, kasAITeamPtr, DockStay |
| `0x005e0290` | 74 | FSM | 2 | 2 | `Watch_Mission02_Decoy` | VarValueGet, TeamCount, kasJump |
| `0x005e3ff0` | 84 | FSM | 2 | 1 | `Watch_Mission03_BigIon` | VarValueGet, TeamCount, VarValueInc, kasJump |
| `0x005e7d90` | 57 | FSM | 2 | 1 | `Init_Mission04_BeastGuard` | VarCreateSet, ShipsSetColor |
| `0x005f1790` | 179 | state | 2 | 1 | `Init_Mission05_Corvettes_Dock` | VarValueSet, VarValueSet, TimerDestroy, kasThisTeamsVectorPtr, kasGrowSelectionPtr, ShipsSort ... |
| `0x005f38f0` | 40 | state | 2 | 2 | `Init_Mission05_Beast_Wait` | TimerCreateSetStart |
| `0x005fd110` | 83 | state | 2 | 1 | `Init_Mission07_Convoy_S_x20220d2` | VarValueGet, TeamSwitchPlayerOwner, VarValueSet, kasShipsVectorPtr, MoveTo |
| `0x005fd250` | 29 | state | 2 | 2 | `Init_Mission07_Convoy_S_x4498602` | StopInstant(tail) |
| `0x00603f60` | 44 | state | 2 | 1 | `Init_Mission08_Spy_Wait` | kasGrowSelectionPtr, Intercept |
| `0x00609950` | 88 | state | 2 | 1 | `Init_Mission09_GuardWing_Build` | Stop, VarValueSet, Random, TimerCreateSetStart, TimerDestroy, TimerCreate |
| `0x0060b670` | 78 | state | 2 | 1 | `Init_Mission09_AttackSpecial_Build` | Stop, VarValueSet, TimerDestroy, Random, TimerCreateSetStart |
| `0x0060fd90` | 35 | state | 2 | 2 | `Init_Mission10_Salvage_Hidden` | HideShips |
| `0x00622420` | 40 | state | 2 | 1 | `Init_Mission12_FSM_xa1082c2_WaitCreate` | TimerCreateSetStart |
| `0x00624250` | 40 | state | 2 | 1 | `Init_Mission13_FleetIntel_S_xb663893` | VarCreateSet |
| `0x00624aa0` | 81 | state | 2 | 2 | `Watch_Mission13_FleetIntel_Failed` | IntelEventEnded, kasAITeamPtr, MsgSend, kasJump |
| `0x006255d0` | 80 | state | 2 | 1 | `Watch_Mission13_Carrier_HyperspaceWait` | TimerExpiredDestroy, DeleteShips, kasJump |
| `0x00626770` | 40 | state | 2 | 1 | `Init_Mission13_Resourcers_HyperspaceWait` | TimerCreateSetStart |
| `0x006272b0` | 440 | state | 2 | 1 | `Init_Mission13_FSM_x4d56e8e_Attack` | kasGrowSelectionPtr, GrowSelectionClear, kasGrowSelectionPtr, ShipsCount, VarValueGet, kasGrowSelectionPtr ... |
| `0x00628380` | 75 | state | 2 | 1 | `Init_Mission13_FSM_xd7b5b43_Launch` | TeamSwitchPlayerOwner, ShipsUpgrade, kasAITeamShipsPtr, ShipsLaunch |
| `0x006289c0` | 57 | state | 2 | 1 | `Watch_Mission13_FSM_x5b05dd3_Launch` | TeamFinishedLaunching, kasJump |
| `0x006292d0` | 503 | state | 2 | 1 | `Init_Mission13_AssultWave2_Attack` | kasGrowSelectionPtr, GrowSelectionClear, kasGrowSelectionPtr, ShipsCount, VarValueGet, kasGrowSelectionPtr ... |
| `0x0062e410` | 126 | state | 2 | 1 | `Init_Mission14_Patrol_AttackPlayer` | TacticsAggressive, kasGrowSelectionPtr, GrowSelectionClear, kasGrowSelectionPtr, kasGrowSelectionPtr, ShipsAdd ... |
| `0x0062ecc0` | 137 | state | 2 | 1 | `Watch_Mission14_LastWave_Post` | IsIdle, kasGrowSelectionPtr, GrowSelectionClear, kasGrowSelectionPtr, kasGrowSelectionPtr, ShipsAdd ... |
| `0x0062fb10` | 159 | state | 2 | 1 | `Init_Mission14_ACVs_Dock` | VarValueSet, VarValueSet, TimerDestroy, kasThisTeamsVectorPtr, kasAITeamShipsPtr, ShipsSort ... |
| `0x00635c40` | 179 | state | 2 | 1 | `Init_Mission14_Fighters_Dock` | VarValueSet, VarValueSet, TimerDestroy, kasThisTeamsVectorPtr, kasAITeamShipsPtr, ShipsSort ... |
| `0x006413e0` | 159 | state | 2 | 1 | `Init_Mission16_CloakedFighters_Dock` | VarValueSet, VarValueSet, TimerDestroy, kasThisTeamsVectorPtr, kasGrowSelectionPtr, ShipsSort ... |
| `0x0064af10` | 39 | state | 2 | 1 | `Init_Mission17_Naggarok_DQT` | EnableNaggarok |
| `0x00652270` | 159 | state | 2 | 1 | `Init_Mission17_CloakedFighters_Dock` | VarValueSet, VarValueSet, TimerDestroy, kasThisTeamsVectorPtr, kasGrowSelectionPtr, ShipsSort ... |
| `0x006539a0` | 163 | state | 2 | 2 | `Init_Mission17_HeavyCruisers_Create` | TimerDestroy, VarValueGet, TimerCreateSetStart, GetBuildTime, VarValueGet, TimerCreateSetStart ... |
| `0x0065a3e0` | 179 | state | 2 | 1 | `Init_Mission17_DFGs_Dock` | VarValueSet, VarValueSet, TimerDestroy, kasThisTeamsVectorPtr, kasAITeamShipsPtr, ShipsSort ... |
| `0x0065b100` | 76 | state | 2 | 1 | `Init_Mission17_kHeavyCruisers_Create` | TimerDestroy, GetBuildTime, VarValueGet, TimerCreateSetStart |
| `0x0065c2f0` | 211 | state | 2 | 1 | `Init_Mission17_kFighters_Attack` | kasThisTeamsVectorPtr, kasGrowSelectionPtr, kasGrowSelectionPtr, ShipsSelectNearby, kasGrowSelectionPtr, kasGrowSelectionPtr ... |
| `0x0065d470` | 76 | state | 2 | 1 | `Init_Mission17_kStandardDestroyers_Create` | TimerDestroy, GetBuildTime, VarValueGet, TimerCreateSetStart |
| `0x0065db80` | 76 | state | 2 | 1 | `Init_Mission17_kIonCannonFrigates_Create` | TimerDestroy, GetBuildTime, VarValueGet, TimerCreateSetStart |
| `0x0065e3b0` | 76 | state | 2 | 1 | `Init_Mission17_kCorvettes_Create` | TimerDestroy, GetBuildTime, VarValueGet, TimerCreateSetStart |
| `0x0065f890` | 76 | state | 2 | 1 | `Init_Mission17_tCarriers_Create` | TimerDestroy, GetBuildTime, VarValueGet, TimerCreateSetStart |
| `0x006603d0` | 76 | state | 2 | 1 | `Init_Mission17_tAdvancedSupportFrigates_Create` | TimerDestroy, GetBuildTime, VarValueGet, TimerCreateSetStart |

## 8. Per-mission FSM and state maps

Format: `addr`(size). ‡ = shared function (section 7.1). E1/E2 = empty bodies. Each state shows Init then Watch. kas2c names follow
HW1: `Init_<Script>_<FSM>`, `Watch_<Script>_<FSM>`, `Init_<Script>_<FSM>_<State>`, `Watch_<Script>_<FSM>_<State>`. Mission level is
`Init_<Script>`/`Watch_<Script>`. Name marks are explained in section 6.

### Mission01 ("Taidan Contras Mount Attack On Higaran Space")

`Init_Mission01` `0x005db0f0`(86), `Watch_Mission01` `0x005db150`(325); block `0x005d6fe0`-`0x005db2a0`, 17088 bytes, 85 functions.

| FSM (hash) | Team label(s) | Init | Watch | States: name (hash) Init / Watch |
|---|---|---|---|---|
| Objectives (`98ddc43`) | Objectives | `5d6fe0`(112) | `5d7050`(2173) | - |
| KushanFlankFrigates (`d788163`) | KushanFlankFrigates | `5d78d0`(140) | E1 | UnderAttack (`633addb`) `5d7960`(44) / `5d7990`(114); AttackCarrier* (`1ff3a12`) `5d7a10`(44) / `5d7a40`(95); DoBattle* (`3669d95`) `5d7aa0`(219) / E2; Null (`0053a0c`) E2 / E2 |
| KushanProximityArray (`4e6b1d9`) | KushanProximityArray | `5d7b80`(90) | `5d7be0`(61) | - |
| KushanProxDefend† (`0eff514`) | KushanProxDefend1, KushanProxDefend2 | `5d7c20`(102) | E1 | Patrolling (`7464cb7`) `5d7c90`(44) / `5d7cc0`(65); Attacking (`857bcb7`) `5d7d10`(44) / `5d7d40`(71) |
| KushanCarriers (`c0abd03`) | KushanCarriers | `5d7d90`(93) | E1 | - |
| TaiidanBombers (`4fcebd3`) | TaiidanBombers | `5d7df0`(57) | E1 | AttackFlank (`7bd9e7b`) `5d7e30`(73) / `5d7e80`(212); AttackMothership (`9dbf1e0`) `5d7f60`(44) / E2 |
| TaiidanCarrierDefense (`3346335`) | TaiidanCarrierDefense | `5d7f90`(53) | E1 | - |
| TaiidanResourcers (`cbd4dc3`) | TaiidanResourcers | `5d7fd0`(53) | `5d8010`(215) | - |
| TaiidanResourcerDefense (`427eea5`) | TaiidanResourcerDefense | `5d80f0`(53) | E1 | - |
| TaiidanDestroyer (`518e0d2`) | TaiidanDestroyer | `5d8130`(74) | E1 | PreFlee* (`569b0c5`) `5d8180`(160) / `5d8220`(179); Flee* (`004b095`) `5d82e0`(88) / `5d8340`(43) |
| TaiidanPunishmentFrigate† (`dec6a65`) | TaiidanPunishmentFrigate1, TaiidanPunishmentFrigate2, TaiidanPunishmentFrigate3 | `5d8370`(57) | E1 | Create (`4869685`) `5d84a0`(98) / `5d8510`(103); NotThere* (`498cf55`) E2 / `5d83b0`(70); Attacking (`857bcb7`) E2 / `5d8400`(153); Launch (`506a278`) E2 / `5d8580`(57) |
| TaiidanCarrier (`2baec02`) | TaiidanCarrier | `5d85c0`‡(35) | E1 | - |
| TaiidanCarrier2 (`baec012`) | TaiidanCarrier2 | `5d85c0`‡(35) | E1 | - |
| FSM_x5a79103? (`5a79103`) | TaiidanC2Interceptor1, TaiidanC2Interceptor2, TaiidanC2MGCorv1 | `5d85f0`(57) | E1 | PreHS* (`05569d3`) `5d8630`(44) / `5d8660`(81); Attacking (`857bcb7`) `5d8730`(29) / `5d8750`(148); Create (`4869685`) `5d87f0`(140) / `5d8880`(103); InHS* (`004e2d3`) E2 / `5d86c0`(103); Launch (`506a278`) E2 / `5d88f0`(57) |
| TaiidanProxAttack† (`47289eb`) | TaiidanHInt2, TaiidanProxAttackAbomb1, TaiidanProxAttackHInt1, TaiidanProxAttackMGCorv1 | `5d8930`(72) | E1 | AttackProx* (`58440c8`) `5d89d0`(139) / `5d8a60`(363); Nothing (`348ce77`) E2 / `5d8980`(65) |
| FSM_x2cce515? (`2cce515`) | KushanMainGuardCorvettes, KushanMainGuardDestroyers, KushanMainGuardFighters1, KushanMainGuardFrigates | `5d8bd0`(96) | E1 | Attacking (`857bcb7`) `5d8c30`(228) / `5d8d20`(344); Create (`4869685`) `5d8e80`(98) / `5d8ef0`(103); Launch (`506a278`) E2 / `5d8f60`(57); Null (`0053a0c`) E2 / E2 |
| TaiidanBattle* (`a3b4115`) | TaiHInt1, TaiICFrig1, TaiidanDefendCruiser, TaiidanMissileDestroyers | `5d8fa0`(121) | E1 | Docked (`4937f94`) `5d9020`(44) / `5d9050`(117); Attacking (`857bcb7`) `5d90d0`(162) / `5d9180`(278); GuardingCruiser* (`e114982`) `5d92a0`(35) / `5d92d0`(53); Create (`4869685`) `5d9310`‡(98) / `5d9380`(103); Launch (`506a278`) E2 / `5d93f0`(57); Null (`0053a0c`) E2 / E2 |
| Pester† (`54a88a2`) | PesterHInt1 | `5d9430`(85) | E1 | Create (`4869685`) `5d9310`‡(98) / `5d9800`(103); Attacking (`857bcb7`) `5d94e0`(274) / `5d9600`(376); WaitCreate* (`8832365`) `5d9780`(40) / `5d97b0`(65); S_xc2ee6b2? (`c2ee6b2`) E2 / `5d9490`(70); Launch (`506a278`) E2 / `5d9870`(116); Null (`0053a0c`) E2 / E2 |
| TaiidanReinforcement* (`539b6c4`) | TaiidanReinforcement1 | `5d98f0`(57) | E1 | DoBattle* (`3669d95`) `5d9980`(153) / E2; SitThere* (`e98cf15`) E2 / `5d9930`(65) |
| FleetIntel (`a8a880c`) | FleetIntel | `5d9a20`(201) | E1 | NISLet1* (`2e80a01`) `5d9af0`(40) / `5d9b20`(1333); Fail (`004a5dc`) `5db0b0`‡(53) / E2; WaitIntel* (`e8e63cc`) `5f4730`‡(29) / `5da060`(431); WaitMessage* (`1f32b25`) `613940`‡(40) / `5da2c0`(3553); WaitTimer* (`e989af2`) E2 / `5da210`(169) |

kasf: 636 call sites, 103 distinct. Cataclysm-only kasf used: kasfCameraFocus, kasfCameraSetAngleDeg, kasfCameraSetDeclinationDeg, kasfCameraSetDistance, kasfDeleteShipsWhenMissionEnds, kasfEnableShipConstruction, kasfFirstShip, kasfGetPlayerMothership, kasfHasShipBeenDiscovered, kasfIsIdle, kasfSetSensorsLookAtPoint, kasfShipsCreateLaunch, kasfShipsSelectPlayer, kasfShipsSelectRace, kasfShipsSetColor, kasfShipsSetInvincible, kasfShipsSetRepairFactor, kasfShipsSort, kasfShipsUpgrade, kasfStartLevelByHyperspacingShips, kasfTutSetObjPointerTargetAIVolume, kasfTutSetObjPointerTargetShipSelection, kasfWideScreenInDontCloseSensors.

### Mission02 ("Rescue Kith Destroyer And Find Pod")

`Init_Mission02` `0x005e05a0`(1532), `Watch_Mission02` `0x005e0bb0`(1446); block `0x005db2a0`-`0x005e1160`, 24256 bytes, 100 functions.

| FSM (hash) | Team label(s) | Init | Watch | States: name (hash) Init / Watch |
|---|---|---|---|---|
| Patrols (`4697443`) | pPatrol0, pPatrol1, pPatrol2, pPatrol3, pPatrol4, pPatrol5 | `5db2a0`(285) | `5db3c0`(178) | GetPatrol* (`946de9c`) `5db480`(231) / E2; Attack (`469857b`) `5db5d0`(281) / `5db6f0`(117); Patrol (`546973c`) E2 / `5db590`(64); Null (`0053a0c`) `65f6f0`‡(40) / E2 |
| FleetIntel (`a8a880c`) | FleetIntel | `5db770`(189) | E1 | WaitIntel* (`e8e63cc`) `5f4730`‡(29) / `5db830`(569); Failed (`4a5e094`) `5f4730`‡(29) / `624aa0`‡(81); WaitMessage* (`1f32b25`) `613940`‡(40) / `5dbac0`(4359); Wait (`005b5e4`) E2 / `5dba70`(65); Null (`0053a0c`) E2 / E2 |
| Carrier (`7676de2`) | pCarrier | `5dd040`(85) | `5dd0a0`(259) | Scuttle (`78a9955`) `5dd250`(40) / `5dd280`(80); Hyperspace (`a7c9b35`) E2 / `5dd1b0`(150); Null (`0053a0c`) E2 / `5dd2d0`(147) |
| CarrierGuard* (`e1ae0b4`) | pGuard | `5dd370`(83) | `5dd3d0`(196) | Guard (`04c9664`) `5dd570`(49) / `5dd5b0`(64); Kamikaze (`61df215`) `5de270`‡(111) / E2; Build (`0479e04`) `65f6f0`‡(40) / `5dd4a0`(194); Null (`0053a0c`) `65f6f0`‡(40) / E2 |
| Strike (`5896df5`) | pFighter1, pFighter2 | `5dd5f0`(123) | `5dd670`(178) | Wait (`005b5e4`) `5dd790`(107) / `5dd800`(105); Attack (`469857b`) `5dd870`(496) / `5dda60`(105); Build (`0479e04`) `5ddad0`(60) / `5ddb10`(330); Dock (`004937b`) `5ddc60`‡(56) / `5ddca0`(57); Kamikaze (`61df215`) `5de270`‡(111) / E2; Launch (`506a278`) `608010`‡(29) / `5ddce0`(69); Hyperspace (`a7c9b35`) E2 / `5dd730`(93); Null (`0053a0c`) `65f6f0`‡(40) / E2 |
| Harass (`4c66683`) | pFighter3 | `5ddd30`(123) | `5dddb0`(219) | Dock (`004937b`) `5ddc60`‡(56) / `5de1e0`(57); Attack (`469857b`) `5ddef0`(250) / `5ddff0`(91); Build (`0479e04`) `5de050`(60) / `5de090`(330); Kamikaze (`61df215`) `5de270`‡(111) / E2; Launch (`506a278`) `608010`‡(29) / `5de220`(69); Hyperspace (`a7c9b35`) E2 / `5dde90`(93); Null (`0053a0c`) `65f6f0`‡(40) / E2 |
| Salvage (`761a5e5`) | pSalvage | `5de2e0`(158) | `5de380`(185) | Wait (`005b5e4`) `5de4a0`(53) / `5de4e0`(77); Switch (`58be878`) `5de530`(110) / E2; Salvage (`761a5e5`) `5de5a0`(49) / `5de5e0`(102); Attack (`469857b`) `5de650`(439) / `5de810`(87); Build (`0479e04`) `5de870`(169) / `5de920`(312); Kamikaze (`61df215`) `5dea60`(57) / E2; Hyperspace (`a7c9b35`) E2 / `5de440`(93); Null (`0053a0c`) `65f6f0`‡(40) / E2 |
| Kamikaze (`61df215`) | Kamikaze | `5deaa0`(63) | `5deae0`(74) | Attack (`469857b`) `5deb30`(104) / `5deba0`(120); Null (`0053a0c`) E2 / E2 |
| MessagePod* (`75ebfb4`) | dMessagePod | `5dec20`(170) | E1 | Wait (`005b5e4`) `5decd0`(40) / `5ded00`(141); Salvage (`761a5e5`) `5dee10`(40) / `5dee40`(183); Find (`004ae24`) E2 / `5ded90`(118); Null (`0053a0c`) E2 / E2 |
| Destroyer (`8970502`) | kDestroyer | `5def00`(446) | `5df0c0`(98) | Null (`0053a0c`) `5df3b0`(40) / `5df3e0`(58); Find (`004ae24`) E2 / `5df130`(179); Repair (`56a45e2`) E2 / `5df1f0`(295); Wait (`005b5e4`) E2 / `5df320`(131) |
| DestroyGates (`051e1d3`) | pTricky | `5df420`(137) | E1 | Ambush (`4617a78`) `5df560`(49) / `5df5a0`(237); Attack (`469857b`) `5df690`(289) / `5df7c0`(120); Hyperspace (`a7c9b35`) E2 / `5df4b0`(165); Null (`0053a0c`) E2 / E2 |
| Ambush (`4617a78`) | Ambush1, Ambush2, Ambush3 | `5df840`(181) | `5df900`(74) | Hyperspace (`a7c9b35`) `5dfe10`(40) / `5dfe40`(119); Attack (`469857b`) `5dfec0`(504) / `5e00c0`(91); Wait (`005b5e4`) `5e0120`(48) / `5e0150`(82); Null (`0053a0c`) `5e01b0`(112) / E2; Build (`0479e04`) `5e3410`‡(29) / `5df950`(1204) |
| Decoy (`0489849`) | Decoy | `5e0220`(103) | `5e0290`‡(74) | Intercept (`8a63464`) `5e0390`(49) / `5e03d0`(110); Attack (`469857b`) `5e0440`(250) / `5e0540`(91); Hyperspace (`a7c9b35`) E2 / `5e02e0`(163); Null (`0053a0c`) `65f6f0`‡(40) / E2 |
| Objectives (`98ddc43`) | Objectives | E1 | `5dcbd0`(817) | - |
| Pirates (`4e668f3`) | Pirates | E1 | `5dcf10`(295) | - |

kasf: 947 call sites, 140 distinct. Cataclysm-only kasf used: kasfCameraDistance, kasfCameraFocusClose, kasfCameraSetAngleDeg, kasfCameraSetDeclinationDeg, kasfCameraSetDistance, kasfDeleteShipsWhenMissionEnds, kasfEnableShipConstruction, kasfFindMeteors, kasfFindSlipgate, kasfGetPlayerMothership, kasfGetShipCountSalvaged, kasfGiveTechToAI, kasfHasShipBeenDiscovered, kasfIsGuarding, kasfIsIdle, kasfLockoutMainRegion, kasfMimic, kasfSalvage, kasfSetAIDepotShip, kasfSetCurrentMimicItem, kasfSetMCVItemIndex, kasfSetSensorsLookAtPoint, kasfShipUnderConstruction, kasfShipsCreateLaunch, kasfShipsInsideMe, kasfShipsScuttle, kasfShipsSelectClamped, kasfShipsSelectEnemyNoStealthy, kasfShipsSelectPlayer, kasfShipsSelectShips, kasfShipsSetColor, kasfShipsSetHelpless, kasfShipsSetInvincible, kasfShipsSetRepairFactor, kasfShipsSort, kasfShipsUnderAttackElsewhere, kasfShipsUpgrade, kasfStartLevelByHyperspacingShips, kasfThisTeamIdle, kasfTutSetObjPointerTargetShipSelection, kasfUnmimic, kasfVectorAdd, kasfVectorNear, kasfVectorParametric, kasfWideScreenInDontCloseSensors.

### Mission03 ("Rescue Ecclesiam From Pirate Blockade")

`Init_Mission03` `0x005e4c60`(1109), `Watch_Mission03` `0x005e50d0`(1247); block `0x005e1160`-`0x005e55b0`, 17488 bytes, 51 functions.

| FSM (hash) | Team label(s) | Init | Watch | States: name (hash) Init / Watch |
|---|---|---|---|---|
| FleetIntel (`a8a880c`) | FleetIntel | `5e1160`(177) | E1 | WaitIntel* (`e8e63cc`) `5f4730`‡(29) / `5e1220`(464); Failed (`4a5e094`) `5f4730`‡(29) / `6063f0`‡(81); WaitMessage* (`1f32b25`) `613940`‡(40) / `5e1440`(3465); Wait (`005b5e4`) E2 / `5e13f0`(65); Null (`0053a0c`) E2 / E2 |
| PatrolPirates* (`08f1ce3`) | MissileCorvette1, MissileCorvette2, MissileCorvette3, MissileCorvette4, MissileCorvette5, MissileCorvette6, MissileCorvette7, MissileCorvette8, patrol1, patrol2, patrol3 | `5e2410`(71) | `5e2460`(482) | Patrol (`546973c`) `5e2650`(497) / `5e2850`(1850); Attack (`469857b`) `5e2f90`(50) / `5e2fd0`(1079); Stop (`0058940`) `5e3410`‡(29) / `5e3430`(65); Null (`0053a0c`) `5e3480`(44) / E2 |
| Cleesan (`809a71e`) | Cleesan | `5e34b0`(98) | `5e3520`(300) | IonsSame* (`43872f5`) `5e3650`(66) / `5e36a0`(121); Hold (`004d404`) `5e38a0`(145) / `5e3940`(836); Go (`00004bf`) `5e3c90`(54) / `5e3cd0`(212); Null (`0053a0c`) `5e3db0`(52) / E2; Wait (`005b5e4`) E2 / `5e3720`(357) |
| Big (`00046d7`) | Big | `5e3df0`(63) | `5e3e30`(307) | Null (`0053a0c`) `5e4510`‡(40) / E2 |
| BigIon (`46dbe3e`) | BigIon | `5e3f70`(118) | `5e3ff0`‡(84) | Attack (`469857b`) `5e40b0`(104) / `5e4120`(97); Null (`0053a0c`) `5e4510`‡(40) / E2; Hyperspace (`a7c9b35`) E2 / `5e4050`(93) |
| BigGuard* (`dbc9204`) | BigGuard0, BigGuard1, BigGuard2 | `5e4190`(118) | `5e3ff0`‡(84) | Guard (`04c9664`) `5e4270`(185) / `5e4330`(163); Attack (`469857b`) `5e43e0`(179) / `5e44a0`(105); Null (`0053a0c`) `5e4510`‡(40) / E2; Hyperspace (`a7c9b35`) E2 / `5e4210`(93) |
| Minelayer (`2a03c42`) | Minelayer | `5e4540`(165) | `5e45f0`(178) | Mines (`051e2a3`) `5e4750`(143) / `5e47e0`(220); Wait (`005b5e4`) `5e48c0`(160) / `5e4960`(187); Sleep (`05809a0`) `5e4a20`(40) / `5e4a50`(65); Attack (`469857b`) `5e4aa0`(306) / `5e4be0`(117); Hyperspace (`a7c9b35`) E2 / `5e46b0`(150); Null (`0053a0c`) `65f6f0`‡(40) / E2 |
| Objectives (`98ddc43`) | Objectives | E1 | `5e21d0`(567) | - |

kasf: 685 call sites, 114 distinct. Cataclysm-only kasf used: kasfCameraDistance, kasfCameraFocusClose, kasfCameraSetAngleDeg, kasfCameraSetDeclinationDeg, kasfCameraSetDistance, kasfEnableShipConstruction, kasfFindEnemyShipsInsideWithoutStealthy, kasfFindMinesInside, kasfFindMinesNearTeam, kasfGetPlayerMothership, kasfHasShipBeenDiscovered, kasfIsIdle, kasfPingMultipleAddShips, kasfSetCurrentMimicItem, kasfSetMCVItemIndex, kasfSetSensorsLookAtPoint, kasfShipUnderConstruction, kasfShipsCreateLaunch, kasfShipsEffectedByMines, kasfShipsInsideMe, kasfShipsScuttle, kasfShipsSelectEnemyNoStealthy, kasfShipsSelectNumber, kasfShipsSelectPlayer, kasfShipsSetColor, kasfShipsSetInvincible, kasfShipsSort, kasfShipsStop, kasfShowMinesInSensors, kasfStartLevelByHyperspacingShips, kasfVectorAdd, kasfVectorNear, kasfVectorParametric, kasfWideScreenInDontCloseSensors.

### Mission04 ("(Empty)")

`Init_Mission04` `0x005ea340`(1548), `Watch_Mission04` `0x005ea950`(890); block `0x005e55b0`-`0x005eacd0`, 22304 bytes, 74 functions.

| FSM (hash) | Team label(s) | Init | Watch | States: name (hash) Init / Watch |
|---|---|---|---|---|
| FleetIntel (`a8a880c`) | FleetIntel | `5e55b0`(201) | `5e5680`(101) | WaitIntel* (`e8e63cc`) `5f4730`‡(29) / `5e56f0`(544); Failed (`4a5e094`) `5f4730`‡(29) / `6063f0`‡(81); WaitMessage* (`1f32b25`) `613940`‡(40) / `5e5960`(3103); Wait (`005b5e4`) E2 / `5e5910`(77); WaitBeastified* (`d65c144`) E2 / `5e6660`(70); Null (`0053a0c`) E2 / E2 |
| Ecclesiam (`80a342d`) | Ecclesiam | `5e68a0`(60) | `5e68e0`(78) | Move (`00524a5`) `5e6a00`(44) / `5e6a30`(489); NeedEscort* (`8a26de4`) `5e6c20`(40) / `5e6c50`(566); Hold (`004d404`) `5e6e90`(67) / `5e6ee0`(65); Wait (`005b5e4`) E2 / `5e6930`(202); Beastified* (`889c4f4`) E2 / `5e6f30`(126); Null (`0053a0c`) E2 / E2 |
| GeneticsModule (`e491135`) | GeneticsModule | `5e6fb0`(59) | `5e6ff0`(61) | Start (`0588674`) `5e7030`(75) / `5e7080`(1027); Beastify* (`96889d9`) E2 / `5e7490`(610) |
| Pirates (`4e668f3`) | pHeavyCruiser, pIonArray1, pIonArray2, pMissle | `5e7700`(81) | `5e7760`(433) | Move (`00524a5`) `5e7920`(248) / `5e7a20`(338); Null (`0053a0c`) E2 / E2 |
| BeastGuard (`888ff04`) | BeastGuard | `5e7d90`‡(57) | `5e7dd0`(146) | Build (`0479e04`) `5e7e70`(60) / `5e7eb0`(194); Guard (`04c9664`) `5e7f80`(79) / `5e7fd0`(153); Attack (`469857b`) `5e8070`(215) / `5e8150`(105) |
| BeastStrike* (`9cffb75`) | BeastStrike1, BeastStrike2, BeastStrike3, BeastStrike4, BeastStrike5, BeastStrike6 | `5e7d90`‡(57) | `5e81c0`(541) | Build (`0479e04`) `5e83e0`(63) / `5e8420`(284); Attack (`469857b`) `5e8560`(370) / `5e86e0`(91) |
| BeastFighter (`eb2b022`) | BeastFighter, BeastFighter1, BeastFighter2, BeastFighter3 | `5e8740`(52) | `5e8780`(612) | Attack (`469857b`) `5e89f0`(267) / `5e8b00`(91); Dock (`004937b`) `5e8b60`(99) / `5e8bd0`(95); Null (`0053a0c`) `5e8c80`(96) / E2; Launch (`506a278`) E2 / `5e8c30`(69) |
| BeastCorvette (`2ec21c5`) | BeastCorvette, BeastCorvette1, BeastCorvette2, BeastCorvette3, BeastCorvette4 | `5e8ce0`(70) | `5e8d30`(629) | Attack (`469857b`) `5e8fb0`(250) / `5e90b0`(91); Dock (`004937b`) `5e9110`(61) / `5e9150`(95); Launch (`506a278`) E2 / `5e91b0`(69); Null (`0053a0c`) `65f6f0`‡(40) / E2 |
| BeastFrigate (`f04de05`) | BeastFrigate, BeastFrigate1, BeastFrigate2, BeastFrigate3, BeastFrigate4, BeastFrigate5 | `5e9200`(70) | `5e9250`(845) | Attack (`469857b`) `5e95a0`(395) / `5e9730`(91); Null (`0053a0c`) `65f6f0`‡(40) / E2 |
| Infected (`2a98c74`) | Infected | `5e9790`(48) | `5e97c0`(255) | Attack (`469857b`) `615ad0`‡(49) / `5e98c0`(103); Null (`0053a0c`) E2 / `5e9930`(57) |
| Harvest (`c67aac4`) | Harvest, Harvest1, Harvest2, Harvest3 | `5e9970`(269) | `5e9a80`(182) | Create (`4869685`) `5eea30`‡(163) / `5e9ee0`(389); Harvest (`c67aac4`) `617a10`‡(29) / `5ea070`(213); Building (`9e08a57`) `61eb40`‡(40) / `5e9d20`(446); Build (`0479e04`) `65f6f0`‡(40) / `5e9b40`(474); Null (`0053a0c`) `65f6f0`‡(40) / E2 |
| Objectives (`98ddc43`) | Objectives | E1 | `5e66b0`(492) | - |
| Beast (`0469684`) | Beast | E1 | `5ea150`(483) | - |
| EcclesiamInfected (`89db174`) | EcclesiamInfected | E1 | `5e7b80`(391) | Hyperspace (`a7c9b35`) `5e7d10`(40) / `5e7d40`(70); Null (`0053a0c`) E2 / E2 |

kasf: 813 call sites, 133 distinct. Cataclysm-only kasf used: kasfCameraFocusClose, kasfCameraSetAngleDeg, kasfCameraSetChaseMode, kasfCameraSetDeclinationDeg, kasfCameraSetDistance, kasfEnableBeastWeaponAutoAttack, kasfEnableShipConstruction, kasfForceParadeFormation, kasfGetBuildCost, kasfGetPlayerMothership, kasfGiveTechToAI, kasfIsIdle, kasfMakeShipsControllable, kasfSetCurrentMimicItem, kasfSetMCVItemIndex, kasfSetSensorsLookAtPoint, kasfShipUnderConstruction, kasfShipsCreateLaunch, kasfShipsHyperspaceIn, kasfShipsHyperspaceOut, kasfShipsInsideMe, kasfShipsSelectBeastified, kasfShipsSelectEnemyNoStealthy, kasfShipsSelectGuarding, kasfShipsSelectNumber, kasfShipsSelectPlayer, kasfShipsSelectShips, kasfShipsSetBeastInfectionUnlimited, kasfShipsSetColor, kasfShipsSetFormation, kasfShipsSetInvincible, kasfShipsSort, kasfShipsStop, kasfShipsUpgrade, kasfSpecialOn, kasfStartLevelByHyperspacingShips, kasfThisTeamIdle, kasfVectorAddTo, kasfVectorCopy, kasfVectorScalarMult, kasfVectorSubFrom, kasfWideScreenInDontCloseSensors.

### Mission05 ("Beast Attacks Bentusi, Station Scuttled")

`Init_Mission05` `0x005f3ba0`(2172), `Watch_Mission05` `0x005f4420`(571); block `0x005eacd0`-`0x005f4660`, 39312 bytes, 134 functions.

| FSM (hash) | Team label(s) | Init | Watch | States: name (hash) Init / Watch |
|---|---|---|---|---|
| FleetIntel (`a8a880c`) | FleetIntel | `5eacd0`(237) | E1 | Fail (`004a5dc`) `5db0b0`‡(53) / E2; WaitIntel* (`e8e63cc`) `5f4730`‡(29) / `5eadc0`(639); WaitTimer* (`e989af2`) `613880`‡(71) / `5eb040`(106); WaitMessage* (`1f32b25`) `613940`‡(40) / `5eb0b0`(3222) |
| Bentusi (`6a39a39`) | trCity | `5ec230`(183) | E1 | Found (`04b4a24`) `5ec360`(186) / `5ec420`(315); UnFound* (`a2b4a74`) E2 / `5ec2f0`(98); Dead (`0048954`) E2 / `5ec560`(396) |
| BeastFighters* (`b2b0293`) | bAcolyte1, bAcolyte10, bAcolyte2, bAcolyte3, bAcolyte9, bHCorv1, bMissileCorv1 | `5ec6f0`(74) | E1 | AttackingBentusi* (`3f85149`) `5ec7e0`(54) / `5ec820`(206); Attacking (`857bcb7`) `5ec8f0`(54) / `5ec930`(99); Create (`4869685`) `5ec9a0`(117) / `5eca20`(127); InHS* (`004e2d3`) E2 / `5ec740`(146); Null (`0053a0c`) E2 / E2 |
| BeastFrigates* (`04de053`) | bICFrig1, bICFrig2, bICFrig3, bICFrig4 | `5ecaa0`(74) | E1 | AttackingBentusi* (`3f85149`) `5ecb60`(162) / `5ecc10`(253); Guarding (`9668ae7`) `5ecd10`(44) / `5ecd40`(80); InHS* (`004e2d3`) E2 / `5ecaf0`(112); Attacking (`857bcb7`) E2 / E2 |
| FSM_xce06a79? (`ce06a79`) | bHCruise1 | `5ecd90`(115) | E1 | Moving (`524ae27`) `5ece60`(44) / `5ece90`(197); Infecting (`a98c407`) `5ecf60`(76) / `5ecfb0`(226); AttackPlayer* (`439e822`) `5ed0a0`(82) / `5ed100`(129); InHS* (`004e2d3`) E2 / `5ece10`(65) |
| FSM_x99bc8d3? (`99bc8d3`) | bAcolyte4, bAcolyte5, bAcolyte6, bAcolyte7, bAcolyte8, bPlayerAttAc2 | `5ed190`(74) | E1 | Guarding (`9668ae7`) `5ed1e0`(311) / `5ed320`(690); Null (`0053a0c`) E2 / E2 |
| FSM_x87f79a2? (`87f79a2`) | bPlayerAttAc1, bPlayerAttICFrig1 | `5ed5e0`(57) | E1 | SittingThere* (`7b65285`) `5ed620`(52) / `5ed660`(94); Attacking (`857bcb7`) `5ed6c0`(87) / `5ed720`(175); Null (`0053a0c`) E2 / E2 |
| FSM_xa528adf? (`a528adf`) | bICFrig5 | `5ed7d0`(59) | E1 | Attacking (`857bcb7`) `5ed8f0`(29) / `5ed910`(262); WaitCreate* (`8832365`) `5eda20`(48) / `5eda50`(65); Create (`4869685`) `5edaa0`(303) / `5edbd0`(80); SittingThere* (`7b65285`) E2 / `5ed810`(215) |
| BEASTCarrier (`30e0522`) | bCarrier1 | `5edc20`(50) | `5edc60`(236) | - |
| LastWave* (`689b3a5`) | bMBFrig1 | `5edd50`(48) | E1 | SitThere* (`e98cf15`) `5edd80`(54) / `5eddc0`(65); Launch (`506a278`) `608010`‡(29) / `5ede10`(317) |
| CaalShto (`56179ff`) | CaalShto, CaalShtoGuard1, CaalShtoGuard2 | `5edf50`(76) | `5edfa0`(116) | S_x8761820? (`8761820`) `5ee140`(77) / `5ee190`(43); InHS* (`004e2d3`) E2 / `5ee020`(173); Attacking (`857bcb7`) E2 / `5ee0d0`(105) |
| FSM_xcc78cb3? (`cc78cb3`) | CaalShtoACV1, CaalShtoAc1, CaalShtoAc2, CaalShtoAc3 | `5ee1c0`(59) | E1 | PreHS* (`05569d3`) `5ee200`(44) / `5ee230`(135); S_x8761820? (`8761820`) `5ee3b0`(44) / E2; InHS* (`004e2d3`) E2 / `5ee2c0`(92); Attacking (`857bcb7`) E2 / `5ee320`(137) |
| Infected (`2a98c74`) | Infected | `5ee3e0`(48) | `63cc00`‡(91) | Attack (`469857b`) `615ad0`‡(49) / `5ee410`(103); Null (`0053a0c`) E2 / `5ee480`(57) |
| Harvest (`c67aac4`) | Harvest, Harvest1 | `5ee4c0`(269) | `5ee5d0`(182) | Create (`4869685`) `5eea30`‡(163) / `5eeae0`(389); Harvest (`c67aac4`) `617a10`‡(29) / `5eec70`(213); Building (`9e08a57`) `61eb40`‡(40) / `5ee870`(446); Build (`0479e04`) `65f6f0`‡(40) / `5ee690`(474); Null (`0053a0c`) `65f6f0`‡(40) / E2 |
| Workers (`c46fa23`) | Worker1, worker | `5eed50`(269) | `5eee60`(182) | Create (`4869685`) `5ef2c0`‡(163) / `5ef370`(389); Heal* (`004c95c`) `5ef6b0`(262) / `5ef7c0`(120); Salvage (`761a5e5`) `5ef840`(229) / `5ef930`(64); Harvest (`c67aac4`) `617a10`‡(29) / `5ef500`(418); Building (`9e08a57`) `61eb40`‡(40) / `5ef100`(446); Build (`0479e04`) `65f6f0`‡(40) / `5eef20`(474); Null (`0053a0c`) `65f6f0`‡(40) / E2 |
| IonArrayFrigates† (`0a9e133`) | IonArrayFrigate, IonArrayFrigate1 | `5ef970`(302) | `5efaa0`(182) | Create (`4869685`) `5eff10`‡(163) / `5effc0`(262); Attack (`469857b`) `5f03e0`(376) / `5f0560`(142); Building (`9e08a57`) `61eb40`‡(40) / `5efd50`(446); Move (`00524a5`) `631dd0`‡(195) / `5f02b0`(293); Hyperspace (`a7c9b35`) `65b260`‡(40) / `5f00d0`(480); Build (`0479e04`) `65f6f0`‡(40) / `5efb60`(491); Null (`0053a0c`) `65f6f0`‡(40) / E2 |
| Corvettes (`7aad0e3`) | Corvette, Corvette1, Corvette2 | `5f05f0`(269) | `5f0700`(239) | Create (`4869685`) `5f0df0`‡(163) / `5f0ea0`(723); Attack (`469857b`) `5f1180`(694) / `5f1440`(91); Guard (`04c9664`) `5f14a0`(676) / `5f1750`(64); Dock (`004937b`) `5f1790`‡(179) / `5f1850`(684); Building (`9e08a57`) `61eb40`‡(40) / `5f0c30`(446); Build (`0479e04`) `65f6f0`‡(40) / `5f07f0`(1074); Null (`0053a0c`) `65f6f0`‡(40) / E2 |
| Fighters (`dbd8ed3`) | Fighter, Fighter1, Fighter2 | `5f1b00`(269) | `5f1c10`(239) | Dock (`004937b`) `5f1790`‡(179) / `5f2c60`(660); Create (`4869685`) `5f2300`‡(163) / `5f23b0`(489); Attack (`469857b`) `5f25c0`(743) / `5f28b0`(91); Guard (`04c9664`) `5f2910`(676) / `5f2bc0`(145); Building (`9e08a57`) `61eb40`‡(40) / `5f2140`(446); Build (`0479e04`) `65f6f0`‡(40) / `5f1d00`(1074); Null (`0053a0c`) `65f6f0`‡(40) / E2 |
| RammingFrigates† (`4c51573`) | RammingFrigate, RammingFrigate1 | `5f2f00`(269) | `5f3010`(182) | Create (`4869685`) `5f3470`‡(163) / `5f3520`(389); Attack (`469857b`) `5f36b0`(385) / `5f3840`(91); Building (`9e08a57`) `61eb40`‡(40) / `5f32b0`(446); Build (`0479e04`) `65f6f0`‡(40) / `5f30d0`(474); Null (`0053a0c`) `65f6f0`‡(40) / E2 |
| Beast (`0469684`) | Beast | `5f38a0`(72) | E1 | Wait (`005b5e4`) `5f38f0`‡(40) / `5f3920`(65); WaitMessage* (`1f32b25`) E2 / `5f3970`(557) |
| Objectives (`98ddc43`) | Objectives | E1 | `5ebd50`(1241) | - |

kasf: 1493 call sites, 150 distinct. Cataclysm-only kasf used: kasfCameraFocus, kasfCameraFocusClose, kasfCameraSetAngleDeg, kasfCameraSetDeclinationDeg, kasfCameraSetDistance, kasfDeleteShipsWhenMissionEnds, kasfEnableBeastWeaponAutoAttack, kasfEnableShipConstruction, kasfForceParadeFormation, kasfGetBuildCost, kasfGetBuildTime, kasfGetDockHost, kasfGetShipsClampedTo, kasfGetTargets, kasfGiveTechToAI, kasfHasShipBeenDiscovered, kasfIsGuarding, kasfIsIdle, kasfRepair, kasfSalvage, kasfSetCurrentMimicItem, kasfSetMCVItemIndex, kasfSetSensorsLookAtPoint, kasfShipUnderConstruction, kasfShipsCreate, kasfShipsCreateLaunch, kasfShipsDockInstant, kasfShipsHyperspaceInDefault, kasfShipsInsideMe, kasfShipsScuttle, kasfShipsSelectEnemyNoStealthy, kasfShipsSelectMoreThanDamaged, kasfShipsSelectPlayer, kasfShipsSelectSalvagable, kasfShipsSelectShips, kasfShipsSetColor, kasfShipsSetHelpless, kasfShipsSetInvincible, kasfShipsSetInvincibleInWidescreen, kasfShipsSetPercentDamaged, kasfShipsSetRepairFactor, kasfShipsShuffle, kasfShipsSort, kasfShipsStop, kasfShipsUpgrade, kasfSpecialOn, kasfStartLevelByHyperspacingShips, kasfStopInstant, kasfTutSetObjPointerTargetShip, kasfWideScreenInDontCloseSensors.

### Mission06 ("Escourt Callisto Into Nebulae")

`Init_Mission06` `0x005fb080`(1603), `Watch_Mission06` `0x005fb6d0`(1572); block `0x005f4660`-`0x005fbd00`, 30368 bytes, 112 functions.

| FSM (hash) | Team label(s) | Init | Watch | States: name (hash) Init / Watch |
|---|---|---|---|---|
| FleetIntel (`a8a880c`) | FleetIntel | `5f4660`(201) | E1 | WaitIntel* (`e8e63cc`) `5f4730`‡(29) / `5f4750`(552); Failed (`4a5e094`) `5f4730`‡(29) / `6063f0`‡(81); WaitMessage* (`1f32b25`) `613940`‡(40) / `5f4980`(3482); Wait (`005b5e4`) E2 / `5f5720`(65); Null (`0053a0c`) E2 / E2 |
| CaalShto (`56179ff`) | CaalShto | `5f5b00`(61) | `5f5b40`(1452) | Wait (`005b5e4`) `5f60f0`(137) / `5f6180`(778); Stop (`0058940`) `5f6490`(538) / `5f66b0`(750); Done (`0049425`) `5f69a0`(44) / `5f69d0`(258); Null (`0053a0c`) E2 / E2 |
| HvCruiser (`879aa02`) | HvCruiser | `5f6ae0`(118) | `5f6b60`(420) | Wait (`005b5e4`) `5f6e60`(81) / `5f6ec0`(127); Attack (`469857b`) `5f6f40`(673) / `5f71f0`(111); Null (`0053a0c`) `5f7260`(327) / E2; Hyperspace (`a7c9b35`) E2 / `5f6d10`(332) |
| HvCruiserSupport (`f3ff484`) | HvCruiserSupport | `5f73b0`(190) | `5f7470`(388) | Guard (`04c9664`) `5f7660`(49) / E2; Heal* (`004c95c`) `5f76a0`(56) / `5f76e0`(134); Hyperspace (`a7c9b35`) E2 / `5f7600`(93); Null (`0053a0c`) `65f6f0`‡(40) / E2 |
| HvCruiserFrigate (`cf71665`) | HvCruiserDFGFrigate, HvCruiserDefender, HvCruiserFrigate, HvCruiserInterceptor | `5f7770`(118) | `5f77f0`(337) | Guard (`04c9664`) `5f79b0`(145) / `5f7a50`(245); Attack (`469857b`) `5f7b50`(238) / `5f7c40`(105); Dock (`004937b`) `5f7cb0`(56) / `5f7cf0`(121); Hyperspace (`a7c9b35`) E2 / `5f7950`(93); Null (`0053a0c`) `65f6f0`‡(40) / E2 |
| Scouts (`5784a93`) | Scouts, Scouts2, Scouts3, Scouts4 | `5f7d70`(160) | `5f7e30`(507) | Wait (`005b5e4`) `5f8030`(63) / `5f8070`(65); Create (`4869685`) `5f80c0`(117) / `5f8140`(82); Patrol (`546973c`) `5f8260`(93) / `5f82c0`(1377); Attack (`469857b`) `5f8830`(238) / `5f8920`(371); Dock (`004937b`) `5f8aa0`(54) / `5f8ae0`(160); Launch (`506a278`) E2 / `5f81a0`(192); Null (`0053a0c`) `65f6f0`‡(40) / E2 |
| StationGuard* (`7a4fe84`) | GuardCarrier, GuardCorvette, GuardDFG, GuardIon, GuardIon2 | `5f8b80`(60) | E1 | - |
| GuardDefender (`5404062`) | GuardDefender | `5f8bc0`(71) | `5f8c10`(74) | Build (`0479e04`) `5f8c60`(67) / `5f8cb0`(65); Create (`4869685`) `5f8d00`(94) / `5f8d60`(69); Guard (`04c9664`) `5f8db0`(49) / `5f8df0`(420) |
| GuardBomber (`84080c2`) | GuardBomber | `5f8fa0`(71) | `5f8ff0`(434) | Build (`0479e04`) `5f91b0`(67) / `5f9200`(65); Create (`4869685`) `5f9250`(94) / `5f92b0`(69); Patrol (`546973c`) `5f9300`(135) / `5f9390`(153); Attack (`469857b`) `5f9430`(228) / `5f9520`(105) |
| StationSupport* (`0cdb3c4`) | GuardSupport | `5f9590`(84) | `5f95f0`(86) | Wait (`005b5e4`) `5f9650`(44) / `5f9680`(181); Heal* (`004c95c`) `5f9740`(44) / `5f9770`(110); Null (`0053a0c`) E2 / E2 |
| Death (`0489688`) | Death, Death2, Death3 | `5f97e0`(72) | `5f9830`(514) | Move (`00524a5`) `5f9a40`(292) / `5f9b70`(172); Attack (`469857b`) `5f9c20`(488) / `5f9e10`(228); Null (`0053a0c`) E2 / E2 |
| DeathDefender (`140a462`) | DeathDefender, DeathDefender2, DeathDefender3 | `5f9f00`(100) | `5f9f70`(86) | Guard (`04c9664`) `5fa050`(521) / `5fa260`(64); Wait (`005b5e4`) `5fa2a0`(58) / `5fa2e0`(70); Hyperspace (`a7c9b35`) `6151c0`‡(42) / `5f9fd0`(121); Null (`0053a0c`) E2 / E2 |
| MinelayerGuard* (`44695a4`) | DeathDefender4 | `5fa330`(100) | `5fa3a0`(86) | Guard (`04c9664`) `5fa480`(107) / `5fa4f0`(64); Wait (`005b5e4`) `5fa530`(58) / `5fa570`(70); Hyperspace (`a7c9b35`) `6151c0`‡(42) / `5fa400`(121); Null (`0053a0c`) E2 / E2 |
| DeathSupport (`c2c3ca4`) | DeathSupport, DeathSupport2, DeathSupport3 | `5fa5c0`(84) | `5fa620`(86) | Wait (`005b5e4`) `5fa680`(114) / `5fa700`(290); Heal* (`004c95c`) `5fa830`(339) / `5fa990`(110); Null (`0053a0c`) E2 / E2 |
| Minelayer (`2a03c42`) | Minelayer | `5faa00`(182) | `5faac0`(178) | Mines (`051e2a3`) `5fac60`(200) / `5fad30`(252); Wait (`005b5e4`) `5fae30`(54) / `5fae70`(65); Attack (`469857b`) `5faec0`(306) / `5fb000`(117); Hyperspace (`a7c9b35`) E2 / `5fab80`(214); Null (`0053a0c`) `65f6f0`‡(40) / E2 |
| Objectives (`98ddc43`) | Objectives | E1 | `5f5770`(900) | - |

kasf: 1107 call sites, 147 distinct. Cataclysm-only kasf used: kasfCameraFocus, kasfCameraFocusClose, kasfCameraSetAngleDeg, kasfCameraSetDeclinationDeg, kasfCameraSetDistance, kasfEnableShipConstruction, kasfEnteringSlipgate, kasfFindMinesInside, kasfFindMinesNearTeam, kasfFindSlipgate, kasfGetBuildTime, kasfGetPlayerMothership, kasfGiveTechToAI, kasfHasShipBeenDiscovered, kasfIsGuarding, kasfIsIdle, kasfMakeShipsControllable, kasfPingMultipleAddShips, kasfSetCurrentMimicItem, kasfSetMCVItemIndex, kasfSetSensorsLookAtPoint, kasfShipsCreateLaunch, kasfShipsGuardShips, kasfShipsInsideMe, kasfShipsScuttle, kasfShipsSelectEnemyNoStealthy, kasfShipsSelectMoreThanDamaged, kasfShipsSelectNumber, kasfShipsSelectPlayer, kasfShipsSelectShips, kasfShipsSetColor, kasfShipsSetFormation, kasfShipsSetHelpless, kasfShipsSetInvincible, kasfShipsSetRepairFactor, kasfShipsSort, kasfShipsStop, kasfShipsUnderAttackElsewhere, kasfShipsUpgrade, kasfSlipgateDock, kasfSpecialOn, kasfStartLevelByHyperspacingShips, kasfTutSetObjPointerTargetShipSelection, kasfVectorAdd, kasfVectorAddTo, kasfVectorCopy, kasfVectorNear, kasfVectorParametric, kasfWideScreenInDontCloseSensors.

### Mission07 ("Rescue Convoy")

`Init_Mission07` `0x00601440`(3633), `Watch_Mission07` `0x00602280`(1195); block `0x005fbd00`-`0x00602730`, 27184 bytes, 74 functions.

| FSM (hash) | Team label(s) | Init | Watch | States: name (hash) Init / Watch |
|---|---|---|---|---|
| FleetIntel (`a8a880c`) | FleetIntel | `5fbd00`(201) | `5fbdd0`(159) | WaitIntel* (`e8e63cc`) `5f4730`‡(29) / `5fbe70`(496); Failed (`4a5e094`) `5f4730`‡(29) / `6063f0`‡(81); WaitMessage* (`1f32b25`) `613940`‡(40) / `5fc0b0`(2007); Wait (`005b5e4`) E2 / `5fc060`(65); Null (`0053a0c`) E2 / E2 |
| Objectives (`98ddc43`) | Objectives | `5fc890`(52) | `5fc8d0`(851) | - |
| Convoy (`4843b49`) | Convoy01, Convoy02, Convoy03, Convoy04, Convoy05, Convoy06, Convoy07, Convoy08, Convoy09, Convoy10, Convoy11, Convoy12 | `5fcc30`(253) | `5fcd30`(416) | S_x485f0d1? (`485f0d1`) `5fcf50`(68) / `5fcfa0`(168); S_xedd1c11? (`edd1c11`) `5fd110`‡(83) / `5fd050`(192); S_x20220d2? (`20220d2`) `5fd110`‡(83) / `5fd170`(223); S_x4498602? (`4498602`) `5fd250`‡(29) / `5fd270`(137); S_xdc11c32? (`dc11c32`) `5fd300`(83) / `5fd360`(196); S_xd2c52c1? (`d2c52c1`) E2 / `5fced0`(115); Null (`0053a0c`) E2 / E2 |
| InfectedConvoyDispatch† (`5335f18`) | InfectedConvoyDispatcher | `5fd430`(76) | `5fecb0`‡(102) | Monitor (`242e912`) `5fd480`(191) / `5fd540`(75); Dispatch (`e846cf8`) E2 / `5fd590`(707) |
| ConvoyWave* (`b4a3295`) | InfectedConvoyWave01, InfectedConvoyWave02, InfectedConvoyWave03, InfectedConvoyWave04, InfectedConvoyWave05 | `5fd860`(60) | `5fd8a0`(239) | Attack (`469857b`) `5fd9d0`(664) / `5fdc70`(140); Launch (`506a278`) `608010`‡(29) / `5fd990`(57); Null (`0053a0c`) E2 / E2 |
| InfectedConvoy* (`aebb739`) | InfectedConvoy01, InfectedConvoy02, InfectedConvoy03, InfectedConvoy04, InfectedConvoy05, InfectedConvoy06, InfectedConvoy07, InfectedConvoy08, InfectedConvoy09, InfectedConvoy10, InfectedConvoy11, InfectedConvoy12 | `5fdd00`(70) | `5fecb0`‡(102) | Pause (`0546a75`) `5fe1f0`(40) / `5fe220`(65); WaitMissiles* (`b3249b3`) `5fe270`(68) / `5fe2c0`(968); S_x69879b4? (`69879b4`) E2 / `5fdd50`(1182); Null (`0053a0c`) E2 / E2 |
| Infector (`2a98da2`) | Infector | `5fe690`(35) | E1 | - |
| IntroWave* (`974f895`) | IntroWave1, IntroWave2 | `5fe6c0`(74) | `5fe710`(57) | Attack (`469857b`) `5fe7e0`(109) / E2; Hidden (`4cd889e`) E2 / `5fe750`(142); Null (`0053a0c`) E2 / E2 |
| Wave (`005b6a5`) | Wave1a, Wave1b, Wave1c, Wave2a, Wave2b, Wave2c, Wave2d, Wave2e, Wave2f, Wave3a, Wave3b, Wave3c, Wave3d, Wave3e, Wave3f, Wave4a, Wave4b, Wave4c, Wave4d, Wave4e, Wave4f, Wave4g, Wave4h, Wave6a, Wave6b, Wave6c | `5fe850`(1117) | `5fecb0`‡(102) | Jumping* (`fa24e67`) `5ff4c0`(40) / `5ff4f0`(1369); Attack (`469857b`) `5ffa50`(819) / `5ffd90`(140); Wait (`005b5e4`) E2 / `5fed20`(1937); Null (`0053a0c`) E2 / E2 |
| Escort (`4a78474`) | Escort3A, Escort3B, Escort3C, Escort4A, Escort4B, Escort4C, Escort4D | `5ffe20`(285) | E1 | Guard (`04c9664`) `5fffe0`(310) / `600120`(1044); Attack (`469857b`) `600540`(737) / `600830`(140); Wait (`005b5e4`) E2 / `5fff40`(156); Null (`0053a0c`) E2 / E2 |
| Beast (`0469684`) | Beast | `6008c0`(48) | `6008f0`(135) | Wave1 (`05b6a61`) `600980`(43) / `6009b0`(129); Wave2 (`05b6a62`) `600a40`(43) / `600a70`(116); Wave3 (`05b6a63`) `600af0`(43) / `600b20`(116); Wave4 (`05b6a64`) `600ba0`(43) / `600bd0`(89); Wave5 (`05b6a65`) `600c30`(40) / `600c60`(138); Wave6 (`05b6a66`) `600cf0`(40) / `600d20`(101); Null (`0053a0c`) E2 / E2 |
| InfectedShips (`7bd5693`) | InfectedShips | `600d90`(60) | `600dd0`(273) | Attack (`469857b`) `600f50`(172) / `601000`(140); S_x69879b4? (`69879b4`) E2 / `600ef0`(86) |
| WaveDispatch (`b32c1f8`) | Wave1, Wave2, Wave3, Wave4, Wave5, Wave6 | E1 | `601090`(937) | - |

kasf: 911 call sites, 115 distinct. Cataclysm-only kasf used: kasfCameraFocus, kasfCameraSetAngleDeg, kasfCameraSetDeclinationDeg, kasfCameraSetDistance, kasfDeleteShipsWhenMissionEnds, kasfEnableShipConstruction, kasfEnteringSlipgate, kasfFindSlipgate, kasfGetPlayerMothership, kasfGetShipsClampedTo, kasfHasShipBeenDiscovered, kasfIsIdle, kasfSetCurrentMimicItem, kasfSetMCVItemIndex, kasfSetSensorsLookAtPoint, kasfShipsBeastify, kasfShipsCreate, kasfShipsCreateLaunch, kasfShipsDockInstant, kasfShipsInsideMe, kasfShipsScuttle, kasfShipsSelectBeastified, kasfShipsSelectEnemyNoStealthy, kasfShipsSelectPlayer, kasfShipsSelectShips, kasfShipsSetColor, kasfShipsSetInvincibleInWidescreen, kasfShipsSetPercentDamaged, kasfShipsSort, kasfShipsUnderAttackElsewhere, kasfSlipgateDock, kasfSlipgateDockInstant, kasfStartLevelByHyperspacingShips, kasfVectorAdd, kasfVectorNear.

### Mission08 ("Rendezvous With Spy")

`Init_Mission08` `0x006052f0`(933), `Watch_Mission08` `0x006056a0`(3202); block `0x00602730`-`0x00606330`, 15360 bytes, 47 functions.

| FSM (hash) | Team label(s) | Init | Watch | States: name (hash) Init / Watch |
|---|---|---|---|---|
| FleetIntel (`a8a880c`) | FleetIntel | `602730`(189) | E1 | WaitIntel* (`e8e63cc`) `5f4730`‡(29) / `6027f0`(496); Failed (`4a5e094`) `5f4730`‡(29) / `6063f0`‡(81); WaitMessage* (`1f32b25`) `613940`‡(40) / `602a30`(2816); Wait (`005b5e4`) E2 / `6029e0`(65); Null (`0053a0c`) E2 / E2 |
| SpyChasers (`7c3ffa3`) | SpyChasers, SpyChasersCorvettes, SpyChasersDefenders | `603820`(100) | `603890`(220) | Attack (`469857b`) `603970`(326) / `603ac0`(91); Null (`0053a0c`) E2 / `603b20`(114) |
| Spy (`0005859`) | Spy | `603ba0`(68) | `603bf0`(850) | Wait (`005b5e4`) `603f60`‡(44) / `603f90`(366); Escape (`4a77645`) `603f60`‡(44) / E2; Null (`0053a0c`) E2 / E2 |
| SpyCarrier (`762e872`) | SpyCarrier | `604100`(48) | E1 | Hyperspace (`a7c9b35`) `604130`(70) / `604180`(101) |
| Support (`8a55424`) | Support | `6041f0`(120) | `604270`(270) | Guard (`04c9664`) `6043e0`(92) / `604440`(176); Heal* (`004c95c`) `6044f0`(175) / `6045a0`(110); Hyperspace (`a7c9b35`) E2 / `604380`(81); Null (`0053a0c`) E2 / E2 |
| Special (`8497d0c`) | Special | `604610`(118) | `604690`(269) | Guard (`04c9664`) `604800`(130) / `604890`(134); SpecialGuard* (`18801b4`) `604920`(179) / `6049e0`(64); Hyperspace (`a7c9b35`) E2 / `6047a0`(86); Null (`0053a0c`) `65f6f0`‡(40) / E2 |
| Frigates (`6db6c13`) | Frigates | `604a20`(120) | `604aa0`(270) | Guard (`04c9664`) `604c30`(96) / `604c90`(69); Attack (`469857b`) `604ce0`(304) / `604e10`(91); Hyperspace (`a7c9b35`) E2 / `604bb0`(125); Null (`0053a0c`) E2 / E2 |
| Resourcers (`4a310f3`) | Resourcers | `604e70`(142) | `604f00`(325) | Harvest (`c67aac4`) `605150`(53) / `605190`(91); Hyperspace (`a7c9b35`) E2 / `605050`(255); Null (`0053a0c`) E2 / E2 |
| Crystals (`7e88293`) | Crystals | `6051f0`(48) | E1 | Check (`047c97b`) E2 / `605220`(199); Null (`0053a0c`) E2 / E2 |
| Objectives (`98ddc43`) | Objectives | E1 | `603560`(696) | - |

kasf: 597 call sites, 120 distinct. Cataclysm-only kasf used: kasfCameraFocusClose, kasfCameraSetAngleDeg, kasfCameraSetDeclinationDeg, kasfCameraSetDistance, kasfDeleteShipsWhenMissionEnds, kasfEnableShipConstruction, kasfGetPlayerMothership, kasfGiveTechToAI, kasfIsIdle, kasfPingMultipleAddShips, kasfSetCurrentMimicItem, kasfSetMCVItemIndex, kasfSetSensorsLookAtPoint, kasfShipsCreateLaunch, kasfShipsInsideMe, kasfShipsSelectClamped, kasfShipsSelectEnemyNoStealthy, kasfShipsSelectMoreThanDamaged, kasfShipsSelectPlayer, kasfShipsSelectRace, kasfShipsSelectShips, kasfShipsSetBulimia, kasfShipsSetColor, kasfShipsSetFormation, kasfShipsSetInvincible, kasfShipsSetPercentDamaged, kasfShipsSort, kasfShipsUnderAttackElsewhere, kasfShipsUpgrade, kasfSpecialOn, kasfStartLevelByHyperspacingShips, kasfVectorAdd, kasfVectorAddTo, kasfVectorNear, kasfVectorParametric, kasfWideScreenInDontCloseSensors.

### Mission09 ("Retrieve Info From Taidan Research Planet")

`Init_Mission09` `0x0060d2d0`(2099), `Watch_Mission09` `0x0060db20`(1398); block `0x00606330`-`0x0060e0b0`, 32128 bytes, 141 functions.

| FSM (hash) | Team label(s) | Init | Watch | States: name (hash) Init / Watch |
|---|---|---|---|---|
| FleetIntel (`a8a880c`) | FleetIntel | `606330`(189) | E1 | Failed (`4a5e094`) `5f4730`‡(29) / `6063f0`‡(81); WaitIntel* (`e8e63cc`) `5f4730`‡(29) / `606450`(484); NISLet (`52e80a4`) `606690`(49) / `6066d0`(196); WaitMessage* (`1f32b25`) `613940`‡(40) / `6067a0`(1696); Wait (`005b5e4`) E2 / `606640`(65); Null (`0053a0c`) E2 / E2 |
| Sensors (`7a38423`) | Sensors | `607040`(89) | E1 | Evaluate (`a619225`) E2 / `6070a0`(106); Null (`0053a0c`) E2 / E2 |
| Resourcers (`4a310f3`) | Resourcers | `607110`(60) | `607150`(141) | Move (`00524a5`) `6073a0`(49) / `6073e0`(103); Dock (`004937b`) `6074a0`(71) / `6074f0`(341); Nothing (`348ce77`) E2 / `6071e0`(363); Wait (`005b5e4`) E2 / `607350`(65); Standby (`8862829`) E2 / `607450`(65); Null (`0053a0c`) E2 / E2 |
| FighterPatrol* (`d9d4fdc`) | (computed), FighterPatrol1, FighterPatrol2, FighterPatrol3, FighterPatrol4, FighterPatrol5, FighterPatrol7, FighterPatrol8 | `607650`(289) | `607780`(292) | Patrol (`546973c`) `6078b0`(197) / `607980`(270); Attack (`469857b`) `607a90`(497) / `607c90`(105); Defend (`489aa24`) `607d00`(171) / `607db0`(105); Build (`0479e04`) `607e20`(65) / `607e70`(216); Dock (`004937b`) `607f50`(56) / `607f90`(120); Launch (`506a278`) `608010`‡(29) / `608030`(110); Alert (`0460a74`) `6080a0`(209) / `608180`(144); Null (`0053a0c`) `60c010`‡(45) / E2 |
| Minelayer (`2a03c42`) | MineLayer1, MineLayer2, MineLayer3, MineLayer4, MineLayer5, Minelayer | `608210`(158) | `6082b0`(149) | Mines (`051e2a3`) `608420`(193) / `6084f0`(106); Sleep (`05809a0`) `608560`(87) / `6085c0`(107); Wait (`005b5e4`) `608630`(50) / `608670`(132); Attack (`469857b`) `608700`(306) / `608840`(117); Controller (`9789492`) E2 / `608350`(96); Hyperspace (`a7c9b35`) E2 / `6083b0`(101); Null (`0053a0c`) `65f6f0`‡(40) / E2 |
| GuardLeader* (`945cec2`) | GuardLeader1, GuardLeader2, GuardLeader3, GuardLeader4 | `6088c0`(138) | `608950`(130) | Wait (`005b5e4`) `6089e0`(215) / `608ac0`(364); Attack (`469857b`) `608c30`(817) / `608f70`(105); Build (`0479e04`) `609950`‡(88) / `608fe0`(305); Null (`0053a0c`) `60c010`‡(45) / E2 |
| GuardWing* (`669f2b7`) | GuardWing1, GuardWing2, GuardWing3, GuardWing4 | `609120`(71) | `609170`(130) | Guard (`04c9664`) `609200`(173) / `6092b0`(236); Attack (`469857b`) `6093a0`(768) / `6096a0`(105); Wait (`005b5e4`) `609710`(178) / `6097d0`(382); Build (`0479e04`) `609950`‡(88) / `6099b0`(305); Null (`0053a0c`) `60c010`‡(45) / E2 |
| GuardSupport (`c6c32e4`) | GuardSupport1, GuardSupport2, GuardSupport3, GuardSupport4 | `609af0`(71) | `609b40`(130) | Guard (`04c9664`) `609bd0`(333) / `609d20`(296); Heal* (`004c95c`) `609e50`(226) / `609f40`(64); Build (`0479e04`) `609f80`(91) / `609fe0`(294); Null (`0053a0c`) `65f6f0`‡(40) / E2 |
| Pest (`0054a84`) | Pest | `60a110`(71) | `60a160`(74) | Build (`0479e04`) `60a210`(75) / `60a260`(219); Hyperspace (`a7c9b35`) `60a340`(45) / `60a370`(126); Hold (`004d404`) `60a3f0`(44) / `60a420`(244); Attack (`469857b`) `60a520`(158) / `60a5c0`(295); Wait (`005b5e4`) E2 / `60a1b0`(82) |
| Scouts (`5784a93`) | Scouts | `60a6f0`(71) | `60a740`(74) | Build (`0479e04`) `60a7f0`(75) / `60a840`(219); Hyperspace (`a7c9b35`) `60a920`(45) / `60a950`(126); Hold (`004d404`) `60a9d0`(44) / `60aa00`(86); Attack (`469857b`) `60aa60`(158) / `60ab00`(208); Retreat (`6a96934`) `60abd0`(77) / `60ac20`(149); Null (`0053a0c`) `60c010`‡(45) / E2; Wait (`005b5e4`) E2 / `60a790`(82) |
| AttackLeader (`c60ddd2`) | AttackLeader | `60acc0`(71) | `60ad10`(74) | Build (`0479e04`) `60adf0`(81) / `60ae50`(850); Attack (`469857b`) `60b370`(158) / `60b410`(295); Hyperspace (`a7c9b35`) `60bc70`‡(45) / `60b1c0`(175); Hold (`004d404`) `60bd20`‡(44) / `60b270`(244); Wait (`005b5e4`) E2 / `60ad60`(133) |
| AttackSpecial (`ed1251c`) | AttackSpecial | `60b540`(118) | `60b5c0`(74) | Build (`0479e04`) `60b670`‡(78) / `60b6c0`(327); Guard (`04c9664`) `60b8f0`(123) / `60b970`(106); Hyperspace (`a7c9b35`) `60bc70`‡(45) / `60b820`(118); Hold (`004d404`) `60bd20`‡(44) / `60b8a0`(70); Null (`0053a0c`) `60c010`‡(45) / E2; Wait (`005b5e4`) E2 / `60b610`(82) |
| AttackSupport (`e3d2c34`) | AttackSupport | `60b9e0`(107) | `60ba50`(74) | Build (`0479e04`) `60b670`‡(78) / `60bb00`(342); Hyperspace (`a7c9b35`) `60bc70`‡(45) / `60bca0`(118); Hold (`004d404`) `60bd20`‡(44) / `60bd50`(70); Guard (`04c9664`) `60bda0`(61) / `60bde0`(244); Heal* (`004c95c`) `60bee0`(226) / `60bfd0`(64); Null (`0053a0c`) `60c010`‡(45) / E2; Wait (`005b5e4`) E2 / `60baa0`(82) |
| FrigatePatrol (`12b21fc`) | FrigatePatrol1, FrigatePatrol2, FrigatePatrol3, FrigatePatrol4 | `60c040`(571) | E1 | Patrol (`546973c`) `60c290`(333) / `60c3e0`(638); Attack (`469857b`) `60c660`(755) / `60c960`(232); Null (`0053a0c`) E2 / E2 |
| FrigatePatrolEscort† (`18c6674`) | FrigatePatrolEscort1, FrigatePatrolEscort2, FrigatePatrolEscort3, FrigatePatrolEscort4 | `60ca50`(505) | E1 | Escort (`4a78474`) `60cc50`(289) / `60cd80`(279); Attack (`469857b`) `60cea0`(477) / `60d080`(140); S_x69879b4? (`69879b4`) `60d110`(351) / `60d270`(90); Null (`0053a0c`) E2 / E2 |
| Objectives (`98ddc43`) | Objectives | E1 | `606e40`(511) | - |

kasf: 1146 call sites, 133 distinct. Cataclysm-only kasf used: kasfCameraFocusClose, kasfCameraSetAngleDeg, kasfCameraSetDeclinationDeg, kasfCameraSetDistance, kasfEnableShipConstruction, kasfFindMinesInside, kasfFindMinesNearTeam, kasfFindSlipgate, kasfGetPlayerMothership, kasfGiveTechToAI, kasfIsIdle, kasfMakeShipsControllable, kasfPingMultipleAddShips, kasfSetCurrentMimicItem, kasfSetMCVItemIndex, kasfSetSensorsLookAtPoint, kasfShipsCreate, kasfShipsCreateLaunch, kasfShipsEffectedByMines, kasfShipsInsideMe, kasfShipsScuttle, kasfShipsSelectEnemyNoStealthy, kasfShipsSelectInside, kasfShipsSelectNumber, kasfShipsSelectPlayer, kasfShipsSelectShips, kasfShipsSetColor, kasfShipsSetInvincible, kasfShipsSetPercentDamaged, kasfShipsSort, kasfShipsUnderAttackElsewhere, kasfShipsUpgrade, kasfSpecialOn, kasfStartLevelByHyperspacingShips, kasfThisTeamIdle, kasfVectorAdd, kasfVectorAddTo, kasfVectorCopy, kasfVectorNear.

### Mission10 ("(Empty)")

`Init_Mission10` `0x00612ac0`(2207), `Watch_Mission10` `0x00613360`(661); block `0x0060e0b0`-`0x00613600`, 21840 bytes, 92 functions.

| FSM (hash) | Team label(s) | Init | Watch | States: name (hash) Init / Watch |
|---|---|---|---|---|
| FleetIntel (`a8a880c`) | FleetIntel | `60e0b0`(237) | E1 | Fail (`004a5dc`) `5db0b0`‡(53) / E2; WaitIntel* (`e8e63cc`) `5f4730`‡(29) / `60e1a0`(539); WaitTimer* (`e989af2`) `613880`‡(71) / `60e3c0`(147); WaitMessage* (`1f32b25`) `613940`‡(40) / `60e920`(3416); PlayNislet* (`e2b30c4`) E2 / `60e460`(1209) |
| BigGun (`46dbc9e`) | BigGun | `60fa30`(71) | `60fa80`(578) | - |
| Salvage (`761a5e5`) | pSCInit1, pSCInit2, pSCInit3, pSCInit4, pSalCorvette1, pSalCorvette2, pSalCorvette3, pSalCorvette4, pSalCorvette5, pSalCorvette6 | `60fcd0`(190) | E1 | Hidden (`4cd889e`) `60fd90`‡(35) / `60fdc0`(80); DestroyGates (`051e1d3`) `60fe10`(199) / `60fee0`(113); Preattack (`969d01b`) `60ff60`(141) / `60fff0`(65); Salvage (`761a5e5`) `6101c0`(44) / `6101f0`(244); Attack (`469857b`) `6102f0`(121) / `610370`(137); AttackSalvage* (`1d9fdf5`) E2 / `610040`(383) |
| DefendBigGun* (`ce416be`) | pFighter2, pMCorvette1 | `610400`(74) | E1 | Salvage (`761a5e5`) E2 / `610450`(151); Attack (`469857b`) E2 / `6104f0`‡(44) |
| MSMimic† (`281e183`) | pMSMimic1, pMSMimic2 | `610520`(74) | E1 | Mimicing (`e1d7937`) `610570`(60) / `6105b0`(112); Attacking (`857bcb7`) `610620`(35) / `610650`(132); Create (`4869685`) `6106e0`(145) / `610780`(57) |
| CloakedFrig* (`fd0a587`) | pIFrig1 | `6107c0`(74) | E1 | Cloaked (`8135fd4`) `610810`(57) / `610850`(127); Attacking (`857bcb7`) `6108d0`(100) / E2 |
| PirateWave2* (`8ff8c02`) | pFighter3, pFighter4, pMCorvette2 | `610940`(75) | E1 | AttackPlayer* (`439e822`) `608010`‡(29) / `610a00`(180); Docked (`4937f94`) E2 / `610990`(100); AttackCaalShto (`ce4069f`) E2 / `610ac0`(44) |
| PirateProtectCarrier† (`e497a32`) | PirateProtectCarrier1, PirateProtectCarrier2, PirateProtectCarrier3, PirateProtectCarrier4 | `610af0`(59) | E1 | Guarding (`9668ae7`) `610b30`(230) / `610c20`(135); Create (`4869685`) `610cb0`(145) / `610d50`(57) |
| AttackCaalShto (`ce4069f`) | AttackCaalShto | `610d90`(54) | `610dd0`(276) | - |
| ImperialFleet* (`c2faf34`) | tDestroyer1, tGravWell1, tHCruiser1, tHDefender1, tIFrig1, tMGCorvette1 | `610ef0`(74) | E1 | S_x4a93a83? (`4a93a83`) `6104f0`‡(44) / `611010`(100); Guarding (`9668ae7`) `611080`(49) / `6110c0`(223); AttackBeast* (`7b00ed4`) `6111a0`(91) / `611200`(97); InHS* (`004e2d3`) E2 / `610f40`(194) |
| FSM_x8212be2? (`8212be2`) | tHInterceptor1, tHInterceptor2, tHInterceptor3 | `611270`(75) | E1 | S_x4a93a83? (`4a93a83`) `611380`(362) / `6114f0`(210); AttackBeast* (`7b00ed4`) `6115d0`(168) / `611680`(234); Create (`4869685`) `611770`(145) / `611810`(98); InHS* (`004e2d3`) E2 / `6112c0`(189) |
| ImperialCarrier* (`3c1f182`) | tCarrier | `611880`(49) | `6118c0`(337) | - |
| CaalShto (`56179ff`) | CaalShto | `611a20`(91) | E1 | S_x4a93a83? (`4a93a83`) `611ae0`(36) / `611b10`(250); InHS* (`004e2d3`) E2 / `611a80`(81) |
| BeastMothership (`a2e1ef0`) | BeastMothership | `611c10`(115) | E1 | NISLet (`52e80a4`) `611c90`(54) / `611cd0`(418); S_x4a93a83? (`4a93a83`) `611e80`(45) / `611eb0`(577); AttackingPlayer* (`427d112`) E2 / `612100`(64) |
| BeastForces* (`8f2f123`) | bABomb1, bACV1, bAcolyte1, bMBFrig1 | `612140`(71) | E1 | InHS* (`004e2d3`) `612190`(44) / `6121c0`(133); Flanking* (`062fa97`) `612250`(350) / `6123b0`(148); Attacking (`857bcb7`) `612450`(193) / `612520`(223); WaitCreate* (`8832365`) `612600`(48) / `612630`(65); Create (`4869685`) `612680`(352) / `6127e0`(144); S_xb1ce825? (`b1ce825`) E2 / `612870`(95) |
| Infected (`2a98c74`) | Infected | `6128d0`(48) | `63cc00`‡(91) | Attack (`469857b`) `615ad0`‡(49) / `612900`(103); Null (`0053a0c`) E2 / `612970`(57) |
| pCarrier (`76768e2`) | pCarrier | `6129b0`(84) | E1 | PostNislet* (`92b3424`) `612a90`(39) / E2; PreNislet* (`a2e35c4`) E2 / `612a10`(124) |
| Objectives (`98ddc43`) | Objectives | E1 | `60f680`(931) | - |

kasf: 797 call sites, 140 distinct. Cataclysm-only kasf used: kasfCameraFocus, kasfCameraFocusClose, kasfCameraSetAngleDeg, kasfCameraSetDeclinationDeg, kasfCameraSetDistance, kasfDisableBigGunFiring, kasfEnableBeastWeaponAutoAttack, kasfEnableShipConstruction, kasfGetShipCountSalvaged, kasfGiveTechToAI, kasfIsGuarding, kasfIsIdle, kasfMimic, kasfSalvage, kasfSetAIDepotShip, kasfSetCurrentMimicItem, kasfSetMCVItemIndex, kasfSetSensorsLookAtPoint, kasfShipUnderConstruction, kasfShipsBeastify, kasfShipsCreateLaunch, kasfShipsGetClamped, kasfShipsInsideMe, kasfShipsScuttle, kasfShipsSelectEnemyNoStealthy, kasfShipsSelectPlayer, kasfShipsSelectRace, kasfShipsSetBeastInfectionUnlimited, kasfShipsSetColor, kasfShipsSetDerelict, kasfShipsSetHelpless, kasfShipsSetInvincible, kasfShipsSetInvincibleInWidescreen, kasfShipsSort, kasfShipsUpgrade, kasfSpecialOn, kasfStartLevelByHyperspacingShips, kasfTeamAttributesBitTest, kasfTutSetObjPointerTargetShip, kasfUnmimic, kasfVectorAdd, kasfVectorAddTo, kasfVectorCopy, kasfVectorCrossProduct, kasfVectorNear, kasfVectorScalarMult, kasfVectorSubFrom, kasfWideScreenInDontCloseSensors.

### Mission11 ("The Factorem Attacks")

`Init_Mission11` `0x0061f690`(2990), `Watch_Mission11` `0x00620240`(542); block `0x00613600`-`0x00620460`, 52832 bytes, 169 functions.

| FSM (hash) | Team label(s) | Init | Watch | States: name (hash) Init / Watch |
|---|---|---|---|---|
| FleetIntel (`a8a880c`) | FleetIntel | `613600`(201) | E1 | Fail (`004a5dc`) `5db0b0`‡(53) / E2; WaitIntel* (`e8e63cc`) `5f4730`‡(29) / `6136d0`(425); WaitTimer* (`e989af2`) `613880`‡(71) / `6138d0`(106); WaitMessage* (`1f32b25`) `613940`‡(40) / `613970`(2025) |
| Objectives (`98ddc43`) | Objectives | `614160`(48) | `614190`(187) | S_xa2dd597? (`a2dd597`) `614370`(71) / `6143c0`(122); BeastAttacking* (`13f80d7`) `614440`(114) / `6144c0`(196); Attacking (`857bcb7`) E2 / `614250`(288) |
| Factorum (`589433d`) | Factorum | `614590`(73) | E1 | Enemies (`a2a1de3`) `6145e0`(343) / `614740`(558); Friends* (`b6da2d3`) E2 / `614970`(116) |
| FactDefendFleet† (`a03d974`) | FactDefend, FactHInt1, FactHInt3, FactIFrig2, FactMBCorv2 | `6149f0`(66) | E1 | Friends* (`b6da2d3`) `614af0`(49) / `614b30`(112); BeastAttacking* (`13f80d7`) `614ba0`(29) / `614bc0`(356); CreateDefenders* (`40a5a83`) `614d30`(133) / `614dc0`(57); S_x5f80993? (`5f80993`) `614e00`(48) / `614e30`(65); CreateFrigates* (`04de593`) `614e80`(204) / `614f50`(57); Guarding (`9668ae7`) `614f90`(108) / `615000`(348); Enemies (`a2a1de3`) E2 / `614a40`(167); Null (`0053a0c`) E2 / E2 |
| BEASTCarrier (`30e0522`) | BEASTCarrier | `615160`(95) | E1 | InHS* (`004e2d3`) `6151c0`‡(42) / `6151f0`(173); Attacking (`857bcb7`) E2 / E2 |
| BeastMissiles* (`81169f3`) | bMissiles | `6152a0`(59) | E1 | Attacking (`857bcb7`) `615330`(138) / `6153c0`(57); WaitCreate* (`8832365`) `615400`(48) / `615430`(65); Create (`4869685`) `615480`(152) / `615520`(57); InHS* (`004e2d3`) E2 / `6152e0`(65) |
| BeastInit* (`688a474`) | BEASTAc3, BEASTAc4, BEASTMBFrig | `615560`(59) | E1 | Attacking (`857bcb7`) `6155a0`(127) / `615620`(225) |
| FSM_x10144a4? (`10144a4`) | BEASTABomb1, BEASTIFrig | `615710`(59) | E1 | Attacking (`857bcb7`) `615750`(44) / E2 |
| ScientistTransfer (`59a8e92`) | ScientistTransfer | `615780`(132) | E1 | Close (`0481475`) `5e3410`‡(29) / `615810`(139); Far (`0004a62`) `6158a0`(44) / `6158d0`(139); S_x0474640? (`0474640`) `615960`(61) / `6159a0`(123); Null (`0053a0c`) `615a20`(40) / `615a50`(79) |
| Infected (`2a98c74`) | BEASTDFGFrig, Infected | `615aa0`(48) | `63cc00`‡(91) | Attack (`469857b`) `615ad0`‡(49) / `615b10`(103); Null (`0053a0c`) E2 / `615b80`(57) |
| Sentinels (`38e7fb3`) | Sentinel, Sentinel1, Sentinel2 | `615bc0`(269) | `615cd0`(182) | Null (`0053a0c`) `60c010`‡(45) / E2; Create (`4869685`) `616130`‡(187) / `6161f0`(409); Defend (`489aa24`) `616390`(1046) / `6167b0`(957); Building (`9e08a57`) `61eb40`‡(40) / `615f70`(446); Wait (`005b5e4`) `63ece0`‡(261) / `616b70`(70); Build (`0479e04`) `65f6f0`‡(40) / `615d90`(474) |
| HiveFrigates* (`215c113`) | HiveFrigate, HiveFrigate1 | `616bc0`(269) | `616cd0`(182) | Building (`9e08a57`) `61eb40`‡(40) / `616f70`(446); Create (`4869685`) `64e740`‡(163) / `617130`(389); Attack (`469857b`) `64e980`‡(352) / `6172c0`(75); Build (`0479e04`) `65f6f0`‡(40) / `616d90`(474); Null (`0053a0c`) `65f6f0`‡(40) / E2 |
| Harvest (`c67aac4`) | Harvest, Harvest1, Harvest2 | `617310`(269) | `617420`(182) | Create (`4869685`) `5eea30`‡(163) / `617880`(389); Harvest (`c67aac4`) `617a10`‡(29) / `617a30`(213); Building (`9e08a57`) `61eb40`‡(40) / `6176c0`(446); Build (`0479e04`) `65f6f0`‡(40) / `6174e0`(474); Null (`0053a0c`) `65f6f0`‡(40) / E2 |
| Workers (`c46fa23`) | Worker1, Worker2, worker | `617b10`(269) | `617c20`(182) | Create (`4869685`) `5ef2c0`‡(163) / `618080`(389); Harvest (`c67aac4`) `617a10`‡(29) / `618210`(418); Heal* (`004c95c`) `6183c0`(262) / `6184d0`(120); Salvage (`761a5e5`) `618550`(229) / `618640`(64); Building (`9e08a57`) `61eb40`‡(40) / `617ec0`(446); Build (`0479e04`) `65f6f0`‡(40) / `617ce0`(474); Null (`0053a0c`) `65f6f0`‡(40) / E2 |
| IonArrayFrigates† (`0a9e133`) | IonArrayFrigate, IonArrayFrigate1 | `618680`(302) | `6187b0`(182) | Create (`4869685`) `5eff10`‡(163) / `618c20`(262); Attack (`469857b`) `619040`(401) / `6191e0`(142); Building (`9e08a57`) `61eb40`‡(40) / `618a60`(446); Move (`00524a5`) `631dd0`‡(195) / `618f10`(293); Hyperspace (`a7c9b35`) `65b260`‡(40) / `618d30`(480); Build (`0479e04`) `65f6f0`‡(40) / `618870`(491); Null (`0053a0c`) `65f6f0`‡(40) / E2 |
| CloakedFighters* (`ee25ac3`) | CloakedFighter, CloakedFighter1 | `619270`(302) | `6193a0`(239) | Launch (`506a278`) `608010`‡(29) / `61a1b0`(126); Move (`00524a5`) `6199d0`‡(200) / `619aa0`(298); Attack (`469857b`) `619bd0`(351) / `619d30`(142); Dock (`004937b`) `619dc0`(159) / `619e60`(837); Building (`9e08a57`) `61eb40`‡(40) / `619680`(446); Create (`4869685`) `651d10`‡(163) / `619840`(394); Build (`0479e04`) `65f6f0`‡(40) / `619490`(491); Null (`0053a0c`) `65f6f0`‡(40) / E2 |
| MultibeamFrigates* (`be76893`) | MultiBeamFrigate, MultiBeamFrigate1 | `61a230`(269) | `61a340`(182) | Attack (`469857b`) `61a930`‡(354) / `61aaa0`(91); Building (`9e08a57`) `61eb40`‡(40) / `61a5e0`(446); Create (`4869685`) `652c50`‡(163) / `61a7a0`(389); Build (`0479e04`) `65f6f0`‡(40) / `61a400`(474); Null (`0053a0c`) `65f6f0`‡(40) / E2 |
| Corvettes (`7aad0e3`) | Corvette, Corvette1, Corvette2 | `61ab00`(269) | `61ac10`(239) | Create (`4869685`) `5f0df0`‡(163) / `61b300`(723); Attack (`469857b`) `61b5e0`(722) / `61b8c0`(91); Guard (`04c9664`) `61b920`(676) / `61bbd0`(64); Building (`9e08a57`) `61eb40`‡(40) / `61b140`(446); Dock (`004937b`) `658c90`‡(179) / `61bc10`(684); Build (`0479e04`) `65f6f0`‡(40) / `61ad00`(1074); Null (`0053a0c`) `65f6f0`‡(40) / E2 |
| Fighters (`dbd8ed3`) | Fighter, Fighter1, Fighter2, Fighter3 | `61bec0`(269) | `61bfd0`(239) | Create (`4869685`) `5f2300`‡(163) / `61c6c0`(489); Attack (`469857b`) `61c8d0`(771) / `61cbe0`(91); Guard (`04c9664`) `61cc40`(676) / `61cef0`(145); Building (`9e08a57`) `61eb40`‡(40) / `61c500`(446); Dock (`004937b`) `61ef20`‡(179) / `61cf90`(660); Build (`0479e04`) `65f6f0`‡(40) / `61c0c0`(1074); Null (`0053a0c`) `65f6f0`‡(40) / E2 |
| Recons (`5698433`) | Recon, Recon1 | `61d230`(269) | `61d340`(239) | Attack (`469857b`) `61d960`(544) / `61db80`(91); Patrol (`546973c`) `61dbe0`(210) / `61dcc0`(145); Building (`9e08a57`) `61eb40`‡(40) / `61d610`(446); Dock (`004937b`) `61ef20`‡(179) / `61dd60`(662); Create (`4869685`) `658650`‡(163) / `61d7d0`(398); Build (`0479e04`) `65f6f0`‡(40) / `61d430`(474); Null (`0053a0c`) `65f6f0`‡(40) / E2 |
| RammingFrigates† (`4c51573`) | RammingFrigate, RammingFrigate1 | `61e000`(269) | `61e110`(182) | Create (`4869685`) `5f3470`‡(163) / `61e570`(389); Building (`9e08a57`) `61eb40`‡(40) / `61e3b0`(446); Attack (`469857b`) `645bb0`‡(410) / `61e700`(91); Build (`0479e04`) `65f6f0`‡(40) / `61e1d0`(474); Null (`0053a0c`) `65f6f0`‡(40) / E2 |
| DFGs† (`0048ac3`) | DFG, DFG1 | `61e760`(269) | `61e870`(239) | Building (`9e08a57`) `61eb40`‡(40) / `61eb70`(446); Dock (`004937b`) `61ef20`‡(179) / `61efe0`(660); Create (`4869685`) `659f50`‡(163) / `61ed30`(398); Guard (`04c9664`) `65a190`‡(493) / `61eec0`(81); Build (`0479e04`) `65f6f0`‡(40) / `61e960`(474); Null (`0053a0c`) `65f6f0`‡(40) / E2 |
| Wave2 (`05b6a62`) | w2DFG1, w2IFrig1 | `61f580`(59) | E1 | InHS* (`004e2d3`) `6151c0`‡(42) / `61f5c0`(193); Null (`0053a0c`) E2 / E2 |
| Beast (`0469684`) | Beast | E1 | `61f280`(755) | - |

kasf: 2085 call sites, 135 distinct. Cataclysm-only kasf used: kasfCameraFocus, kasfCameraFocusClose, kasfCameraSetAngleDeg, kasfCameraSetDeclinationDeg, kasfCameraSetDistance, kasfDisableBigGunFiring, kasfEnableBeastWeaponAutoAttack, kasfForceParadeFormation, kasfGetBuildCost, kasfGetBuildTime, kasfGetDockHost, kasfGetTargets, kasfGiveTechToAI, kasfIsGuarding, kasfIsIdle, kasfRepair, kasfSalvage, kasfSetCurrentMimicItem, kasfSetMCVItemIndex, kasfSetSensorsLookAtPoint, kasfShipsCreate, kasfShipsCreateLaunch, kasfShipsDockInstant, kasfShipsHyperspaceInDefault, kasfShipsHyperspaceOut, kasfShipsInsideMe, kasfShipsSelectEnemyNoStealthy, kasfShipsSelectMoreThanDamaged, kasfShipsSelectPlayer, kasfShipsSelectRace, kasfShipsSelectSalvagable, kasfShipsSelectShips, kasfShipsSetColor, kasfShipsSetFormation, kasfShipsSetInvincible, kasfShipsSort, kasfShipsStop, kasfShipsUpgrade, kasfStartLevelByHyperspacingShips, kasfVectorAdd, kasfVectorNear, kasfWideScreenInDontCloseSensors.

### Mission12 ("Negotiations With Pirates And Help From Contras")

`Init_Mission12` `0x00623690`(2034), `Watch_Mission12` `0x00623e90`(631); block `0x00620460`-`0x00624110`, 15536 bytes, 73 functions.

| FSM (hash) | Team label(s) | Init | Watch | States: name (hash) Init / Watch |
|---|---|---|---|---|
| FleetIntel (`a8a880c`) | FleetIntel | `620460`(204) | E1 | Fail (`004a5dc`) `5db0b0`‡(53) / E2; WaitIntel* (`e8e63cc`) `5f4730`‡(29) / `620530`(527); WaitTimer* (`e989af2`) `613880`‡(71) / `620740`(106); WaitMessage* (`1f32b25`) `613940`‡(40) / `6207b0`(2998) |
| PirateOutpost* (`04f3c54`) | pStation | `621570`(74) | E1 | S_xb1dedfb? (`b1dedfb`) `621780`(85) / `6217e0`(48); UnderAttack (`633addb`) E2 / `6215c0`(443); Null (`0053a0c`) E2 / E2 |
| SecurityTeam (`be7033d`) | SecurityTeam | `621810`(48) | E1 | Launching (`a279e47`) `608010`‡(29) / `621940`(57); Docking (`937fe67`) `621880`(51) / `6218c0`(116); GoingBack* (`e2b294b`) `621980`(78) / `6219d0`(103); Pre (`0005565`) E2 / `621840`(57); Null (`0053a0c`) E2 / E2 |
| PirateMothership* (`a2a1600`) | pMothership | `621a40`(93) | `621aa0`(92) | - |
| FSM_x47bfbd4? (`47bfbd4`) | pMimicAst1, pMimicAst2, pMimicAst3, pMimicAst4 | `621b00`(74) | `6229d0`‡(80) | Mimicked (`e1d7a84`) `621b50`(44) / `621b80`(99); Attacking (`857bcb7`) `621bf0`(41) / `6104f0`‡(44); Null (`0053a0c`) E2 / E2 |
| FSM_xe2aa713? (`e2aa713`) | pIFrig1, pIFrig2 | `621c20`(59) | `621c60`(70) | Patrolling (`7464cb7`) `621cb0`(49) / `621cf0`(140); Attacking (`857bcb7`) `621d80`(49) / `621dc0`(140); Null (`0053a0c`) E2 / E2 |
| FSM_x0a52a12? (`0a52a12`) | pMimicPlayer1, pMimicPlayer2 | `621e50`(95) | `6229d0`‡(80) | Attacking (`857bcb7`) `622050`(53) / `622090`(99); Patrolling (`7464cb7`) E2 / `621eb0`(388); Null (`0053a0c`) E2 / E2 |
| FSM_xa1082c2? (`a1082c2`) | pReconKiller1, pReconKiller2, pReconKiller3 | `622100`(110) | `6229d0`‡(80) | Attacking (`857bcb7`) `622170`(40) / `6221a0`(590); WaitCreate* (`8832365`) `622420`‡(40) / `622450`(65); Create (`4869685`) `6224a0`(155) / `622540`(159); WaitAttack* (`86c309b`) E2 / `6223f0`(48); Null (`0053a0c`) E2 / E2 |
| FSM_xc7789a2? (`c7789a2`) | pFighter1, pFighter2, pMCorv1 | `6225e0`(59) | `6229d0`‡(80) | WaitCreate* (`8832365`) `622420`‡(40) / `622760`(65); Attacking (`857bcb7`) `622620`(133) / `6226b0`(176); Create (`4869685`) `6227b0`(197) / `622880`(145); WaitAttack* (`86c309b`) E2 / `622920`(48); Null (`0053a0c`) E2 / E2 |
| PirateSalvage* (`3c7cd45`) | pSalvage1 | `622950`(127) | `6229d0`‡(80) | WaitSalvage* (`71afb25`) `622a90`(67) / `622ae0`(245); Salvaging (`1a5eb47`) `622be0`(198) / `622cb0`(240); WaitCreate* (`8832365`) `622da0`(43) / `622dd0`(65); Create (`4869685`) `622e20`(145) / `622ec0`(80); SittingThere* (`7b65285`) E2 / `622a20`(99); Null (`0053a0c`) E2 / E2 |
| FSM_x5c43eb3? (`5c43eb3`) | pCloakedFrig1, pCloakedFrig2 | `622f10`(290) | `623040`(296) | AttackMothership (`9dbf1e0`) E2 / `623170`(133); SitThere* (`e98cf15`) E2 / `623200`(354); Null (`0053a0c`) E2 / E2 |
| Taiidan (`85dd80e`) | tFleet1, tFleet2, tFleet3, tFleet4, tFleet5, tFleet6 | `623370`(59) | E1 | InHS* (`004e2d3`) `6151c0`‡(42) / `6233b0`(65); S_x4a93a83? (`4a93a83`) `623400`(282) / `623520`(65); Gone (`004c425`) `623570`(29) / E2 |
| Salvaged (`61a5ee4`) | JustSalvaged | `623590`(48) | `6235c0`(80) | Attack (`469857b`) `623610`(60) / `623650`(64); Null (`0053a0c`) `65f6f0`‡(40) / E2 |
| Objectives (`98ddc43`) | Objectives | E1 | `621370`(502) | - |

kasf: 554 call sites, 116 distinct. Cataclysm-only kasf used: kasfCameraFocus, kasfCameraSetAngleDeg, kasfCameraSetDeclinationDeg, kasfCameraSetDistance, kasfDeleteShipsWhenMissionEnds, kasfDisableBigGunFiring, kasfFirstShip, kasfGiveTechToAI, kasfHasShipBeenDiscovered, kasfIsIdle, kasfMakeShipsControllable, kasfMimic, kasfSalvage, kasfSetAIDepotShip, kasfSetCurrentMimicItem, kasfSetMCVItemIndex, kasfSetSensorsLookAtPoint, kasfShipsCreateLaunch, kasfShipsInsideMe, kasfShipsScuttle, kasfShipsSelectEnemyNoStealthy, kasfShipsSelectNumber, kasfShipsSelectPlayer, kasfShipsSelectSalvaged, kasfShipsSetColor, kasfShipsSetDerelict, kasfShipsSetHelpless, kasfShipsSetInvincible, kasfShipsSort, kasfShipsStop, kasfShipsUpgrade, kasfStartLevelByHyperspacingShips, kasfUnmimic, kasfVectorAdd, kasfVectorNear, kasfWideScreenInDontCloseSensors.

### Mission13 ("Knocked From Hyperspace")

`Init_Mission13` `0x00629a90`(3171), `Watch_Mission13` `0x0062a720`(2235); block `0x00624110`-`0x0062aff0`, 28384 bytes, 125 functions.

| FSM (hash) | Team label(s) | Init | Watch | States: name (hash) Init / Watch |
|---|---|---|---|---|
| FleetIntel (`a8a880c`) | FleetIntel | `624110`(201) | `6241e0`(107) | Failed (`4a5e094`) `5f4730`‡(29) / `624aa0`‡(81); WaitIntel* (`e8e63cc`) `5f4730`‡(29) / `624b00`(432); WaitMessage* (`1f32b25`) `613940`‡(40) / `624d00`(1316); S_xb663893? (`b663893`) `624250`‡(40) / `624280`(1247); FaalCorum (`6080dcd`) `624250`‡(40) / `624760`(826); Wait (`005b5e4`) E2 / `624cb0`(65); Null (`0053a0c`) E2 / E2 |
| Objectives (`98ddc43`) | Objectives | `625230`(29) | `625250`(325) | - |
| Carrier (`7676de2`) | SpyCarrier | `6253a0`(59) | `6253e0`(98) | Retreat (`6a96934`) `6254d0`(56) / `625510`(180); HyperspaceWait* (`b30c974`) `626770`‡(40) / `6255d0`‡(80); Support (`8a55424`) E2 / `625450`(117); Null (`0053a0c`) E2 / E2 |
| Bombers (`7416a33`) | Bombers1, Bombers2, Bombers3 | `625620`(98) | `625690`(103) | Attack (`469857b`) `625700`(527) / `625910`(140); Pause (`0546a75`) `6259a0`(54) / `6259e0`(130); S_x69879b4? (`69879b4`) `625a70`(179) / `625b30`(221); Waiting (`b5e8e77`) E2 / `625c10`(151); Null (`0053a0c`) E2 / E2 |
| BomberEscorts* (`d372db3`) | Escorts1, Escorts2, Escorts3 | `625cb0`(98) | `625d20`(103) | Protect (`57489d4`) `625d90`(131) / `625e20`(324); S_x69879b4? (`69879b4`) `625ff0`(179) / `6260b0`(221); Standby (`8862829`) E2 / `625f70`(127); Null (`0053a0c`) E2 / E2 |
| TaiKamikaze (`645ab15`) | TaiKamikaze | `626190`(48) | E1 | Attack (`469857b`) `6261c0`(64) / `626200`(140); Idle (`004d905`) E2 / `626290`(57) |
| TaiPester (`e4ad0f2`) | TaiPester | `6262d0`(48) | E1 | Attack (`469857b`) `626300`(280) / `626420`(106); Idle (`004d905`) E2 / `626490`(57) |
| Resourcers (`4a310f3`) | TaiRC1, TaiRC2 | `6264d0`(59) | `626510`(65) | Harvest (`c67aac4`) `617a10`‡(29) / `626560`(57); S_x69879b4? (`69879b4`) `6265a0`(156) / `626640`(93); Retreat (`6a96934`) `6266a0`(44) / `6266d0`(151); HyperspaceWait* (`b30c974`) `626770`‡(40) / `6255d0`‡(80); Null (`0053a0c`) E2 / E2 |
| Defenders (`aa2c2e3`) | TaiDefenders | `6267a0`(59) | E1 | Guard (`04c9664`) `6267e0`(49) / `626820`(121); Panic (`05462d3`) `6268a0`(48) / `6268d0`(125); Null (`0053a0c`) E2 / E2 |
| DefenderMaker† (`30bfd62`) | TaiDefenderMaker | `626950`(58) | `626990`(125) | Reinforce (`e2b10e5`) `626a60`(259) / `626b70`(254); Wait (`005b5e4`) E2 / `626a10`(72); Null (`0053a0c`) E2 / E2 |
| FSM_xc4f9c2e? (`c4f9c2e`) | MainAssult1A, MainAssult1B, MainAssult1C | `626ec0`(59) | E1 | Attack (`469857b`) `6292d0`‡(503) / `626f80`(220); Wait (`005b5e4`) E2 / `626f00`(127); Null (`0053a0c`) E2 / E2 |
| FSM_x4d56e8e? (`4d56e8e`) | Backup1A, Backup1B, Backup1C | `627060`(59) | E1 | Protect (`57489d4`) `627120`(158) / `6271c0`(229); Attack (`469857b`) `6272b0`‡(440) / `627470`(220); Wait (`005b5e4`) E2 / `6270a0`(127); Null (`0053a0c`) E2 / E2 |
| ThirdWaveAssult (`7016874`) | ThirdWaveAssult | `627550`(74) | E1 | Attack (`469857b`) `627630`(64) / `627670`(140); Hidden (`4cd889e`) E2 / `6275a0`(143); Null (`0053a0c`) E2 / E2 |
| ThirdWaveBackup (`70caec0`) | ThirdWaveBackup | `627700`(74) | E1 | Protect (`57489d4`) `6277e0`(44) / `627810`(114); Attack (`469857b`) `627890`(181) / `627950`(140); Hidden (`4cd889e`) E2 / `627750`(143); Null (`0053a0c`) E2 / E2 |
| ThirdWaveAntiFighter (`caf7e92`) | ThirdWaveAntiFighter | `6279e0`(74) | E1 | Attack (`469857b`) `627ac0`(358) / `627c30`(140); Hidden (`4cd889e`) E2 / `627a30`(143); Null (`0053a0c`) E2 / E2 |
| FCFleet (`a7b09e4`) | FCFleet | `627cc0`(291) | `627df0`(127) | HSJump* (`4d7fa20`) `627e70`(73) / `627ec0`(228); Null (`0053a0c`) E2 / E2 |
| FaalCorum (`6080dcd`) | FaalCorum | `627fb0`(69) | E1 | ExitingHS* (`e8e6113`) `628080`(89) / `6280e0`(174); InHS* (`004e2d3`) E2 / `628000`(127); Attacking (`857bcb7`) E2 / E2 |
| FSM_xd7b5b43? (`d7b5b43`) | GiftACVs, GiftDestroyers, GiftIonCannons, GiftRammingFrigates | `628190`(126) | E1 | Launch (`506a278`) `628380`‡(75) / `6283d0`(57); InHS* (`004e2d3`) E2 / `628210`(353); S_x3caeb82? (`3caeb82`) E2 / `628410`(144); Null (`0053a0c`) E2 / E2 |
| FaalCorumWorkers (`cc62733`) | FaalCorumWorkers | `6284a0`(69) | E1 | Launch (`506a278`) `628570`(89) / `6285d0`(65); Repair (`56a45e2`) `628620`(297) / `628750`(177); S_x69879b4? (`69879b4`) `628810`(128) / `628890`(57); InHS* (`004e2d3`) E2 / `6284f0`(127); Null (`0053a0c`) E2 / E2 |
| FSM_x5b05dd3? (`5b05dd3`) | FCInterceptor1, FCInterceptor2, FCMGCorv1 | `6288d0`(97) | E1 | Launch (`506a278`) `628380`‡(75) / `6289c0`‡(57); Attack (`469857b`) `628a00`(615) / `628c70`(192); S_x69879b4? (`69879b4`) `628d30`(128) / `6289c0`‡(57); InHS* (`004e2d3`) E2 / `628940`(127); Null (`0053a0c`) E2 / E2 |
| FSM_xed6bf13? (`ed6bf13`) | FCICFrig1 | `628db0`(69) | E1 | Attack (`469857b`) `628ea0`(534) / `6290c0`(140); InHS* (`004e2d3`) E2 / `628e00`(149); Null (`0053a0c`) E2 / E2 |
| AssultWave2* (`1dde2c2`) | AssultWave2A, AssultWave2B, AssultWave2C, AssultWave2D | `629150`(74) | E1 | Attack (`469857b`) `6292d0`‡(503) / `6294d0`(220); Hidden (`4cd889e`) E2 / `6291a0`(294); Null (`0053a0c`) E2 / E2 |
| SupportWave2* (`2113f22`) | SupportWave2A, SupportWave2B, SupportWave2C, SupportWave2D | `6295b0`(217) | E1 | Attack (`469857b`) `6272b0`‡(440) / `6299b0`(220); Protect (`57489d4`) `6297c0`(201) / `629890`(279); Hidden (`4cd889e`) E2 / `629690`(294); Null (`0053a0c`) E2 / E2 |
| WaveDispatch (`b32c1f8`) | WaveDispatch | E1 | `626c70`(579) | - |

kasf: 990 call sites, 117 distinct. Cataclysm-only kasf used: kasfCameraFocus, kasfCameraSetAngleDeg, kasfCameraSetDeclinationDeg, kasfCameraSetDistance, kasfDisableBigGunFiring, kasfForceParadeFormation, kasfGetPlayerMothership, kasfGetShipsClampedTo, kasfHasShipBeenDiscovered, kasfIsIdle, kasfMakeShipsControllable, kasfPingMultipleAddShips, kasfRepair, kasfSetCurrentMimicItem, kasfSetMCVItemIndex, kasfSetSensorsLookAtPoint, kasfShipsCreateLaunch, kasfShipsDockInstant, kasfShipsFinishedLaunching, kasfShipsHyperspaceIn, kasfShipsHyperspaceOut, kasfShipsLaunch, kasfShipsSelectNumber, kasfShipsSetColor, kasfShipsSetInvincible, kasfShipsSetPercentDamaged, kasfShipsSort, kasfShipsUnderAttackElsewhere, kasfShipsUpgrade, kasfStartLevelByHyperspacingShips, kasfStopInstant, kasfWideScreenInDontCloseSensors.

### Mission14 ("(blank)")

`Init_Mission14` `0x00636d30`(2979), `Watch_Mission14` `0x006378e0`(677); block `0x0062aff0`-`0x00637b90`, 52128 bytes, 173 functions.

| FSM (hash) | Team label(s) | Init | Watch | States: name (hash) Init / Watch |
|---|---|---|---|---|
| FleetIntel (`a8a880c`) | FleetIntel | `62aff0`(225) | E1 | Fail (`004a5dc`) `5db0b0`‡(53) / E2; WaitIntel* (`e8e63cc`) `5f4730`‡(29) / `62b0e0`(584); WaitTimer* (`e989af2`) `613880`‡(71) / `62b330`(147); WaitMessage* (`1f32b25`) `613940`‡(40) / `62b810`(1819); S_x6881124? (`6881124`) E2 / `62b3d0`(1073) |
| Naggarok (`5bb641b`) | Naggarok | `62c350`(107) | `62c3c0`(43) | PreDock* (`569932b`) E2 / `62c3f0`(787); PostDock* (`488962b`) E2 / `62c710`(95) |
| DockMimic (`801a8e3`) | DockMimic | `62c770`(48) | E1 | Docking (`937fe67`) `62c7e0`(65) / `62c830`(183); Launching (`a279e47`) `62c8f0`(108) / `62c960`(158); Wait (`005b5e4`) E2 / `62c7a0`(57) |
| BeastMothership (`a2e1ef0`) | BeastMothership | `62ca00`(124) | E1 | InHS* (`004e2d3`) `6151c0`‡(42) / `62caa0`(65); S_x4a93a83? (`4a93a83`) `62caf0`(110) / `62cb60`(724); Null (`0053a0c`) `62ce40`‡(40) / `62ce70`‡(58) |
| Objectives (`98ddc43`) | Objectives | E1 | `62bf30`(1044) | Null (`0053a0c`) E2 / E2 |
| FSM_x678e133? (`678e133`) | MissileGuard1 | E1 | `62d5e0`(65) | Attacking (`857bcb7`) `62d630`(49) / `62d670`(154); Create (`4869685`) `62d710`(133) / `62d7a0`(80) |
| BeastMissiles* (`81169f3`) | Missile1 | `62ceb0`(48) | E1 | Attacking (`857bcb7`) `62cf30`(594) / `62d190`(624); WaitCreate* (`8832365`) `62d400`(152) / `62d4b0`(65); Create (`4869685`) `62d500`(144) / `62d590`(80); InHS* (`004e2d3`) E2 / `62cee0`(65) |
| FSM_x3a6d090? (`3a6d090`) | bHCruise1 | `62d7f0`(74) | E1 | InHS* (`004e2d3`) `6151c0`‡(42) / `62d840`(121); Null (`0053a0c`) `62ce40`‡(40) / `62ce70`‡(58); S_x4a93a83? (`4a93a83`) `62d8c0`(136) / `62d950`(205) |
| Infected (`2a98c74`) | Infected | `62da20`(48) | `62da50`(123) | Attack (`469857b`) `615ad0`‡(49) / `62dad0`(103); Null (`0053a0c`) E2 / `62db40`(57) |
| FOWRecon (`4c6948e`) | FOWRecon | `62db80`(33) | `62dbb0`(72) | - |
| Wave1 (`05b6a61`) | W1pF1, W1pF2, W1tInt1, W1tInt2, pMCorv1, tDefend1, tDefend2, tIFrig1, tSFrig1 | `62dc00`(179) | E1 | Attacking (`857bcb7`) `62dd50`(82) / `62ddb0`(288); Guarding (`9668ae7`) `62ded0`(87) / `62df30`(169); WaitCreate* (`8832365`) `62dfe0`(59) / `62e020`(65); Create (`4869685`) `62e070`(148) / `62e110`(85); Waiting (`b5e8e77`) E2 / `62dcc0`(135); Null (`0053a0c`) E2 / E2 |
| Patrol (`546973c`) | PatrolMCorv1, PatroltInt1, PatroltInt2, PatroltInt3 | `62e170`(74) | E1 | Patrolling (`7464cb7`) `62e1c0`(87) / `62e220`(218); Attacking (`857bcb7`) `62e300`(44) / `62e330`(218); AttackPlayer* (`439e822`) `62e410`‡(126) / `62e490`(64); Hyperspace (`a7c9b35`) `62e4d0`(45) / `62e500`(58) |
| FSM_x62b637b? (`62b637b`) | HangAround1, HangAround2, HangAround3, HangAround4, HangAround5, RepairNaggarok | `62e540`(59) | E1 | Null (`0053a0c`) `62ce40`‡(40) / `62ce70`‡(58); Attacking (`857bcb7`) `62e410`‡(126) / `62ecc0`‡(137); HangingAround* (`705f844`) `62e580`(40) / `62e5b0`(354); Create (`4869685`) `62e720`(499) / `62e920`(80); SetFormation (`46c077e`) `62e970`(29) / `62e990`(78); WaitHyperspace* (`f909e85`) `62e9e0`(53) / `62ea20`(66); Hyperspace (`a7c9b35`) `62ea70`(64) / `62eab0`(65) |
| GravWells (`6bbed73`) | tGrav1, tGrav2, tGrav3 | `62eb00`(60) | `62eb40`(58) | - |
| LastWave* (`689b3a5`) | LastWave1, LastWave2 | `62eb80`(59) | E1 | Pre (`0005565`) `6151c0`‡(42) / `62ebc0`(65); Post (`0055484`) `62ec10`(170) / `62ecc0`‡(137) |
| Carrier (`7676de2`) | tCarrier | `62ed50`(39) | `62ed80`(95) | - |
| ACVs (`00458b3`) | ACV, ACV1, ACV2 | `62ede0`(269) | `62eef0`(239) | Null (`0053a0c`) `60c010`‡(45) / E2; Building (`9e08a57`) `61eb40`‡(40) / `62f1c0`(446); Patrol (`546973c`) `62f510`(586) / `62f760`(222); Attack (`469857b`) `62f840`(551) / `62fa70`(156); Dock (`004937b`) `62fb10`‡(159) / `62fbb0`(675); Create (`4869685`) `63d2b0`‡(163) / `62f380`(392); Wait (`005b5e4`) `64d110`‡(63) / `62fe60`(65); Build (`0479e04`) `65f6f0`‡(40) / `62efe0`(474) |
| Sentinels (`38e7fb3`) | Sentinel, Sentinel1, Sentinel2, Sentinel3 | `62feb0`(269) | `62ffc0`(182) | Null (`0053a0c`) `60c010`‡(45) / E2; Create (`4869685`) `616130`‡(187) / `630420`(409); Building (`9e08a57`) `61eb40`‡(40) / `630260`(446); Defend (`489aa24`) `6305c0`(1046) / `6309e0`(957); Wait (`005b5e4`) `63ece0`‡(261) / `630da0`(70); Build (`0479e04`) `65f6f0`‡(40) / `630080`(474) |
| HiveFrigates* (`215c113`) | HiveFrigate, HiveFrigate1, HiveFrigate2 | `630df0`(269) | `630f00`(182) | Building (`9e08a57`) `61eb40`‡(40) / `6311a0`(446); Create (`4869685`) `64e740`‡(163) / `631360`(389); Attack (`469857b`) `64e980`‡(352) / `6314f0`(75); Build (`0479e04`) `65f6f0`‡(40) / `630fc0`(474); Null (`0053a0c`) `65f6f0`‡(40) / E2 |
| IonArrayFrigates† (`0a9e133`) | IonArrayFrigate, IonArrayFrigate1 | `631540`(302) | `631670`(182) | Create (`4869685`) `5eff10`‡(163) / `631ae0`(262); Building (`9e08a57`) `61eb40`‡(40) / `631920`(446); Move (`00524a5`) `631dd0`‡(195) / `631ea0`(293); Attack (`469857b`) `631fd0`(401) / `632170`(142); Hyperspace (`a7c9b35`) `65b260`‡(40) / `631bf0`(480); Build (`0479e04`) `65f6f0`‡(40) / `631730`(491); Null (`0053a0c`) `65f6f0`‡(40) / E2 |
| CloakedFighters* (`ee25ac3`) | CloakedFighter, CloakedFighter1 | `632200`(302) | `632330`(239) | Launch (`506a278`) `608010`‡(29) / `632fd0`(126); Move (`00524a5`) `6199d0`‡(200) / `632960`(298); Building (`9e08a57`) `61eb40`‡(40) / `632610`(446); Dock (`004937b`) `62fb10`‡(159) / `632c80`(837); Attack (`469857b`) `632a90`(351) / `632bf0`(142); Create (`4869685`) `651d10`‡(163) / `6327d0`(394); Build (`0479e04`) `65f6f0`‡(40) / `632420`(491); Null (`0053a0c`) `65f6f0`‡(40) / E2 |
| MultibeamFrigates* (`be76893`) | MultiBeamFrigate, MultiBeamFrigate1, MultiBeamFrigate2 | `633050`(269) | `633160`(182) | Attack (`469857b`) `61a930`‡(354) / `633750`(91); Building (`9e08a57`) `61eb40`‡(40) / `633400`(446); Create (`4869685`) `652c50`‡(163) / `6335c0`(389); Build (`0479e04`) `65f6f0`‡(40) / `633220`(474); Null (`0053a0c`) `65f6f0`‡(40) / E2 |
| Corvettes (`7aad0e3`) | Corvette, Corvette1, Corvette2 | `6337b0`(269) | `6338c0`(239) | Create (`4869685`) `5f0df0`‡(163) / `633fb0`(723); Building (`9e08a57`) `61eb40`‡(40) / `633df0`(446); Attack (`469857b`) `634290`(722) / `634570`(91); Guard (`04c9664`) `6345d0`(676) / `634880`(64); Dock (`004937b`) `635c40`‡(179) / `6348c0`(684); Build (`0479e04`) `65f6f0`‡(40) / `6339b0`(1074); Null (`0053a0c`) `65f6f0`‡(40) / E2 |
| Fighters (`dbd8ed3`) | Fighter, Fighter1, Fighter2, Fighter3, Fighter4 | `634b70`(269) | `634c80`(239) | Create (`4869685`) `5f2300`‡(163) / `635370`(489); Building (`9e08a57`) `61eb40`‡(40) / `6351b0`(446); Attack (`469857b`) `635580`(771) / `635890`(91); Guard (`04c9664`) `6358f0`(676) / `635ba0`(145); Dock (`004937b`) `635c40`‡(179) / `635d00`(660); Build (`0479e04`) `65f6f0`‡(40) / `634d70`(1074); Null (`0053a0c`) `65f6f0`‡(40) / E2 |
| RammingFrigates† (`4c51573`) | RammingFrigate, RammingFrigate1 | `635fa0`(269) | `6360b0`(182) | Create (`4869685`) `5f3470`‡(163) / `636510`(389); Building (`9e08a57`) `61eb40`‡(40) / `636350`(446); Attack (`469857b`) `645bb0`‡(410) / `6366a0`(91); Build (`0479e04`) `65f6f0`‡(40) / `636170`(474); Null (`0053a0c`) `65f6f0`‡(40) / E2 |
| Beast (`0469684`) | Beast | `636700`(48) | E1 | Wait (`005b5e4`) `636730`(40) / `636760`(65); WaitMessage* (`1f32b25`) E2 / `6367b0`(1158); Null (`0053a0c`) E2 / E2 |
| FSM_xa6c62b3? (`a6c62b3`) | PHSentinel | `636c40`(48) | E1 | Attack (`469857b`) `636c70`(54) / `636cb0`(57); Null (`0053a0c`) E2 / `636cf0`(57) |

kasf: 1977 call sites, 149 distinct. Cataclysm-only kasf used: kasfCameraFocus, kasfCameraSetAngleDeg, kasfCameraSetDeclinationDeg, kasfCameraSetDistance, kasfDisableBigGunFiring, kasfEnableBeastWeaponAutoAttack, kasfEnableNaggarok, kasfEnableShipConstruction, kasfFindSlipgate, kasfFirstShip, kasfForceParadeFormation, kasfGetBuildCost, kasfGetBuildTime, kasfGetDockHost, kasfGetPlayerMothership, kasfGetShipCountSalvaged, kasfGetShipsClampedTo, kasfGiveTechToAI, kasfIsEMPReady, kasfIsGuarding, kasfIsIdle, kasfMakeShipsControllable, kasfPingMultipleAddShips, kasfSetCurrentMimicItem, kasfSetMCVItemIndex, kasfSetSensorsLookAtPoint, kasfSetStayMimicked, kasfShipsCreate, kasfShipsCreateLaunch, kasfShipsDockInstant, kasfShipsHyperspaceInDefault, kasfShipsHyperspaceOut, kasfShipsInsideMe, kasfShipsLaunch, kasfShipsSelectBeastified, kasfShipsSelectEnemyNoStealthy, kasfShipsSelectPlayer, kasfShipsSelectRace, kasfShipsSelectSalvaged, kasfShipsSelectShips, kasfShipsSetBeastInfectionUnlimited, kasfShipsSetColor, kasfShipsSetFormation, kasfShipsSetInvincible, kasfShipsSort, kasfShipsStop, kasfShipsUnderAttackElsewhere, kasfShipsUpgrade, kasfSpecialOn, kasfStartLevelByHyperspacingShips, kasfVectorAdd, kasfVectorNear, kasfWideScreenInDontCloseSensors.

### Mission15 ("Convince Bentusi To Help")

`Init_Mission15` `0x00639ce0`(1215), `Watch_Mission15` `0x0063a1a0`(3121); block `0x00637b90`-`0x0063adf0`, 12896 bytes, 31 functions.

| FSM (hash) | Team label(s) | Init | Watch | States: name (hash) Init / Watch |
|---|---|---|---|---|
| Objectives (`98ddc43`) | Objectives | E1 | `638740`(410) | - |
| FleetIntel (`a8a880c`) | FleetIntel | `637b90`(189) | E1 | WaitMessage* (`1f32b25`) `613940`‡(40) / `637c50`(2587); Null (`0053a0c`) E2 / E2 |
| GateNode (`68a3735`) | GateNode | `6388e0`(48) | E1 | Peace (`0549575`) E2 / `638910`(292); Attacked (`9857bf4`) E2 / `638a40`(220); Null (`0053a0c`) E2 / E2 |
| Stations (`868e7b3`) | Station1, Station11, Station15, Station19, Station2, Station23, Station27, Station3, Station7 | `638b20`(147) | `638bc0`(454) | Wait (`005b5e4`) `5e3410`‡(29) / `638fa0`(94); S_xe6faab5? (`e6faab5`) `5fd250`‡(29) / `638eb0`(65); MoveToSlipgate* (`44e27e5`) `638d90`(44) / `638dc0`(226); EnterSlipgate* (`adc11c5`) `638f00`(67) / `638f50`(68); Null (`0053a0c`) `65f6f0`‡(40) / E2 |
| Fighters (`dbd8ed3`) | Fighter, Fighter1, Fighter2, Fighter3 | `639000`(40) | `639030`(460) | Build (`0479e04`) `639200`(62) / `639240`(366); Attack (`469857b`) `6393d0`(467) / `6395b0`(138); Attack2 (`6985782`) `639640`(390) / `6397d0`(97); Wait (`005b5e4`) `639840`(171) / `6398f0`(157); Null (`0053a0c`) `65f6f0`‡(40) / E2 |
| NewStations† (`83a90b3`) | NewStation1, NewStation2, NewStation3 | `639990`(135) | `639a20`(151) | Attack (`469857b`) `639b30`(326) / `639c80`(91); Hyperspace (`a7c9b35`) E2 / `639ac0`(104); Null (`0053a0c`) `65f6f0`‡(40) / E2 |

kasf: 493 call sites, 100 distinct. Cataclysm-only kasf used: kasfCameraFocus, kasfCameraSetAngleDeg, kasfCameraSetDeclinationDeg, kasfCameraSetDistance, kasfDisableBigGunFiring, kasfEnableShipConstruction, kasfEnteringSlipgate, kasfFindSlipgate, kasfGetPlayerMothership, kasfGiveTechToAI, kasfHaltAllPlayerShips, kasfIsIdle, kasfPingMultipleAddShips, kasfSetCurrentMimicItem, kasfSetMCVItemIndex, kasfShipsCreateLaunch, kasfShipsHealthAverage, kasfShipsInsideMe, kasfShipsSelectEnemyNoStealthy, kasfShipsSelectPlayer, kasfShipsSelectShips, kasfShipsSetInvincible, kasfShipsSetPercentDamaged, kasfShipsSort, kasfShipsUnderAttackElsewhere, kasfShipsUpgrade, kasfSlipgateDock, kasfSlipgateExplode, kasfStartLevelByHyperspacingShips, kasfVectorAdd, kasfVectorNear.

### Mission16 ("(Empty)")

`Init_Mission16` `0x00647c90`(2194), `Watch_Mission16` `0x00648530`(651); block `0x0063adf0`-`0x006487c0`, 55760 bytes, 162 functions.

| FSM (hash) | Team label(s) | Init | Watch | States: name (hash) Init / Watch |
|---|---|---|---|---|
| FSM_x678e133? (`678e133`) | MissileGuard | E1 | `63ca50`(65) | Attacking (`857bcb7`) `63caa0`(44) / `63cad0`(60); Create (`4869685`) `63cb10`(117) / `63cb90`(57) |
| FleetIntel (`a8a880c`) | FleetIntel | `63adf0`(225) | E1 | Fail (`004a5dc`) `5db0b0`‡(53) / E2; WaitIntel* (`e8e63cc`) `5f4730`‡(29) / `63aee0`(556); WaitTimer* (`e989af2`) `613880`‡(71) / `63b110`(106); WaitMessage* (`1f32b25`) E2 / `63b180`(2237) |
| Objectives (`98ddc43`) | Objectives | `63ba40`(63) | `63ba80`(124) | Wave1 (`05b6a61`) E2 / `63bb00`(581); Wave2 (`05b6a62`) E2 / `63bd50`(353) |
| Cleesan (`809a71e`) | Cleesan | `63bec0`(75) | E1 | Damaged (`8615bd4`) `63c0d0`(33) / `63c100`(442); Normal (`534715c`) E2 / `63bf10`(444); Dead (`0048954`) E2 / E2 |
| DockMimic (`801a8e3`) | DockMimic | `63c2c0`(48) | E1 | Docking (`937fe67`) `63c330`(82) / `63c390`(133); Launching (`a279e47`) `63c420`(130) / E2; Wait (`005b5e4`) E2 / `63c2f0`(57) |
| CleesanEscort* (`aaee354`) | bHCruiser1 | `63c4b0`(48) | E1 | AttackingPlayer* (`427d112`) `63c5b0`(50) / `63c5f0`(145); S_x91c5f69? (`91c5f69`) E2 / `63c4e0`(204) |
| FSM_x6c02300? (`6c02300`) | bMothership | `63c690`(59) | `63c6d0`(272) | - |
| BeastMissiles* (`81169f3`) | Missile1 | `63c7e0`(48) | E1 | Attacking (`857bcb7`) `63c860`(101) / `63c8d0`(57); WaitCreate* (`8832365`) `63c910`(48) / `63c940`(65); Create (`4869685`) `63c990`(128) / `63ca10`(57); InHS* (`004e2d3`) E2 / `63c810`(65) |
| Infected (`2a98c74`) | Infected | `63cbd0`(48) | `63cc00`‡(91) | Attack (`469857b`) `615ad0`‡(49) / `63cc60`(103); Null (`0053a0c`) E2 / `63ccd0`(57) |
| ACVs (`00458b3`) | ACV, ACV1, ACV2 | `63cd10`(269) | `63ce20`(239) | Null (`0053a0c`) `60c010`‡(45) / E2; Building (`9e08a57`) `61eb40`‡(40) / `63d0f0`(446); Create (`4869685`) `63d2b0`‡(163) / `63d360`(392); Patrol (`546973c`) `63d4f0`(586) / `63d740`(222); Attack (`469857b`) `63d820`(551) / `63da50`(156); Dock (`004937b`) `6413e0`‡(159) / `63daf0`(675); Wait (`005b5e4`) `64d110`‡(63) / `63dda0`(65); Build (`0479e04`) `65f6f0`‡(40) / `63cf10`(474) |
| Sentinels (`38e7fb3`) | Sentinel, Sentinel1, Sentinel2, Sentinel3 | `63ddf0`(269) | `63df00`(182) | Null (`0053a0c`) `60c010`‡(45) / E2; Create (`4869685`) `616130`‡(187) / `63e360`(409); Building (`9e08a57`) `61eb40`‡(40) / `63e1a0`(446); Defend (`489aa24`) `63e500`(1046) / `63e920`(957); Wait (`005b5e4`) `63ece0`‡(261) / `63edf0`(70); Build (`0479e04`) `65f6f0`‡(40) / `63dfc0`(474) |
| HiveFrigates* (`215c113`) | HiveFrigate, HiveFrigate1, HiveFrigate2 | `63ee40`(269) | `63ef50`(182) | Building (`9e08a57`) `61eb40`‡(40) / `63f1f0`(446); Create (`4869685`) `64e740`‡(163) / `63f3b0`(389); Attack (`469857b`) `64e980`‡(352) / `63f540`(75); Build (`0479e04`) `65f6f0`‡(40) / `63f010`(474); Null (`0053a0c`) `65f6f0`‡(40) / E2 |
| Harvest (`c67aac4`) | Harvest, Harvest1, Harvest2, Harvest3 | `63f590`(269) | `63f6a0`(182) | Create (`4869685`) `5eea30`‡(163) / `63fb00`(389); Harvest (`c67aac4`) `617a10`‡(29) / `63fc90`(213); Building (`9e08a57`) `61eb40`‡(40) / `63f940`(446); Build (`0479e04`) `65f6f0`‡(40) / `63f760`(474); Null (`0053a0c`) `65f6f0`‡(40) / E2 |
| IonArrayFrigates† (`0a9e133`) | IonArrayFrigate, IonArrayFrigate1, IonArrayFrigate2 | `63fd70`(302) | `63fea0`(182) | Create (`4869685`) `5eff10`‡(163) / `640310`(262); Building (`9e08a57`) `61eb40`‡(40) / `640150`(446); Move (`00524a5`) `631dd0`‡(195) / `640600`(293); Attack (`469857b`) `640730`(401) / `6408d0`(142); Hyperspace (`a7c9b35`) `65b260`‡(40) / `640420`(480); Build (`0479e04`) `65f6f0`‡(40) / `63ff60`(491); Null (`0053a0c`) `65f6f0`‡(40) / E2 |
| CloakedFighters* (`ee25ac3`) | CloakedFighter, CloakedFighter1 | `640960`(302) | `640a90`(239) | Launch (`506a278`) `608010`‡(29) / `6417d0`(126); Move (`00524a5`) `6199d0`‡(200) / `6410c0`(298); Building (`9e08a57`) `61eb40`‡(40) / `640d70`(446); Attack (`469857b`) `6411f0`(351) / `641350`(142); Dock (`004937b`) `6413e0`‡(159) / `641480`(837); Create (`4869685`) `651d10`‡(163) / `640f30`(394); Build (`0479e04`) `65f6f0`‡(40) / `640b80`(491); Null (`0053a0c`) `65f6f0`‡(40) / E2 |
| MultibeamFrigates* (`be76893`) | MultiBeamFrigate, MultiBeamFrigate1, MultiBeamFrigate2 | `641850`(269) | `641960`(182) | Attack (`469857b`) `61a930`‡(354) / `641f50`(91); Building (`9e08a57`) `61eb40`‡(40) / `641c00`(446); Create (`4869685`) `652c50`‡(163) / `641dc0`(389); Build (`0479e04`) `65f6f0`‡(40) / `641a20`(474); Null (`0053a0c`) `65f6f0`‡(40) / E2 |
| Corvettes (`7aad0e3`) | Corvette, Corvette1, Corvette2, Corvette3, Corvette4 | `641fb0`(269) | `6420c0`(239) | Create (`4869685`) `5f0df0`‡(163) / `6427b0`(723); Building (`9e08a57`) `61eb40`‡(40) / `6425f0`(446); Attack (`469857b`) `642a90`(722) / `642d70`(91); Guard (`04c9664`) `642dd0`(676) / `643080`(64); Dock (`004937b`) `646540`‡(179) / `6430c0`(684); Build (`0479e04`) `65f6f0`‡(40) / `6421b0`(1074); Null (`0053a0c`) `65f6f0`‡(40) / E2 |
| Fighters (`dbd8ed3`) | Fighter, Fighter1, Fighter2, Fighter3, Fighter4, Fighter5 | `643370`(269) | `643480`(239) | Create (`4869685`) `5f2300`‡(163) / `643b70`(489); Building (`9e08a57`) `61eb40`‡(40) / `6439b0`(446); Attack (`469857b`) `643d80`(771) / `644090`(91); Guard (`04c9664`) `6440f0`(676) / `6443a0`(145); Dock (`004937b`) `646540`‡(179) / `644440`(660); Build (`0479e04`) `65f6f0`‡(40) / `643570`(1074); Null (`0053a0c`) `65f6f0`‡(40) / E2 |
| Recons (`5698433`) | Recon, Recon1, Recon2, Recon3 | `6446e0`(269) | `6447f0`(239) | Building (`9e08a57`) `61eb40`‡(40) / `644ac0`(446); Attack (`469857b`) `644e10`(544) / `645030`(91); Patrol (`546973c`) `645090`(210) / `645170`(145); Dock (`004937b`) `646540`‡(179) / `645210`(662); Create (`4869685`) `658650`‡(163) / `644c80`(398); Build (`0479e04`) `65f6f0`‡(40) / `6448e0`(474); Null (`0053a0c`) `65f6f0`‡(40) / E2 |
| RammingFrigates† (`4c51573`) | RammingFrigate, RammingFrigate1 | `6454b0`(269) | `6455c0`(182) | Create (`4869685`) `5f3470`‡(163) / `645a20`(389); Building (`9e08a57`) `61eb40`‡(40) / `645860`(446); Attack (`469857b`) `645bb0`‡(410) / `645d50`(91); Build (`0479e04`) `65f6f0`‡(40) / `645680`(474); Null (`0053a0c`) `65f6f0`‡(40) / E2 |
| DFGs† (`0048ac3`) | DFG, DFG1 | `645db0`(269) | `645ec0`(239) | Building (`9e08a57`) `61eb40`‡(40) / `646190`(446); Dock (`004937b`) `646540`‡(179) / `646600`(660); Create (`4869685`) `659f50`‡(163) / `646350`(398); Guard (`04c9664`) `65a190`‡(493) / `6464e0`(81); Build (`0479e04`) `65f6f0`‡(40) / `645fb0`(474); Null (`0053a0c`) `65f6f0`‡(40) / E2 |
| HeavyCruisers* (`b08c7b3`) | HeavyCruiser, HeavyCruiser1, HeavyCruiser2 | `6468a0`(312) | `6469e0`(283) | Attack (`469857b`) `61a930`‡(354) / `647350`(91); Building (`9e08a57`) `61eb40`‡(40) / `646ed0`(446); Beastify* (`96889d9`) `646b00`(334) / `646c50`(159); Create (`4869685`) `6539a0`‡(163) / `647090`(279); Hyperspace (`a7c9b35`) `65b260`‡(40) / `6471b0`(404); Build (`0479e04`) `65f6f0`‡(40) / `646cf0`(474); Null (`0053a0c`) `65f6f0`‡(40) / E2 |
| Beast (`0469684`) | Beast | `6473b0`(48) | E1 | Wait (`005b5e4`) `5f38f0`‡(40) / `6473e0`(65); WaitMessage* (`1f32b25`) `613940`‡(40) / `647430`(1799) |
| Carrier (`7676de2`) | bCarrier1 | `647b40`(76) | `5e0290`‡(74) | Move (`00524a5`) `647be0`(44) / `647c10`(125); Wait (`005b5e4`) E2 / `647b90`(79); Null (`0053a0c`) `65f6f0`‡(40) / E2 |

kasf: 2214 call sites, 139 distinct. Cataclysm-only kasf used: kasfCameraFocus, kasfCameraSetAngleDeg, kasfCameraSetDeclinationDeg, kasfCameraSetDistance, kasfEnableBeastWeaponAutoAttack, kasfFirstShip, kasfForceParadeFormation, kasfGetBuildCost, kasfGetBuildTime, kasfGetDockHost, kasfGiveTechToAI, kasfHasShipBeenDiscovered, kasfIsBeastReady, kasfIsEMPReady, kasfIsGuarding, kasfIsIdle, kasfSetCurrentMimicItem, kasfSetMCVItemIndex, kasfSetSensorsLookAtPoint, kasfShipsCreate, kasfShipsCreateLaunch, kasfShipsDockInstant, kasfShipsHyperspaceInDefault, kasfShipsInsideMe, kasfShipsScuttle, kasfShipsSelectBeastified, kasfShipsSelectEnemyNoStealthy, kasfShipsSelectNotIdle, kasfShipsSelectPlayer, kasfShipsSelectRace, kasfShipsSelectShips, kasfShipsSetColor, kasfShipsSetFormation, kasfShipsSetInvincible, kasfShipsSort, kasfShipsStop, kasfShipsStopForResearchDocking, kasfShipsUnderAttackElsewhere, kasfShipsUpgrade, kasfSpecialOn, kasfStartLevelByHyperspacingShips, kasfTeamAttributesBitTest, kasfVectorAdd, kasfVectorCopy, kasfVectorNear, kasfWideScreenInDontCloseSensors.

### Mission17 ("(Empty)")

`Init_Mission17` `0x00666510`(10741), `Watch_Mission17` `0x00668f20`(1540); block `0x006487c0`-`0x00669530`, 134512 bytes, 383 functions.

| FSM (hash) | Team label(s) | Init | Watch | States: name (hash) Init / Watch |
|---|---|---|---|---|
| Objectives (`98ddc43`) | Objectives | E1 | `64a160`(690) | - |
| FleetIntel (`a8a880c`) | FleetIntel | `6487c0`(189) | E1 | Victory (`ad89429`) `5f4730`‡(29) / `648880`(62); WaitIntel* (`e8e63cc`) `5f4730`‡(29) / `6488c0`(486); WaitMessage* (`1f32b25`) `613940`‡(40) / `648b00`(5516); Wait (`005b5e4`) E2 / `648ab0`(77); Null (`0053a0c`) E2 / E2 |
| Bentusi (`6a39a39`) | Bentusi | `64a420`(42) | `64a450`(65) | Wait (`005b5e4`) `64a4a0`(59) / `64a4e0`(70); Null (`0053a0c`) E2 / E2 |
| NomadMoon (`159517e`) | NomadMoon | `64a530`(120) | `64a5b0`(292) | Wait (`005b5e4`) E2 / `64a6e0`(64); Fire (`004ae65`) E2 / `64a720`(420); Defend (`489aa24`) E2 / E2; Null (`0053a0c`) `65f6f0`‡(40) / E2 |
| Naggarok (`5bb641b`) | Naggarok | `64a8d0`(161) | `64a980`(458) | RunFar* (`57a2a62`) `64ab50`(140) / `64abe0`(151); RunNear* (`7a32932`) `64ac80`(132) / `64ad10`(365); DQT (`0004964`) `64af10`‡(39) / `64af40`(145); KeepSafe* (`9a57155`) `64af10`‡(39) / `64b360`(261); Fire (`004ae65`) `64afe0`(186) / `64b0b0`(169); Avoid (`046b3d4`) `64b160`(193) / `64b230`(293); Helpless (`a150e43`) `64b470`(144) / `64b500`(65); Run (`000579e`) `64b550`(319) / `64b690`(277); Wait (`005b5e4`) E2 / `64ae80`(140); Null (`0053a0c`) E2 / E2 |
| Missiles (`e87e5b3`) | Missiles1, Missiles2, Missiles3, Missiles4, Missiles5, Missiles6, Missiles7, Missiles8, missiles | `64b7b0`(56) | `64b7f0`(239) | Launch (`506a278`) `64b8e0`(56) / `64b920`(69); Attack (`469857b`) `64b970`(366) / `64bae0`(91); Null (`0053a0c`) `65f6f0`‡(40) / E2 |
| Crystals (`7e88293`) | Crystals | `64bb40`(44) | `64bb70`(168) | - |
| Beast (`0469684`) | Beast | `64bc20`(64) | `64bc60`(995) | - |
| Infected (`2a98c74`) | Infected | `64c050`(48) | `63cc00`‡(91) | Attack (`469857b`) `615ad0`‡(49) / `64c080`(103); Null (`0053a0c`) E2 / `64c0f0`(57) |
| ACVs (`00458b3`) | ACV, ACV1, ACV2, ACV3, ACV4, ACV5, ACV6, ACV7 | `64c130`(269) | `64c240`(239) | Null (`0053a0c`) `60c010`‡(45) / E2; Building (`9e08a57`) `61eb40`‡(40) / `64c510`(446); Create (`4869685`) `63d2b0`‡(163) / `64c6d0`(392); Patrol (`546973c`) `64c860`(586) / `64cab0`(222); Attack (`469857b`) `64cb90`(551) / `64cdc0`(156); Wait (`005b5e4`) `64d110`‡(63) / `64d150`(65); Dock (`004937b`) `652270`‡(159) / `64ce60`(675); Build (`0479e04`) `65f6f0`‡(40) / `64c330`(474) |
| Sentinels (`38e7fb3`) | Sentinel, Sentinel1, Sentinel10, Sentinel11, Sentinel12, Sentinel13, Sentinel14, Sentinel15, Sentinel16, Sentinel2, Sentinel3, Sentinel4, Sentinel5, Sentinel6, Sentinel7, Sentinel8, Sentinel9 | `64d1a0`(269) | `64d2b0`(182) | Null (`0053a0c`) `60c010`‡(45) / E2; Create (`4869685`) `616130`‡(187) / `64d710`(409); Building (`9e08a57`) `61eb40`‡(40) / `64d550`(446); Wait (`005b5e4`) `63ece0`‡(261) / `64e180`(70); Defend (`489aa24`) `64d8b0`(1197) / `64dd60`(1054); Build (`0479e04`) `65f6f0`‡(40) / `64d370`(474) |
| HiveFrigates* (`215c113`) | HiveFrigate, HiveFrigate1, HiveFrigate2, HiveFrigate3, HiveFrigate4, HiveFrigate5, HiveFrigate6, HiveFrigate7, HiveFrigate8 | `64e1d0`(269) | `64e2e0`(182) | Building (`9e08a57`) `61eb40`‡(40) / `64e580`(446); Create (`4869685`) `64e740`‡(163) / `64e7f0`(389); Attack (`469857b`) `64e980`‡(352) / `64eae0`(75); Build (`0479e04`) `65f6f0`‡(40) / `64e3a0`(474); Null (`0053a0c`) `65f6f0`‡(40) / E2 |
| Harvest (`c67aac4`) | Harvest, Harvest1, Harvest2, Harvest3 | `64eb30`(269) | `64ec40`(248) | Create (`4869685`) `5eea30`‡(163) / `64f0e0`(389); Harvest (`c67aac4`) `617a10`‡(29) / `64f270`(213); Building (`9e08a57`) `61eb40`‡(40) / `64ef20`(446); Heal* (`004c95c`) `64f350`(56) / `64f390`(241); Build (`0479e04`) `65f6f0`‡(40) / `64ed40`(474); Null (`0053a0c`) `65f6f0`‡(40) / E2 |
| Processors (`7ad3343`) | Processor, Processor1, Processor2 | `64f490`(269) | `64f5a0`(182) | Building (`9e08a57`) `61eb40`‡(40) / `64f840`(446); Create (`4869685`) `64fa00`(175) / `64fab0`(262); Guard (`04c9664`) `64fd90`(379) / `64ff10`(64); Wait (`005b5e4`) `64ff50`(53) / `64ff90`(65); Hyperspace (`a7c9b35`) `65b260`‡(40) / `64fbc0`(463); Build (`0479e04`) `65f6f0`‡(40) / `64f660`(474); Null (`0053a0c`) `65f6f0`‡(40) / E2 |
| Workers (`c46fa23`) | Worker1, Worker2, Worker3, worker | `64ffe0`(269) | `6500f0`(182) | Create (`4869685`) `5ef2c0`‡(163) / `650550`(389); Harvest (`c67aac4`) `617a10`‡(29) / `6506e0`(418); Building (`9e08a57`) `61eb40`‡(40) / `650390`(446); Heal* (`004c95c`) `650890`(262) / `6509a0`(120); Salvage (`761a5e5`) `650a20`(229) / `650b10`(64); Build (`0479e04`) `65f6f0`‡(40) / `6501b0`(474); Null (`0053a0c`) `65f6f0`‡(40) / E2 |
| IonArrayFrigates† (`0a9e133`) | IonArrayFrigate, IonArrayFrigate1, IonArrayFrigate2, IonArrayFrigate3, IonArrayFrigate4 | `650b50`(302) | `650c80`(182) | Create (`4869685`) `5eff10`‡(163) / `6510f0`(262); Building (`9e08a57`) `61eb40`‡(40) / `650f30`(446); Move (`00524a5`) `631dd0`‡(195) / `6513e0`(293); Attack (`469857b`) `651510`(401) / `6516b0`(142); Hyperspace (`a7c9b35`) `65b260`‡(40) / `651200`(480); Build (`0479e04`) `65f6f0`‡(40) / `650d40`(491); Null (`0053a0c`) `65f6f0`‡(40) / E2 |
| CloakedFighters* (`ee25ac3`) | CloakedFighter, CloakedFighter1, CloakedFighter2 | `651740`(302) | `651870`(239) | Launch (`506a278`) `608010`‡(29) / `652660`(126); Move (`00524a5`) `6199d0`‡(200) / `651f50`(298); Building (`9e08a57`) `61eb40`‡(40) / `651b50`(446); Create (`4869685`) `651d10`‡(163) / `651dc0`(394); Attack (`469857b`) `652080`(351) / `6521e0`(142); Dock (`004937b`) `652270`‡(159) / `652310`(837); Build (`0479e04`) `65f6f0`‡(40) / `651960`(491); Null (`0053a0c`) `65f6f0`‡(40) / E2 |
| MultibeamFrigates* (`be76893`) | (computed), MultiBeamFrigate, MultiBeamFrigate1, MultiBeamFrigate2, MultiBeamFrigate3, MultiBeamFrigate4 | `6526e0`(269) | `6527f0`(182) | Attack (`469857b`) `61a930`‡(354) / `652e90`(91); Building (`9e08a57`) `61eb40`‡(40) / `652a90`(446); Create (`4869685`) `652c50`‡(163) / `652d00`(389); Build (`0479e04`) `65f6f0`‡(40) / `6528b0`(474); Null (`0053a0c`) `65f6f0`‡(40) / E2 |
| HeavyCruisers* (`b08c7b3`) | (computed), HeavyCruiser, HeavyCruiser1, HeavyCruiser10, HeavyCruiser11, HeavyCruiser12, HeavyCruiser2, HeavyCruiser3, HeavyCruiser4, HeavyCruiser5, HeavyCruiser6, HeavyCruiser7, HeavyCruiser8, HeavyCruiser9 | `652f10`(322) | `653060`(392) | Attack (`469857b`) `61a930`‡(354) / `653d10`(91); Building (`9e08a57`) `61eb40`‡(40) / `6537e0`(446); Beastify* (`96889d9`) `6531f0`(464) / `6533c0`(159); BeastifyCheat* (`d4141f4`) `653460`(247) / `653560`(147); Create (`4869685`) `6539a0`‡(163) / `653a50`(279); Hyperspace (`a7c9b35`) `65b260`‡(40) / `653b70`(404); Null (`0053a0c`) `65f6f0`‡(40) / E2; Build (`0479e04`) `65f6f0`‡(40) / `653600`(474) |
| Carriers (`676de03`) | Carrier, Carrier1, Carrier2, Carrier3, Carrier4 | `653d70`(305) | `653eb0`(588) | Building (`9e08a57`) `61eb40`‡(40) / `6546e0`(446); Beastify* (`96889d9`) `654100`(464) / `6542d0`(136); BeastifyCheat* (`d4141f4`) `654360`(247) / `654460`(159); Create (`4869685`) `6548a0`(163) / `654950`(255); Move (`00524a5`) `654bf0`(510) / `654df0`(415); Attack (`469857b`) `654f90`(304) / `6550c0`(216); Hyperspace (`a7c9b35`) `65b260`‡(40) / `654a50`(404); Null (`0053a0c`) `65f6f0`‡(40) / E2; Build (`0479e04`) `65f6f0`‡(40) / `654500`(474) |
| Corvettes (`7aad0e3`) | Corvette, Corvette1, Corvette10, Corvette11, Corvette12, Corvette13, Corvette14, Corvette2, Corvette3, Corvette4, Corvette5, Corvette6, Corvette7, Corvette8, Corvette9 | `6551a0`(269) | `6552b0`(239) | Create (`4869685`) `5f0df0`‡(163) / `6559a0`(723); Building (`9e08a57`) `61eb40`‡(40) / `6557e0`(446); Attack (`469857b`) `655c80`(766) / `655f80`(91); Guard (`04c9664`) `655fe0`(720) / `6562b0`(64); Dock (`004937b`) `658c90`‡(179) / `6562f0`(684); Null (`0053a0c`) `65f6f0`‡(40) / E2; Build (`0479e04`) `65f6f0`‡(40) / `6553a0`(1074) |
| Fighters (`dbd8ed3`) | Fighter, Fighter1, Fighter2, Fighter3 | `6565a0`(269) | `6566b0`(239) | Create (`4869685`) `5f2300`‡(163) / `656da0`(489); Building (`9e08a57`) `61eb40`‡(40) / `656be0`(446); Attack (`469857b`) `656fb0`(815) / `6572e0`(91); Guard (`04c9664`) `657340`(720) / `657610`(145); Dock (`004937b`) `658c90`‡(179) / `6576b0`(660); Null (`0053a0c`) `65f6f0`‡(40) / E2; Build (`0479e04`) `65f6f0`‡(40) / `6567a0`(1074) |
| RammingFrigates† (`4c51573`) | RammingFrigate, RammingFrigate1, RammingFrigate10, RammingFrigate2, RammingFrigate3, RammingFrigate4, RammingFrigate5, RammingFrigate6, RammingFrigate7, RammingFrigate8, RammingFrigate9 | `657950`(269) | `657a60`(182) | Create (`4869685`) `5f3470`‡(163) / `657ec0`(389); Building (`9e08a57`) `61eb40`‡(40) / `657d00`(446); Attack (`469857b`) `645bb0`‡(410) / `658050`(91); Null (`0053a0c`) `65f6f0`‡(40) / E2; Build (`0479e04`) `65f6f0`‡(40) / `657b20`(474) |
| Recons (`5698433`) | Recon, Recon1, Recon2, Recon3, Recon4, Recon5 | `6580b0`(269) | `6581c0`(239) | Building (`9e08a57`) `61eb40`‡(40) / `658490`(446); Create (`4869685`) `658650`‡(163) / `658700`(398); Attack (`469857b`) `658890`(544) / `658ab0`(91); Patrol (`546973c`) `658b10`(210) / `658bf0`(145); Dock (`004937b`) `658c90`‡(179) / `658d50`(662); Null (`0053a0c`) `65f6f0`‡(40) / E2; Build (`0479e04`) `65f6f0`‡(40) / `6582b0`(474) |
| CruiseMissiles* (`919be33`) | CruiseMissile, CruiseMissile1 | `658ff0`(269) | `659100`(182) | Building (`9e08a57`) `61eb40`‡(40) / `6593a0`(446); Create (`4869685`) `659560`(163) / `659610`(389); Attack (`469857b`) `6597a0`(425) / `659950`(91); Null (`0053a0c`) `65f6f0`‡(40) / E2; Build (`0479e04`) `65f6f0`‡(40) / `6591c0`(474) |
| DFGs† (`0048ac3`) | DFG, DFG1, DFG2, DFG3, DFG4, DFG5 | `6599b0`(269) | `659ac0`(239) | Building (`9e08a57`) `61eb40`‡(40) / `659d90`(446); Create (`4869685`) `659f50`‡(163) / `65a000`(398); Guard (`04c9664`) `65a190`‡(493) / `65a380`(81); Dock (`004937b`) `65a3e0`‡(179) / `65a4a0`(660); Null (`0053a0c`) `65f6f0`‡(40) / E2; Build (`0479e04`) `65f6f0`‡(40) / `659bb0`(474) |
| Kushan (`50a7c5e`) | Kushan | `65a740`(112) | `65a7b0`(1414) | - |
| kHeavyCruisers† (`b08c303`) | kHeavyCruiser, kHeavyCruiser1, kHeavyCruiser2, kHeavyCruiser3, kHeavyCruiser4 | `65ad40`(247) | `65ae40`(334) | Building (`9e08a57`) `61eb40`‡(40) / `65b030`(198); Create (`4869685`) `65b100`‡(76) / `65b150`(271); Hyperspace (`a7c9b35`) `65b260`‡(40) / `65b290`(385); Attack (`469857b`) `65de70`‡(241) / `65b420`(91); Null (`0053a0c`) `65f6f0`‡(40) / E2; Build (`0479e04`) `65f6f0`‡(40) / `65af90`(159) |
| kCarriers† (`6769503`) | kCarrier, kCarrier1, kCarrier2 | `65b480`(264) | `65b590`(334) | Building (`9e08a57`) `61eb40`‡(40) / `65b780`(198); Hyperspace (`a7c9b35`) `65b260`‡(40) / `65b960`(385); Move (`00524a5`) `65baf0`(328) / `65bc40`(162); Attack (`469857b`) `65de70`‡(241) / `65bcf0`(115); Null (`0053a0c`) `65f6f0`‡(40) / E2; Build (`0479e04`) `65f6f0`‡(40) / `65b6e0`(159); Create (`4869685`) `65f890`‡(76) / `65b850`(271) |
| kFighters† (`dbdc5d3`) | kFighter, kFighter1, kFighter2, kFighter3, kFighter4, kFighter5, kFighter6 | `65bd70`(247) | `65be70`(239) | Building (`9e08a57`) `61eb40`‡(40) / `65c000`(198); Create (`4869685`) `65c0d0`(76) / `65c120`(444); Attack (`469857b`) `65c2f0`‡(211) / `65c3d0`(91); Dock (`004937b`) `65c430`(179) / `65c4f0`(330); Null (`0053a0c`) `65f6f0`‡(40) / E2; Build (`0479e04`) `65f6f0`‡(40) / `65bf60`(159) |
| kAdvancedSupportFrigates† (`50312d3`) | kAdvancedSupportFrigate, kAdvancedSupportFrigate1, kAdvancedSupportFrigate2, kAdvancedSupportFrigate3 | `65c640`(247) | `65c740`(182) | Building (`9e08a57`) `61eb40`‡(40) / `65c920`(198); Move (`00524a5`) `65cb80`(414) / `65cd20`(396); Heal* (`004c95c`) `65ceb0`(377) / `65d030`(120); Null (`0053a0c`) `65f6f0`‡(40) / E2; Build (`0479e04`) `65f6f0`‡(40) / `65c800`(278); Create (`4869685`) `6603d0`‡(76) / `65c9f0`(385) |
| kStandardDestroyers† (`f943223`) | kStandardDestroyer, kStandardDestroyer1, kStandardDestroyer2 | `65d0b0`(247) | `65d1b0`(334) | Building (`9e08a57`) `61eb40`‡(40) / `65d3a0`(198); Hyperspace (`a7c9b35`) `65b260`‡(40) / `65d5d0`(385); Create (`4869685`) `65d470`‡(76) / `65d4c0`(271); Attack (`469857b`) `65de70`‡(241) / `65d760`(91); Null (`0053a0c`) `65f6f0`‡(40) / E2; Build (`0479e04`) `65f6f0`‡(40) / `65d300`(159) |
| kIonCannonFrigates† (`436e863`) | kIonCannonFrigate, kIonCannonFrigate1, kIonCannonFrigate2, kIonCannonFrigate3 | `65d7c0`(247) | `65d8c0`(334) | Building (`9e08a57`) `61eb40`‡(40) / `65dab0`(198); Hyperspace (`a7c9b35`) `65b260`‡(40) / `65dce0`(385); Create (`4869685`) `65db80`‡(76) / `65dbd0`(271); Attack (`469857b`) `65de70`‡(241) / `65df70`(91); Null (`0053a0c`) `65f6f0`‡(40) / E2; Build (`0479e04`) `65f6f0`‡(40) / `65da10`(159) |
| kCorvettes† (`7ae60e3`) | kCorvette, kCorvette1, kCorvette2 | `65dfd0`(247) | `65e0d0`(239) | Building (`9e08a57`) `61eb40`‡(40) / `65e2e0`(198); Dock (`004937b`) `65a3e0`‡(179) / `65e5f0`(330); Attack (`469857b`) `65c2f0`‡(211) / `65e590`(91); Create (`4869685`) `65e3b0`‡(76) / `65e400`(385); Null (`0053a0c`) `65f6f0`‡(40) / E2; Build (`0479e04`) `65f6f0`‡(40) / `65e1c0`(278) |
| Taiidan (`85dd80e`) | Taiidan | `65e740`(100) | `65e7b0`(861) | - |
| Gravitationsfeldes (`b08c273`) | tHeavyCruiser, tHeavyCruiser1, tHeavyCruiser2 | `65eb10`(293) | `65ec40`(334) | Building (`9e08a57`) `61eb40`‡(40) / `65ee30`(198); Create (`4869685`) `65b100`‡(76) / `65ef00`(299); Hyperspace (`a7c9b35`) `65b260`‡(40) / `65f030`(385); Attack (`469857b`) `65f1c0`(565) / `65f400`(91); Null (`0053a0c`) `65f6f0`‡(40) / E2; Build (`0479e04`) `65f6f0`‡(40) / `65ed90`(159) |
| tCarriers† (`6768203`) | tCarrier, tCarrier1, tCarrier2 | `65f460`(309) | `65f5a0`(334) | Building (`9e08a57`) `61eb40`‡(40) / `65f7c0`(198); Hyperspace (`a7c9b35`) `65b260`‡(40) / `65fa10`(385); Null (`0053a0c`) `65f6f0`‡(40) / E2; Build (`0479e04`) `65f6f0`‡(40) / `65f720`(159); Create (`4869685`) `65f890`‡(76) / `65f8e0`(299); Move (`00524a5`) `65fba0`(539) / `65fdc0`(450); Attack (`469857b`) `6610b0`‡(598) / `65ff90`(115) |
| tAdvancedSupportFrigates† (`41312d3`) | tAdvancedSupportFrigate, tAdvancedSupportFrigate1, tAdvancedSupportFrigate2 | `660010`(269) | `660120`(182) | Building (`9e08a57`) `61eb40`‡(40) / `660300`(198); Null (`0053a0c`) `65f6f0`‡(40) / E2; Build (`0479e04`) `65f6f0`‡(40) / `6601e0`(278); Create (`4869685`) `6603d0`‡(76) / `660420`(392); Move (`00524a5`) `6605b0`(182) / `660670`(396); Heal* (`004c95c`) `660800`(377) / `660980`(120) |
| tStandardDestroyers† (`6943223`) | tStandardDestroyer, tStandardDestroyer1, tStandardDestroyer2 | `660a00`(293) | `660b30`(334) | Building (`9e08a57`) `61eb40`‡(40) / `660d20`(198); Hyperspace (`a7c9b35`) `65b260`‡(40) / `660f20`(385); Create (`4869685`) `65d470`‡(76) / `660df0`(299); Null (`0053a0c`) `65f6f0`‡(40) / E2; Build (`0479e04`) `65f6f0`‡(40) / `660c80`(159); Attack (`469857b`) `6610b0`‡(598) / `661310`(91) |
| tIonCannonFrigates† (`766e863`) | tIonCannonFrigate, tIoncannonFrigate1, tIoncannonFrigate2 | `661370`(293) | `6614a0`(334) | Building (`9e08a57`) `61eb40`‡(40) / `661690`(198); Hyperspace (`a7c9b35`) `65b260`‡(40) / `661890`(385); Create (`4869685`) `65db80`‡(76) / `661760`(299); Null (`0053a0c`) `65f6f0`‡(40) / E2; Build (`0479e04`) `65f6f0`‡(40) / `6615f0`(159); Attack (`469857b`) `6610b0`‡(598) / `661a20`(91) |
| tCorvettes† (`7af10e3`) | tCorvette, tMultiGunCorvette1, tMultiGunCorvette2, tMultiGunCorvette3, tMultiGunCorvette4, tMultiGunCorvette5 | `661a80`(269) | `661b90`(239) | Building (`9e08a57`) `61eb40`‡(40) / `661d20`(198); Create (`4869685`) `65e3b0`‡(76) / `661df0`(392); Null (`0053a0c`) `65f6f0`‡(40) / E2; Build (`0479e04`) `65f6f0`‡(40) / `661c80`(159); Guard (`04c9664`) `661f80`(676) / `662230`(64); Attack (`469857b`) `662270`(782) / `662580`(105); Dock (`004937b`) `6625f0`(179) / `6626b0`(330) |
| TaiidanHyperspace (`262c6e5`) | TaiidanHyperspace, tAdvancedSupportFrigate1, tAdvancedSupportFrigate2, tCarrier1, tCarrier2, tHeavyCruiser1, tHeavyCruiser2, tIoncannonFrigate1, tIoncannonFrigate2, tMultiGunCorvette1, tMultiGunCorvette2, tMultiGunCorvette3, tMultiGunCorvette4, tMultiGunCorvette5, tStandardDestroyer1, tStandardDestroyer2 | `662800`(156) | `6628a0`(806) | Hyperspace (`a7c9b35`) E2 / `662bd0`(96); Wait (`005b5e4`) E2 / `662c30`(69) |
| FSM_xbcc1a14? (`bcc1a14`) | HeavyCruiserGuard, HeavyCruiserGuard1, HeavyCruiserGuard2, HeavyCruiserGuard3, HeavyCruiserGuard4, HeavyCruiserGuard5 | `662c80`(410) | `662e20`(763) | Building (`9e08a57`) `61eb40`‡(40) / `663730`(198); Hyperspace (`a7c9b35`) `65b260`‡(40) / `663970`(407); Null (`0053a0c`) `65f6f0`‡(40) / E2; Build (`0479e04`) `65f6f0`‡(40) / `663690`(159); Beastify* (`96889d9`) `663120`(566) / `663360`(211); BeastifyCheat* (`d4141f4`) `663440`(349) / `6635a0`(234); Create (`4869685`) `663800`(76) / `663850`(279); Guard (`04c9664`) `663b10`(107) / `663b80`(338); Attack (`469857b`) `663ce0`(376) / `663e60`(189) |
| FSM_x9c2e0e4? (`9c2e0e4`) | MultiBeamFrigateGuard, MultiBeamFrigateGuard1, MultiBeamFrigateGuard2, MultiBeamFrigateGuard3, MultiBeamFrigateGuard4, MultiBeamFrigateGuard5, MultiBeamFrigateGuard6, MultiBeamFrigateGuard7 | `663f20`(269) | `664030`(182) | Building (`9e08a57`) `61eb40`‡(40) / `664190`(198); Null (`0053a0c`) `65f6f0`‡(40) / E2; Build (`0479e04`) `65f6f0`‡(40) / `6640f0`(159); Create (`4869685`) `664260`(76) / `6642b0`(398); Guard (`04c9664`) `664440`(294) / `664570`(64) |
| FSM_x048a873? (`048a873`) | xDFG, xDFG1, xDFG2, xDFG3, xDFG4 | `6645b0`(296) | `6646e0`(283) | Building (`9e08a57`) `61eb40`‡(40) / `6648a0`(446); Dock (`004937b`) `658c90`‡(179) / `6652b0`(660); Create (`4869685`) `659f50`‡(163) / `664a60`(410); Null (`0053a0c`) `65f6f0`‡(40) / E2; Build (`0479e04`) `65f6f0`‡(40) / `664800`(159); Guard (`04c9664`) `664c00`(1524) / `665210`(147) |
| BeastHyperspace (`cf0dda5`) | ACV4, BeastHyperspace, Carrier3, CloakedFighter1, HeavyCruiser6, HeavyCruiser7, HiveFrigate2, MultiBeamFrigate4, RammingFrigate2, RammingFrigate3, RammingFrigate4, Sentinel6, Sentinel7 | `665550`(156) | `6655f0`(711) | Wait (`005b5e4`) E2 / `6658c0`(69); Hyperspace (`a7c9b35`) E2 / `665910`(96) |
| BeastHyperspace2 (`f0ddaa2`) | BeastHyperspace2, Corvette3, Corvette4, HeavyCruiser11, HeavyCruiser12, HeavyCruiser4, HeavyCruiser5, HiveFrigate3, HiveFrigate4, HiveFrigate7, HiveFrigate8, IonArrayFrigate3, IonArrayFrigate4, Sentinel15, Sentinel16, Sentinel8, Sentinel9 | `665970`(539) | `665b90`(1329) | Hyperspace (`a7c9b35`) E2 / `6662e0`(309); Wait (`005b5e4`) `6660d0`(440) / `666290`(69) |
| SpecialMissile* (`57fad95`) | SpecialMissile1, SpecialMissile2 | `666420`(70) | `666470`(156) | - |

kasf: 5117 call sites, 187 distinct. Cataclysm-only kasf used: kasfCameraFocus, kasfCameraFocusClose, kasfCameraSetAngleDeg, kasfCameraSetDeclinationDeg, kasfCameraSetDistance, kasfDecNaggarokMissiles, kasfEnableBeastWeaponAutoAttack, kasfEnableNaggarok, kasfEnableShipConstruction, kasfForceParadeFormation, kasfGetBuildCost, kasfGetBuildTime, kasfGetDockHost, kasfGetNaggarokMissiles, kasfGetPlayerMothership, kasfGetShipsClampedTo, kasfGetTargets, kasfGetVectorX, kasfGetVectorY, kasfGetVectorZ, kasfGiveTechToAI, kasfHasShipBeenDiscovered, kasfIsBeastReady, kasfIsEMPReady, kasfIsGuarding, kasfIsIdle, kasfIsSwarmReady, kasfMakeShipsControllable, kasfNomadMoonCanFire, kasfNomadMoonEmitterCount, kasfNomadMoonFire, kasfPingMultipleAddShips, kasfRKOAvoidLocation, kasfRKOBeware, kasfRKOIsSafe, kasfRepair, kasfSalvage, kasfSetCurrentMimicItem, kasfSetMCVItemIndex, kasfSetSensorsLookAtPoint, kasfSetShipPositionXYZ, kasfShipsCreate, kasfShipsCreateLaunch, kasfShipsDockInstant, kasfShipsHealthAverage, kasfShipsHyperspaceIn, kasfShipsHyperspaceInDefault, kasfShipsHyperspaceOut, kasfShipsInsideMe, kasfShipsParalyze, kasfShipsScuttle, kasfShipsSelectDocked, kasfShipsSelectEnemyNoStealthy, kasfShipsSelectInside, kasfShipsSelectMoreThanDamaged, kasfShipsSelectNumber, kasfShipsSelectPlayer, kasfShipsSelectRace, kasfShipsSelectSalvagable, kasfShipsSelectShips, kasfShipsSetBeastInfectionUnlimited, kasfShipsSetColor, kasfShipsSetFormation, kasfShipsSetHelpless, kasfShipsSetInvincible, kasfShipsSetInvincibleInWidescreen, kasfShipsSetPercentDamaged, kasfShipsSort, kasfShipsStop, kasfShipsUnderAttackElsewhere, kasfShipsUpgrade, kasfSpecialOn, kasfStartLevelByHyperspacingShips, kasfStopInstant, kasfTimesHitByEMP, kasfTutSetObjPointerTargetShipSelection, kasfVectorAdd, kasfVectorAddTo, kasfVectorCopy, kasfVectorCrossProduct, kasfVectorNear, kasfVectorParametric, kasfVectorScalarMult, kasfVectorSubFrom, kasfWideScreenInDontCloseSensors.

### Tutorial1 (tutorial)

`Init_Tutorial1` `0x006704e0`(946), `Watch_Tutorial1` `0x006708a0`(368); block `0x00669530`-`0x00670a10`, 29920 bytes, 59 functions.

| FSM (hash) | Team label(s) | Init | Watch | States: name (hash) Init / Watch |
|---|---|---|---|---|
| SpeechEvents (`92e1ae3`) | SpeechEvents | `669530`(245) | `669630`(485) | WaitIntel* (`e8e63cc`) `5f4730`‡(29) / `669820`(512); WaitTimer* (`e989af2`) E2 / `669a20`(128); WaitMessage1* (`f32b271`) E2 / `669c60`(1039); WaitMessage2* (`f32b272`) E2 / `66a080`(535); WaitMessage3* (`f32b273`) E2 / `66a2a0`(1171); WaitMessage4* (`f32b274`) E2 / `66a740`(737); WaitMessage5* (`f32b275`) E2 / `66aa30`(1858); WaitMessage6* (`f32b276`) E2 / `66b180`(1461); WaitMessage7* (`f32b277`) E2 / `66b770`(1005); WaitMessage8* (`f32b278`) E2 / `66bb60`(1536); WaitMessage9* (`f32b279`) E2 / `66c160`(1292); WaitMessage10† (`32b27d0`) E2 / `66c670`(815); WaitMessage11† (`32b27d1`) E2 / `66c9a0`(1838); WaitMessage12† (`32b27d2`) E2 / `66d0d0`(595); S_x995f835? (`995f835`) `669aa0`(387) / E2 |
| Tutorial (`a9468cc`) | Tutorial | `66d330`(70) | `66d380`(161) | Inactive (`2588a45`) E2 / E2; S_x176e135? (`176e135`) `66d430`(47) / `66d460`(533); Lesson1* (`0a883a1`) `66d6b0`(138) / `66d740`(187); Lesson2* (`0a883a2`) `66d800`(222) / `66d8e0`(659); Lesson3* (`0a883a3`) `66db80`(286) / `66dca0`(1155); Lesson4* (`0a883a4`) `66e130`(142) / `66e1c0`(362); Lesson5* (`0a883a5`) `66e330`(154) / `66e3d0`(1082); Lesson6* (`0a883a6`) `66e810`(185) / `66e8d0`(737); Lesson7* (`0a883a7`) `66ebc0`(142) / `66ec50`(598); Lesson8* (`0a883a8`) `66eeb0`(200) / `66ef80`(459); Lesson9* (`0a883a9`) `66f150`(200) / `66f220`(529); Lesson10* (`a883a20`) `66f440`(278) / `66f560`(702); Lesson11* (`a883a21`) `66f820`(147) / `66f8c0`(1135); Lesson12* (`a883a22`) `66fd30`(274) / `66fe50`(743) |
| Targets (`866bac3`) | targets | `670140`(63) | E1 | SittingThere* (`7b65285`) `670180`(54) / `6701c0`(57); Create (`4869685`) `670200`(103) / `670270`(78) |
| SalvageRepair* (`d0be002`) | ToRepair, ToSalvage | `6702c0`(148) | E1 | HyperspacingOut* (`3858d14`) `60fd90`‡(35) / `6704b0`(48); InHyperspace* (`e9a9b35`) E2 / `670360`(77); HyperspacingIn* (`b38574e`) `6703b0`(54) / `6703f0`(48); S_x342cf95? (`342cf95`) `670420`(40) / `670450`(87) |

kasf: 1310 call sites, 109 distinct. Cataclysm-only kasf used: kasfBuildIsOpen, kasfCameraFocus, kasfCameraSetAngleDeg, kasfCameraSetDeclinationDeg, kasfCameraSetDistance, kasfEnableShipConstruction, kasfFirstShip, kasfForceCataclysmDefaultMode, kasfFormationParade, kasfGetShipCountSalvaged, kasfMsgQueueFlush, kasfObjectiveGetAny, kasfResearchCloseIfOpen, kasfResearchIsOpen, kasfSetSensorsLookAtPoint, kasfShipsCreate, kasfShipsHealthAverage, kasfShipsSelectUpgradable, kasfShipsSetColor, kasfShipsSetInvincible, kasfShipsSetPercentDamaged, kasfShipsStop, kasfShipsUpgrade, kasfSubtitleReset, kasfTutPrevButtonClicked, kasfTutSelectShips, kasfTutSetPointerTargetObj, kasfTutShowQuitButton.

## 9. What a kas2c-style regeneration needs

Goal: turn the 2,085 functions back into editable `.kas` and recompile them, either with a Cataclysm-dialect kas2c or as plain C in the
recreation. Needed, in order:

1. **Function boundaries in Ghidra.** Create functions at every address in section 8 (2,085 entries, all with a scope prologue), then
   re-export the decompile. Use one designated Ghidra stage; a script can read the addresses from this doc's tables. Until then,
   decompile them out of band from the capstone walk.
2. **A lifter (c2kas).** The output is mechanical, so pattern rules are enough:
   - drop the scope prologue
   - `kasJump(h, I, W); return` becomes `JUMP <state>;`
   - `kasFSMCreate(h, I, W, kasAITeamPtr(t))` becomes `FSMCREATE(<fsm>, TEAM_<t>);`
   - `kasGrowSelectionPtr(h)` becomes `SHIPS_<label>`; `kasAITeamShipsPtr` becomes `TEAMSHIPS_`; `kasThisTeamsVectorPtr` becomes `THISTEAMSPOINT`; and so on (parser.y:230-247)
   - kasf calls become KAS names, using `cat_kasFunctionTable` (exact KAS names and parameter types are in the exe)
   - the `_VIFONCE` pattern becomes `IFONCE`
   - the message-queue loop becomes the Cataclysm message-case construct (section 4.6)
   - `LSTRING` arrays (3 languages) become `LOCALIZATION` blocks
3. **Names.** Use the recovered names, keeping `FSM_x<hash>`/`S_x<hash>` placeholders for unresolved ones. Timer, variable and message
   names need the same dictionary pass; this doc did not do it (their hashes are immediates at `Timer*`/`Var*`/`Msg*` call sites). Any
   string with the right hash is functionally equivalent. Team, path, point and volume labels must match the `.mission` file labels exactly,
   because `kasAddShipToTeam` and `kasLabelled*Add` hash the layout labels at load time.
4. **A Cataclysm kas2c.** The HW1 kas2c (`tools/kas2c`) needs:
   - name hashing with the `G_` to bit-31 rule
   - the scope-name-from-team rule
   - 3-language `LSTRING`s
   - the message-case statement
   - the 187 Cataclysm-only table entries (186 C functions; `ShipsSetColour` is an alias), which can be emitted straight from `cat_kasFunctionTable`, minus the three HW1 entries Cataclysm dropped
   - mission-level `Init`/`Watch` emitted last
5. **Engine contract.** Keep the global var names in section 4.4 and the speech-event and objective label strings. Keep
   `SP_KAS_UPDATE_FRAME` cadence and `kasExecute` order: mission Watch, then for each ScriptTeam its FSM Watch, then its state Watch.
6. **Original save compatibility (optional).** Original `.SAV` files store `func - Watch_MissionNN` offsets in `kasSave`/AITeam save
   (`0x00418040`). A recompiled mission will not reproduce the layout. Loading original saves needs a lookup table from
   (mission, offset) to the regenerated function, built from section 8. ICF-shared bodies resolve to any of their contexts, which is safe
   because those bodies are identical. If original saves are out of scope, use HW1-SDL-style per-mission function lists (`MissionNN.functions.c`).
7. **Validation.** Recompile one mission (Mission15 is smallest: 31 functions, 6 FSMs) and diff its kasf call sequence per function
   against the original. The call-site counts per mission in section 7 are a quick checksum.

## 10. Open questions

- 28 FSM and 23 state hashes are unresolved (see the table in section 6). Most are team-specific FSMs whose names are not the team stem (for example `0x2cce515` for
  the `KushanMainGuard*` teams). Words from the `.mission` layouts, the shipped `.script` files, exe strings and HW1 `.kas` sources did not hit.
- The KAS keyword for the message-case construct is unknown. No Cataclysm `.kas` or kas2c survives, so a lifter has to invent the syntax.
- `cat_kasShipRemovedFromSelections` (`0x004911e0`) is a byte-identical, unfolded twin of `kasShipDied` with different callers
  (`univUpdate` and four functions at `0x00548f40`-`0x00549880`). Its source name is a guess.
- Mission titles are the `; Info:` lines of the shipped `.mission` files (big-archive-contents section 5.1). Missions 04, 10, 16 and 17 say "(Empty)" and 14 says "(blank)".

