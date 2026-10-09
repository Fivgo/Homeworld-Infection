export const meta = {
  name: 'homeworld-infection-binary-map-r2',
  description: 'Round 2 of the cataclysm.exe binary map: create the mission and ship-slot functions Ghidra missed, re-admit dropped data names, import struct types, fill the ranked naming gaps with audited Opus producers, then one Ghidra apply and a coverage update',
  phases: [
    { title: 'Ghidra prep', detail: 'two sequential Ghidra passes: function creation, then data labels + struct headers', model: 'opus' },
    { title: 'Propose', detail: 'six incremental producers over the new export', model: 'opus' },
    { title: 'Verify', detail: 'one adversarial auditor per producer', model: 'opus' },
    { title: 'Reconcile + Apply', detail: 'merge with round 1, one headless apply, coverage, commit', model: 'opus' },
  ],
}

const ROOT = '/home/evanmedinger/Documents/GameDev/Homeworld-Infection'
const REF = ROOT + '/reference'
const EXP1 = ROOT + '/ghidra/exports/cataclysm_gog_r1'
const EXP2 = ROOT + '/ghidra/exports/cataclysm_gog_r2a'
const PROP = ROOT + '/ghidra/proposals/r2'

const COMMON = `
PROJECT: Homeworld-Infection at ${ROOT}: a 1-to-1 recreation of Homeworld: Cataclysm (GOG "Homeworld: Emergence", retail 1.01) for Linux and Windows 11. Read ${ROOT}/cycle/STATE.md first (where things stand), then ${ROOT}/docs/binary-map.md (round-1 coverage and the ranked gaps), ${ROOT}/docs/ghidra-pipeline.md (pipeline and export formats), ${ROOT}/docs/mission-code-map.md (the KAS runtime table and the 2,085 mission Init/Watch names), ${ROOT}/docs/cet-knowledge.md, ${ROOT}/docs/hw1-engine-core.md / -gameplay.md / -presentation.md as checklists. Homeworld 1 source: ${REF}/HomeworldSDL/src and ${REF}/gardens-of-kadesh. CET: ${REF}/CataclysmEngineTweaks.

ROUND-1 STATE: Ghidra project ${ROOT}/ghidra/projects/cataclysm_gog has 8,312 applied symbols (${ROOT}/ghidra/symbols_round1.json; reconcile notes in ${ROOT}/ghidra/proposals/reconcile_report.md). Export with those names: ${EXP1}/ (functions.csv, strings.csv, symbols.csv, call_graph.csv, decompiled.c + decompiled_index.csv, imports.txt, exports.txt, datatypes.h). Still FUN_*: 2,202 functions (653 KB), all in the game region 0x401000-0x5c9fff. The mission-code region 0x5d6fe0-0x670a0f (629 KB) has NO function entries yet.

OUTPUT CONTRACT for proposals: ${PROP}/<key>.json = {"source":"<key>","method":"...","done_subsystems":[...],"proposals":[{"address":"0x00xxxxxx","name":"identifierOrEmpty","kind":"function|data","class":"crt|won|bink|rgl|hw1:<File.c>|cat-new|lib:<name>|unknown","confidence":"high|medium|low","evidence":"<=200 chars","hw1_equivalent":"HomeworldSDL/src/Game/File.c:func or empty","signature":"optional"}]}. Names are C identifiers; HW1 names verbatim when the function is the HW1 one; Cataclysm-only functions cat_<subsystem>Verb; never rename a CET anchor without high-confidence evidence (say so in evidence). Validate JSON with python3 before finishing.

SESSION RULE (this session restarts without warning, roughly hourly): write your output file after EVERY finished subsystem (overwrite the whole file, keep "done_subsystems" current). RESUME RULE: if your output file already exists, read it, keep its proposals, finish only the subsystems not in done_subsystems, then return. If everything is done, return its stats immediately.

HARD RULES: do NOT run Ghidra unless your task says you are a Ghidra stage (one project lock); read-only on exports and reference clones; no git commit (the final stage commits); ground every claim in a file you read; label guesses low. Your final text is a return value, not a message.
`

const PROP_SCHEMA = { type: 'object', properties: {
  proposals_path: { type: 'string' }, count: { type: 'number' }, summary: { type: 'string' },
  key_findings: { type: 'array', items: { type: 'string' } }, open_questions: { type: 'array', items: { type: 'string' } } },
  required: ['proposals_path', 'count', 'summary', 'key_findings', 'open_questions'] }
