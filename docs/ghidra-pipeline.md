# Ghidra headless pipeline: Homeworld: Cataclysm (GOG "Homeworld: Emergence")

Status 2026-10-08: **installed and smoke-tested on stand-ins. A parallel agent made one real-exe import with the pre-edit scripts (§6.7), then one reuse-mode pass with the current `ImportSymbolsJson`, `MatchHW1Strings` and `ExportAnalysis` (§6.8). The current `ConfigureAnalysis.py` (quarantine, WindowsResourceReference off) has not run on the game, because it only runs on import.** The real GOG
offline installer landed during this round (`original/setup_homeworld_emergence_1.01_hotfix_(12948).exe`,
360,195,200 bytes) and another agent extracted it to `original/game/` (`tools/extract_installer.sh`,
facts in `docs/binary-fingerprint.md`). The main target is therefore on disk:
`original/game/cataclysm.exe`, 5,369,856 bytes, sha256 `f03d9a3ef26f74c04916b8d17d344cdcf0b5acd040dd2924cd7499af6eef3ed4`
(re-hashed here; it matches `original/game/MANIFEST.sha256`). This agent's scope forbade disassembling the game,
so §6.1–6.6 are stand-in tests. A parallel agent has since run a **snapshot of these scripts** on the real exe
(project `ghidra/projects/cataclysm_gog`, exports `ghidra/exports/cataclysm_gog/`, 13:54–14:00). §6.7 reads its
exports, and §4 and §8.0 have been adjusted from that run. §8.0 is the exact command list.
`original/GOG_Galaxy_Homeworld_Emergence.exe` is only GOG Galaxy's 504 KB download stub; the wrapper
refuses it (exit 3 on any `GOG_Galaxy_*` file).

| Piece | Path | State |
|---|---|---|
| Ghidra 12.1.4 PUBLIC (build 2026-09-21) | `~/Applications/ghidra_12.1.4_PUBLIC` | installed, headless verified |
| JDK 21 (javac) | apt `openjdk-21-jdk-headless` 21.0.12.1 | installed (Ghidra refuses a JRE, §7 G1) |
| PyGhidra 3.1.0 + JPype1 1.5.2 venv | `~/.config/ghidra/ghidra_12.1.4_PUBLIC/venv` | created offline from Ghidra's bundled wheels |
| Wrapper | `tools/ghidra_headless.sh` | tested: fresh import, re-import, reuse (`-process`), non-PE input, `--noexec-sections` |
| Pre-script: analysis options | `ghidra/scripts/ConfigureAnalysis.py` | tested (profiles `msvc6`, `fast`; `noexec=` section quarantine, §6.5) |
| Pre/post-script: CET symbols | `ghidra/scripts/ImportSymbolsJson.py` | tested with a 15-entry fixture plus absent, broken and mismatched files |
| Post-script: exports | `ghidra/scripts/ExportAnalysis.py` | tested (d3d8.dll, MCX.EXE) |
| Post-script: HW1 string matcher | `ghidra/scripts/MatchHW1Strings.py` | tested: source index, an unrelated binary (0 false positives), HW1 ELF ground truth (§6.4) |

---

## 1. Install (what was done and how to repeat it)

```bash
# 1. Ghidra (the zip was fetched by the orchestrator; fallback below)
#    gh release download Ghidra_12.1.4_build -R NationalSecurityAgency/ghidra \
#       --pattern "ghidra_12.1.4_PUBLIC_*.zip" -D ~/Applications/ghidra-dl
unzip -q ~/Applications/ghidra-dl/ghidra_12.1.4_PUBLIC_20260921.zip -d ~/Applications/   # 3.4 s
#    sha256 ddac49f903da9d5bac833e5cc79395098b9c33cfd3279be5f31bd00387d2d4db  (569,649,598 bytes)

# 2. a JDK, not just a JRE (only openjdk-21-jre was installed)
sudo apt-get install -y openjdk-21-jdk-headless       # also bumped the JRE 21.0.11 -> 21.0.12.1

# 3. PyGhidra venv: tools/ghidra_headless.sh does this automatically on first use
python3 -m venv ~/.config/ghidra/ghidra_12.1.4_PUBLIC/venv
~/.config/ghidra/ghidra_12.1.4_PUBLIC/venv/bin/pip install --no-index \
   -f ~/Applications/ghidra_12.1.4_PUBLIC/Ghidra/Features/PyGhidra/pypkg/dist pyghidra
```

Facts from the install tree:
- `Ghidra/application.properties:8` `application.java.min=21`; `:11` `application.python.supported=3.14 … 3.9` (system Python is 3.12.3).
- Python scripting in 12.x is **PyGhidra (CPython 3)**. Jython is no longer built in: it ships only as an optional extension zip, `Extensions/Ghidra/ghidra_12.1.4_PUBLIC_20260921_Jython.zip`, which is not installed. All our scripts carry `# @runtime PyGhidra`, the same tag as the bundled `Ghidra/Features/PyGhidra/ghidra_scripts/PyGhidraBasics.py`.
- `support/pyghidraRun` calls `Ghidra/Features/PyGhidra/support/pyghidra_launcher.py`, which asks `input()` questions (`:169,172,179,204`) on first run. That breaks headless automation. The wrapper therefore calls the same entry point the launcher ends with (`:291`) directly: `venv/bin/python3 -m pyghidra.ghidra_launch --install-dir $GHIDRA_HOME ghidra.app.util.headless.AnalyzeHeadless …`.
- The venv path matches the one the launcher would create (`get_ghidra_venv`, `:131`, under `~/.config/ghidra/ghidra_<ver>_PUBLIC/`), so the GUI started with `support/pyghidraRun` reuses it.
- FunctionID DBs bundled: `vs2012/2015/2017/2019_{x86,x64}.fidbf`, `vsOlder_{x86,x64}.fidbf` (`Ghidra/Features/FunctionID/data/`). None of them covers the VC6 CRT in practice (§6.3).
- Ghidra's GUI is not needed for any of this. To browse a project interactively, use `~/Applications/ghidra_12.1.4_PUBLIC/support/pyghidraRun` (reuses the venv) or `ghidraRun`. The GUI was not launched in this round, so behaviour on COSMIC/Wayland is UNVERIFIED (it is a Swing app, so expect Xwayland). Close the GUI before a headless pass on the same project (§7 G7).

---

## 2. Layout

```
tools/ghidra_headless.sh              wrapper (the only entry point you need)
ghidra/scripts/ConfigureAnalysis.py   -preScript : analysis options (profiles msvc6 | fast | default; noexec= quarantine)
ghidra/scripts/ImportSymbolsJson.py   -preScript phase=pre, -postScript phase=post (or phase=all)
ghidra/scripts/MatchHW1Strings.py     -postScript (opt-in): HW1-source string matching
ghidra/scripts/ExportAnalysis.py      -postScript : flat-file exports
ghidra/cet_symbols.json               produced by another agent from CET (optional; absent = skipped)
ghidra/projects/<project>.gpr|.rep    Ghidra projects            (NOT covered by .gitignore yet, see below)
ghidra/exports/<pe-basename>/         exports + logs/            (gitignored: ghidra/exports/)
```

`.gitignore` currently ignores `ghidra/*.rep/`, `ghidra/*.gpr`, `ghidra/*.lock`. Those patterns miss
`ghidra/projects/*.rep`. **Add `ghidra/projects/` and `__pycache__/` to `.gitignore`** before committing: a project DB
contains the copyrighted program bytes. The stand-in project created by the smoke test (`standin-d3d8`, 6 MB)
was moved out of the repo to `/tmp/claude-1000/ghidra-standin-projects/`. Its exports stay in `ghidra/exports/d3d8.dll/`
(gitignored) for inspection; they were regenerated on 2026-10-08 14:02 (fresh `--reimport` + fixture, 17.9 s) with the scripts as they stood then, i.e. **before** the 14:05 `WindowsResourceReference` edit to `ConfigureAnalysis.py`: their `logs/headless.out` shows `applied=20` and `program.json` still has `WindowsResourceReference`=`true`. **`ghidra/projects/` is no
longer empty**: the parallel real-exe run left `cataclysm_gog.gpr` + `cataclysm_gog.rep/` (81 MB) there (§6.7).

---

## 3. Usage

```bash
tools/ghidra_headless.sh [options] <pe-file> [project-name]
#   project-name defaults to the PE basename without extension; export dir = ghidra/exports/<PE basename>
```

