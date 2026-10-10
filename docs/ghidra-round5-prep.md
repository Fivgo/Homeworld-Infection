# Ghidra round 5 preparation

Notes on the round-5 Ghidra passes over `cataclysm.exe`. The project is `ghidra/projects/cataclysm_gog` in the main checkout. Round 4's notes are in `docs/ghidra-round4-prep.md`, and its results are in `docs/binary-map.md` "Round 4". The targets are the R4.6 gaps listed there.

## D1: deferred round-4 functions and the `.data` artefacts (2026-10-09)

D1 was one headless pass on the existing program:
- no re-import and no full re-analysis;
- decompile on;
- run 19:01:51-19:02:54, rc=0, wall time 63 s;
- first attempt, no project-lock retry.

The pass wrote the export `ghidra/exports/cataclysm_gog_r5a/`, and its full log is `ghidra/exports/r5a_headless.log`. That log contains no `ERROR`, `Traceback` or `Unable to lock project` line. The only grep hit for "lock" is the analyzer name "Thread Environment Block".

### Inputs and outputs

| file | role |
|---|---|
| `ghidra/proposals/r4/reconcile_deferred_functions.json` | the 54 audited round-4 names whose address is code but not a function entry (50 high, 4 medium; 33 carry a signature) |
| `ghidra/create_functions_r5.json` | (new) those 54 in CreateFunctions format (`address`, `name`, `reason: r4-deferred`), sorted by address |
| `ghidra/create_functions_r5.report.csv` | CreateFunctions' result for each entry |
| `ghidra/scripts/RemoveDataArtefacts.py` | (new) post-script that checks function objects in a non-executable range and removes the ones that are not code |
| `ghidra/remove_data_artefacts_r5.report.csv` | its result for each function: `address,name,size,block,perm,code_unit,refs,flow_refs,via_pointer,rva_target,status,detail` |
| `ghidra/finish_r5.report.csv` | FinishMissionRegion.py report (guard, off-cut re-check, call checks) |
| `ghidra/scripts/CreateFunctions.py` | new `mark=` argument, see below |

**Pre-check of the 54 addresses.** Each address was checked against the r4 `functions.csv` ranges `[address, address+size)`:
- none falls inside an existing function;
- none is already a function entry;
- none of the names is already a function name in r4, and none appears in `symbols_round1..4.json`.

So there was no `inside:<owner>` case and no owner was split. The CreateFunctions report confirms this: 54 `created`, 54 `named`, 0 `skipped`, 0 `failed`.

The 54 by class:

| class | count |
|---|---:|
| Options.c | 10 |
| UIControls.c | 9 |
| AIHandler.c | 5 |
| AIPlayer.c | 5 |
| cat-new | 4 |
| SaveGame.c | 3 |
| Stats.c | 3 |
| AIUtilities.c, ResCollect.c, ScenPick.c, mainrgn.c | 2 each |
| AIAttackMan.c, AIMoves.c, AIShip.c, AITeam.c, SinglePlayer.c, debugwnd.c, main.c | 1 each |

### Command

The command uses the form given in the brief, with the round-4 symbols file:

```
cd .claude/worktrees/r2-apply && \
GHIDRA_PROJECTS_DIR=<repo>/ghidra/projects GHIDRA_EXPORTS_DIR=<repo>/ghidra/exports \
  tools/ghidra_headless.sh --symbols <worktree>/ghidra/symbols_round4.json \
    --post-script "CreateFunctions.py json=<worktree>/ghidra/create_functions_r5.json report=<worktree>/ghidra/create_functions_r5.report.csv analyze=0 returning= region= tag=R5_created mark=R5" \
    --post-script "RemoveDataArtefacts.py report=<worktree>/ghidra/remove_data_artefacts_r5.report.csv" \
    --post-script "FinishMissionRegion.py report=<worktree>/ghidra/finish_r5.report.csv created=<worktree>/ghidra/create_functions_r5.report.csv tables=0 tag=R5_finish" \
    --export-name cataclysm_gog_r5a <repo>/original/game/cataclysm.exe cataclysm_gog
```

