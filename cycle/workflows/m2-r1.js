export const meta = {
  name: 'homeworld-infection-m2-r1',
  description: 'M2-R1, the delta inventory (Option A): run the HW1 engine against Cataclysm data, document the changed loader formats from the binary, classify every hw1:* function as identical/modified/rewritten, bootstrap src/ with the unmodified HW1 tree building on Linux, and design the checksum oracle; each audited, then STATE.md gets the M2-R2 order',
  phases: [
    { title: 'Inventory', detail: 'five incremental producers, one per M2_BRIEF R1 item', model: 'opus' },
    { title: 'Verify', detail: 'one adversarial auditor per producer', model: 'opus' },
    { title: 'Integrate', detail: 'exit-criteria check, M2-R2 order in STATE.md, checkpoint', model: 'opus' },
  ],
}

// Same two-root layout as the binary-map rounds. REPO is the git worktree this round writes and
// commits in (branch m2, from main eae37c9; subagents of the background orchestrator cannot write to
// the main checkout). MAIN holds what git never tracks: Ghidra exports, original/, reference/.
const MAIN = '/home/evanmedinger/Documents/GameDev/Homeworld-Infection'
const REPO = MAIN + '/.claude/worktrees/m2'
const REF = MAIN + '/reference'
const GOK = REF + '/gardens-of-kadesh'
const EXP = MAIN + '/ghidra/exports/cataclysm_gog_r5'
const SCRATCH = MAIN + '/original/scratch-m2'

const COMMON = `
PROJECT: Homeworld-Infection: a 1-to-1 recreation of Homeworld: Cataclysm (GOG "Homeworld: Emergence", retail 1.01) for Linux and Windows 11. M1 (the binary map) is complete; M2 (reimplementation) starts now with Option A: start from the unmodified Relic Homeworld 1 source (Gardens of Kadesh root commit ad1bb82) with the GoK SDL layer as platform donor, port only the Cataclysm differences. This round, M2-R1, is MEASUREMENT, not porting.
PATHS (important): the working copy is the git worktree ${REPO} (branch m2). Read docs and write EVERY tracked file there; never write into ${MAIN} directly (writes to the main checkout are blocked). Untracked inputs live only in ${MAIN}: the final named Ghidra export ${EXP}/ (functions.csv, symbols.csv, strings.csv, call_graph.csv, decompiled.c + decompiled_index.csv, datatypes.h), the game ${MAIN}/original/ (game/cataclysm.exe, game/cataclysm.big, game_extracted/cataclysm/), the reference clones ${REF} (HomeworldSDL, gardens-of-kadesh, CataclysmEngineTweaks, hw_rgl; READ-ONLY, never modify them) and the native HW1 build ${REF}/build-hwsdl/homeworld. Scratch space for anything derived from game data: ${SCRATCH}/ (gitignored, never inside the worktree). If a git command against a reference clone is refused by the harness, retry it as a plain command run from inside that clone's directory (cd first, in its own Bash call), and if that is refused too, read the files from the working tree and say so.
READ FIRST: ${REPO}/cycle/M2_BRIEF.md (this round's spec; your task is one of its R1 items), ${REPO}/docs/CONSTRAINTS.md (the user's rules), ${REPO}/docs/PLAN.md, ${REPO}/cycle/STATE.md. Then as needed: ${REPO}/docs/binary-map.md (Round 5 is authoritative), ${REPO}/docs/hw1-port-lessons.md (how the HW1/GoK build works on this machine), ${REPO}/docs/hw1-engine-core.md / -gameplay.md / -presentation.md, ${REPO}/docs/cet-knowledge.md, ${REPO}/docs/mission-code-map.md, ${REPO}/docs/big-archive-contents.md, ${REPO}/docs/ghidra-round2-prep.md (struct types).

SESSION RULE (the session can restart without warning): write your output after EVERY finished subsystem (overwrite the whole file; keep a progress marker: a "done_subsystems" list in JSON outputs, or an HTML comment <!-- done: a, b --> at the top of Markdown outputs). RESUME RULE: if your output already exists, read it, keep what is done, finish only what is missing, then return. If everything is done, return its stats immediately.

HARD RULES (docs/CONSTRAINTS.md): never commit or copy game data into the worktree (no .big, .exe, extracted assets, screenshots of game frames); never launch the GOG Cataclysm game; never inject input (no xdotool/XTEST/ydotool); no gamescope anywhere; do NOT run Ghidra (the export above is final for this round); keep the HW1 REULA notice wherever HW1 source goes; no git commit (the integrate stage commits); ground every claim in a file you read or a command you ran; label guesses. Your final text is a return value, not a message.
`