| Option | Effect |
|---|---|
| (first run) | `-import` with `-processor x86:LE:32:default -cspec windows -loader PeLoader` (added automatically when the file starts with `MZ`; other formats such as ELF are auto-detected), analysis, then all post-scripts |
| (later runs) | Detects the program in `<project>.rep/idata/~index.dat` and uses `-process <name> -noanalysis` plus `ImportSymbolsJson phase=all`, then re-exports. Takes ~5 s on d3d8. |
| `--reimport` | `-import -overwrite`: fresh DB and fresh analysis |
| `--reanalyze` | `-process` with analysis re-enabled |
| `--no-decompile` | skips `decompiled.c` and deletes any stale one |
| `--threads N` / `--timeout S` | decompiler workers (default 16) / per-function timeout (default 60 s) |
| `--max-cpu N` | analyzeHeadless `-max-cpu` |
| `--symbols FILE` | CET JSON (default `ghidra/cet_symbols.json`, env `CET_SYMBOLS`) |
| `--profile msvc6\|fast\|default` | `ConfigureAnalysis.py` profile (import only, like `--noexec-sections`) |
| `--export-name X` | export directory name (default: the PE basename). Needed when two inputs share a basename (the original `rgl.dll` vs CET's hw_rgl `rgl.dll`) and for side-by-side projects (§8.0 steps 4 and 7) |
| `--noexec-sections .cms_t[,…]` | import-time only: clear the execute flag of the named blocks and set program option `Disassembler` → `Restrict Disassembly to Executable Memory`=true before analysis (§4, §6.5). An unknown block name is a logged warning, not an error. In reuse mode the wrapper prints `WARNING: --noexec-sections/--profile apply only on import` and goes on without them (tested on a copy of the d3d8 stand-in project); add `--reimport` |
| `--hw1-match` / `--hw1-apply` / `--hw1-validate` / `--hw1-src DIRS` | run `MatchHW1Strings.py`: propose / apply / score against existing names / pick source dirs |
| more than 2 positional args | exit 2, `too many arguments (did a glob match several files?)`. Without this guard, a glob that matched two files would make the second one the project name (§8.0 step 7) |
| env `GHIDRA_HOME`, `GHIDRA_MAXMEM` (default 8G), `GHIDRA_PROJECTS_DIR`, `GHIDRA_EXPORTS_DIR`, `PYGHIDRA_VENV` | overrides; the two dir overrides let throw-away runs live under `/tmp/claude-1000` |

Script order on a fresh import: `ConfigureAnalysis` → `ImportSymbolsJson phase=pre` → auto-analysis →
`ImportSymbolsJson phase=post report=<out>/cet_apply_report.csv` → [`MatchHW1Strings`] → `ExportAnalysis`.
Exit code: analyzeHeadless returns **0 even when a Python script raises** (§7 G4). The wrapper greps
`logs/headless.out` for `ERROR <x>.py>`, `Traceback`, `Import failed`, `program file(s) not found` and
`Unable to lock project`, and then exits 5. Logs: `ghidra/exports/<name>/logs/{headless.out,application.log,script.log}`.

---

## 4. Analysis options (`ConfigureAnalysis.py`, profile `msvc6`)

Every option name was checked against `getCurrentAnalysisOptionsAndValues()` on Ghidra 12.1.4 (127
options on a PE32). The script reads each value back after setting it and warns on any mismatch. None occurred.

| Option | Value | Default | Why |
|---|---|---|---|
| Aggressive Instruction Finder | true | false | finds code reachable only through tables and callbacks (KAS/ETG dispatch, AI) |
| Decompiler Parameter ID (+ timeout 90 s) | true | PE **under 2 MiB** only (60 s) | params/returns. Ghidra enables it by default only when `Memory.getNumAddresses() < 2097152` and the format is PE (`DecompilerFunctionAnalyzer.getDefaultEnablement`, `javap -c` on `Ghidra/Features/Decompiler/lib/Decompiler.jar`; the analyzer's description says "Turned off by default for large programs"). cataclysm.exe's blocks total about 9 MB (`.data` alone is 5.28 MB), so without this line it would be **off**. It cost 20.8 s on the §6.7 run |
| Function Start Search / Pre Search / After Code / After Data | true | true | x86win prologue patterns |
| WindowsPE x86 Propagate External Parameters | true | false | Win32 API arg names/types flow into callers |
| Function ID, Demangler Microsoft | true | true | CRT recognition (weak for VC6, §6.3) |
| Create Address Tables, {Data,}Reference.Switch Table References | true | true/false | jump and pointer tables |
| ASCII Strings min length / {Data,}Reference min string length | LEN_4 / 4 | LEN_5 / 5 | more anchors for string matching |
| Stack.Create Param Variables | true | false | better decompiler output |
| Variadic Function Signature Override | true | false | printf-style call sites decompile correctly. Side effect: USER_DEFINED `prt_<addr>_<hash>` labels in `<func>::override` namespaces (1,041 on MCX.EXE) |
| Windows x86 PE RTTI Analyzer, Windows x86 PE Exception Handling | true | — | Registered only when Ghidra identifies an MSVC/Clang PE: both `canAnalyze` methods return `PEUtil.isVisualStudioOrClangPe(program)` (`javap -c` on `Ghidra/Features/MicrosoftCodeAnalyzer/lib/MicrosoftCodeAnalyzer.jar`). Registration therefore says nothing about RTTI being present. Skipped on the MinGW d3d8.dll; applied on MCX.EXE and cataclysm.exe |
| PDB Universal.Search untrusted symbol servers | false | false | no network |
| WindowsResourceReference | **false** (`resref=1` re-enables) | true | On the real `cataclysm.exe` run (§6.7) this one analyzer took **199.8 s of the 347 s** auto-analysis and produced 2 bookmarks; the `.rsrc` holds a single icon. Off-state verified by read-back on the SMACKW32 stand-in (`/tmp/claude-1000/ghidra-smack/exports-noexec2/`, 14:05: `applied=21`, `program.json` → `false`). The repo's d3d8 stand-in exports predate this edit (§2) |

**Section quarantine** (`noexec=`, any profile): `MemoryBlock.setExecute(false)` plus
`Program.getOptions("Disassembler").setBoolean("Restrict Disassembly to Executable Memory", true)`.
The option name is the constant `Disassembler.RESTRICT_DISASSEMBLY_TO_EXECUTE_MEMORY_PROPERTY`. `javap` on
`Ghidra/Framework/SoftwareModeling/lib/SoftwareModeling.jar` shows the `Disassembler(Program,…)` constructor reading it
through `isRestrictToExecuteMemory(Program)` → `getInitializedMemory(program, true)`. A `Note/pipeline` bookmark is
placed at the block start. Use it for `cataclysm.exe`'s `.cms_t` (§8.0 step 4); measured behaviour is in §6.5.

Profile `fast` turns off Decompiler Parameter ID, Decompiler Switch Analysis, Call Convention ID, Stack,
AIF, Function ID, DWARF and Variadic Override. Use it for triage and for huge symbolised ground-truth builds.
Profile `default` leaves all options untouched.

---

## 5. Scripts

### 5.1 `ImportSymbolsJson.py`
Input schema (from the task contract): `{"target":{…},"symbols":[{"address":"0x…","name":"…","kind":"function|data|vtable|string|patch_site","type":"…","source":"…","hw1_equivalent":"…","notes":"…"}]}`.
A bare top-level list is also accepted. Args: `json=` `phase=pre|post|all` `report=` `force=1` `tag=CET` `apply_proposals=0|1`.

| Check / kind | Behaviour (all names use SourceType **IMPORTED**: auto-analysis never overrides them, and they never clobber a USER_DEFINED name) |
|---|---|
| file missing / empty / bad JSON | logs and returns; the headless run continues (tested) |
| `target.sha256`/`md5`/`image_base` ≠ program | **refuses to apply** unless `force=1` (tested: `image_base mismatch: json=0x400000 program=0x10000000`). A different `exe`/`file`/`name` is only logged. |
| address not in memory | `not-in-memory` (a CET `0x00524841` on the stand-in, tested) |
| `function` | Sorted by address and created first, so an enclosing function exists before any address inside it. Pre-analysis it disassembles and runs `CreateFunctionCmd(IMPORTED)`. It **refuses to create a function inside an existing body** (`label-only`, detail `inside:<fn>@<entry>`; tested). Otherwise it renames (a USER_DEFINED name is kept and ours added as a secondary label), adds tag `CET` and a `[CET]` plate comment, flags `overlaps:` when another function's body covers the entry, and, **in the post/all phase only**, applies `type` as a prototype via `CParserUtils.parseSignature` + `ApplyFunctionSignatureCmd`. `WINAPI`/`CALLBACK` become `__stdcall`, and a leading `ret name(` is handled. If analysis (e.g. the demangler) took over the name, the stale label is deleted and the name re-asserted (G16). Accepted forms: a full prototype, `ret (args)` or `ret (*)(args)` (tested: `int __stdcall (void *iface, int a, int b, int c)` and `void (int)` applied). If the prototype names types Ghidra does not know, it falls back to a **degraded prototype**: HW1/CET typedefs map to built-ins (`udword`→`uint`, `sdword`→`int`, `real32`→`float`, `bool32`→`int`, … from CET `game/Game/TYPES.h:11-24`), unknown pointers become `void *`, unknown by-value types become `undefined4`, and `T x[N]` becomes `T *x`. This keeps the parameter count and pointer-ness (detail `proto-applied-degraded`; tested: `bool32 (Player* player, ShipType type, int p3)` → `int f(void * player, undefined4 type, int p3)`; `void (const char CommandMessage[MAX_MESSAGE_LENGTH], udword flags)` → `void f(char * CommandMessage, uint flags)`). A prototype with no calling-convention keyword **keeps the function's analysed convention** (verified: `ApplyFunctionSignatureCmd` with convention `unknown` left `__stdcall`/purge 16 unchanged; an explicit `__cdecl` overrides it). |
| `data` | label (made primary when the existing primary is DEFAULT/ANALYSIS) + PRE comment. `type` is cleaned first: trailing `(…)` annotations dropped, HW1 typedefs mapped, `T[NAME=60]` → `T[60]`, unknown pointee → `void *`. `T[]`, `[512?]`, `end of …` and unknown by-value structs give `type-unknown` (label only). The result goes through `DataTypeParser`, and data is created with `CLEAR_ALL_CONFLICT_DATA`, but only in non-executable blocks. Tested: `real32 (constant 240.0f)`→`float`, `Ship* (pointer variable)`→`void *`, `udword[TOTAL_STD_SHIPS=60]`→`uint[60]`. |
| `vtable`, `string` | label + PRE comment |
| `patch_site` | Label in the containing function's namespace (moved there in the post phase if pre-analysis created it as global), PRE comment and a `Note/CET` bookmark. It is **never disassembled**, because CET can patch operand bytes. Report columns `containing_function`, `proposed_container_name` (from `hw1_equivalent`, else the `prefix_` of names like `utyGameSystemsInit_UNIVERSE_UPDATE_PERIOD`). `hw1_equivalent` may be `HomeworldSDL/src/Game/ETG.c:etgAtDefine` (the producer's format); the part after the last `:` is used. With `apply_proposals=1` (default), a DEFAULT/ANALYSIS-named container whose patch sites agree on `hw1_equivalent` is renamed (source ANALYSIS, tag `CET_container`). Disagreements get a plate comment, and a name already used by another function is not applied (`name … already used`) (tested: `FUN_10005a60` → `vbGetDescHW1`). |
| idempotency | comments are de-duplicated on the `[CET] …` line. Re-running `phase=all` on the same DB changed nothing (tested). |

Report: `ghidra/exports/<name>/cet_apply_report.csv`, with columns `address,name,kind,status,function_at,containing_function,container_entry,proposed_container_name,detail`.

### 5.2 `ExportAnalysis.py` → `ghidra/exports/<name>/`
Args: `outdir=` `decompile=1` `threads=16` `timeout=60` `types=1`. Every CSV is written with Python's `csv` module (RFC 4180 quoting). String values are C-escaped onto one line (`\n`, `\t`, `\\`, `\xNN`).

| File | Content |
|---|---|
| `functions.csv` | `address,name,size,calls_in,calls_out` (required), then `call_sites_in,is_thunk,thunk_target,source,calling_convention,param_count,varargs,no_return,section,strings_referenced,tags,signature`. `size` = body byte count; `calls_in/out` = unique caller/callee functions; `source` = DEFAULT means still `FUN_*`. |
| `call_graph.csv` | `caller_address,caller,callee_address,callee`. External callees appear as `EXTERNAL:LIB::name`. |
| `strings.csv` | `address,string,xref_count,first_xref_function` (required), then `section,datatype,length,xref_functions,data_refs,indirect_functions`. `xref_functions` = `name@0xaddr\|…`. `indirect_*` follows one hop through pointer tables, which matters for HW1-style `char *names[] = {…}`. A `~` prefix in `first_xref_function` marks an indirect-only hit. |
| `imports.txt` | `LIB :: symbol [Ordinal_n] iat=<slot> code_refs=<n>`, grouped by library |
| `exports.txt` | external entry points (PE exports + entry) |
| `symbols.csv` | every non-DEFAULT memory symbol: `address,name,namespace,type,source,primary` |
| `symbols_summary.txt` | blocks **with functions / `FUN_*` per block** (also in `program.json` `memory_blocks[]`), function counts by name source, tags, FID bookmarks, strings, imports, bookmarks, decompile stats |
| `program.json` | hashes, image base, language/cspec, compiler, blocks, all counts, **full analysis-option snapshot**, timings |
| `decompiled.c` | **One concatenated file**, sorted by address. Each function is preceded by `//==== FUNCTION 0x<addr> <name> size=<n> status=ok\|failed\|error\|thunk`. Split it with `csplit -z decompiled.c '/^\/\/==== FUNCTION/' '{*}'`. |
| `decompiled_index.csv` | `address,name,status,line_start,line_end` (1-based lines into `decompiled.c`) |
| `datatypes.h` | program DTM written by `DataTypeWriter` (structs/typedefs applied so far) |
| `cet_apply_report.csv`, `hw1_string_matches.csv` | from the other two post-scripts |

Decompilation runs one `DecompInterface` per Python thread. JPype releases the GIL inside Java calls, so it
runs in parallel: on MCX.EXE, 5,596 functions took 72.8 s on 1 thread and 33.0 s on 16 threads. That is only a **2.2× speed-up** (probably program-DB access or JPype contention; Ghidra's Java `ParallelDecompiler` is the upgrade path if this ever matters) (§6.3).

### 5.3 `MatchHW1Strings.py`
- **Standalone source index:** `python3 ghidra/scripts/MatchHW1Strings.py --index reference/HomeworldSDL/src -o hw1_strings.csv`. A small C tokenizer that:
  - keeps only the first live arm of each `#if/#ifdef` chain (`#if 0` arms are dead);
  - drops `ThirdParty/`;
  - concatenates adjacent literals and unescapes C escapes;
  - expands object-like `#define X "str"` macros used in a function body;
  - attributes each literal to its enclosing function, or to the initialised global (`kind=data`).
- **On HomeworldSDL (0.9 s):** 9,292 literal occurrences (5,221 in functions, 4,071 in data initialisers); 1,830 functions use literals; 3,013 distinct strings of 4+ characters, 2,643 of them used by exactly one function; 1,246 functions own at least one unique string. These were re-measured after the `DEFINE_TASK` macro fix (§6.4); the first version counted 1,822 and 1,238.
- **gardens-of-kadesh** (HEAD `3af31fb`, 2026-09-25) indexes almost identically: 9,290 occurrences, 3,016 distinct strings. Indexing both trees together yields 3,018 distinct strings, only **5 more** than HomeworldSDL alone, and 1,246 owning functions, because names de-duplicate across trees. Adding it is harmless and gains nearly nothing. Without the `#if`-arm rule, five files (`Game/ETG.c`, `SDL/main.c`, `SDL/texreg.c`, `SDL/utility.c`, `SDL/utility_mac.c`) ended with unbalanced braces; with it, none do.
- **Ghidra mode:** args `src=dir[,dir] out= apply=0|1 validate=0|1 min_score=1.0 min_margin=0.5 min_strings=3 minlen=4`.
- **Score** for binary function F and HW1 function h: Σ over shared strings s of `w(s) / (|HW1 fns using s| · |binary fns referencing s|)`.
  - `w(s)` = letters left after stripping printf specifiers, divided by 12 and capped at 1. Examples: `%.2f`=0, `Free`=0.33, `Error - Attack Type Not Found`=1.
  - A whitespace- or case-normalised match counts half.
- **Proposal rule:** score ≥ 1, margin ≥ 0.5 over the runner-up, and either one fully specific string that is unique on both sides or ≥3 strings. Proposals are one-to-one: if several binary functions want the same name, the best score wins and the rest are marked `dup`.
- **`apply=1`** renames only DEFAULT (`FUN_*`) functions, with source ANALYSIS and tag `HW1_STR`. CET names (IMPORTED) and hand-typed names therefore always win.

---

## 6. Smoke tests (all on this machine, 2026-10-08)

### 6.1 Stand-in A: `GE-Proton10-34/files/lib/wine/i386-windows/d3d8.dll`
313,155 bytes, PE32 i386, MinGW (linker 2.42), image base 0x10000000. Copied to `/tmp/claude-1000/ghidra-standin/`.
**Caveat:** Ghidra recovers most names from this DLL's own symbols (445 functions are ANALYSIS-named), so it tests plumbing, not naming.

| Run | Wall | Peak RSS | Notes |
|---|---|---|---|
| fresh import, no symbol file | **17.6 s** | 1.35 GB | auto-analysis "Total Time 11 secs"; 469 functions; decompile 468 ok / 0 failed in 1.0 s (16 threads) |
| `--reimport` + fixture (13 entries, 15 after adding two CET-style prototypes) | 16.6 s | 1.40 GB | report below |
| reuse (`-process -noanalysis`, phase=all) | **5 s** | — | idempotent: one `[CET]` comment per function |
| `--no-decompile` with mismatched / broken JSON | 4 s each | — | `not applying (pass force=1…)` / `could not parse … skipping` |

Export sizes (fresh import): `functions.csv` 73 KB, `call_graph.csv` 87 KB (1,168 edges), `strings.csv` 107 KB (884 strings, 670 with xrefs),
`imports.txt` 13 KB (172 symbols), `exports.txt` 264 B (6), `symbols.csv` 202 KB, `decompiled.c` 363 KB, `decompiled_index.csv` 25 KB,
`datatypes.h` 10 KB, `program.json` 9.7 KB, `symbols_summary.txt` 1.8 KB (sizes after the 14:02 regeneration; the per-block lines added 0.4 KB).

Heads:
```
functions.csv
address,name,size,calls_in,calls_out,call_sites_in,is_thunk,thunk_target,source,calling_convention,param_count,varargs,no_return,section,strings_referenced,tags,signature
0x1000100c,__wine_spec_relay_entry_point_1,23,0,0,0,0,,IMPORTED,__stdcall,0,0,0,.text,0,,undefined __wine_spec_relay_entry_point_1(void)
strings.csv
address,string,xref_count,first_xref_function,section,datatype,length,xref_functions,data_refs,indirect_functions
0x10018000,"iface %p, desc %p.\n",3,d3d8_vertexbuffer_GetDesc,.rdata,string,20,d3d8_vertexbuffer_GetDesc@0x10011000|d3d8_indexbuffer_GetDesc@0x1000a190,1,
imports.txt
KERNEL32.DLL  (9 symbols, path=None)
  KERNEL32.DLL :: DisableThreadLibraryCalls  iat=0x10026324  code_refs=1
call_graph.csv
0x10001a40,_Direct3DCreate8@4,0x1000ac50,d3d8_init
0x10001a40,_Direct3DCreate8@4,,EXTERNAL:UCRTBASE.DLL::calloc
decompiled.c
//==== FUNCTION 0x1000100c __wine_spec_relay_entry_point_1 size=23 status=ok
void __wine_spec_relay_entry_point_1(void)
{
  (*DAT_10016004)(&DAT_10016000,0x50000);
  return;
}
```

### 6.2 Importer fixture
The fixture is `/tmp/claude-1000/ghidra-fixture/cet_symbols_d3d8_fixture.json`. Its `target` carries the real sha256 and image base. Resulting `cet_apply_report.csv`:

| address | name | kind | status | detail |
|---|---|---|---|---|
| 0x10005a00 | cetTest_vbMethod | function | function | was `FUN_10005a00`; renamed + `proto-applied` → `int cetTest_vbMethod(void *iface,int a,int b,int c)` in `decompiled.c`. That detail is from a reuse run on an analysed DB. The current 14:02 fresh-import report reads `same-name;proto-applied`, because the pre phase already created the function under the CET name. Every fresh-import function row reads `same-name;…` except `cetTest_d3d8_init` |
| 0x1000ac50 | cetTest_d3d8_init | function | function | `renamed;proto-applied` (`void (int)`). The pre phase renamed it, analysis put the DLL's own `d3d8_init` (ANALYSIS) back as the function name, and the post phase re-asserted ours (G16) |
| 0x10008600 | cetTest_rmCanBuildShip | function | function | `proto-applied-degraded`: `bool32 (Player* player, ShipType type, int p3)` → `int (void * player, undefined4 type, int p3)` |
| 0x10014970 | cetTest_clCommandMessage | function | function | `proto-applied-degraded`: `void (const char CommandMessage[MAX_MESSAGE_LENGTH], udword flags)` → `void (char * CommandMessage, uint flags)` |
| 0x10005a06 | cetTest_midFunction | function | label-only | `inside:cetTest_vbMethod@10005a00`. The first build **did** carve a bogus function here and shrank its parent to 6 bytes; fixed by ordering + the containment guard |
| 0x10008070 | TestNs::cetTestMember | function | function | namespace `TestNs` created |
| 0x10016004 / 0x10016000 | cetTest_relayFnPtr / cetTest_relayCtx | data | label | `void *` / `int` applied pre-analysis; post: `type-same` |
| 0x10018000 / 0x1001cd00 | string / vtable | label | label | |
| 0x10005a79 | cetTestVb2_PatchSite | patch_site | patch-site | container `FUN_10005a60` → renamed `vbGetDescHW1` (tag `CET_container`) |
| 0x10001a50 | Direct3DCreate8_jneTraceCheck | patch_site | patch-site | label lives in `_Direct3DCreate8@4` namespace |
| 0x00524841 | univUpdate1 | patch_site | not-in-memory | |
| — | noAddress / weirdKind | — | bad-address-or-name / unknown-kind | |

### 6.2b The real `ghidra/cet_symbols.json` (another agent's output, 184 entries)
Contents: 56 `function`, 78 `data`, 50 `patch_site`. `target` = `{exe: "cataclysm.exe …", image_base: "0x00400000", notes}`, with no hash (CET never checks one).
To exercise the importer on the real content, every entry was **remapped** onto the stand-in, a test only:
- functions → distinct d3d8 function entries;
- data → `.rdata` slots 0x100 apart;
- patch sites → interior instruction addresses (from `objdump`). Sites sharing an `hw1_equivalent` container landed in the same d3d8 function.

The remapped file is `/tmp/claude-1000/ghidra-fixture/cet_symbols_real_remapped_to_d3d8.json` (image_base set to 0x10000000), and the project is `/tmp/claude-1000/ghidra-cetcompat/`.

| Result (`--reimport`, 15 s) | Count |
|---|---|
| functions created/renamed | 56/56. Prototypes: 21 `proto-applied`, 34 `proto-applied-degraded`, 1 `no-proto` (`sub_53F9F0` has an empty `type`) |
| calling conventions after post-phase prototypes | all kept from analysis (53 `__stdcall`, 3 `__cdecl` on this stand-in), none `unknown`. The first version applied prototypes pre-analysis and left `GetShipStaticInfo` as `unknown`; now post-only. |
| data labels | 78/78. Typed: 53 (`type-applied`/`type-same`). `type-unknown`: 25, all game structs (`Universe`, `CaptainGameInfo`, `AISTeamEntry[34]`…) or Win32 types not in the program DTM (`LPDIRECTSOUND`, `WAVEFORMATEX`), pending the CET-header `.gdt` (§8 a) |
| patch sites | 50/50, in 28 containers; 15 containers named from `hw1_equivalent`; 2 refused because the name was already on a remapped CET function (`allianceFormWith`, `univUpdate`). On the real exe those patch sites sit **inside** the CET-named function, so this guard is not expected to fire. |
| first-version bug found | `sub_53F9F0`: d3d8's demangler renamed our pre-created function after analysis; the post-phase rename then hit `DuplicateNameException` on our stale label (G16). Fixed. |

### 6.3 Stand-in B (scale/realism): MechCommander Gold `MCX.EXE` (1999)
Source: `/mnt/Ark/Additional/Games/MCG/`, copied to scratch; the project and exports went to `/tmp/claude-1000/ghidra-mcx/` (nothing in the repo).
It is the closest local analogue to Cataclysm (2000): **MSVC linker 6.00, Rich header, PE32 GUI, 3,607,856 bytes, .text 1.54 MB, statically linked CRT** (no MSVCRT import).
It does carry a COFF symbol table, though: 4,955 functions get demangled ANALYSIS names. A stripped exe will have far more `FUN_*`.

| Metric | Value |
|---|---|
| total wall (import → analysis → exports → save) | **122 s** (user 265 s, peak RSS 2.83 GB) |
| auto-analysis | 81 s. Top analyzers: Decompiler Switch Analysis 31.8 s, Variadic Function Signature Override 14.0 s, Stack 5.9 s, x86 Constant Reference 5.0 s, WindowsResourceReference 4.4 s, Decompiler Parameter ID 3.2 s |
| functions | 5,596 (262 `FUN_*`, 8 thunks); 17,858 call edges; 5,465 strings (4,263 with xrefs); 204 imports |
| decompile | 5,585 ok, 3 failed, **33.0 s on 16 threads** (72.8 s on 1 thread) |
| Function ID | **2 hits** on a statically linked VC6 CRT: the bundled FID DBs do not cover VC6 |
| export sizes | `decompiled.c` 10.2 MB, `symbols.csv` 2.0 MB, `call_graph.csv` 1.15 MB, `functions.csv` 760 KB, `strings.csv` 659 KB, `decompiled_index.csv` 284 KB, `datatypes.h` 78 KB |
| HW1 matcher on this unrelated game | first version: 3 false proposals from generic strings (`%.2f`, `  %s`, `Free`+`Allocated`); with specificity weighting and the min_strings rule: **0 proposed**, 40 weak (reuse run, 9 s) |
| determinism | d3d8.dll: `decompiled.c` and `functions.csv` byte-identical for `--threads 1` vs `--threads 16` (1.8 s vs 1.0 s). MCX.EXE: 5,595 of 5,596 functions identical. One (`0x00748620`) swapped two FP temporaries (`fVar12`↔`dVar12`); the two runs were on DB states one post-script analysis pass apart, so that may be the cause rather than threading. |

Cataclysm in comparison: `.text` is 0x329000 = 3,313,664 bytes, **2.1× MCX's 1.54 MB** (`docs/binary-fingerprint.md`).
The real run (§6.7) took 6 min 14 s wall, of which 347 s was auto-analysis. That is slower than linear MCX scaling
would predict, almost entirely because of WindowsResourceReference (200 s), which is now off by default. Decompile was faster
than predicted: 16.2 s for 11,108 functions.

### 6.4 Ground truth for the string matcher: HW1 built from source
Purpose: measure the precision of `MatchHW1Strings.py` on code whose true names are known. Binary: `reference/build-hwsdl/homeworld`, an x86-64 ELF built by another agent (meson debug, `-O0`, ASan+UBSan, not stripped, .text 15.3 MB).
Run: `--profile fast --no-decompile --hw1-validate`, project in `/tmp/claude-1000/ghidra-hw1elf/`. The existing (true) function names serve as ground truth.

| Metric | Value |
|---|---|
| import + `fast` analysis + exports | **365 s** wall (auto-analysis 320 s), peak RSS 3.85 GB, `GHIDRA_MAXMEM=16G`. Later reuse runs that only re-match and re-export: 38–43 s |
| program | 11,415 functions, 227,832 defined strings (42,877 distinct of 4+ chars) |
| `MatchHW1Strings`, index = `reference/HomeworldSDL/src` only (the setting that applies to Cataclysm) | 1,689 functions with any match. **825 proposed, 824 correct (99.9% precision)**, 857 weak, 7 dup. The single error, `Watch_Mission09 → singleHackResearchWarp`, comes from KAS-generated mission C that is not in the source tree, so the true owner was missing from the index. 361 of the 857 weak rows still have the correct top candidate, which makes them a review queue. |
| same, index also includes the generated mission C (`reference/build-hwsdl/homeworld.p/*.c`, 36 files from `.kas`) | **1,416 proposed, 1,416 correct (100%)**; 2,478 weak, of which 1,552 have the correct top candidate |
| first version, before the macro fix | 822 proposed, 820 correct. The other error: `rndRenderTask` was attributed to `DEFINE_TASK`, from `Task.h:149 #define DEFINE_TASK(name) void name(taskContext *…)`. The tokenizer now treats `ALLCAPS(ident) {` as a function-defining macro. |

Caveats:
- This is HW1 matched against HW1: gcc `-O0`, no inlining, the same strings. Cataclysm will score lower (changed text, MSVC inlining, Cataclysm-only code).
- The 99.9% precision is the property that transfers: with these thresholds, wrong names come almost only from code missing from the index. **This transfers only partly (§6.8).** On `cataclysm.exe`, 2 of the 4 proposals that could be checked against CET names disagreed. The causes were MSVC inlining and SDL-port restructuring, and an HW1-vs-HW1 test cannot show either.
- Coverage is bounded by how many functions use strings at all. 1,246 HW1 functions own a unique string of 4+ characters.

### 6.5 Section quarantine (`--noexec-sections`) on a stand-in with a second code section
Why: `cataclysm.exe` has a second executable section, `.cms_t` (80 KB, entropy 7.26, SecuROM-style;
`docs/binary-fingerprint.md`). With the Aggressive Instruction Finder and function-start search on, packed
or encrypted bytes there would turn into junk functions. The stand-in is RAD's `SMACKW32.DLL` (MechCommander Gold,
98,304 bytes, sha256 `7997c6ba…620f`). It has a second code section, `_UNSTEXT` (0x1000d000, 25,600 bytes, `r-x`).
Both runs used fresh projects under `/tmp/claude-1000/ghidra-smack/` and an **absent** symbol file (logged
`symbol file not found, skipping`, rc=0).

| | plain (`msvc6`) | `--noexec-sections _UNSTEXT,.bogus` |
|---|---|---|
| wall / auto-analysis | 15.4 s / 9 s | 14.5 s / 8 s |
| `_UNSTEXT` perm after import | `r-x` | `r--`, plus a `Note/pipeline` bookmark; `.bogus` → `WARNING noexec: no memory block named '.bogus'` |
| functions in `.text` (FUN_*) | 159 (76) | 159 (76): unchanged |
| functions in `_UNSTEXT` | 17 with real bodies (6–2,088 bytes) | **11, all size 1**: entry-only placeholders, one per call target reached **from `.text`** |
| call edges | 392 | 384 (the 8 intra-block edges are gone; every `.text`→block edge is kept) |
| decompile | 176 ok | 170 ok. The decompiler still emits C for the size-1 placeholders, because `DecompInterface` follows flow from the raw bytes and ignores the listing |

Reading: quarantine stops Ghidra from disassembling the block, so it adds no listing instructions and no
pattern-found or AIF functions. It keeps every reference into the block from real code, so you can still see which game functions
call SecuROM. Functions reachable only from inside the block disappear, which is the intent.

### 6.6 Re-check after this round's script edits
`ExportAnalysis.py` (per-block counts), `ConfigureAnalysis.py` (`noexec=`) and the wrapper
(`PYTHONDONTWRITEBYTECODE`) were re-run in reuse mode on the stand-in d3d8 project with the 15-entry fixture.
Result: rc=0, 7 s, and the same `cet_apply_report.csv` statuses as §6.2 (`same-name;proto-applied`,
`label-only inside:cetTest_vbMethod@10005a00`, `not-in-memory`, `unknown-kind`, `bad-address-or-name`). No `__pycache__` was created.

### 6.7 First real run on `cataclysm.exe` (by a parallel agent; read from its exports)
Provenance: `ghidra/exports/cataclysm_gog/logs/headless.out`. Scripts were run from
`/tmp/claude-1000/…/scratchpad/snap/`, which is byte-identical to this pipeline's 09:54 wrapper, i.e. **before** this
round's `noexec=`, `WindowsResourceReference` and per-block-count edits. The profile was `msvc6`, with the CET JSON snapshot of
`ghidra/cet_symbols.json` (184 entries). The project DB is 81 MB and sits in `ghidra/projects/cataclysm_gog.rep`, **inside
the repo and not gitignored** (§2).

| Metric | Value |
|---|---|
| wall | 13:54:35 → 14:00:49 (`logs/application.log`), about 6 min 14 s |
| auto-analysis | 347 s. Top: WindowsResourceReference **199.8 s**, Decompiler Switch Analysis 68.0 s, Decompiler Parameter ID 20.8 s, x86 Constant Reference 14.1 s, Stack 11.2 s, AIF 3.3 s |
| `ConfigureAnalysis` | `applied=22 skipped=none`. The RTTI and EH analyzers registered because Ghidra identified an MSVC PE (§4); registration alone proves nothing about RTTI. The RTTI analyzer then **did** find RTTI: `symbols.csv` has 266 `RTTI_Type_Descriptor` labels in these namespaces: `WONMsg` 76, `CryptoPP` 62, `std` 59, `Magick` 23, `WONMisc` 22, `WONCrypt` 8, `WONCommon` 6, `WONAuth` 5, global 5 (e.g. `WONCommon::WONException` @0x008b6e50, `Magick::ErrorCache` @0x008dad58). Statically linked C++ libraries, all to exclude from the game work list: WON/Titan, **Crypto++** and **Magick++** |
| functions | **11,194**: `.text` 10,980 (8,205 `FUN_*`), `.cms_t` 172 (152 `FUN_*`), `.data` 42 (all `FUN_*`, all size 1, at 0x00891a1c–0x00892114). Thunks 86. Name sources: DEFAULT 8,399, ANALYSIS 2,715, IMPORTED 74, USER_DEFINED 6 (`lpStartAddress_*`, `lpLocaleEnumProc_*`, `lpTopLevelExceptionFilter_*`: callback names from external-parameter propagation) |
| call edges | 29,405: `.text`→`.text` 28,056, `.text`→external 1,012, `.cms_t`→`.cms_t` 263, `.cms_t`→`.data` 74. **Zero edges between `.text` and `.cms_t` in either direction** |
| Function ID | **254 hits** (+38 `Function ID Conflict`), against 2 on MCX.EXE. The bundled DBs do cover part of this exe's CRT |
| strings / imports / exports | 13,094 strings (9,397 with xrefs). 257 import symbols in 11 DLLs; the fingerprint counted 263, one more in each of 6 DLLs, cause not checked. **17 named exports** + entry: `opendir`/`readdir`/`seekdir`/`telldir`/`closedir`, `mmap`/`munmap`, `NTElapsedTime`, `NTErrorHandler`, `NTSystemCommand`, `NTTemporaryFilename`, `NTUserTime`, `NTWarningHandler`, `IsWindows95`, `Exit`, `SetClientName`, `ImageFormatConflict` (INFERENCE from the names: an ImageMagick-style `nt.c` port layer, i.e. another statically linked library. The 23 `Magick::` RTTI classes in the `ConfigureAnalysis` row corroborate it) |
| decompile | 11,105 ok, 3 failed (`0x00671da2` in `.text`, plus 2 of the size-1 `.data` stubs), 86 thunks, **16.2 s** on 16 threads. The many `pcode error … Unable to resolve constructor` warnings are all at `.data` 0x0089xxxx or `.cms_t` 0x00c5xxxx addresses |
| bookmarks | Found Code 3,564, Address Table 1,058, AIF 866, Error/Bad Instruction 13 |
| memory map | the 7 PE sections plus a `tdb` block at 0xffdff000–0xffdfffff (rw, 4 KB) that is not in the PE section table. Ghidra adds it; it is absent from the d3d8 and SMACKW32 stand-ins |

**CET gate (§8.0 step 5) on this run:** 56/56 `function` rows have status `function` with no `inside:`/`overlaps:`
(prototypes: 21 applied, 34 degraded, 1 none). 78/78 `data` rows are labelled. 50/50 `patch_site` rows are placed, but **4 have
no containing function**:

| patch site | CET source | hw1_equivalent |
|---|---|---|
| 0x00562084 `ActivateMe_ReInitRenderer` | `Cataclysm.h:98` | `HomeworldSDL/src/SDL/main.c:ActivateMe` |
| 0x0056208e `ActivateMe_AfterReInitRenderer` | `PatchRGL.cpp:93` | same |
| 0x00562401 `WindowProc_JumpIfF12KeyUp` | `Cataclysm.h:109` | (empty) |
| 0x00562943 `WindowProc_NormalKeyUpEvent` | `Cataclysm.h:110` | (empty) |

All four fall in one function-less gap between `FUN_00561e80` (109 bytes) and `FUN_005631a0`, about 4.8 KB. Ghidra
created no function for the Win32 `WindowProc` (reached only through a `WNDCLASS.lpfnWndProc` pointer) or for
`ActivateMe` (called from it). Remedy: find the `RegisterClass*` call's WNDCLASS initialiser in the decompiled code and
create a function at its `lpfnWndProc` value. Then create `ActivateMe` from its call site and re-run reuse mode
(`phase=all`). The containers will then resolve. Ideally the producer adds both as `function` entries in
`cet_symbols.json` once their addresses are confirmed.

Also on this run (`cet_apply_report.csv`, `logs/headless.out`):
- `data` typing: 54 typed (51 `type-same`, 3 `type-applied`) and 24 `type-unknown` (game structs, pending the `.gdt`).
- Container naming: `container names applied=12 conflicts=0`. The 12 `CET_container` functions are `etgAtDefine` 0x0046de60, `gunShipFirePower` 0x0048cb60, `nisUpdateTask` 0x004cc600, `nisStartup` 0x004cc8f0, `partRenderBillSystem` 0x004dc6f0, `CollectResources` 0x004f17a0, `taskCallBackInit` 0x0052af00, `universeUpdateTask` 0x0053e460, `CheckPlayerWin` 0x005485e0, `InitWindow` 0x005631f0 (disputed, §6.8), `utyGameSystemsPreInit` 0x005ab810 and `utyGameSystemsInit` 0x005abe70. They are ANALYSIS-sourced, so later evidence can rename them.
- None of the 50 patch sites is a function entry. `target check: file name differs (informational)` is cosmetic: the JSON's `target.exe` holds a description (`cataclysm.exe (GOG "Homeworld: Emergence"; …)`), not a bare file name.

**`.cms_t` verdict from this run:** it is a closed island, as noted in the call-edge row (internal calls, 74 calls into encrypted-looking `.data` stubs, nothing
to or from game code). Quarantine (`--noexec-sections .cms_t`) is therefore safe for the canonical project. It should remove about
214 junk functions (`.cms_t` 172 + `.data` 42) and leave every `.text` call edge unchanged. Both are expectations,
UNVERIFIED until §8.0 step 3 runs. Note that the restrict option is **program-wide** (G21), so the `.data` stubs go too.
That is wanted here.

### 6.8 HW1 string matcher on `cataclysm.exe` (reuse pass with the current scripts; read from its exports)
Provenance: `ghidra/exports/cataclysm_gog_hw1match/logs/headless.out`, 14:05:10–14:05:20 (`application.log`). This was a reuse-mode pass on the
`cataclysm_gog` project (`-process`, no analysis) with the repo's current scripts: `ImportSymbolsJson phase=all`, then
`MatchHW1Strings src=reference/HomeworldSDL/src apply=0 validate=0`, then `ExportAnalysis decompile=0`. It is the only run of the current
`ExportAnalysis.py` on the real exe, and its `symbols_summary.txt` is the source of §6.7's per-block counts. It saved into the
project DB, but `apply=0` means it renamed nothing, and the `[CET]` re-assertion is idempotent.

| Metric | Value |
|---|---|
| index | 9,292 literal occurrences, 3,013 distinct strings (same as §5.3) |
| binary side | 12,286 distinct defined strings of 4+ chars |
| result | 355 functions with a match: **120 `proposed`** (116 on `FUN_*`, 2 on IMPORTED CET names, 2 on ANALYSIS names), 232 `weak`, 3 `dup` |
| CET re-apply | `function:56, label:78, patch-site:50`; `container names applied=0` (all 12 were already named) |

**Spot check against names we already trust.** Four `proposed` rows fell on functions that CET or the CET-container step had already named:

| address | current name (source) | proposed | verdict |
|---|---|---|---|
| 0x00527820 | `scriptSetTweakableGlobals` (IMPORTED) | same | agrees (score 23.0, 24 strings) |
| 0x005abe70 | `utyGameSystemsInit` (ANALYSIS, CET_container) | same | agrees |
| 0x005197d0 | `singlePlayerGameUpdate` (IMPORTED; CET hooks this address, `Cataclysm.h:169`, `PatchUniverse.cpp:339`) | `singlePlayerKasMissionStart` (9.33, 16 strings `mission16`, `mission15`, …) | **wrong**: in HW1, `singlePlayerKasMissionStart` is `static` (`SinglePlayer.c:220`) and is called from `singlePlayerGameUpdate` (`:2648`). MSVC inlined it, so its strings moved into the caller (§8 b iii) |
| 0x005631f0 | `InitWindow` (ANALYSIS, CET_container) | `main_postinit` (`\r\nProgram already running.\r\n`, `LSMap`) | **disputed**: the strings belong to HW1 `main` (`SDL/main.c:2135`). The index names it `main_postinit` because `main.c:2033` opens `#ifdef __EMSCRIPTEN__`, and the first-arm rule (§5.3) keeps that arm (`main_postinit`, `:2101`) and blanks `#else int main` (`:2106`). UNVERIFIED which of two things is true: 0x005631f0 is Cataclysm's `WinMain` with `InitWindow` inlined, or the container name is wrong |

Among `weak` rows on already-named functions, 5 of 8 top candidates agree (`etgAtDefine`, `mgResourceInjectionInterval`,
`mgResourceLumpSumInterval`, `mgGameTypesOtherButtonPressed`, `utyGameSystemsPreInit`). Three do not:
`opOptionsAccept` (score 0.008), `univUpdate` (0.208), and the FID-named `_strerror` → `rgluErrorString` (0.5).

Reading: 2 of the 4 proposals that could be checked disagreed. Both causes, inlining and port restructuring, are invisible in §6.4's
HW1-vs-HW1 test. On Cataclysm, treat `proposed` as a review queue, not as ground truth. Review before `--hw1-apply` (§8.0 step 6).
The sample is 4 rows, so the error rate on the 116 `FUN_*` proposals is unmeasured.

---

## 7. Ghidra 12.1.x gotchas hit (and what we did)

| # | Gotcha | Fix |
|---|---|---|
| G1 | With only a JRE installed, `support/analyzeHeadless` dies with `ERROR: Unable to prompt user for JDK path, no TTY detected.` | `apt-get install openjdk-21-jdk-headless`; the wrapper checks for `javac` |
| G2 | Python is PyGhidra in 12.x; `.py` scripts need the PyGhidra launcher. Jython is an optional extension zip. | scripts tagged `# @runtime PyGhidra`; wrapper launches through `pyghidra.ghidra_launch` |
| G3 | `support/pyghidraRun -H` prompts `Do you wish to install PyGhidra (y/n)?` (`pyghidra_launcher.py:169`); Ubuntu's Python is EXTERNALLY-MANAGED | venv created non-interactively from `pypkg/dist` wheels with `--no-index` (offline) |
| G4 | **analyzeHeadless exits 0 when a Python script raises.** Verified with a script that only raises: log shows `ERROR Boom.py> Traceback …`, rc=0 | wrapper greps the log and returns 5 |
| G5 | Python `print` goes to stdout, not `-scriptlog` (`support/analyzeHeadlessREADME.md:241-242`) | scripts log with `println` |
| G6 | Comment API: `Listing.setComment(Address, CommentType, String)`; the `int` overloads are now deprecated default methods (`javap` on 12.1.4) | scripts use `ghidra.program.model.listing.CommentType` |
| G7 | A second process on an open project fails with `LockException: Unable to lock project!` (rc=1, verified). The GUI holds the same lock. | close the GUI first; the wrapper flags the message |
| G8 | Options are registered per program: RTTI/EH options exist only when Ghidra identifies an MSVC/Clang PE (`PEUtil.isVisualStudioOrClangPe`, not RTTI presence; §4), and DWARF only for ELF. `setAnalysisOption` on a missing name is silently useless. | `ConfigureAnalysis` checks `containsKey`, logs skipped names, reads values back |
| G9 | Enum options (`ASCII Strings.Minimum String Length`) take the enum constant string (`LEN_4`) | done; read-back confirmed |
| G10 | Post-script changes trigger a further "ANALYZING changes made by post scripts" pass before the save. Exports are written **before** that pass. | Harmless (only CET/HW1 renames precede the export). Re-run in reuse mode if you need post-pass state. |
| G11 | `CreateFunctionCmd` happily creates a function in the middle of another one before analysis has run | importer orders function symbols ascending and refuses an address inside an existing body |
| G12 | `Variadic Function Signature Override` adds USER_DEFINED `prt_*` labels in `<fn>::override` namespaces | expected; filter `namespace` ending in `::override` from `symbols.csv` when counting analyst names |
| G13 | `-import` without `-overwrite` silently skips a file already in the project | wrapper detects existing programs and switches to `-process` (substring check on `~index.dat`; give unusual names their own project) |
| G14 | Ghidra extensions are version-locked: BinExport's latest release targets Ghidra 11.0.3 (`gh release list -R google/binexport`) | must be built from source against 12.1.4 to use BinDiff (UNVERIFIED) |
| G15 | Bundled FID DBs (vs2012…2019, vsOlder) gave 2 hits on MCX.EXE's VC6 static CRT, but 254 (+38 conflicts) on `cataclysm.exe` (§6.7) | coverage depends on the CRT build. A VC6 FID DB (§8 c) is still worthwhile for the remainder |
| G16 | An IMPORTED function name is **not** safe from every analyzer. On d3d8.dll the demangler made its ANALYSIS-sourced name primary over our pre-created `sub_53F9F0` and left ours as a label, so `setName` then threw `DuplicateNameException` | post phase deletes our stale label and re-asserts the name (Cataclysm is C with no exports, so the demangler should not fire there) |
| G17 | A prototype applied **before** analysis (function convention still `unknown`) stays `unknown`. Applied after analysis without a convention keyword, `ApplyFunctionSignatureCmd` keeps the analysed convention and purge. | prototypes are applied in the post/all phase only |
| G18 | PyGhidra leaves `ghidra/scripts/__pycache__/*.cpython-312.pyc` behind | wrapper exports `PYTHONDONTWRITEBYTECODE=1` (verified: no `__pycache__` after 3 runs); still add `__pycache__/` to `.gitignore` for hand-run scripts |
| G19 | Clearing a block's execute flag alone does not stop disassembly (tested: `noexec=_UNSTEXT restrict=0` on SMACKW32 still gave 17 functions with full bodies there, the same as the plain run). The program option `Disassembler` → `Restrict Disassembly to Executable Memory` (default false) must be set too. Even then, call targets inside the block still become **size-1 placeholder functions**, and the decompiler still decompiles them from raw bytes | `ConfigureAnalysis noexec=` does both. Treat size-1 functions in a quarantined block as "called-into-protection" markers (§6.5) |
| G20 | `WindowsResourceReference` can be pathological: 199.8 s of 347 s on `cataclysm.exe` (4.4 s on MCX.EXE) for 2 bookmarks | off in profile `msvc6` (`resref=1` to re-enable) |
| G21 | `Restrict Disassembly to Executable Memory` is **program-wide**: `getInitializedMemory(program, true)` keeps only blocks with `isInitialized() && isExecute()` (`javap -c` on `Disassembler`). Every non-exec block (`.data`, `.rdata`, `.cms_d`) becomes non-disassemblable, not just the quarantined one | intended for cataclysm.exe (the only out-of-`.text` code is reached from `.cms_t`, §6.7). Re-check on any other binary before using `--noexec-sections` |

---

## 8. Plan for the real binary

### 8.0 Day-1 runbook (inputs are on disk now; not executed in this round)
Inputs: the sha256 values are from `original/game/MANIFEST.sha256`, re-hashed 2026-10-08.

| PE | bytes | sha256 (prefix) | project | notes |
|---|---|---|---|---|
| `original/game/cataclysm.exe` | 5,369,856 | `f03d9a3ef26f…` | `cataclysm` | main target. MSVC 6 linker, base 0x00400000, entry 0x0067bbe0 (`docs/binary-fingerprint.md`) |
| `original/game/.cet-backup/<ts>/rgl.dll` | 176,128 | `06082ec88900…` | `rgl` | **The original.** `install_cet.sh` replaced `original/game/rgl.dll` with CET's hw_rgl build (228,352 bytes, `93947184…`), which is not the game's code |
| `original/game/.cet-backup/<ts>/rgld3d.dll` | 118,784 | `b67fad3a0503…` | `rgld3d` | the original as well; `original/game/rgld3d.dll` is now hw_rgl's (380,928 bytes) |
| `original/game/rglsw.dll` | 798,720 | `6c2c2b01f8e1…` | `rglsw` | software rasteriser back end (untouched by CET) |
| `original/game/rglfx3.dll` | 94,208 | `59d9cc78b53f…` | `rglfx3` | Glide back end (untouched by CET) |

Not targets: `CataclysmEngineTweaks.dll` and `cataclysm_loader.exe` (CET, source in `reference/`), `binkw32.dll`
(RAD Bink), `glide*.dll`, `3DfxSpl*.dll` and `nglide_config.exe` (GOG's 2017 nGlide wrapper), and `goggame-*.dll`.

0. **Keep the DB out of git.** `.gitignore` does not cover `ghidra/projects/` yet (§2), and a project DB contains the game's bytes.
   Either the synthesis agent adds `ghidra/projects/` first, or every real run sets
   `GHIDRA_PROJECTS_DIR=$HOME/ghidra-projects/homeworld-infection` (an env override; tested with `/tmp` paths in §6).
1. Integrity: `(cd original/game && grep ' ./cataclysm.exe$' MANIFEST.sha256 | sha256sum -c)`.
2. Ask the producer of `ghidra/cet_symbols.json` to add `"sha256": "f03d9a3ef26f74c04916b8d17d344cdcf0b5acd040dd2924cd7499af6eef3ed4"`
   to `target`. The importer then refuses any other build (§5.1). Today `target` has only `exe`/`image_base`/`notes`.
3. `tools/ghidra_headless.sh --hw1-match --noexec-sections .cms_t original/game/cataclysm.exe cataclysm` →
   `ghidra/exports/cataclysm.exe/`. Expect about 3 min: §6.7 measured 6 min 14 s with WindowsResourceReference still on,
   and that analyzer alone took 200 s (an estimate, UNVERIFIED). The parallel agent's `cataclysm_gog` project is a separate, older-settings
   project; keep or delete it, but do not mix the two. The default `--hw1-src` is `reference/HomeworldSDL/src`. Adding
   `reference/gardens-of-kadesh/src` brings only 5 more distinct strings (§5.3).
4. **Confirm the quarantine** (chosen from the evidence in §6.7). The per-block lines of `symbols_summary.txt` should show
   `.cms_t` and `.data` with 0 functions, or only size-1 placeholders. `.text` should stay near §6.7's 10,980 functions, and the
   `.text`→`.text` edges in `call_graph.csv` near 28,056. If `.text` drops noticeably, the quarantine hid real code:
   re-import without it into a second project
   (`tools/ghidra_headless.sh --export-name cataclysm.exe-noq original/game/cataclysm.exe cataclysm-noq`) and compare.
   Still open: whether the 1,309 `.text` dwords that point into the `.cms_*` range (fingerprint byte scan) are live **data**
   references. §6.7 shows no call edges either way, and data references were not counted.
5. **CET gate.** In `cet_apply_report.csv`, all 56 `function` rows must have status `function` with no `inside:`/`overlaps:`
   detail, and all 50 `patch_site` rows must have a `containing_function`. Any `label-only`, `not-in-memory` or
   container-less row means a wrong JSON entry, a different build, or a function Ghidra did not find. Fix it before
   trusting any other name. In §6.7 everything passed except the 4 `WindowProc`/`ActivateMe` patch sites, which are a
   missing-function case with a known remedy (§6.7).
6. Review `hw1_string_matches.csv` (`proposed` first, then `weak` rows whose top candidate is plausible), then re-run
   with `--hw1-apply` (reuse mode, under a minute on MCX-sized programs; the §6.8 pass on cataclysm.exe took 10 s).
   On Cataclysm, a `proposed` row can be an inlining artefact (§6.8: `singlePlayerGameUpdate` was proposed as
   `singlePlayerKasMissionStart`). If the proposed HW1 function is `static` and has few HW1 callers, check whether the
   binary function is really one of those callers before accepting the name.
7. The rGL DLLs, one project each, with no CET JSON (`--symbols none` is just a missing path, which is skipped):
   `tools/ghidra_headless.sh --symbols none --export-name rgl-orig original/game/.cet-backup/20261008-135434/rgl.dll rgl`,
   and the same for `rgld3d.dll`, `rglsw.dll` and `rglfx3.dll`. Use the explicit timestamp directory, not a `*` glob:
   `tools/install_cet.sh` makes a new `.cet-backup/<ts>/` on every run, and the wrapper now refuses more than two positional
   arguments (exit 2). `--export-name rgl-orig` keeps these exports apart from any later run on CET's own `rgl.dll`.
   Check the backups first with `(cd original/game/.cet-backup/20261008-135434 && sha256sum -c BACKUP_MANIFEST.sha256)`
   (verified OK); `original/game/MANIFEST.sha256:50-51` holds the same pre-CET hashes. `reference/hw_rgl` reimplements the
   rGL export interface (`reference/hw_rgl/draw.def`: `LIBRARY rgl`, `EXPORTS glAlphaFunc=_glAlphaFunc@8`, …), so its export names
   and prototypes are the ground truth for each DLL's `exports.txt`.
8. Run BSim/VT/FID (§8 c), then hand the exports to the decompilation effort (§8 d).

### (a) CET address anchors
- **What CET gives us:** `Cataclysm.h` defines **55** `DEFINE_FUNCTION` entries in `namespace Functions` (`:125`). `grep -c 'DEFINE_FUNCTION('` gives 56 only because it also counts the `#define` at `:17`. The JSON's 56th `function` is `sub_53F9F0`, named in a comment at `game/Game/AISkirmish.h:105`. These are exact entry points with C prototypes, e.g. `:128 aisRequestShip, void, (Player* player, ShipType shipType, sdword buildCost), 0x00431510`.
- It also has **44** `DEFINE_ADDRESS` entries (the 45th grep hit is the `#define` at `:14`), all in `namespace Instructions` (`:38`–`:124`). All are mid-function instruction addresses, and none coincides with a function entry on the §6.7 run. Three of them (`:98`, `:109`, `:110`) fall in the function-less `WindowProc`/`ActivateMe` gap (§6.7). They are named `<hw1Function>_<what>`, e.g. `utyGameSystemsInit_UNIVERSE_UPDATE_PERIOD 0x005AC174`.
- `namespace Globals` (`:185`) holds typed globals, e.g. `HYPERSPACE_SLICE_RATE (float*)0x0089410C`, plus 3 `GamePointer<>` pointer-to-struct globals (`:250-252`).
- Outside `Cataclysm.h` there are only **10** image-range address literals:
  - `PatchRGL.cpp:29,93,98,99`;
  - `GameHooks.cpp:158` (on a commented-out line) and `:166`;
  - `PatchUniverse.cpp:393,416` (data operands);
  - two in the `static_assert` at `game/Game/AISkirmish.h:134`.
  
  Other hex in that numeric range in `game/Game/*.h` is flag constants (e.g. `region.h:124`, `spaceobj.h:1173`). A script check confirmed that all 10, and every `Cataclysm.h` address, are in `ghidra/cet_symbols.json`. Its remaining `data` rows (20 from `universe.h`, 2 from `MultiplayerGame.cpp`, 1 from `singleplayer.h`) are derived as global base + struct offset, as its `target.notes` says.
- **Mapping to JSON kinds:**
  - `function` ← `DEFINE_FUNCTION`;
  - `patch_site` ← `Instructions::*` and the inline `assembler.Write(…, (void*)0x…)` sites;
  - `data` ← `Globals::*` / `GamePointer`.
- **How the importer uses them:**
  - The pre-phase seeds entry points before analysis, so CET functions also anchor disassembly.
  - The `prefix_` of patch-site names names their containers. Set `hw1_equivalent` in the JSON to make that automatic.
- **Types:** CET's reconstructed headers (`game/Game/*.h`, e.g. `spaceobj.h`, `universe.h`) are the struct layouts for Cataclysm, which differ from HW1's. Parse them into a Ghidra data type archive (`.gdt`) with Ghidra's C parser, using the `parseC` headless path or `CParserUtils` from a script (UNVERIFIED which). Do this **before** importing, so `Ship*`/`Player*` prototypes apply with real types. Until then they apply in degraded form (`void *`/`undefined4`), §5.1.

### (b) String-xref matching against the HW1 source (implemented: `MatchHW1Strings.py`)
1. **Source side:** literal → {HW1 function or initialised global, file:line} (§5.3).
2. **Binary side:** defined string → referencing functions. Ghidra xrefs plus one pointer-table hop are in `strings.csv` `indirect_functions`; the matcher currently uses direct refs only.
3. **Vote** with specificity and uniqueness weights. Accept one-to-one proposals above the threshold.
4. **Iterate:** after names are applied, **call-graph propagation** can name more functions (not yet implemented). If F is named h, and h has a unique callee h' in the HW1 source that has no strings, the single unnamed callee of F at the matching call position is h'. The HW1 call graph comes from the same tokenizer (identifier followed by `(` inside a body). The binary call graph is `call_graph.csv`. Proposed as `PropagateCallGraph.py`, reusing the source index.
5. **Data tables:** strings attributed to `kind=data` (4,071 occurrences: ship names, command strings, KAS tables) can name the *pointer tables* themselves. The `indirect_functions` column gives the readers.
6. **Things that will reduce recall on Cataclysm:**
   - (i) Cataclysm changed text. Indexing a second HW1 port does not help: gardens-of-kadesh adds 5 distinct strings (§5.3). Strings that exist only in Cataclysm simply stay unmatched, as `strings.csv` rows with no HW1 owner. Treat those rows as the manual-naming queue for Cataclysm-only code.
   - (ii) A pristine 2003 Relic drop would be better than the SDL port. These clones are depth-1 (`git log` shows one commit), so fetch full history if the first commit is the original drop (UNVERIFIED).
   - (iii) MSVC `/O2` inlines static helpers, which moves their strings into callers. The `dup` and `weak` rows are where to look.
   - (iv) Code that is not in the index produces the only wrong names seen in §6.4. Cataclysm's KAS missions and new subsystems are exactly that kind of code, so a `proposed` row whose function sits among unmatched neighbours deserves a second look.
   - (v) `dbgAssert`/`__FILE__` strings exist only when asserts were compiled in. The retail build kept few:
     `docs/binary-fingerprint.md` counts 122 distinct `*.c`/`*.cpp` basenames in `cataclysm.exe`, and only 14 of them are game files
     (`C:\Cataclysm\src\game\btg.c`, `…\win32\render.c`, …; listed in `docs/appendix/exe_source_basenames.txt`).
     The xref functions of those 14 strings in `strings.csv` give compilation-unit anchors. The other 108 (MSVC 6 CRT and the
     WON/Titan SDK) mark library code to exclude. The §6.7 RTTI labels add two more statically linked C++ libraries, **Crypto++** (62 classes)
     and **Magick++** (23), whose RTTI type descriptors name their classes. Only 1 `vftable` label was created, so mapping methods to classes is still manual work. The fingerprint estimates CRT + WON at about a third of `.text`, which is
     why the VC6 FID DB in (c) pays off.

### (c) Function matching beyond strings
| Tool | Status here | Use |
|---|---|---|
| **Diaphora** | **Not installable for this pipeline.** Not on PyPI (`pip index versions diaphora` → no distribution). The git repo (`joxeankoret/diaphora`, v3.4, pushed 2026-09-04) is "working as an IDA plugin" only, with no Ghidra exporter. It was cloned only to scratch to check. | Would need IDA, or a Ghidra→Diaphora-SQLite exporter. Not worth it. |
| **Ghidra BSim** (built in) | available: `Ghidra/Features/BSim/ghidra_scripts/CreateH2BSimDatabaseScript.java`, `AddProgramToH2BSimDatabaseScript.java`, `LocalBSimQueryScript.java`, `CompareExecutablesScript.java` | Primary cross-binary matcher. Populate a local H2 DB with a **retail Homeworld 1 exe** (same Relic codebase, same VC6 era), named by running this pipeline + `MatchHW1Strings --hw1-apply` on it first. Then query every Cataclysm `FUN_*`. HW1 retail exe + its own source ≈ near-perfect string naming; HW1 → Cataclysm via BSim carries the names across the fork. **Blocked on a retail HW1 exe.** None is installed: there is no Homeworld under `~/.local/share/Steam/steamapps/common` or the Ark libraries. `/mnt/Ark/Additional/Documents-Backup/my games/Homeworld Remastered` suggests the user played the Remastered Collection, which ships "Homeworld Classic" (UNVERIFIED). The ASan ELF in `reference/build-hwsdl/` uses a different compiler and arch, so BSim on it is weak (UNVERIFIED). |
| **Version Tracking** (built in) | `Ghidra/Features/VersionTracking/ghidra_scripts/AutoVersionTrackingScript.java` (+ `SetAutoVersionTrackingOptionsScript.java`) | Same HW1-retail → Cataclysm transfer, using exact/structural correlators. Also for GOG-vs-CD Cataclysm exe differences. |
| **ghidriff** | `pip install ghidriff` (1.0.0, requires `pyghidra>=2.0.0`) **installs cleanly** into a scratch venv next to the bundled pyghidra 3.1.0, and `ghidriff --help` runs. Diffing itself was not exercised on 12.1.4 (UNVERIFIED). | Quick Markdown diff reports (SimpleDiff / StructualGraphDiff / VersionTrackingDiff engines) between exe versions |
| **BinDiff 8** + BinExport | BinDiff 8 is open source (`google/bindiff` v8 release). BinExport's newest release targets Ghidra 11.0.3, so it needs a source build for 12.1.4 (G14). | Optional second opinion; not required |
| **FunctionID for VC6 CRT** | bundled DBs miss VC6 (G15) | Build a VC6 FID DB. Option 1: `ImportMSLibs.java` / `CreateMultipleLibraries.java` over VC6 `libc.lib`/`libcmt.lib`, if available. Option 2 (local, no redistribution): seed it from a symbolised VC6 binary we already have, MCX.EXE's COFF-named CRT functions, with `CreateEmptyFidDatabase.java` + "Populate FidDb". Then attach it with `AttachFidDatabase.java` and re-run analysis. This removes the statically linked CRT from the work list (its share of Cataclysm's functions is unknown; UNVERIFIED). |

### (d) Export formats for the decompilation / port effort
- **Per-function work units:**
  - `decompiled_index.csv` + `decompiled.c` give a stable address key, a name and the C text. Split with `csplit` for one file per function.
  - `functions.csv`'s `source` column is the progress tracker: DEFAULT means unknown, IMPORTED means CET, ANALYSIS means matched, USER_DEFINED means a human reviewed it.
  - The address is the primary key everywhere; names change.
- **Graph and strings:** `call_graph.csv` and `strings.csv` feed tooling such as the matcher, coverage reports and "next function to reverse" ranking (by `calls_in`).
- **Types:** `datatypes.h` today. Once the CET headers are in a `.gdt`, it becomes the shared header for reimplemented code.
- **Names round-trip:** treat `ghidra/cet_symbols.json` (same schema) as the source of truth for CET names, and add `ghidra/symbols/manual.json` + `ghidra/symbols/matched.json` for analyst and matcher names. Proposed: `ExportSymbolsJson.py`, which dumps every IMPORTED, ANALYSIS (minus `::override`) and USER_DEFINED function and data name back into that schema. Any DB can then be rebuilt from scratch with `tools/ghidra_headless.sh --reimport` + `ImportSymbolsJson` (the DB itself is never committed).
- **Whole-DB snapshots for humans:** Ghidra's `ExportProgramScript.java` (`Ghidra/Features/Base/ghidra_scripts/`) or a `.gzf` from the GUI. Keep these local only, because they contain game bytes.
- **Port strategy these exports support:** a CET-style hook DLL that replaces original functions one at a time with reimplemented C, keeping the original exe runnable at every step. Function addresses and prototypes come straight from `functions.csv` + `decompiled.c`. Bit-exact "matching" decompilation would additionally need the exact MSVC 6 compiler, identifiable from the Rich header (`program.json:compiler`, Ghidra reports `visualstudio:unknown` for MCX). It is not required for a behavioural 1:1 port.

---

## 9. Open items
- `.gitignore`: add `ghidra/projects/` and `__pycache__/` (the synthesis agent owns that file).
- `ExportSymbolsJson.py` and `PropagateCallGraph.py` are proposed, not written.
- CET header → `.gdt` import not done; prototypes with game types apply only in degraded form (`void *`/`undefined4`) until it is.
- `data` types: the annotated forms in the real JSON (`real32 (constant 240.0f)`, `Ship* (pointer variable)`, `udword[TOTAL_STD_SHIPS=60]`) are handled. The 25 struct-typed globals (`Universe`, `CaptainGameInfo`, …) stay untyped labels until the `.gdt` exists. Avoid C++ declarators such as `GamePointer<Ship>` in the JSON (emit `Ship *`).
- **Next round: run §8.0 on `original/game/cataclysm.exe`** with the current scripts (quarantine and WindowsResourceReference off). §6.7 came from the pre-edit snapshot, so re-measure wall time, per-block `FUN_*` counts and the CET gate.
- Create `WindowProc` / `ActivateMe` (§6.7) so the last 4 CET patch sites get containers.
- `ghidra/projects/cataclysm_gog.rep` (81 MB of program DB) is in the repo tree **now**: `.gitignore` must gain `ghidra/projects/` before any `git add` of `ghidra/`.
- `ghidra/cet_symbols.json` `target` should gain the exe's `sha256` (§8.0 step 2), so the importer's build guard is active.
- `.cms_t` quarantine stays opt-in in the wrapper. §8.0 passes it explicitly for cataclysm.exe, based on §6.7.
- Retail HW1 exe for BSim/VT (§8 c): not on this machine.
- `MatchHW1Strings.py` first-arm rule: `SDL/main.c:2033 #ifdef __EMSCRIPTEN__` makes HW1 `main` index as `main_postinit` (§6.8). Fix: treat `__EMSCRIPTEN__` arms as dead in `first_arm_only`, then re-measure §5.3 and §6.4. This round did not change it, so the measured numbers stay valid for the script as it is.
- `ghidra/scripts/__pycache__/MatchHW1Strings.cpython-312.pyc` appeared at 14:14:02. It did not come from the wrapper: a 14:16 wrapper reuse run left nothing new there. It is probably a hand import by another agent. Delete it, and add `__pycache__/` to `.gitignore`.

---

## Verification

Skeptical re-check on 2026-10-08, about 14:10–14:20. Every check was made against the files on disk: the scripts, `reference/` clones,
the Ghidra 12.1.4 install (`javap`), `ghidra/exports/{d3d8.dll,cataclysm_gog,cataclysm_gog_hw1match}/` and the `/tmp/claude-1000/ghidra-*`
test directories. Two runs were made, both on stand-ins: the wrapper's safe paths (`bash -n`, GOG-stub refusal exit 3, missing file
exit 2, unknown option exit 2), and one reuse pass on a scratch **copy** of the d3d8 stand-in project. Nothing was run on the game binary.