The pass was launched detached, as `setsid nohup bash -c '<wrapper>' > ghidra/exports/r5a_headless.log 2>&1 &`. The wrapper retries every 60 s, up to 10 times, on "Unable to lock project"; no retry was needed. The wrapper is `.claude/d1/run_d1.sh` in the worktree (gitignored). Its `rehearsal` mode points at the scratch copy.

**Order inside the pass:**
1. ImportSymbolsJson (pre-script, phase=all): `symbols=3526 results=function:3526`. This makes `symbols_round4.json` the latest applied file, so the `_free`/`_malloc` fixes are not reverted.
2. CreateFunctions (post-script, `analyze=0 returning= region=`). It only creates and names the functions, exactly as in C1.
3. RemoveDataArtefacts (post-script).
4. FinishMissionRegion (post-script, `tables=0`), which has two jobs:
   - The only analysis of the pass. It runs inside the FixFunctionBodies snapshot guard, over the 54 new bodies (17,589 bytes at creation).
   - The off-cut walk of the mission region and the 4 call checks, before and after the analysis. These were kept as a no-op check.
5. ExportAnalysis (decompile on, 16 threads).

**The 41 `CALL_RETURN` overrides.** FinishMissionRegion's walk clears flow overrides only on `CALL` instructions, the rule from round 4. Both walks reported `flow_cleared=0` and `ft_cleared=0`, so no override was touched. The tail-call `JMP` overrides are intact.

**Script changes:**
- **CreateFunctions.py `mark=`.** The plate-comment prefix is now an argument:
  - `mark=R5` and `mark=[R5]` both give `[R5] created by CreateFunctions.py (r4-deferred)`.
  - Without `mark=`, the old `[R2]` is written, so the change is backward compatible.
  - All 54 new functions carry the `[R5]` plate (54 hits in `decompiled.c`) and the tag `R5_created`.
- **RemoveDataArtefacts.py** is new; see the next section.

**Rehearsals.** Both rehearsals ran on a copy of the project in `ghidra/projects/scratch-r5/`, with exports and reports inside that copy. The copy was deleted after the real pass.
1. Rehearsal 1, decompile on: CreateFunctions, the guard and the export behaved exactly as in the real pass. RemoveDataArtefacts removed 37 of the 42 functions and kept 5: `0x00891a2c`, `0x00891ade`, `0x00891b20`, `0x00891d04` and `0x00891d1a`. Their verbatim reason was `direct-flow:COMPUTED_CALL<-00c65c8e'CALL ESI'|...`. These are register calls (`MOV ESI,dword ptr [0x00c915b8]` ... `CALL ESI`), which the first version of the check did not recognise as pointer-derived. The raw bytes of all 6 sites were then checked: each one is `8b /r [slot]` followed by `ff d?`, and each slot holds the stub's own value. The script now walks back from a `CALL reg` to the instruction that last loaded that register.
2. Rehearsal 2, on a fresh copy, decompile on: 42 of 42 removed. All other numbers were identical to the real pass.

### Deferred names landed

All **54 of 54** were created and carry their audited name as the primary name (`IMPORTED` source). The table below covers the 54 deferred names.

| measure | value |
|---|---|
| bodies | 21,410 bytes in total, 21-4,248 bytes each |
| bodies at creation | 17,589 bytes |
| body grown by the analysis | `WindowProc` 0x00561f10, from 427 to 4,248 bytes (its next neighbour is 4,752 bytes away) |
| direct callers | 0 for every one of the 54, as expected: all are callbacks, handler-table entries or window procedures referenced by pointer |
| prototypes | not applied. ExportAnalysis shows Ghidra's own guessed signatures, for example `undefined4 opMusicVolumeProcess(float param_1)`. The 33 audited signatures in the deferred file take effect only when these names are re-emitted in a round-5 symbol file |

**Function found by the analysis (1).** No function existed in the r4 snapshot that was gone or shrunk afterwards.

