# Ghidra round 4 preparation

Notes on the round-4 Ghidra passes over `cataclysm.exe`. The project is `ghidra/projects/cataclysm_gog` in the main checkout. Round 3's notes are in `docs/ghidra-round3-prep.md`, and its results are in `docs/binary-map.md` "Round 3" (the R3.6 gaps are the targets here).

## C1: deferred round-3 functions and the rest of the mission region (2026-10-09)

C1 was one headless pass on the existing program:
- no re-import and no full re-analysis;
- decompile on;
- run 17:50:53-17:52:13, rc=0, wall time 80 s;
- no project-lock retry was needed.

The pass wrote the export `ghidra/exports/cataclysm_gog_r4a/`. Its full log is `ghidra/exports/r4a_headless.log`. That log contains no `ERROR`, `Traceback` or lock line, and no analyzer exception this time either.

### Inputs and outputs

| file | role |
|---|---|
| `ghidra/proposals/r3/reconcile_deferred_functions.json` | 186 audited round-3 names whose address is code but not a function entry |
| `ghidra/create_functions_r4.json` | (new) those 186 in CreateFunctions format, `reason: r3-deferred`, sorted by address |
| `ghidra/create_functions_r4.report.csv` | CreateFunctions' per-entry result |
| `ghidra/scripts/FinishMissionRegion.py` | (new) post-script: switch tables, off-cut repair, call checks, guarded analysis |
| `ghidra/finish_missions_r4.report.csv` | per-item result: `kind,address,name,status,before,after,detail` |

**Pre-check of the 186 addresses** against r3 `functions.csv` (`[address, address+size)`):
- none falls inside an existing function;
- none is already an entry;
- no name collides with an existing function or symbol name.

So there was no `inside:<owner>` case and no owner had to be split.

### Command (adapted from the brief)

```
cd .claude/worktrees/r2-apply && \
GHIDRA_PROJECTS_DIR=<repo>/ghidra/projects GHIDRA_EXPORTS_DIR=<repo>/ghidra/exports \
  tools/ghidra_headless.sh --symbols <worktree>/ghidra/symbols_round3.json \
    --post-script "CreateFunctions.py json=<worktree>/ghidra/create_functions_r4.json report=<worktree>/ghidra/create_functions_r4.report.csv analyze=0 returning= region= tag=R4_created" \
    --post-script "FinishMissionRegion.py report=<worktree>/ghidra/finish_missions_r4.report.csv created=<worktree>/ghidra/create_functions_r4.report.csv" \
    --export-name cataclysm_gog_r4a <repo>/original/game/cataclysm.exe cataclysm_gog
```

The command was launched detached (`setsid nohup`) through a wrapper that retries on "Unable to lock project". No retry was needed.

**Deviation from the brief:** CreateFunctions runs as a `--post-script` with `analyze=0 returning= region=`, not through `--create-functions`.

Why: in reuse mode the wrapper's `--create-functions` runs CreateFunctions as a pre-script with its default analysis. That analysis has only CreateFunctions' own `autoreturn` repair, not the FixFunctionBodies snapshot guard. Its default `region=` also re-runs `fixupFunctionBody` over all 2,085 mission functions, and that could shrink the hand-set round-3 bodies again (the 38 bodies with undisassembled bytes in particular).

With `analyze=0` and the repair options empty, CreateFunctions only creates and names the functions. The one analysis of the pass then runs inside FinishMissionRegion's snapshot guard, over the 186 new bodies plus the 38 changed mission bodies (224 functions, 55,191 bytes).

**Order inside the pass:**
1. ImportSymbolsJson (pre-script, `symbols_round3.json`, phase=all, idempotent: `symbols=3087 results=function:3082, label:5`).
2. CreateFunctions (post-script).
3. FinishMissionRegion (post-script).
4. ExportAnalysis (post-script).

**Rehearsal.** The command was run three times on a copy of the project in `ghidra/projects/scratch-r4/`, with exports in `scratch-r4/exports/` and reports in `/tmp`.
- First run: a `PointerDataType.dataType` AttributeError.
- Second run: showed that clearing flow overrides on every call-flow instruction also removed the 41 legitimate `CALL_RETURN` overrides on tail-call `JMP`s; see below.
- Third run (decompile on): produced exactly the numbers of the real pass.