**46 claims checked; 8 were wrong and are now fixed:**

| # | Claim (where) | Finding | Fix |
|---|---|---|---|
| 1 | "The current scripts have not been run on the game" (status line) | A 14:05 reuse pass (`ghidra/exports/cataclysm_gog_hw1match/`) ran the current `ImportSymbolsJson`, `MatchHW1Strings` and `ExportAnalysis` on cataclysm.exe; only `ConfigureAnalysis` (import-only) has not run there | status line reworded; new §6.8 |
| 2 | d3d8 stand-in exports "regenerated … with the current scripts" (§2) | Their `headless.out` shows `applied=20` and `program.json` shows `WindowsResourceReference=true`. They predate the 14:05 edit, and the off-state was verified only on SMACKW32 `/tmp/claude-1000/ghidra-smack/exports-noexec2/` | §2 and §4 provenance corrected |
| 3 | Decompiler Parameter ID "already default for PE" (§4) | `DecompilerFunctionAnalyzer.getDefaultEnablement` is true only when PE **and** `Memory.getNumAddresses() < 2097152`. cataclysm.exe has about 9 MB of blocks, so its default is off | §4 row rewritten |
| 4 | RTTI/EH options "only registered when RTTI/EH is present" (§4, G8, §6.7 inference) | `RttiAnalyzer`/`PEExceptionAnalyzer.canAnalyze` return `PEUtil.isVisualStudioOrClangPe(program)`: registration follows compiler ID. The §6.7 conclusion still holds, because 266 `RTTI_Type_Descriptor` labels exist | §4, G8, §6.7 corrected; RTTI facts added; misleading comment fixed in `ConfigureAnalysis.py` |
| 5 | `Cataclysm.h` has 56 `DEFINE_FUNCTION` (§8 a) | 55 uses. The 56th grep hit is `#define` `:17`; the JSON's 56th function is `sub_53F9F0` (`AISkirmish.h:105`) | corrected |
| 6 | 45 `DEFINE_ADDRESS`, "most" interior (§8 a) | 44 uses (45th hit = `#define` `:14`), all in `Instructions` `:38`–`:124`; none is a function entry on the §6.7 run | corrected |
| 7 | "About 410 further address literals" in `Patch*.cpp`/`GameHooks.cpp`/`game/Game/*.h` (§8 a) | 10: `PatchRGL.cpp:29,93,98,99`, `GameHooks.cpp:158,166`, `PatchUniverse.cpp:393,416`, `AISkirmish.h:134` ×2. All are in `cet_symbols.json` | corrected, with list |
| 8 | §6.2 fixture row "was `FUN_10005a00`; renamed" vs §6.6 "same statuses as §6.2 (`same-name;…`)" | The current report reads `same-name;proto-applied`; the §6.2 detail came from an earlier reuse run | annotated |