const PROD_SCHEMA = { type: 'object', properties: {
  outputs: { type: 'array', items: { type: 'string' } }, summary: { type: 'string' },
  counts: { type: 'object' }, key_findings: { type: 'array', items: { type: 'string' } },
  open_questions: { type: 'array', items: { type: 'string' } } },
  required: ['outputs', 'summary', 'key_findings', 'open_questions'] }
const AUDIT_SCHEMA = { type: 'object', properties: {
  audit_path: { type: 'string' }, sampled: { type: 'number' }, wrong: { type: 'number' }, precision: { type: 'number' },
  fixed_in_place: { type: 'number' }, verdict: { type: 'string' }, notes: { type: 'array', items: { type: 'string' } } },
  required: ['audit_path', 'sampled', 'wrong', 'precision', 'verdict', 'notes'] }

const TASKS = [
  { key: 'data-deltas', label: 'r1:data-deltas',
    prompt: `TASK (key data-deltas; M2_BRIEF R1 item 1): run the native HW1 engine ${REF}/build-hwsdl/homeworld against Cataclysm's data and catalogue every failure and divergence. Output: ${REPO}/docs/m2-r1-data-deltas.md.
1. Static inventory first (no run needed): list cataclysm.big with ${REF}/build-hwsdl/tools/biggie/biggie (see docs/hw1-port-lessons.md and docs/big-archive-contents.md) and compare it with the file set HW1 opens (its loaders' path strings in ${GOK}/src and the HW1 data layout): files Cataclysm lacks, files HW1 never asks for, renamed directories. Read the headers of a sample of each format (mesh .geo version 0x500 vs HW1's, .peo, .lif/textures, .script, ship .shp static info, string tables, .fib front-end screens) with xxd/python from ${MAIN}/original/game_extracted/cataclysm/ and compare with the HW1 loaders' expectations (version checks, magic numbers, field counts). Compare the 145 Cataclysm ship types (CET ShipDefs.h, ${REF}/CataclysmEngineTweaks) with HW1's ShipType enum.
2. Then run it. Assemble a scratch assets dir ${SCRATCH}/hw1-assets/ holding symlinks under the names HW1 expects (Homeworld.big -> cataclysm.big, etc.; read the GoK file-open code for the exact names and the HW_Data variable). Run with no input at all, bounded by timeout: first headless (SDL_VIDEODRIVER=offscreen, then dummy), and only if the engine cannot get far enough headless, windowed with /window, closed by the timeout (no input). Example: cd ${SCRATCH} && HW_Data=${SCRATCH}/hw1-assets SDL_VIDEODRIVER=offscreen timeout 60 ${REF}/build-hwsdl/homeworld /window > run1.log 2>&1. The build is an ASan/UBSan debug build: keep its reports. Iterate: each time it stops on a missing or incompatible file, record it, and where a harmless workaround (a symlink, a renamed path) lets it get further, apply it in the scratch dir and run again; never patch the reference build. Use the engine's own debug/log options (read main.c's command-line switches) to get the furthest load trace.
3. The document: one table per format (what HW1 expects, what Cataclysm ships, where it fails, the evidence: log line or byte offset), the run log excerpts that matter, the list of missing/renamed files, and a ranked "blocks the first frame" list. Subsystems: big-inventory, formats-static, ship-types, run-headless, run-windowed (if needed), write-up.` },
  { key: 'formats', label: 'r1:formats',
    prompt: `TASK (key formats; M2_BRIEF R1 item 2): document the Cataclysm versions of the loaders that changed vs HW1, from the named decompiled code in ${EXP} (use symbols.csv/functions.csv to find each loader by its now-applied name, decompiled.c for the body, datatypes.h and docs/ghidra-round2-prep.md for the struct types) and the HW1 source loaders in ${GOK}/src (at ad1bb82 where it differs from GoK HEAD). One file per format, written as soon as that format is done: ${REPO}/docs/formats/mesh.md (geometry version 0x500 and the .peo loader; HW1's meshLoad/.geo layout), textures.md (.lif and texture registry: texreg), script.md (.script parsing: every new key, with the parser function that consumes it), shipstatic.md (the ship static-info parser: new fields, 145 types, the CustShipHeader slots), savegame.md (save layout, version 0x10ad0027, chunk order vs HW1 SaveGame.c), fib.md (front-end .fib screens, FEMan). Each: the on-disk layout as a C struct with offsets and sizes, the loader's control flow in a few lines, a "diff vs HW1" table (field/key/branch, HW1, Cataclysm, evidence: function name + address), and check at least two real files from ${MAIN}/original/game_extracted/cataclysm/ against the layout with a small python reader (keep the reader in ${REPO}/tools/formats/<format>_probe.py; it must read files given on the command line and never copy data). Subsystems: mesh, textures, script, shipstatic, savegame, fib; write after each.` },
  { key: 'function-deltas', label: 'r1:function-deltas',
    prompt: `TASK (key function-deltas; M2_BRIEF R1 item 3): classify every function the binary map matched to Homeworld 1 (class hw1:<File.c>; find them through the class= notes in ${REPO}/ghidra/symbols_round*.json against the names in ${EXP}/functions.csv) as IDENTICAL / MODIFIED / REWRITTEN relative to its HW1 source function (${GOK}/src at ad1bb82: read the file with git show ad1bb82:<path> in the GoK clone, or diff the working tree against it).
Method, script-assisted: write ${REPO}/tools/delta_map.py that, per function, extracts the HW1 body (by name, from the C file; a simple brace matcher or clang -Xclang -ast-dump=json is fine) and the Cataclysm body from ${EXP}/decompiled.c, and computes comparable features: callee set (HW1 calls vs call_graph.csv callees by name), string literals, numeric constants, switch/case counts, loop and branch counts, parameter count; scores them; and proposes a class (IDENTICAL: same callees, strings and constants up to compiler noise; MODIFIED: mostly the same with a describable change; REWRITTEN: little in common). Then review by hand (read both bodies) every function the script cannot place confidently, and a sample of each class, and write a one-line "change" for each MODIFIED one (what Cataclysm added/removed/changed, e.g. "adds a 9th ship race to the switch", "new parameter for team colour"). Output ${REPO}/ghidra/delta_map.json = {"done_subsystems":[...],"method":"...","functions":[{"address","name","hw1_file","hw1_func","class":"IDENTICAL|MODIFIED|REWRITTEN","change":"one line or empty","confidence":"high|medium|low","features":{...},"reviewed":true|false}]} and ${REPO}/docs/m2-r1-function-deltas.md with a per-HW1-file table (file, matched, identical, modified, rewritten, bytes per class) and the totals: how much of the engine is free. Subsystems: tool, then one per HW1 directory group (Game a-f, Game g-m, Game n-s, Game t-z, Ships, other), then review, write-up; write after each.` },
  { key: 'bootstrap', label: 'r1:bootstrap',
    prompt: `TASK (key bootstrap; M2_BRIEF R1 item 4): create the recreation's source tree ${REPO}/src/ and make the UNMODIFIED Homeworld 1 engine build on Linux as hwi-engine. "Unmodified" means no Cataclysm changes; portability changes are allowed and must be recorded.
1. Read ${REPO}/docs/hw1-port-lessons.md completely (what GoK changed for 64-bit/Linux, how reference/build-hwsdl was built, packages, flags).
2. Base: the Relic tree at GoK commit ad1bb82 (src/Game, src/Ships, src/Win32 kept as reference only, plus what the build needs: tools/kas2c and the mission sources it compiles, the third-party pieces the build needs). Donor: from GoK HEAD take the SDL platform layer and ONLY the portability fixes the build needs. Get the files with git archive / git show from ${GOK} (read-only; export into ${SCRATCH}/gok-export/ first if that is easier, then copy into ${REPO}/src/). If the minimal-donor build turns into hundreds of hand-picked hunks, stop that approach and import GoK's whole src/ at HEAD instead (it descends from ad1bb82), and record that as a decision for the user. Either way write ${REPO}/src/PROVENANCE.md: base commit, donor commit, the list of files taken from each, and a diffstat of src/ against ad1bb82.
3. Licence: the HW1 source licence notice (GoK's COPYING.REULA and the README text about it: non-commercial, Relic/Gearbox terms) goes to ${REPO}/src/COPYING.REULA and at the top of ${REPO}/src/README.md, and a short licence paragraph in ${REPO}/README.md.
4. Build: ${REPO}/meson.build (or src/meson.build plus a root one) that builds the executable hwi-engine, reusing GoK's meson structure; ${REPO}/tools/build.sh [--debug|--release] [--clean] that runs meson setup + compile into ${REPO}/build/ (gitignored; check .gitignore). Mission .kas must go through kas2c as in GoK. It must build with zero errors; warnings are recorded, -Werror only if it passes. Build both debug and release once.
5. Write ${REPO}/docs/m2-r1-bootstrap.md: exact commands, package prerequisites, build time, the binary's size (debug and release), warnings count, and what was taken from where. Do not run the built binary against game data (data-deltas does that with the reference build); a --help style or no-data start to prove it links and starts is fine, with a timeout and no input.
Never put build outputs or game data into git. Subsystems: lessons, import, licence, meson, build-debug, build-release, write-up; write after each (record progress in the doc's done marker).` },
  { key: 'checksum-oracle', label: 'r1:checksum-oracle',
    prompt: `TASK (key checksum-oracle; M2_BRIEF R1 item 5): DESIGN (do not build) the behaviour oracle. Output ${REPO}/docs/m2-checksum-oracle.md.
1. HW1: how the multiplayer sync checksum is computed and when (Universe.c / univCheckSum*, the sync packets, the per-tick order of univUpdate; docs/hw1-engine-gameplay.md and ${GOK}/src at ad1bb82), what state it covers (ship positions/health, RNG seeds, resources...), and which float/compiler details make it deterministic only on the same binary.
2. Cataclysm: find the equivalents in ${EXP} by their applied names (symbols.csv) and decompiled bodies: the checksum function(s), their call sites per tick, the sync/packet code that compares them, the RNG. Diff against HW1.
3. The hook: a CET-style DLL for the GOG build (read how ${REF}/CataclysmEngineTweaks hooks and patches the exe, its loader, and docs/cet-knowledge.md) that, per universe tick, appends (tick, checksum, and a small set of sub-checksums for diagnosis) to a file. Exact hook sites (address, prologue bytes, calling convention from the applied prototypes), the record format, and how it is loaded under Wine/Proton without gamescope.
4. Driving a run without input injection: candidates to evaluate from the code: HW1's packet recording/playback (recPack*/ the record/playback command-line switches), a skirmish whose players are all computer players, a save game loaded and left to run, a mission's NIS. Pick the one that needs no input and is reproducible, with the evidence.
5. The recreation side: where hwi-engine would emit the same stream, and the comparison tool (first divergent tick, which sub-checksum).
6. Risks: x87 vs SSE floats, compiler differences that make bit-exact impossible, and the fallback (tolerance-based state comparison).
Subsystems: hw1-checksum, cat-checksum, hook-design, driving, recreation-side, risks; write after each (done marker in the doc).` },
]

