# Ghidra round 2 preparation

Stage notes for the round-2 Ghidra passes of `cataclysm.exe` (project `ghidra/projects/cataclysm_gog`).

> **Section A1 is missing.** This file did not exist in the checkout when A2 started, so A1's report never landed here. A1's own result: 3,190 functions created (11,194 → 14,384); r2a export `ghidra/exports/cataclysm_gog_r2a`; per-reason counts are in `ghidra/create_functions_r2.report.csv`.

## A2: data names and struct types (2026-10-09)

One headless pass on the existing program: no re-import, no analysis, decompile off. It applied `ghidra/symbols_round1_data.json` (ImportSymbolsJson.py, phase=all) and then ran `ghidra/scripts/ImportHeaders.py` as a post-script. The pass ran twice:
- **First run:** the symbols applied, but ImportHeaders.py did nothing. Its entry guard tested `"currentProgram" in globals()`, and PyGhidra does not place `currentProgram` in the script's globals.
- **Second run:** the guard was fixed and the same command was re-run. The symbol step is idempotent.

Export: `cataclysm_gog_r2b`. It has 14,384 functions, the same as r2a, because no functions were created. `symbols.csv` grew from 35,717 to 36,642 lines (+925).

Command (from the A2 worktree; the project is the main checkout's):

```
GHIDRA_PROJECTS_DIR=<repo>/ghidra/projects GHIDRA_EXPORTS_DIR=<worktree>/ghidra/exports \
  tools/ghidra_headless.sh --no-decompile --symbols ghidra/symbols_round1_data.json \
    --export-name cataclysm_gog_r2b \
    --post-script "ImportHeaders.py decls=<worktree>/ghidra/exports/headers_r2b/decls.json" \
    <repo>/original/game/cataclysm.exe cataclysm_gog
```

`tools/ghidra_headless.sh` gained one option for this: `--post-script "S.py k=v ..."`. It is repeatable and runs before the export. It is backward compatible.

### Data labels (re-admitted round-1 "rule 5" drops)

`ghidra/symbols_round1_data.json` uses the `cet_symbols.json` schema: kind `data`, no type, and notes `class=...; confidence=...; sets=...; section=...; readmitted=r2-A2; evidence=...`. It was built from the 925 rows in the "Dropped by rule 5" table of `ghidra/proposals/reconcile_report.md`. Each row was looked up in its `.verified.json` set.

| | count |
|---|---|
| rows in the report table | 925 (core 844, ships 57, sim-cataclysm 24) |
| inside `.data` 0x74a000-0xc54fff | 916 |
| inside `.rdata` 0x72a000-0x749fff | 9 |
| rejected (outside both ranges / not in the verified set / not a C identifier / duplicate address) | 0 |
| name collisions with round 1 or with IMPORTED/USER names already in r2a (would get `_2`) | 0 |
| **applied in Ghidra** (`cet_apply_report.csv`: status `label`) | **925** |
| failed | 0 |

### Struct headers

**Pipeline.** `tools/prep_ghidra_headers.py`, which runs outside Ghidra, writes `ghidra/exports/headers_r2b/`. That output is generated and never committed. The steps:
1. Stage CET's reconstructed headers: `reference/CataclysmEngineTweaks/game/Game/*.h` (including `KEY.H`) and `game/Win32/*.h`.
2. Fill the three includes CET references but does not ship with the HW1 headers `Memory.h`, `Switches.h` and `Chatting.h`. CET already covers Universe, Player, SpaceObj/Ship, Bullet, Effect and Task, so no other HW1 header was needed.
3. Check the whole set with clang: target `i686-pc-windows-msvc`, C2x, `-fpack-struct=4` (= `/Zp4`).
4. Preprocess the set with `clang -E` and split it into top-level declarations per originating header (`decls.json`).

ImportHeaders.py then works in four steps:
1. Parses each declaration on its own, into a scratch data type manager that has the program's data organization and `#pragma pack(4)`.
2. Moves the parsed types to `/Cataclysm`; typedef'd function prototypes go to `/Cataclysm/functions`.
3. Copies them into the program.
4. Re-checks every CET layout assert against Ghidra's own layout.

**Changes made to the headers before parsing.** Each change is recorded in `decls.json` "fixups".

| change | where | why |
|---|---|---|
| lower-cased every quoted `#include` and staged file name | all | Windows file system is case-insensitive (`types.h` vs `TYPES.h`, `KEY.H`) |
| `#pragma once` added | all without one | `gamestats.h` has no include guard and is included twice |
| `constexpr T X = V;` → `#define X (V)` | aiskirmish.h (2), etg.h (1) | C++-only |
| `typedef struct X X;` added | aiskirmish.h (AISTeamSelection, AISTeamEntry), timer.h (Timer) | C++ uses a bare struct tag as a type name |
| `bool` → `sdword` | all | Relic's original `TYPES.H` has `typedef sdword bool` (gardens-of-kadesh `tools/win32/*/TYPES.H`); CET compiles as C++, where `bool` is 1 byte |
| removed CET's `sdword unknown;` before `grainCounter` in `shiptrail` | trails.h | with a 1-byte bool CET needed this pad to reach the asserted 0x3C. With the 4-byte bool (HW1 `Trails.h` has `bool32 wobbly`) the original field list reaches 0x3C without it, and keeping the pad breaks the assert |
| `Player`: `ubyte cat_unknown_02EC[0x6208]` after `researchinfo`, `ubyte cat_unknown_6500[0x30]` at the end; asserts `sizeof(Player)==0x6530`, `AllianceProposals@0x64F6`, `AllianceProposalsTimeout@0x64F8` added | universe.h | exe 12948 evidence in `ghidra/cet_symbols.json` (`universe_players` notes): the `players[]` stride is 0x6530 and `allianceFormWith` reads +0x64F6/+0x64F8. CET's 0x2F8 Player is asserted only up to 0x274. The pad's contents are unknown |
| `static_assert` / `ASSERT_OFFSET` stripped from the parsed text | all | checked by clang, then re-checked in Ghidra from `decls.json` |
| 34 array dimensions and assert indices evaluated by clang and replaced with numbers (25 declarations) | spaceobj.h and others | Ghidra's C parser silently makes `T x[ENUM_CONST]` zero-length; before this fix 129 of 298 asserts failed in Ghidra (ShipStaticInfo shifted by 0x24 at `bulletRange`) |
| stubs `windows.h` (a minimal set of Win32 typedefs, including `PIXELFORMATDESCRIPTOR` as an opaque struct), `wchar.h`, `string.h`, `main.h` (empty) | stubs | no Win32 SDK here; the port's SDL `main.h` is not Relic's |

**Results.**
- Headers: 79 staged, 67 of which contribute declarations; the rest only define macros or include others. 1,639 top-level declarations.
- Parse: 1,639 declarations OK, 0 failed. Headers parsed: 67. Partial: 0. Skipped: 0.
- Copy: 1,735 types copied into `/Cataclysm`.
- Layout check: 298 of 298 asserts pass in Ghidra (295 from CET + the 3 Player asserts above). The same set also passes under clang `/Zp4`, and under clang's default packing.

### Types applied to globals

Rules, from ImportHeaders.py:
- **CET anchors.** Anchors whose `cet_symbols.json` "type" names a parsed struct, a pointer to one, or an array of them. These clear conflicting data.
- **Declared globals.** Any named data global with a declared type. The type comes from the CET header `extern` (including blocks CET disabled with `#if 0`), otherwise from the HW1 `.c` definition named by its `hw1_equivalent`.
- **Guards:**
  - the global must be in a non-executable block;
  - a type larger than a pointer must fit its block;
  - it must match a size stated in the CET note;
  - it must not cover another named global, except the parent's own `<name>_*` labels;
  - a scalar never replaces a differently sized item that is already defined there.

Unsized `T x[]` and unresolved dimensions are not typed. Those are 45 globals, mostly font-name strings and script tables; they are listed in `prep_report.json`.

Result: 817 globals typed (811 applied + 6 already the same), 12 skipped.

**Struct-shaped, 52 typed:**
- **`universe` @0x00b62a00: `Universe`**
  - 0x3D9BC bytes.
  - Players are at +0x4B00 with the 0x6530 stride, so `world[]` lands at +0x3D9B0. That position is derived and unverified. The CET note's "+0x3DA30" disagrees by 0x80.
  - The CET-derived `universe_*` labels stay; their own data items were folded into the struct.
- `aisTeams` @0x008dfd60: `AISTeamEntry[34]`; 0x9D10 each, matching CET's own assert.
- `singlePlayerGameInfo` @0x00c0a5c0: `SinglePlayerGameInfo`.
- `taskData` @0x00bb31e0: `taskdata *[32]`, the task table.
- `memMainPool` @0x00c34f40: `mempool` (HW1 Memory.h layout, low confidence for Cataclysm).
- `shipHeaders` @0x008b1938: `CustShipHeader *[145]`.
- 30 per-ship `*Header` tables: `CustShipHeader`, from AdvanceSupportFrigateHeader @0x008c3408 to StandardFrigateHeader @0x008c6a20.
- Pointers:
  - `aiCurrentAIPlayer`: `AIPlayer *`
  - `aiCarrier1Ship`, `aiCarrier2Ship`: `Ship *`
  - `mgResourceInjectionIntervalEntryBox`, `mgResourceLumpSumIntervalEntryBox`: `textentryhandle`
  - `ghMainRegion`: `regionhandle`
- `ProcessSyncPktQ`, `ProcessRequestedSyncPktQ`, `ProcessCaptaincyPktQ`, `ProcessCmdPktQ`: `Queue`.
- Other structs:
  - `collBlobProperties`: `BlobProperties`
  - `keyBuffer`: `keybufferchar[16]`
  - `callbacks`: `CallBacks`
  - `keySaveScan`, `keyScanCode`: `keyScanType`
- Already the same before this pass: `waveFormatEX`: `WAVEFORMATEX` and `realtimeremainder`: `LARGE_INTEGER`.

**Scalar, 765 typed.** These are mostly tweakables: real32 493, sdword 114, color 47, udword 44, bool32 17, TypeOfFormation 13, plus enums and pointers. By source: 513 from HW1 `.c` definitions and 292 from CET header externs; the remaining 10 are CET anchors and 2 came from parsed externs.

**Skipped (12):**
- 7 CET-derived `universe_*` sub-labels lie inside the newly typed `universe`. They are covered by its fields.
- `tpGameCreated`: CET's `CaptainGameInfo` parses to 0x268 bytes, but the CET note says 688 (0x2B0) bytes in Cataclysm, so the layout is unknown.
- `dbgFatalErrorString` (char[2048]): the existing item is an `LPCSTR`. In Cataclysm this global may be a pointer, not a buffer; worth a look.
- `DefaultScenario` (char[100]) and `ROUTING_SERVER_NAME` (char[30]): shorter strings are already defined there, and were left alone.
- `HYPERSPACE_RANGE` (real32): a 5-byte string is defined at 0x0089418c. Either the HW1 type differs here or the round-1 address is off; to audit.

Reports: `ghidra/exports/headers_r2b/import_headers_report.json` (per header, per assert, per global) and `prep_report.json`. Logs: `ghidra/exports/r2b_headless.log`; the first, no-op run is in `r2b_headless.pass1.log`.