const VERIFY_SCHEMA = { type: 'object', properties: {
  verified_path: { type: 'string' }, sampled: { type: 'number' }, wrong: { type: 'number' }, precision: { type: 'number' },
  dropped: { type: 'number' }, notes: { type: 'array', items: { type: 'string' } } },
  required: ['verified_path', 'sampled', 'wrong', 'precision', 'dropped', 'notes'] }
const GHIDRA_SCHEMA = { type: 'object', properties: {
  export_dir: { type: 'string' }, functions_created: { type: 'number' }, functions_total: { type: 'number' },
  summary: { type: 'string' }, problems: { type: 'array', items: { type: 'string' } } },
  required: ['export_dir', 'functions_created', 'functions_total', 'summary', 'problems'] }

phase('Ghidra prep')
log('Ghidra prep A1: create the missing function entries (mission region, ship slot targets, pointer-table targets)')
const a1 = await agent(`${COMMON}
TASK (Ghidra stage A1, you may run Ghidra): make Ghidra know the functions it missed, then re-export. RESUME FIRST: if ${ROOT}/ghidra/create_functions_r2.json and ${ROOT}/ghidra/scripts/CreateFunctions.py already exist (a previous attempt was cut off), reuse them; if ${EXP2}/decompiled_index.csv exists and ${EXP2}/logs/headless.out ends with the export 'done' line, the headless pass already completed: skip to step 4 and report from the export and ${ROOT}/ghidra/create_functions_r2.report.csv (if the report is missing, derive counts by comparing ${EXP1}/functions.csv with ${EXP2}/functions.csv).
1. Build ${ROOT}/ghidra/create_functions_r2.json = [{"address":"0x...","name":"<name or empty>","reason":"mission-init|mission-watch|ship-slot|pointer-table|prologue-scan"}]:
   a. every mission Init/Watch address listed in ${ROOT}/docs/mission-code-map.md (2,085 names; parse its tables; keep the documented names);
   b. every ship-header function-slot target: read the ship header tables described in ${ROOT}/ghidra/proposals/ships.verified.json (data entries) by reading the raw bytes of ${ROOT}/original/game/cataclysm.exe (.data VA 0x74a000 = file offset 0x34a000, .rdata VA 0x72a000 = offset 0x32a000, .text VA 0x401000 = offset 0x1000); a slot target is a dword inside 0x401000-0x72a000;
   c. a sweep of .data/.rdata for dwords pointing into .text at addresses that are not function entries in ${EXP1}/functions.csv but start with a plausible prologue (55 8B EC, or 83 EC xx, or 81 EC, or 56/57/53 push sequences); record them as pointer-table targets.
2. Write ${ROOT}/ghidra/scripts/CreateFunctions.py in the same style as ${ROOT}/ghidra/scripts/ImportSymbolsJson.py (read that file for the API conventions that worked headless in Ghidra 12.1.4): for each entry, if no function exists at the address, disassemble there and create a function; if a name is given, apply it as the primary label; count created/skipped/failed; write ${ROOT}/ghidra/create_functions_r2.report.csv.
3. Run ONE headless pass on the existing project that executes CreateFunctions.py as a pre-script, then analysis of the newly created code (the existing ConfigureAnalysis profile is fine), then the normal export: read ${ROOT}/tools/ghidra_headless.sh, add a --create-functions FILE option if it lacks one (keep it backward compatible), and run it with --export-name cataclysm_gog_r2a and decompile ON (do NOT pass --reimport). Wait first until no 'tools/ghidra_headless.sh' process exists; on a project-lock error wait 60 s and retry up to 10 times. The run may take 20-40 minutes; that is expected. DETACH IT so a session restart cannot kill it: start it with  setsid nohup <command> > ${ROOT}/ghidra/exports/r2a_headless.log 2>&1 &  then poll every 60 s (sleep is fine inside your Bash) until no 'tools/ghidra_headless.sh' process exists; if you are cut off, the run continues and the next attempt finds the finished export.
4. Report in ${ROOT}/docs/ghidra-round2-prep.md (section "A1"): counts per reason, how many bytes of 0x5d6fe0-0x670a0f are now inside functions, the functions_total before/after, and any failures verbatim.
Return via StructuredOutput: export_dir=${EXP2}, functions_created, functions_total (after), summary, problems.`,
  { label: 'ghidra:A1-create-functions', phase: 'Ghidra prep', schema: GHIDRA_SCHEMA, model: 'opus', effort: 'high' })