const AUDITS = {
  'data-deltas': `Re-run a sample of the claims: re-list cataclysm.big and check 10 of the "missing/renamed" file entries; re-read 5 format headers with xxd and check the stated versions/magic; re-run the engine once with the doc's final command (same timeout, no input, headless if the doc says it got far enough headless) and confirm the first blocking failure matches. Check the doc never asks to commit data and that nothing derived from game data landed in ${REPO} (git status in ${REPO}).`,
  'formats': `For each of the six docs, check 5 layout fields/keys (30 in all) against the decompiled loader in ${EXP}/decompiled.c and the HW1 loader; run each tools/formats/*_probe.py on two different real files and confirm it parses without error and the values are plausible. A "diff vs HW1" row must name a function + address that really does that.`,
  'function-deltas': `Sample 40 functions from ${REPO}/ghidra/delta_map.json (python random.seed(23); stratified: 15 IDENTICAL, 15 MODIFIED, 10 REWRITTEN, at least 10 with reviewed=false): read both bodies yourself and give your own class; precision = agreement. Check the per-file totals in ${REPO}/docs/m2-r1-function-deltas.md add up from the JSON (script it). Re-run tools/delta_map.py on 3 functions to confirm it reproduces their features.`,
  'bootstrap': `From a clean build directory (rm -rf ${REPO}/build first, or use a fresh ${SCRATCH}/build-audit dir), run tools/build.sh exactly as the doc says, debug and release: both must succeed; compare binary sizes and warning counts with the doc. Check PROVENANCE.md against the files (spot-check 20: is the file from the commit it says, by git show/hash-object), that COPYING.REULA and the README licence notice are present, and that git status in ${REPO} shows no build outputs or game data staged or untracked outside .gitignore.`,
  'checksum-oracle': `Check every address/function/hook site the design names against ${EXP} (symbols.csv, decompiled.c) and every HW1 claim against the source; sample at least 25 claims. Check that the proposed driving method really needs no input (find the code path) and that nothing in the design injects input or needs gamescope.`,
}

