# Ghidra round 3 preparation

Stage notes for the round-3 Ghidra passes of `cataclysm.exe` (project `ghidra/projects/cataclysm_gog`, main checkout). Round 2's notes are in `docs/ghidra-round2-prep.md`, and its results are in `docs/binary-map.md` "Round 2".

## B1: mission function bodies and the deferred round-2 names (2026-10-09)

B1 was one headless pass on the existing program. It did not re-import and did not re-run full analysis. Decompile was on. The pass ran at 14:33-14:35, with rc=0, wall time 97 s, and no project-lock retry. It wrote the export `ghidra/exports/cataclysm_gog_r3a/`.

### Inputs and outputs

| file | role |
|---|---|
| `ghidra/proposals/r2/missions-r2.body-fix.csv` | 984 mission functions whose Ghidra body was shorter than the kas2c size in `docs/mission-code-map.md` |
| `ghidra/proposals/r2/reconcile_deferred_functions.json` | 105 audited round-2 names whose address was code but not a function entry |
| `ghidra/create_functions_r3.json` | (new) the 105 names in CreateFunctions format, `reason: r2-deferred` |
| `ghidra/scripts/FixFunctionBodies.py` | (new) the body-fix and uncovered-run post-script |
| `ghidra/fix_bodies_r3.report.csv` | per-row result: `address,name,reason,status,old_size,new_size,doc_end,detail` |
| `ghidra/create_functions_r3.report.csv` | CreateFunctions' per-entry result |
| `ghidra/exports/r3a_headless.log` | full log of the pass |

### Command

The pass ran detached (`setsid nohup`) through a retry wrapper. The wrapper retries on "Unable to lock project", up to 10 times at 60 s intervals; no retry was needed.

```
cd .claude/worktrees/r2-apply && \
GHIDRA_PROJECTS_DIR=<repo>/ghidra/projects GHIDRA_EXPORTS_DIR=<repo>/ghidra/exports \
  tools/ghidra_headless.sh --symbols ghidra/symbols_round2.json \
    --create-functions <worktree>/ghidra/create_functions_r3.json \
    --post-script "FixFunctionBodies.py csv=<worktree>/ghidra/proposals/r2/missions-r2.body-fix.csv report=<worktree>/ghidra/fix_bodies_r3.report.csv" \
    --export-name cataclysm_gog_r3a <repo>/original/game/cataclysm.exe cataclysm_gog
```

**Deviation from the stage brief: `--symbols ghidra/symbols_round2.json`.** In reuse mode the wrapper always re-applies a symbol file (ImportSymbolsJson phase=all). Without `--symbols` it uses `ghidra/cet_symbols.json`, and that file would undo round-2 renames. For example, it would turn 0x00673080 `_free` back into `memFree` and 0x006725d0 `_malloc` back into `memAlloc`; see binary-map.md R2.1, "libs-unknown compromise". Re-applying `symbols_round2.json` is idempotent: 962 symbols, `function:911, label:51`, and the export shows 0 renames against r2.

**Order inside the pass:**
1. CreateFunctions.py (pre-script, creates the 105 functions, then analyses only the new code).
2. ImportSymbolsJson.py (pre-script).
3. FixFunctionBodies.py (post-script).
4. ExportAnalysis.py (post-script).

FixFunctionBodies runs its own auto-analysis restricted to the code it changed, so one pass was enough and a separate first pass was not needed.

**Pre-run tests.** Before the real run, the same command ran twice against a copy of the project in `/tmp` (`--no-decompile`). The first copy run exposed the no-return problem described below, and the script was fixed before the real pass. A copy of the project from before B1 is kept at `/tmp/claude-r3a/backup/`. It is not committed and is lost on reboot.

### FixFunctionBodies.py

The script follows CreateFunctions.py's conventions: PyGhidra, `key=value` args, plain `main()` at module level with no `currentProgram in globals()` guard (round-2 A2 gotcha), tolerant per-row errors, and a CSV report.

**`doc_end` is exclusive.** `doc_end == address + doc_size` for all rows (for example, 0x005d7aa0 + 219 = 0x005d7b7b), and mission-code-map.md section 7 defines size as "entry to the end of the last instruction". The body set is therefore `[address, doc_end - 1]`. With this reading, no other function entry lies strictly inside any row's range. Read inclusively, 46 rows would have "clipped" at the next function, which starts exactly at `doc_end`.

**Per row:**
- Walk the range.
  - Existing instructions: clear any fall-through or call flow override.
  - Defined data in the way: clear it.
  - Anything else: `DisassembleCommand`, with flow restricted to the range.
  - If disassembly fails, the walk skips to the next existing instruction; it never slides byte by byte.
- Clip at any function entry inside the range.
- Subtract any other function's body.
- `Function.setBody(old body + range)`.

**Uncovered-run scan of 0x5d6fe0-0x670a0f (afterwards).** A candidate must meet all of these conditions:
- it is 16-byte aligned (all 2,085 mission entries are);
- it does not look like an address table;
- it is either a call, data-pointer or foreign-jump target, or it has a mission prologue (`a1 e8 3e a4 00` = `mov eax,[0x00a43ee8]` in 2,004 of 2,085 functions) after padding or a RET.

**Analysis and repair:**
- Auto-analysis runs on the changed bodies, with "Non-Returning Functions - Discovered" and WindowsResourceReference switched off.
- Every function body is snapshotted first. Afterwards, functions that are newly flagged no-return but hold a RET get the flag cleared, deleted functions are re-created under their old name, and shrunk bodies are re-disassembled and re-grown.

### Results