log('Ghidra prep A2: re-admit dropped data names and import struct headers')
const a2 = await agent(`${COMMON}
TASK (Ghidra stage A2, you may run Ghidra; A1 has finished and exported ${EXP2}; its result: ${JSON.stringify(a1).slice(0, 1500)}).
1. Re-admit the 925 data names that round-1 reconcile dropped ("rule 5": address not in symbols.csv). They are listed in ${ROOT}/ghidra/proposals/reconcile_report.md; their sources are the .verified.json files under ${ROOT}/ghidra/proposals/ (core 844, ships 57, sim-cataclysm 24). Accept any address inside the mapped .data (0x74a000-0xc54fff) or .rdata (0x72a000-0x749fff) ranges. Write ${ROOT}/ghidra/symbols_round1_data.json in the cet_symbols.json schema (kind "data", notes "class=..."), unique names (suffix _2 on collisions, report them).
2. Struct types: Ghidra's headless C parser can load headers into the program's data type manager. Build a curated 32-bit (MSVC6, -Zp4) header set: CET's reconstructed headers under ${REF}/CataclysmEngineTweaks/game/Game (these are the Cataclysm layouts; prefer them) and, where CET has none, the Homeworld 1 headers for Universe, SpaceObj/Ship, Player, Task, Memory, Bullet, Effect (${REF}/HomeworldSDL/src/Game/*.h). Write ${ROOT}/ghidra/scripts/ImportHeaders.py that parses them (strip C++-only syntax first with a small preprocessor step if needed; document what you had to remove) and then applies pointer-to-struct types to the known globals from ${ROOT}/ghidra/cet_symbols.json (universe, the task table, etc.). Best effort: if a header fails to parse, skip it, keep going, and list it.
3. Run ONE headless pass (no --reimport), DETACHED with setsid nohup and polled as in A1, that applies symbols_round1_data.json and runs ImportHeaders.py, with decompile OFF (use --no-decompile; the final stage re-exports with decompile) and --export-name cataclysm_gog_r2b. Same lock rules as before.
4. Append section "A2" to ${ROOT}/docs/ghidra-round2-prep.md: data labels applied/failed, headers parsed/skipped with the reason, types applied to which globals.
Return via StructuredOutput: export_dir=<r2b dir>, functions_created=0, functions_total (from its functions.csv), summary, problems.`,
  { label: 'ghidra:A2-data-and-types', phase: 'Ghidra prep', schema: GHIDRA_SCHEMA, model: 'opus', effort: 'high' })