The copy was deleted after the real pass.

### Why the 4 CALL references were missing

The call instructions did not exist. At each of the 4 sites, the real instruction stream was covered by older instructions that start 1-3 bytes off the stream ("off-cut"). FixFunctionBodies.py (B1) cannot disassemble over an existing instruction, and it never slides byte by byte. So it skipped to the off-cut instruction and left 1-4 bytes undisassembled. Those bytes are the "56 bytes in 38 rows" of `fix_bodies_r3.report.csv`.

Example, `Watch_Mission13_Resourcers` 0x00626510:
- The real code is `push 0x6a96934` at 0x00626543, then `call kasJump` at 0x00626548.
- Ghidra instead held:
  - `TEST EAX,0xa0f3e806` at 0x00626546;
  - `OUT 0xff,AL` at 0x0062654b.
- 0x00626543-0x00626545 stayed undefined.

The other three sites have the same shape:

| function | call site | off-cut instruction |
|---|---|---|
| `Watch_Mission16_Corvettes_Dock` | 0x00643363 `call kasfGetDockHost` | `DEC EAX` at 0x00643364 |
| `Init_Mission17_Workers` | 0x00650051 `call kasfTeamCount` | `SUB AL,0xe4` at 0x00650053 |
| `Watch_Tutorial1_SpeechEvents_WaitMessage5` | 0x0066ab64 `call kasfCameraFocus` | `MOV EBP,0x1b6affe2` at 0x0066ab66 |

**Where the off-cut code came from.** The 44 repaired holes fall into three groups:

- **26 holes:** a one-byte off-cut `PUSH EDI`/`PUSH ESI` sits on the displacement byte of a `jz`/`jnz`. Its opcode byte (0x74/0x75) was left undefined.
- **12 holes:** the first off-cut instruction carries a `DATA` reference (14 references in all) from an unrelated address whose value happens to equal the off-cut address. The sources are 0x0072d2e4, 0x0072d2bc, 0x0072e1a0, 0x007340ec, 0x007514b4, 0x0088c1e4, 0x00897534, 0x008bbb58, 0x008d4ae4, 0x008da84c, 0x006d8d3d, 0x004d3c65, 0x0060c244 and 0x0060c3a3. So earlier pointer-driven disassembly most likely started there. This is an inference from those references, not a logged event. Two of these 12 are the `ADD AL,0x50` two bytes after `call kasGrowSelectionPtr` (0x0061645f, 0x00666666), which is the fall-through that round 2's wrong no-return flag on that function had cut.
- **6 holes:** the off-cut instruction has no reference, or only one from another off-cut instruction. One example is `TEST AH,DL` at 0x0064334c, reached by a `JNZ` that was itself off-cut.

The DATA references were left in place.

### FinishMissionRegion.py

It follows the conventions of FixFunctionBodies.py:
- PyGhidra;
- `key=value` args;
- `main()` at module level;
- tolerant per-item errors;
- a CSV report.

**1. Switch tables.** The script finds the tables from the code, not from a list. It looks for every `jmp dword [reg*4 + T]` and every `mov r8, byte [reg + T]` whose `T` lies in the region outside every function body. It sizes each table from the bound check:
- direct table: `cmp reg, N; ja; jmp` gives `pointer[N+1]`;
- index table: `cmp reg, N; ja; xor edx,edx; mov dl,[reg+I]; jmp [edx*4+T]` gives `byte[N+1]` at `I` and `pointer[max(I)+1]` at `T`.

Each table is then checked:
- every pointer must land in the dispatching function;
- tables must not overlap;
- no instruction may sit inside a table.

The script then types the table as an array, keeps or creates the `switchdataD_<addr>` label, and adds the dispatch's `DATA` reference and one `COMPUTED_JUMP` reference per distinct target where it was missing.

**2. Off-cut repair.** For every function with an entry in 0x5d6fe0-0x670a0f, the script walks from the entry along fall-throughs and jump flows, including the table targets. It does not follow calls, `RET`, or tail-call `JMP`s.