| measure | round 2 (`cataclysm_gog_r2`) | B1 (`cataclysm_gog_r3a`) |
|---|---:|---:|
| functions in `functions.csv` | 14,384 | **14,490** (+106, 0 removed, 0 renamed) |
| functions in the program (incl. 257 externals) | 14,641 | 14,747 |
| function bytes (sum of sizes) | 2,679,918 | 2,999,412 |
| mission region 0x5d6fe0-0x670a0f inside a function body (union of address+size) | 320,296 of 629,296 (50.9%) | **612,691 of 629,296 (97.4%)** |
| body-fix rows: fixed / clipped / skipped / failed | - | **984** / 0 / 0 / 0 |
| body-fix rows whose exported size is still below `doc_size` | 984 | 0 |
| uncovered-run functions created | - | 0 |
| deferred names created and landed as the function's name | 0 of 105 | **105 of 105** (CreateFunctions: created 105, named 105) |
| other new functions | - | 1: `FUN_0067b680` (CRT range, 120 bytes, found by analysis of the new code) |
| decompile ok / thunk / failed / error | 14,295 / 86 / 3 / 0 | **14,401 / 86 / 3 / 0** |
| `call_graph.csv` rows | 41,530 | 49,551 |

The 3 decompile failures are the same 3 as in rounds 1 and 2: `lib_crt_00671da2` 0x00671da2, `FUN_00892106` 0x00892106 and `FUN_00892114` 0x00892114. No new failures.

**Lost callees recovered.** 968 rows listed `lost_callee_names` (7,717 names in all). In `cataclysm_gog_r3a/call_graph.csv`, 7,713 of the 7,717 now appear as callees of their function, and 964 of the 968 rows recovered every lost callee. Example: `Init_Mission01_KushanFlankFrigates_DoBattle` 0x005d7aa0 grew from 45 to 219 bytes and now decompiles with `kasfGrowSelectionClear`, `kasVolumePtr`, `kasfFindShipsInside`, `kasfShipsSelectFriendly`, `kasAITeamShipsPtr`, `kasfAttack` and `kasfShipsRemove`.

**Fall-through overrides inside the documented ranges.** 982 of the 984 rows had at least one fall-through or flow override inside their documented range; the report's detail column shows `overrides-cleared:N`. Which instructions carried them was not logged. In the same pass, CreateFunctions' repair loop found `fallthrough_overrides: 0` at the 8,920 CALL-referenced call sites of the six known-returning kas functions, so the overrides sat on other instructions. One candidate is calls whose CALL reference is missing, as in the 4 cases below; this is unverified. The overrides are presumably left over from A1's wrong no-return flags.

**What is still uncovered.** 16,605 bytes of the region lie outside any body, in 1,947 runs:
- 1,911 runs (14,866 bytes) are alignment padding (0x90/0xCC).
- 36 runs (1,739 bytes) are not padding. They are inline switch tables: after 0-3 alignment bytes (`90`, `8d 49 00`, `8b ff`) they hold dwords that point back into the region. The largest is 225 bytes at 0x005e657f, then 213 bytes at 0x0063866b and 212 bytes at 0x0064a08c.

The scan correctly created no function on any of them.

### Failures and anomalies (verbatim where logged)

- **The no-return analyzer runs while switched off.** The analysis inside FixFunctionBodies logged `ERROR Analysis Task: Non-Returning Functions - Discovered -  (AnalysisTaskWrapper) java.util.ConcurrentModificationException`. The analyzer ran even though `setAnalysisOption` had set it to false. It then re-flagged two functions that return. The script repaired it:
  - `cleared wrong no-return on kasfShipsCount@0x00492d20`
  - `cleared wrong no-return on kasGrowSelectionPtr@0x00490bb0`

  Before the snapshot repair existed (first stand-in run in `/tmp`), this cut 14 mission bodies outside the CSV, for example `Watch_Mission11_Fighters` 239 → 199 bytes, and deleted `Init_Mission04_Ecclesiam_Move` 0x005e6a00. In the real pass, the snapshot repair logged:
  - `restore: re-created Init_Mission04_Ecclesiam_Move@0x005e6a00`
  - `snapshot restore: recreate_failed=0, recreated=1, regrow_failed=0, regrow_partial=0, regrown=996`

  Compared with r2, no function shrank and none disappeared. The no-return set in the export is back to the 5 CRT exits.

  The wrapper did not count this ERROR line, because its error grep only matches `ERROR <script>.py>`. Any later pass that runs analysis on mission code needs the same snapshot guard. CreateFunctions.py has its own `autoreturn` repair, but that repair re-fixes only the region's bodies after cut call sites.
- **56 bytes in 38 rows stayed undisassembled** (`undisassembled-bytes:1..3` in the report). The bodies still cover the bytes, because the range is set as a whole.
- **4 lost callees are still absent from `call_graph.csv`:**
  - `kasJump` in `Watch_Mission13_Resourcers` 0x00626510
  - `kasfGetDockHost` in `Watch_Mission16_Corvettes_Dock` 0x006430c0
  - `kasfTeamCount` in `Init_Mission17_Workers` 0x0064ffe0
  - `kasfCameraFocus` in `Watch_Tutorial1_SpeechEvents_WaitMessage5` 0x0066aa30

  All 4 rows have undisassembled bytes. In the first case, the decompiler output does show the `kasJump(0x6a96934, Init_Mission13_Resourcers_Retreat, Watch_Mission13_Resourcers_Retreat)` call. So the instruction-level CALL reference is probably what is missing (unverified; inspecting it needs a Ghidra session).
- **CreateFunctions.py tags and comments.** It tags its functions `R2_created` and writes plate comments that say `[R2] ... (r2-deferred)`, because the wrapper cannot pass `tag=`. The reason field still tells B1's 105 functions apart. FixFunctionBodies tags its functions `R3_bodyfix`.