const TASKS = [
  { key: 'missions-r2', label: 'propose:missions-r2', prompt: `TASK (key missions-r2): name the mission code now that A1 created its functions (${EXP2}/functions.csv; A1 result: ${JSON.stringify(a1).slice(0, 800)}). Map every created Init/Watch function to the names in ${ROOT}/docs/mission-code-map.md (mechanical, script it), then look at the decompiled C of a sample of 40 to confirm the kas2c shape (switch on state, kasf* calls, FSM/state name strings) and fix any address/name mismatch. Also name the per-mission helper functions those Init/Watch bodies call that are still FUN_* (reached only from mission code: class cat-new, prefix mission<NN>_). Subsystems = one per mission plus tutorial and NIS levels; write after each.` },
  { key: 'ships-r2', label: 'propose:ships-r2', prompt: `TASK (key ships-r2): name the per-ship behaviour functions behind the ship-header slots. Inputs: ${ROOT}/ghidra/proposals/ships.verified.json (tables, slot layout), ${EXP2}/functions.csv and decompiled.c (A1 created the slot targets), the HW1 ship files ${REF}/HomeworldSDL/src/Ships/*.c for the shared ship types (compare bodies; reuse HW1 names like HeavyCorvetteAttack), and CET's ShipDefs.h for the 145 type names. Name each target <ShipName><Slot> (HW1 convention); for Cataclysm-only ship types the same convention with class cat-new; also emit the 57 header data entries as kind data. Subsystems = one per race folder (race_sect, race_beast, race_kushan, race_taidan, race_p1, race_traders, race_derelict); write after each.` },
  { key: 'gaps-render-fe', label: 'propose:gaps-render-fe', prompt: `TASK (key gaps-render-fe): fill these round-1 gaps from ${EXP2}: the renderer/particle cluster 0x4b0000-0x4b6fff and 0x4e0640 (caller partCreationHelper); the front-end print/draw functions 0x4a5e40-0x4aa4f0; the HW1 files with no or few matches: Camera.c, MeshAnim.c, BTG.c (candidate 0x429280), ETG.c (25/243 matched: name the rest by walking from the named ETG entry points through callees and comparing with ${REF}/HomeworldSDL/src/Game/ETG.c). Subsystems: particle, render-cluster, fe-print, camera, meshanim, btg, etg; write after each.` },
  { key: 'gaps-ai-flight', label: 'propose:gaps-ai-flight', prompt: `TASK (key gaps-ai-flight): fill these round-1 gaps from ${EXP2}: AI/command movement at 0x465860, 0x467b80, 0x468650; AIMoves.c (16/270 matched), AIOrders.c (0), FlightMan.c (0/63), ChannelFSM.c (0), Dock.c (5/113), and the HW1 Ships/*.c behaviour files (22/335). Method: start from the named neighbours (callers/callees in call_graph.csv), compare decompiled bodies with the HW1 source function by function (same control flow, same constants, same string literals), and name by evidence. Subsystems: aimoves, aiorders, flightman, channelfsm, dock, movement-cluster, ships-c; write after each.` },
  { key: 'gaps-input-save-sp', label: 'propose:gaps-input-save-sp', prompt: `TASK (key gaps-input-save-sp): fill these round-1 gaps from ${EXP2}: input handling at 0x566b60 and 0x56a610 (Game_BandBoxAttack strings; HW1 mouse/keyboard routing), Eval.c (0), SaveGame.c (13/180: the save/load chunk writers; the version 0x10ad0027 users), SinglePlayer.c (13/107), and RE-CHECK the 10 names that came only from CET patch sites (etgAtDefine, gunShipFirePower, nisUpdateTask, nisStartup, partRenderBillSystem, CollectResources, taskCallBackInit, universeUpdateTask, CheckPlayerWin, utyGameSystemsPreInit): confirm or correct each with evidence. Subsystems: input, eval, savegame, singleplayer, cet-recheck; write after each.` },
  { key: 'libs-unknown', label: 'propose:libs-unknown', prompt: `TASK (key libs-unknown): identify the three unknown statically linked library clusters that round 1 carved but could not name: "magick" (360 functions, 82 KB), "ijg" (76 functions, 23 KB), "cms" (12 functions). Use their strings (strings.csv), their callers from game code, and the HW1 source's third-party list (${REF}/HomeworldSDL/src/ThirdParty, the texture/.lif and .jpg loaders) to decide which libraries they are (candidate guesses to test, not assume: ImageMagick or a derivative for image loading, the IJG libjpeg, a colour-management or compression library for "cms"). Name the entry points the game calls (class lib:<name>) and leave internals class-only. Then resolve the round-1 compromise names: memFree_2 at 0x004ae130 is the real HW1 memFree while CET's memFree at 0x00673080 is the CRT free; propose the Ghidra names that are TRUE (CRT entry -> _free with CET's alias in notes; HW1 entry -> memFree) and flag that CET's own label differs; utyGameQuitToMain_2 at 0x005ab5b0 gets a cat_ name from its body; review the 91 _N-suffixed names in ${ROOT}/ghidra/symbols_round1.json and propose fixes where the duplicate is wrong. Subsystems: magick, ijg, cms, compromises, suffixes; write after each.` },
]

function verifyPrompt(t, r) {
  return `${COMMON}
TASK: ADVERSARIAL AUDITOR for ${PROP}/${t.key}.json (producer summary: ${JSON.stringify(r && r.summary || '')}). RESUME RULE: if ${PROP}/${t.key}.verified.json exists with an "audit" object, return its numbers immediately.
Sample 30 proposals (python random.seed(11); stratified, at least 10 'high'); establish the truth independently from ${EXP2} (decompiled C, strings, call graph) and, for hw1 claims, the HW1 source function named; verdict correct/wrong/unverifiable; precision = correct/(correct+wrong). Policy: >= 0.9 keep (drop the sampled-wrong); 0.7-0.9 downgrade high->medium and drop wrong; < 0.7 keep only 'high' entries you verified (verify up to 40 more). Write ${PROP}/${t.key}.verified.json (same shape + top-level "audit":{sampled,wrong,precision,action}); never edit the producer's file; validate JSON. No Ghidra, no commit.`
}

phase('Propose')
log(`Round 2: ${TASKS.length} producers over ${EXP2}`)
const results = await pipeline(
  TASKS,
  t => agent(COMMON + '\n' + t.prompt + `\nWrite ${PROP}/${t.key}.json (mkdir -p ${PROP}). Return via StructuredOutput with proposals_path=${PROP}/${t.key}.json.`,
             { label: t.label, phase: 'Propose', schema: PROP_SCHEMA, model: 'opus' }),
  (r, t) => agent(verifyPrompt(t, r), { label: 'audit:' + t.key, phase: 'Verify', schema: VERIFY_SCHEMA, model: 'opus' })
             .then(v => ({ key: t.key, produced: r, audit: v })),
)
const done = results.filter(Boolean)
log(`${done.length}/${TASKS.length} round-2 sets produced and audited`)