**Verified correct (sample, with evidence):**
- Hashes and sizes: `cataclysm.exe` 5,369,856 B, `f03d9a3e…` (`sha256sum -c` against `MANIFEST.sha256` OK). Installer 360,195,200 B. Ghidra zip 569,649,598 B. rGL originals and replacements: all 6 sizes and hash prefixes in §8.0 match; `BACKUP_MANIFEST.sha256` checks OK.
- Ghidra install: `application.properties:8,11`; `pyghidra_launcher.py` `input()` at `:169,172,179,204`, `get_ghidra_venv` at `:131`, `ghidra_launch` at `:291`; `analyzeHeadlessREADME.md:241-242`; Jython zip under `Extensions/Ghidra/`; FID DB list; venv has pyghidra 3.1.0 + JPype1 1.5.2; JDK 21.0.12.1.
- `Disassembler.RESTRICT_DISASSEMBLY_TO_EXECUTE_MEMORY_PROPERTY` = `"Restrict Disassembly to Executable Memory"`, and `getInitializedMemory` tests `isInitialized()` and `isExecute()` (javap). Every BSim, VT, FID and `ExportProgramScript.java` path in §8 c/d exists.
- §6.7, recomputed from the exports: analyzer times; 11,194 functions and per-block counts; name sources; 29,405 edges split 28,056/1,012/263/74, with no `.text`↔`.cms_t` edge; FID 254+38; strings, imports (257 in 11 DLLs) and exports (17 + entry); the 3 failed decompiles and their addresses; pcode errors only at `0x0089…`/`0x00c5…`; the `tdb` block; the CET gate figures (56/21/34/1, 78, 50, the 4 container-less sites and their CET lines); the ≈4.8 KB gap (0x561eed–0x5631a0); `.data` is `rw-`, so the program-wide restrict does remove the 42 `.data` stubs.
- The snapshot run used pre-edit `ConfigureAnalysis`/`ExportAnalysis`/wrapper (no `noexec`/WRR/`blk_funcs` in the snapshot); `ImportSymbolsJson` and `MatchHW1Strings` are byte-identical to the repo's.
- §5.3, re-run standalone: 9,292 / 1,830 / 3,013 / 2,643 / 1,246; GoK 9,290 / 3,016; both together 3,018 / 1,246; GoK HEAD `3af31fb` 2026-09-25; no unbalanced-brace warnings. §6.4 figures (825/824, 1,416/1,416, 822/820) match `/tmp/claude-1000/ghidra-hw1elf/exports{,2,3}/homeworld/logs/headless.out`. §6.5 SMACKW32 (17→11, 176→170, 392→384; flag-only 17) matches `/tmp/claude-1000/ghidra-smack/exports-*`. §6.2b (21/34/1, 53/25, 28 containers, 15 named, 2 refused) matches `/tmp/claude-1000/ghidra-cetcompat`. G4 and G7 evidence is in `/tmp/claude-1000/ghidra-probe/{boom.out,lock.out}`.
- Script behaviour in §5.1/§5.2/§5.3 (sha/md5/image-base gate, `label-only inside:`, patch sites never disassembled, report columns, CSV columns, decompile marker and index, matcher scoring/threshold/one-to-one/apply rules) matches the code.