| function | size | caller | what it is |
|---|---:|---|---|
| `FUN_00528580` | 102 | `WindowProc` 0x00561f10 (the only call edge) | Calls `GetKeyboardLayout(0)` and sets `strCurKeyboardLanguage` to 0/1/2/3/4 for primary language IDs 9/0xc/7/0xa/0x10. The same mapping, in the same order, is in HW1 `StringSupport.c:strSetCurKeyboard` (HomeworldSDL/src/Game/StringSupport.c:679). The function sits directly before `strLoadLanguage` 0x005285f0, as it does in HW1. **Candidate name `strSetCurKeyboard` (high).** Not applied in D1 |

### The 42 one-byte `.data` "functions": not code, all removed

The range is **0x00891a1c-0x00892114**, which is also the range given in `docs/ghidra-pipeline.md` §6.7. `docs/binary-map.md` R4.6 item 6 says `0x00891f6c`; that is wrong and is not the range.

**What the bytes are.** The file bytes were read directly from the PE sections. None of the 42 addresses holds code:
- the `.data` block is `rw-` and is not executable;
- the bytes at the entries are parts of integer tables and strings, for example `"0?.lif"` at 0x00891f6c and `"RM_ModuleNameDraw"` near 0x00892106;
- Ghidra had already typed 8 of the 42 addresses as data: `pointer` ×4, `string` ×3, `undefined4` ×1;
- 7 more lie inside a larger code unit, so the listing has no unit that starts there (`code_unit=none` in the report);
- the other 27 addresses were undefined bytes with no instruction.

**Why Ghidra made functions there.** `.idata` (0xc91000) holds the import table of the `.cms_t` protection stub. The loader never binds that table: the PE import directory is 0x749950 in `.rdata`.
- The stub's ILT (0xc91184...) and IAT (0xc915b4...) still hold the RVAs of their KERNEL32 hint/name entries.
- Every one of the 42 entry addresses is exactly such an RVA read as a VA. For example, 0x00891a1c is the RVA of `GetVersionExA`'s hint/name entry at 0xc91a1c (hint 373), and 0x00892114 is the RVA of `GetSystemTime`'s entry at 0xc92114.
- `.cms_t` calls through those IAT slots, either as `CALL dword ptr [slot]` or as `MOV reg,[slot]` ... `CALL reg`. Ghidra followed the unbound slot value as a code pointer.

**Every reference was checked.** RemoveDataArtefacts classified all references into the 42 entries:
- **221 references in total.**
- **137 are calls derived from those pointers.** Each comes from an indirect call in `.cms_t` whose slot, or whose register's load slot, holds the entry value.
- **84 are data references.** The 84 are the ILT and IAT copies of the values in `.idata`.
- **0 are direct calls or jumps.** No `E8`/`E9`/`0F 8x` rel32 in `.cms_t` lands on any of the 42 addresses (raw scan).

**What was removed.** All 42 function objects were removed. Data, labels and references were left as they were (`clear=0`; there was no instruction to clear).
- Function counts: 14,990 → 14,948 after the removal, and `.data` now holds 0 functions.
- The 2 long-standing decompile failures `FUN_00892106` and `FUN_00892114` are gone with them.
- The 74 `.cms_t`→`.data` call edges left `call_graph.csv`.