phase('Reconcile + Apply')
const digest = JSON.stringify(done.map(d => ({ key: d.key, count: d.produced && d.produced.count, precision: d.audit && d.audit.precision, findings: d.produced && d.produced.key_findings }))).slice(0, 16000)
const merged = await agent(`${COMMON}
TASK: RECONCILE round 2. Inputs: ${ROOT}/ghidra/symbols_round1.json (already applied), ${ROOT}/ghidra/symbols_round1_data.json (applied by A2), the verified sets ${PROP}/*.verified.json, and A1's created-function names (${ROOT}/ghidra/create_functions_r2.json). Produce ${ROOT}/ghidra/symbols_round2.json in the cet_symbols.json schema containing ONLY the symbols to apply now (new names, and renames of round-1 names where a round-2 set gives higher-confidence evidence; list every rename with both names and the reason in ${ROOT}/ghidra/proposals/r2/reconcile_report.md). Rules: data addresses are accepted anywhere inside mapped .data/.rdata; function addresses must exist in ${EXP2}/functions.csv; names unique across round 1 + round 2 (suffix only when two DIFFERENT functions legitimately share a base name; prefer fixing the wrong one); never override a CET anchor unless the libs-unknown set's compromise resolution says so, and then record it under "CET differs". Validate JSON. Digest: ${digest}
Return via StructuredOutput: {symbols_path, total, renames: number, conflicts: number, dropped: number, report_path}`,
  { label: 'reconcile:r2', phase: 'Reconcile + Apply', model: 'opus', effort: 'high',
    schema: { type: 'object', properties: { symbols_path: { type: 'string' }, total: { type: 'number' }, renames: { type: 'number' }, conflicts: { type: 'number' }, dropped: { type: 'number' }, report_path: { type: 'string' } }, required: ['symbols_path', 'total', 'renames', 'conflicts', 'dropped', 'report_path'] } })

if (!merged || !merged.symbols_path) {
  log('reconcile did not complete; stopping before Apply so it can be resumed')
  return { a1, a2, producers: done.map(d => ({ key: d.key, count: d.produced && d.produced.count, precision: d.audit && d.audit.precision })), merged: null, applied: null }
}

const applied = await agent(`${COMMON}
TASK: APPLY round 2 (you may run Ghidra). Wait until no 'tools/ghidra_headless.sh' process exists; run ONE headless pass (no --reimport), DETACHED with setsid nohup and polled every 60 s, applying ${merged.symbols_path} with decompile ON and --export-name cataclysm_gog_r2; lock rules as before. Then compute coverage exactly as round 1 did (read the method in ${ROOT}/docs/binary-map.md): real-named functions and bytes overall and in the game region 0x401000-0x5c9fff, the mission region 0x5d6fe0-0x670a0f now, per class, per HW1 source file (files with 0 matches listed), the 60 largest still-unnamed functions with string hints. Append a "## Round 2" section to ${ROOT}/docs/binary-map.md with the before/after table and the new ranked gaps; update ${ROOT}/cycle/STATE.md (binary-map-r2 complete, next = round 3 targets). Checkpoint: cd ${ROOT} && tools/checkpoint.sh "Binary map round 2: mission functions, ship slots, data names, struct types, gap fills" (the script refuses forbidden paths itself).
Reconcile result: ${JSON.stringify(merged)}
Return via StructuredOutput: {commit, map_path, named_functions, total_functions, named_bytes_pct_game_region, mission_region_named_pct, apply_failures, top_gaps: [string]}`,
  { label: 'apply:r2', phase: 'Reconcile + Apply', model: 'opus', effort: 'high',
    schema: { type: 'object', properties: { commit: { type: 'string' }, map_path: { type: 'string' }, named_functions: { type: 'number' }, total_functions: { type: 'number' }, named_bytes_pct_game_region: { type: 'number' }, mission_region_named_pct: { type: 'number' }, apply_failures: { type: 'number' }, top_gaps: { type: 'array', items: { type: 'string' } } }, required: ['commit', 'named_functions', 'total_functions', 'apply_failures', 'top_gaps'] } })

return { a1, a2, producers: done.map(d => ({ key: d.key, count: d.produced && d.produced.count, precision: d.audit && d.audit.precision })), merged, applied }