**Missing content that was added:** §6.8 (real-exe matcher results and the 2-of-4 disagreement); real-exe container naming and data typing (§6.7); RTTI-derived library list (Crypto++, Magick++, WON, std); `--export-name` in §3; a safe §8.0 step 7.

**Script fixes (tested):**
- `tools/ghidra_headless.sh` refuses more than 2 positional args (exit 2, tested).
- `tools/ghidra_headless.sh` warns in reuse mode when `--noexec-sections`/`--profile` would be ignored (tested on a scratch copy of the d3d8 stand-in project: warning printed, rc=0, 5 s).
- `ConfigureAnalysis.py` RTTI comment corrected (comment only; `py_compile` OK).

**Remaining issues (not fixed here):**
- The `__EMSCRIPTEN__` first-arm defect in `MatchHW1Strings.py` (§6.8, §9) is documented, not patched, because a patch would invalidate the measured §5.3/§6.4 figures.
- Matcher precision on Cataclysm is unmeasured beyond the 4-row spot check. Whether 0x005631f0 is `InitWindow` or an inlined `WinMain` is UNVERIFIED.
- §4 "Default" column entries other than Decompiler Parameter ID (60 s, PE under 2 MiB) and the string-length minimums (`OPTION_DEFAULT_MIN_STRING_LENGTH = 5`) were not checked against Ghidra code: AIF, Propagate External Parameters, Stack Create Param Variables, Variadic Override, Function Start *, Switch Table References.
- The quarantine's effect on the real exe (§8.0 steps 3–4) is still an expectation. The 4 container-less CET patch sites still need `WindowProc`/`ActivateMe` functions.
- `.gitignore` still lacks `ghidra/projects/` (81 MB `cataclysm_gog.rep` in the tree) and `__pycache__/`; a stray `ghidra/scripts/__pycache__/MatchHW1Strings.cpython-312.pyc` (14:14, not from the wrapper) is present.
- The import-count gap (257 Ghidra vs 263 fingerprint) is still unexplained.