| address | name the slot value points to | refs | via pointer |
|---|---|---:|---:|
| 0x00891a1c | GetVersionExA | 3 | 1 |
| 0x00891a2c | GetProcAddress | 42 | 40 |
| 0x00891a3e | LoadLibraryA | 6 | 4 |
| 0x00891a80 | GetModuleHandleA | 4 | 2 |
| 0x00891a9e | GetLastError | 7 | 5 |
| 0x00891ade | VirtualAlloc | 7 | 5 |
| 0x00891b20 | VirtualFree | 7 | 5 |
| 0x00891bf2 | WriteFile | 5 | 3 |
| 0x00891bfe | SetFilePointer | 3 | 1 |
| 0x00891d04 | WideCharToMultiByte | 10 | 8 |
| 0x00891d1a | MultiByteToWideChar | 10 | 8 |
| 0x00891d30 | ExitProcess | 4 | 2 |
| 0x00891d3e | TerminateProcess | 3 | 1 |
| 0x00891d52 | GetCurrentProcess | 3 | 1 |
| 0x00891d84 | GetModuleFileNameA | 5 | 3 |
| 0x00891db2 | GetTimeZoneInformation | 4 | 2 |
| 0x00891dcc | GetLocalTime | 3 | 1 |
| 0x00891dea | GetStdHandle | 4 | 2 |
| 0x00891dfa | GetStartupInfoA | 3 | 1 |
| 0x00891e0c | GetCommandLineA | 3 | 1 |
| 0x00891e5e | LCMapStringA | 4 | 2 |
| 0x00891e6e | LCMapStringW | 6 | 4 |
| 0x00891e8a | HeapReAlloc | 6 | 4 |
| 0x00891e98 | HeapFree | 7 | 5 |
| 0x00891ea4 | GetEnvironmentVariableA | 3 | 1 |
| 0x00891ebe | HeapDestroy | 3 | 1 |
| 0x00891ecc | HeapCreate | 3 | 1 |
| 0x00891eda | GetFileType | 4 | 2 |
| 0x00891efe | SetHandleCount | 3 | 1 |
| 0x00891f10 | GetStringTypeA | 7 | 2 |
| 0x00891f22 | GetStringTypeW | 4 | 2 |
| 0x00891f6c | SetEnvironmentVariableA | 3 | 1 |
| 0x00891f86 | GetCPInfo | 5 | 3 |
| 0x00891fa8 | UnhandledExceptionFilter | 3 | 1 |
| 0x00891fd8 | FlushFileBuffers | 3 | 1 |
| 0x00891fec | FreeEnvironmentStringsA | 3 | 1 |
| 0x00892006 | FreeEnvironmentStringsW | 3 | 1 |
| 0x00892020 | GetEnvironmentStrings | 4 | 2 |
| 0x008920d2 | CompareStringA | 3 | 2 |
| 0x008920e4 | CompareStringW | 3 | 2 |
| 0x00892106 | GetVersion | 2 | 1 |
| 0x00892114 | GetSystemTime | 3 | 1 |

The bogus call references themselves are still in the program. A future full re-analysis would re-create these functions unless the `.idata` slots are typed as `ImageBaseOffset32` or `.cms_t` is quarantined (`--noexec-sections .cms_t`, docs/ghidra-pipeline.md).

### Results

| measure | round 4 (`cataclysm_gog_r4`) | D1 (`cataclysm_gog_r5a`) |
|---|---:|---:|
| functions in `functions.csv` | 14,679 | **14,692** (+55 created, -42 removed, 0 renamed, 0 existing functions resized) |
| functions in the program (with 257 externals) | 14,936 | 14,949 |
| deferred round-4 names created and landed as the function's primary name | 0 of 54 | **54 of 54** (tag `R5_created`, 21,410 bytes) |
| other new functions (found by the guarded analysis) | - | 1 (`FUN_00528580`, 102 bytes, candidate `strSetCurKeyboard`) |
| `.data` artefact functions | 42 | **0** (42 removed) |
| `FUN_*` names, whole program | 500 | 459 |
| `FUN_*` names in the game region 0x401000-0x5ca0ff | 458 (99,908 bytes) | 459 (100,010 bytes): the 54 were never `FUN_*`, plus `FUN_00528580` |
| function bytes, total | 3,026,177 | 3,047,647 |
| mission region 0x5d6fe0-0x670a0f inside a body | 612,687 (97.4%) | 612,687 (97.4%) |
| mission off-cut holes, before and after the analysis | 0 | 0 (`holes_fixed=0, unresolved=0, undis_after=0`) |
| the 4 mission CALL references | ok | ok 4/4, before and after |
| decompile ok / thunk / failed / error | 14,590 / 86 / 3 / 0 | **14,605 / 86 / 1 / 0** |
| `call_graph.csv` rows | 50,094 | 50,319 |