At a reachable address that is not an instruction start, it:
1. decodes the real stream with `PseudoDisassembler` until it meets an existing instruction start (at most 64 bytes);
2. clears the off-cut code units over that range;
3. re-disassembles, with flow restricted to the range.

It refuses to clear over another function's entry. Fall-through overrides and flow overrides are cleared only on `CALL` instructions.

The 41 `CALL_RETURN` overrides in the region are correct and are kept. They sit on tail-call `JMP`s such as `Init_Mission03_PatrolPirates_Stop` `JMP kasfStop`.

Note that FixFunctionBodies.py's walk clears them too, because its condition is any call-flow override. B1's analysis put them back on the bodies it re-analysed. Any re-use of that walk should get the same `CALL`-only restriction.

Alignment padding left undisassembled at the end of a body is trimmed from the body.

**3. Call checks.** For the 4 sites, the script raw-scans the body for `e8 rel32` to the target, then checks that the instruction exists and has an `UNCONDITIONAL_CALL` reference to the target. If the reference is missing, it adds one. The checks run before and after the analysis.

**4. Guarded analysis.** This is the FixFunctionBodies.py snapshot guard, with the functions copied over:
1. Every function body is snapshotted.
2. Auto-analysis runs on the changed bodies with "Non-Returning Functions - Discovered" and WindowsResourceReference switched off.
3. Newly no-return functions that hold a `RET` get the flag cleared.
4. Deleted functions are re-created, and shrunk bodies are re-disassembled and re-grown.
5. The off-cut repair and the call checks run again.

### Results

| measure | round 3 (`cataclysm_gog_r3`) | C1 (`cataclysm_gog_r4a`) |
|---|---:|---:|
| functions in `functions.csv` | 14,490 | **14,679** (+189, 0 removed, 0 renamed) |
| functions in the program (incl. 257 externals) | 14,747 | 14,936 |
| deferred round-3 names created and landed as the function's primary name | 0 of 186 | **186 of 186** (CreateFunctions: `created:186`, `named:186`; 25,963 bytes, 8-1,316 bytes each; tag `R4_created`) |
| other new functions (found by the guarded analysis of the new code) | - | 3, all unnamed, all outside the mission region |
| inline switch tables typed | 0 | **46 tables in the 36 runs** (42 `pointer[N]`, 4 `byte[N]` index tables; 232 pointer entries; 0 failed) |
| `COMPUTED_JUMP` references added | - | 9 (2 dispatches had none: 0x00621fcc in `Watch_Mission12_FSM_x0a52a12_Patrolling`, 0x00666935 in `Init_Mission17`) |
| off-cut holes repaired | - | **44 in 38 functions** (the 38 rows of `fix_bodies_r3.report.csv`), 0 unresolved |
| undisassembled bytes inside mission bodies | 56 | **0** |
| missing CALL references | 4 | **0** (all 4 `ok`: the instructions now exist and carry their call reference; no reference had to be added by hand) |
| mission region 0x5d6fe0-0x670a0f inside a body (union) | 612,691 of 629,296 (97.4%) | 612,687 (97.4%) |
| decompile ok / thunk / failed / error | 14,401 / 86 / 3 / 0 | **14,590 / 86 / 3 / 0** |
| `call_graph.csv` rows | 49,551 | 50,094 |

**The 46 switch tables by size:** `pointer[5]` 22, `pointer[4]` 8, `pointer[6]` 6, `pointer[7]` 2, `pointer[8]` 2, `pointer[12]` 2, `byte[161]` 2, `byte[162]` 1, `byte[19]` 1.

**Index-table dispatchers.** Three FleetIntel message functions hold two dispatches each:
- `Watch_Mission04_FleetIntel_WaitMessage`: two `pointer[5]` tables, indexed by `byte[19]` and `byte[162]`;
- `Watch_Mission15_FleetIntel_WaitMessage`: `pointer[5]` indexed by `byte[161]`, plus a direct `pointer[7]`;
- `Watch_Mission17_FleetIntel_WaitMessage`: a direct `pointer[4]`, plus `pointer[8]` indexed by `byte[161]`.