function auditPrompt(t, r) {
  return `${COMMON}
TASK: ADVERSARIAL AUDITOR for M2-R1 item "${t.key}" (producer result: ${JSON.stringify(r || {}).slice(0, 2500)}). RESUME RULE: if ${REPO}/docs/audits/m2-r1-${t.key}.md exists and ends with a "Verdict:" line, return its numbers immediately.
${AUDITS[t.key]}
Fix factual errors you prove directly in the producer's documents (mark each fix inline with "[audit: ...]") and count them; anything you cannot settle goes to notes. Write ${REPO}/docs/audits/m2-r1-${t.key}.md: what you checked, each finding (correct/wrong/unverifiable with evidence), precision = correct/(correct+wrong), fixes made, and a last line "Verdict: PASS|PASS-WITH-FIXES|FAIL - <reason>". No commit.`
}

phase('Inventory')
log(`M2-R1: ${TASKS.length} producers, each audited`)
const results = await pipeline(
  TASKS,
  t => agent(COMMON + '\n' + t.prompt + `\nReturn via StructuredOutput: outputs (paths you wrote), summary, counts (the key numbers), key_findings, open_questions.`,
             { label: t.label, phase: 'Inventory', schema: PROD_SCHEMA, model: 'opus' }),
  (r, t) => agent(auditPrompt(t, r), { label: 'audit:' + t.key, phase: 'Verify', schema: AUDIT_SCHEMA, model: 'opus' })
             .then(v => ({ key: t.key, produced: r, audit: v })),
)
const done = results.filter(Boolean)
log(`${done.length}/${TASKS.length} M2-R1 items produced and audited`)