`symbols_summary.txt` counts 493 "default-named" functions. That figure counts name source `DEFAULT`, which includes `thunk_FUN_`/`switchD` names; this table counts names that start with `FUN_`.

**Guard repair counts** (`snapshot restore`, 14,691 bodies snapshotted, 17,589 bytes analysed in 54 functions):
- `noreturn_cleared=0`, `noreturn_new_kept=0`;
- `recreated=0`, `recreate_failed=0`;
- `regrown=0`, `regrow_partial=0`, `regrow_failed=0`;
- `still_gone=0`, `still_shrunk=0`;
- `new_functions_by_analysis=1`.

The no-return set did not change.

### Failures and anomalies (verbatim where logged)

- **None in the real pass.** rc=0, and the reports hold 0 `failed`/`kept` rows. `r5a_headless.log` has no `ERROR` line.
- **The single decompile failure** is `lib_crt_00671da2` 0x00671da2. It has failed in every round since round 1.
- **Rehearsal 1 kept 5 artefacts.** The log line was `NOT REMOVED 0x00891a2c FUN_00891a2c: kept direct-flow:COMPUTED_CALL<-00c65c8e'CALL ESI'|COMPUTED_CALL<-00c65c9f'CALL ESI'|COMPUTED_CALL<-00c65cac'CALL ESI'`. The same happened at 0x00891ade, 0x00891b20, 0x00891d04 and 0x00891d1a. The register-load walk-back fixed this before the real pass.
- **Polling gotcha.** `pgrep -f 'tools/ghidra_headless.sh'` also matches the waiting shell's own command line, so a wait loop built on it never ends. Writing the pattern as `[t]ools/...` does not help, because the other shells' command lines still contain the literal string. Wait for the wrapper's final line in the detached log instead (here `[d1] mode=real ... rc=`, or `[pipeline] rc=`).
- **The 54 functions have no applied prototype yet.** 33 audited signatures are waiting in `ghidra/proposals/r4/reconcile_deferred_functions.json`.

## Apply pass: off-cut merges and the 15 deferred names (2026-10-09)

The round-5 apply pass (export `cataclysm_gog_r5`; results in `docs/binary-map.md` R5.1) ran ImportSymbolsJson with `symbols_round5.json`, then three post-scripts in this order:

- **`MergeOffcuts.py json=ghidra/symbols_round5.json report=ghidra/merge_offcuts_r5.report.csv`** (new). It reads the top-level `removals` list that reconcile writes. For each entry it checks four things:
  - the function at the address still has its default `FUN_*` name;
  - the named parent starts at `parent_address`;
  - no CALL reaches the address;
  - at least one jump into it comes from the parent body.

  It then removes the function object only, and re-grows the parent with `CreateFunctionCmd.fixupFunctionBody`. If the result does not cover the old parent body and the off-cut, it sets the union. `dry=1` reports without changing anything. Both round-5 off-cuts merged: `recievedChatPacketCB` grew from 59 to 224 bytes, and `primBeveledRectSolid` from 49 to 720.
- **`CreateFunctions.py json=ghidra/create_functions_r5b.json ... analyze=0 returning= region= tag=R5_created mark=R5`.** It created and named the 15 names reconcile deferred (`ghidra/proposals/r5/reconcile_deferred_functions.json`, converted to `create_functions_r5b.json`). Their signatures are not applied yet.
- **`DropStaleComments.py drop=<the 25 renames>`**, as in round 4.

The pass was rehearsed first on a project copy under `/tmp/r5apply/proj` with `--no-decompile`, and that copy was deleted before the real pass. The real pass ran detached (`setsid nohup`), with D1's lock-retry wrapper, and wrote its log to `ghidra/exports/r5_headless.log`. It finished with rc=0 in 52 s, with 0 apply failures.

`tools/uncovered_code_candidates.py` (new) lists pointer-referenced code in the game region that lies outside every function body (R5.5). It is the input for a later function-discovery pass.