That is why there are 46 tables in 36 runs. 48 bytes of the 36 runs remain untyped. They are alignment filler between or before tables (`90`, `8b ff`, `8d 49 00`), not code.

**Region coverage, -4 bytes.** `Watch_Mission16_Corvettes_Dock` shrank from 688 to 684 bytes, which is its documented kas2c size (`doc_end` 0x0064336c).

In B1 an off-cut `INC dword ptr [EBX + 0x90c30cc4]` at 0x00643367 straddled `doc_end`. FixFunctionBodies' "straddling instruction belongs to the function" rule then grew the body over 0x0064336c-0x0064336f. Those 4 bytes are a `0x90` byte plus 3 NOPs of alignment, and C1 trimmed them. It is the only function whose size changed. The other 37 repaired bodies kept their size, because B1 had already set them as whole ranges.

**Guard repair counts** (`snapshot restore`):
- `noreturn_cleared=0`, `noreturn_new_kept=0`;
- `recreated=0`, `recreate_failed=0`;
- `regrown=0`, `regrow_partial=0`, `regrow_failed=0`;
- `still_gone=0`, `still_shrunk=0`;
- `new_functions_by_analysis=3`.

The post-analysis off-cut pass found 0 new holes. The no-return set is still the 5 CRT exits. Unlike in B1, the no-return analyzer did not throw or re-flag anything this time. So the guard had nothing to repair, but it stays required.

**Spot check.** `Watch_Mission13_Resourcers` now decompiles as:

```
sVar1 = kasfVarValueGet((char *)0x872a2e92);
if (sVar1 != 0) {
  kasJump((char *)0x6a96934,Init_Mission13_Resourcers_Retreat,Watch_Mission13_Resourcers_Retreat);
}
```

`call_graph.csv` now holds all 4 edges:
- `Watch_Mission13_Resourcers` -> `kasJump`
- `Watch_Mission16_Corvettes_Dock` -> `kasfGetDockHost`
- `Init_Mission17_Workers` -> `kasfTeamCount`
- `Watch_Tutorial1_SpeechEvents_WaitMessage5` -> `kasfCameraFocus`

**The 3 functions the analysis found.** None has a direct caller. Each one is a callback whose pointer is passed in one of the newly created functions:

| function | size | registered by | HW1 equivalent |
|---|---:|---|---|
| `FUN_004bc250` | 125 | `taskCallBackRegister(FUN_004bc250, 0, NULL, 1.0)` in `mgPingAllGames` 0x004bb4e0 | none in HomeworldSDL (low: Cataclysm-only ping task) |
| `FUN_004da730` | 302 | item-draw callback of `uicListWindowInit` in `opResolution` 0x004da9a0 | HW1 Options.c passes `opResItemDraw` in the same slot (medium candidate) |
| `FUN_004daa80` | 379 | item-draw callback of `uicListWindowInit` in `opRender` 0x004dada0 | HW1 Options.c passes `opRenderItemDraw` there (medium candidate) |

These names are candidates for the next naming pass. They are not applied.

### Failures and anomalies (verbatim where logged)

- None in the real pass: 0 failed rows in either report, rc=0, and no `ERROR` line in `r4a_headless.log`.
- The 3 decompile failures are the same 3 as in rounds 1-3: `lib_crt_00671da2` 0x00671da2, `FUN_00892106` 0x00892106 and `FUN_00892114` 0x00892114.
- **Rehearsal 1 failure, fixed before the real pass:** `AttributeError: Field 'dataType' is static. Did you mean: 'getDataType'?` The script now builds `PointerDataType()` / `ByteDataType()` instances.
- **CreateFunctions.py marking.** It still writes plate comments `[R2] created by CreateFunctions.py (r3-deferred)`, because its `MARK` is hard-coded. The tag is `R4_created`, and that tag plus the reason field tell C1's functions apart.
- **The 186 functions have no prototypes yet.** Some reconcile entries carry a signature, for example `gcInGameChatEntry`. Those signatures only take effect when the deferred names are re-emitted in a round-4 symbol file and applied.