phase('Integrate')
const digest = JSON.stringify(done.map(d => ({ key: d.key, outputs: d.produced && d.produced.outputs, counts: d.produced && d.produced.counts, findings: d.produced && d.produced.key_findings, questions: d.produced && d.produced.open_questions, audit: d.audit && { precision: d.audit.precision, verdict: d.audit.verdict } }))).slice(0, 20000)
const integrated = await agent(`${COMMON}
TASK: INTEGRATE M2-R1. Digest of the five items and their audits: ${digest}
1. Exit criteria (M2_BRIEF R1): the five documents exist (docs/m2-r1-data-deltas.md, docs/formats/*.md, docs/m2-r1-function-deltas.md + ghidra/delta_map.json, docs/m2-r1-bootstrap.md + src/, docs/m2-checksum-oracle.md) and each audit verdict is PASS or PASS-WITH-FIXES; src/ builds: run ${REPO}/tools/build.sh --release once more and confirm. List any criterion not met, with the reason.
2. Write ${REPO}/docs/m2-r1-summary.md: one paragraph per item with its numbers, the audit precision and verdict, and the M2-R2 plan: the order from M2_BRIEF (file formats, mesh 0x500/.peo first -> universe and ships -> UI -> AI -> LAN -> missions) adjusted by what R1 measured, with the delta counts per stage (IDENTICAL/MODIFIED/REWRITTEN/cat-new functions and bytes) and the first concrete M2-R2 producers (about 10 agents per round).
3. Update ${REPO}/cycle/STATE.md: add a "## M2" section above "## Opus workflows in flight" with M2-R1 COMPLETE (date, the key numbers, links to the docs), the M2-R2 order with the delta counts, and under "Decisions the user owns" any decision R1 surfaced (e.g. whether src/ is minimal-donor or whole-GoK import, LAN interop with stock 1.01 from docs/PLAN.md, anything blocked). Keep the rest of the file; in the workflow table, the m2-r1 row is completed by the orchestrator, do not add one.
4. Check git status in ${REPO}: nothing from original/, reference/, build/ or game data may be staged or untracked outside .gitignore (fix .gitignore if a new output path needs it). Then checkpoint: cd ${REPO} && CHECKPOINT_COAUTHOR="Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>" tools/checkpoint.sh "M2-R1: delta inventory (data deltas, formats, function delta map, src/ bootstrap, checksum oracle design)" (it refuses forbidden paths itself; it commits on branch m2, which is correct).
Return via StructuredOutput.`,
  { label: 'integrate:m2-r1', phase: 'Integrate', model: 'opus', effort: 'high',
    schema: { type: 'object', properties: {
      commit: { type: 'string' }, criteria_met: { type: 'boolean' }, unmet: { type: 'array', items: { type: 'string' } },
      identical: { type: 'number' }, modified: { type: 'number' }, rewritten: { type: 'number' },
      build_ok: { type: 'boolean' }, binary_size_release: { type: 'string' },
      m2r2_order: { type: 'array', items: { type: 'string' } }, user_decisions: { type: 'array', items: { type: 'string' } }, summary: { type: 'string' } },
      required: ['commit', 'criteria_met', 'unmet', 'build_ok', 'm2r2_order', 'user_decisions', 'summary'] } })

return { items: done.map(d => ({ key: d.key, outputs: d.produced && d.produced.outputs, precision: d.audit && d.audit.precision, verdict: d.audit && d.audit.verdict })), integrated }
