export const meta = {
  name: 'homeworld-infection-binary-map-r5',
  description: 'Round 5 of the cataclysm.exe binary map: create the 54 deferred functions in Ghidra, name the last 458 game-region FUN_* clusters with audited Opus producers, close the prototype gaps, re-sample the round-4 low-confidence names, then one Ghidra apply and a coverage update',
  phases: [
    { title: 'Ghidra prep', detail: 'one Ghidra pass: deferred + analysis-found function creation, .data artefact check, re-export', model: 'opus' },
    { title: 'Propose', detail: 'five incremental producers over the r5a export', model: 'opus' },
    { title: 'Verify', detail: 'one adversarial auditor per producer', model: 'opus' },
    { title: 'Reconcile + Apply', detail: 'merge with rounds 1-4, one headless apply, coverage, commit', model: 'opus' },
  ],
}

// Two roots, as in rounds 3-4. REPO is the git worktree this round writes and commits in (branch
// r2-apply-coverage; subagents of the background orchestrator cannot write to the main checkout).
// MAIN holds what git never tracks: the Ghidra project, the exports, original/ and reference/.
const MAIN = '/home/evanmedinger/Documents/GameDev/Homeworld-Infection'
const REPO = MAIN + '/.claude/worktrees/r2-apply'
const REF = MAIN + '/reference'
const EXP_PREV = MAIN + '/ghidra/exports/cataclysm_gog_r4'
const EXP_NEW = MAIN + '/ghidra/exports/cataclysm_gog_r5a'
const PROP = REPO + '/ghidra/proposals/r5'
const GHIDRA = `cd ${REPO} && GHIDRA_PROJECTS_DIR=${MAIN}/ghidra/projects GHIDRA_EXPORTS_DIR=${MAIN}/ghidra/exports tools/ghidra_headless.sh`
const EXE = `${MAIN}/original/game/cataclysm.exe cataclysm_gog`
const APPLIED = `${REPO}/ghidra/symbols_round1.json, symbols_round1_data.json, symbols_round2.json, symbols_round3.json, symbols_round4.json`

const COMMON = `
PROJECT: Homeworld-Infection: a 1-to-1 recreation of Homeworld: Cataclysm (GOG "Homeworld: Emergence", retail 1.01) for Linux and Windows 11.
PATHS (important): the working copy for this round is the git worktree ${REPO} (branch r2-apply-coverage). Read docs and write EVERY file there; never write into ${MAIN} directly (writes to the main checkout are blocked). Untracked inputs live only in ${MAIN}: the Ghidra project ${MAIN}/ghidra/projects/cataclysm_gog, the exports ${MAIN}/ghidra/exports/*, the game ${MAIN}/original/, the reference clones ${REF} (read-only). Ghidra runs (Ghidra stages only) use exactly: ${GHIDRA} --symbols ${REPO}/ghidra/symbols_round4.json [options] ${EXE}  (always pass the latest applied symbols file: without --symbols, reuse mode re-applies ghidra/cet_symbols.json and reverts the _free/_malloc fixes).
Read ${REPO}/cycle/STATE.md first, then ${REPO}/docs/binary-map.md (rounds 1-4; the "Round 4" section is authoritative, its R4.5 lists the largest unnamed functions with callers and strings, its R4.6 lists the gaps this round targets), ${REPO}/docs/ghidra-pipeline.md (export formats), ${REPO}/docs/ghidra-round2-prep.md (struct types), ${REPO}/docs/ghidra-round3-prep.md and ${REPO}/docs/ghidra-round4-prep.md (body fix, the snapshot guard, off-cut repair), ${REPO}/docs/mission-code-map.md, ${REPO}/docs/cet-knowledge.md. Homeworld 1 source: ${REF}/HomeworldSDL/src and ${REF}/gardens-of-kadesh. CET: ${REF}/CataclysmEngineTweaks.

ROUND-4 STATE: 14,679 functions; real names on 9,049 (79.5% of function bytes); game region 93.1% named with 458 FUN_* left there (99,908 bytes); the mission region is done. Applied symbol files: ${APPLIED}. Round-4 export with all names and prototypes: ${EXP_PREV}/. Types are under /Cataclysm in the program.

OUTPUT CONTRACT for proposals: ${PROP}/<key>.json = {"source":"<key>","method":"...","done_subsystems":[...],"proposals":[{"address":"0x00xxxxxx","name":"identifierOrEmpty","kind":"function|data","class":"crt|won|bink|rgl|hw1:<File.c>|cat-new|lib:<name>|unknown","confidence":"high|medium|low","evidence":"<=200 chars","hw1_equivalent":"HomeworldSDL/src/Game/File.c:func or empty","signature":"optional C prototype using /Cataclysm type names","replaces":"optional: the applied name this proposal renames"}]}. Names are C identifiers; HW1 names verbatim when the function is the HW1 one; Cataclysm-only functions cat_<subsystem>Verb; never rename a CET anchor or an applied round-1..4 name without high-confidence evidence (fill "replaces" and say why in evidence). Validate JSON with python3 before finishing.

SESSION RULE (the session can restart without warning): write your output file after EVERY finished subsystem (overwrite the whole file, keep "done_subsystems" current). RESUME RULE: if your output file already exists, read it, keep its proposals, finish only the subsystems not in done_subsystems, then return. If everything is done, return its stats immediately.

HARD RULES: do NOT run Ghidra unless your task says you are a Ghidra stage (one project lock); read-only on exports and reference clones; never launch the game; no git commit (the final stage commits); ground every claim in a file you read; label guesses low. Your final text is a return value, not a message.
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
  deferred_named: { type: 'number' }, data_artefacts_removed: { type: 'number' },
  summary: { type: 'string' }, problems: { type: 'array', items: { type: 'string' } } },
  required: ['export_dir', 'functions_created', 'functions_total', 'deferred_named', 'summary', 'problems'] }

phase('Ghidra prep')
log('Ghidra prep D1: deferred functions, analysis-found functions, .data artefacts')
const d1 = await agent(`${COMMON}
TASK (Ghidra stage D1, you may run Ghidra): create the functions round 4 deferred, then re-export. RESUME FIRST: if ${EXP_NEW}/decompiled_index.csv exists and ${EXP_NEW}/logs/headless.out ends with the export 'done' line, the headless pass already completed: skip to step 5 and report from the export and the report CSVs (derive counts by comparing ${EXP_PREV}/functions.csv with ${EXP_NEW}/functions.csv where a report is missing). Reuse any of the files below that a cut-off attempt already wrote.
1. Deferred functions: ${REPO}/ghidra/proposals/r4/reconcile_deferred_functions.json (54 audited round-4 names at addresses that are code but not function entries: Options.c 10, UIControls.c 9, AIHandler.c 5, AIPlayer.c 5, cat-new 4, ...). Write ${REPO}/ghidra/create_functions_r5.json in CreateFunctions' input format (reason "r4-deferred"). Pre-check each address against ${EXP_PREV}/functions.csv ranges; report any inside an existing function ("inside:<owner>") rather than splitting it unless the deferred evidence shows the owner's body is wrong.
2. Use round 4's mechanism exactly (${REPO}/docs/ghidra-round4-prep.md): CreateFunctions.py as a --post-script with analyze=0, then the only analysis inside the FixFunctionBodies snapshot guard; keep the 41 CALL_RETURN overrides on tail-call JMPs (clear overrides only on CALL instructions). Make CreateFunctions.py's plate-comment mark take a parameter (tag/mark) instead of the hard-coded "[R2]" while you are there, backward compatible.
3. The 42 one-byte FUN_* "functions" in .data at 0x00891f6c-0x00892114 (docs/binary-map.md R4.6 item 6): decide from the bytes and their references whether they are code. If they are not (no executable section, no call/jump into them, data references only), remove the function objects (leave any data and labels) and list each; if any one is real, keep it and say why.
4. Run ONE headless pass on the existing project (command form in PATHS, --export-name cataclysm_gog_r5a, decompile ON, never --reimport). Rehearse first on a copy in ${MAIN}/ghidra/projects/scratch-r5/ if you change any script logic (inside the gitignored projects dir, never /tmp: the project holds the game's bytes; delete the copy when done). Wait first until no 'tools/ghidra_headless.sh' process exists; on a project-lock error wait 60 s and retry up to 10 times. DETACH IT: setsid nohup bash -c '<command>' > ${MAIN}/ghidra/exports/r5a_headless.log 2>&1 &  then poll every 60 s until no 'tools/ghidra_headless.sh' process exists. Grep the log for 'ERROR' lines too.
5. Measure: functions before/after, deferred names landed, analysis-found new functions, .data artefacts removed, snapshot repair counts, decompile ok/failed. Write ${REPO}/docs/ghidra-round5-prep.md (section "D1") with those numbers and any failures verbatim.
Return via StructuredOutput: export_dir=${EXP_NEW}, functions_created, functions_total (after), deferred_named, data_artefacts_removed, summary, problems.`,
  { label: 'ghidra:D1-deferred', phase: 'Ghidra prep', schema: GHIDRA_SCHEMA, model: 'opus', effort: 'high' })

const D1 = JSON.stringify(d1).slice(0, 1200)
const METHOD = `Method: start from named neighbours in ${EXP_NEW}/call_graph.csv, compare decompiled bodies (${EXP_NEW}/decompiled.c) with the HW1 function (control flow, constants, string literals, call order of adjacent functions), and name by evidence. Every FUN_* in your clusters gets a proposal: a real name where the evidence carries it, otherwise a low-confidence cat_<subsystem>Verb from its callers and body (say so in evidence).`
const TASKS = [
  { key: 'ui-screens', label: 'propose:ui-screens', prompt: `TASK (key ui-screens): from ${EXP_NEW} (D1 result: ${D1}), the UI clusters of docs/binary-map.md R4.5/R4.6 item 1: selection/mouse UI and Cataclysm special-ship UI 0x00507xxx-0x00508xxx (0x00508be0 with 17 callers, 0x00508900, 0x005086a0, 0x00507bc0, 0x00507740, 0x00507f10, 0x005080f0) and 0x005298e0 / 0x00529cb0; fog of war 0x0047f-0x00480 (0x00480650, 0x0047fd80, 0x004802d0, 0x0047f850); region screens 0x00534300, 0x00532df0; game stats 0x00488e70, 0x00488c80, 0x00489470; key bindings 0x0049b1b0, 0x0049b590, 0x0054e410; colour 0x0052ba90, 0x0052b890 and the colour picker 0x0043df90, 0x0043e4c0; load/init 0x004c3490 ('Matrox', the largest left), 0x004b7880, 0x00528190, 0x00523d00, 0x004a3a10, 0x005732f0; HW1 files mainrgn.c, UIControls.c, TaskBar.c, main.c, Options.c, Stats.c. Then every remaining FUN_* in the 0x0053xxxx band (36, untouched since round 3). ${METHOD} Subsystems: selection-ui, fog-of-war, region-screens, stats-keys, colour, load-init, hw1-ui-files, band-53; write after each.` },
  { key: 'render-combat', label: 'propose:render-combat', prompt: `TASK (key render-combat): from ${EXP_NEW} (D1 result: ${D1}), the rendering, combat and ship clusters of docs/binary-map.md R4.5/R4.6 item 1: rendering 0x005750c0, 0x00576010, 0x0050dfe0, 0x0050d7c0, 0x0050d560, 0x00575c90, 0x00572010, 0x00569030, 0x00570300, 0x004e7240; combat and ship code 0x0048a2f0, 0x0048cd40, 0x00542cb0, 0x0043a6b0, 0x004361c0, 0x00460850, 0x004605b0, 0x005c8e30, 0x0044f290, 0x0044f490, 0x00449790; 0x005556e0 (mixfft.c?); HW1 files AIMoves.c, Dock.c, SalCapCorvette.c, Sensors.c, Nebulae.c. Then every remaining FUN_* in the 0x0050xxxx band (48, untouched since round 3) and the 0x0057xxxx band (37). ${METHOD} Subsystems: render, combat, ship-code, aimoves, dock-salcap, sensors-nebulae, band-50, band-57; write after each.` },
  { key: 'bands-files', label: 'propose:bands-files', prompt: `TASK (key bands-files): from ${EXP_NEW} (D1 result: ${D1}), every FUN_* left in the game region 0x401000-0x5c9fff that is NOT in the bands 0x0050xxxx, 0x0053xxxx or 0x0057xxxx and not in the clusters listed in docs/binary-map.md R4.5 item 1 (other producers cover those): list them from ${EXP_NEW}/functions.csv, then work band by band (0x0052xxxx 44, 0x0048xxxx 21, 0x004axxxx 33, 0x0055xxxx 38, 0x0043xxxx 20, and the rest), using the HW1 files with the largest gaps as candidates (SaveGame.c 180/83, SinglePlayer.c 107/45, MultiplayerGame.c 233/175, CommandLayer.c 144/97). Also give real names to the 14 titan-libs placeholders (hw1_ShipSelect_00511ea0 ... cat_new_005166b0; R4.6 item 6) and the 3 functions round 4's analysis found (FUN_004bc250, FUN_004da730 = opResItemDraw?, FUN_004daa80 = opRenderItemDraw?), with "replaces" set. ${METHOD} Subsystems: one per 0x00XXxxxx band you touch, then placeholders, analysis-found; write after each.` },
  { key: 'prototypes-r5', label: 'propose:prototypes-r5', prompt: `TASK (key prototypes-r5): prototypes (R4.6 item 3), from the HW1 declarations translated to /Cataclysm type names, each checked against the body in ${EXP_NEW}/decompiled.c and the stack cleanup at its call sites. (a) The 1,821 real-named functions whose symbol entry has no prototype (196 of them round-4 names): hw1:* ones from the HW1 declaration; cat-new ones only where the body makes the parameter count and types clear (else skip). (b) The 107 of round 4's C1 functions that have an audited signature in ${REPO}/ghidra/proposals/r3/reconcile_deferred_functions.json, after checking each against the binary. (c) The 46 still applied degraded and the 2 round-1 prototypes that do not parse (univHideShipFromSpheres 0x00547c20, univReallyDeleteThisDerelictRightNow 0x00549700). (d) The D1-created functions. Emit proposals that repeat the CURRENT applied name exactly with kind "function" and the "signature" field set (no renames). Subsystems: Game-a-f, Game-g-m, Game-n-s, Game-t-z, Ships-other, cat-new, r3-deferred-sigs, degraded-unparsed, d1; write after each.` },
  { key: 'lowconf-r4', label: 'propose:lowconf-r4', prompt: `TASK (key lowconf-r4): re-check the 180 low-confidence names applied in round 4 (find them by confidence in ${REPO}/ghidra/proposals/r4/*.verified.json among the entries of ${REPO}/ghidra/symbols_round4.json; 136 are cat_* names, by band 0x0051xxxx 35, 0x005cxxxx 30, 0x005axxxx 27, 0x005bxxxx 24, e.g. the Acolyte/ACV/module and sentinel-field families) against the bodies in ${EXP_NEW}: for each, confirm (no proposal), raise to medium/high with new evidence (proposal with the same name and the new confidence), correct (new name, "replaces" set) or mark for removal (name "", "replaces" set, evidence "remove: ..."). Note: round 4's quality-recheck proposed 4 changes and the audit rejected 3; a rename needs the old name shown WRONG, not merely vaguer. Subsystems: band-51, band-5a, band-5b, band-5c, other; write after each.` },
]

function verifyPrompt(t, r) {
  return `${COMMON}
TASK: ADVERSARIAL AUDITOR for ${PROP}/${t.key}.json (producer summary: ${JSON.stringify(r && r.summary || '')}). RESUME RULE: if ${PROP}/${t.key}.verified.json exists with an "audit" object, return its numbers immediately.
Sample 30 proposals (python random.seed(19); stratified, at least 10 'high', and at least 10 renames/removals if the set has any); establish the truth independently from ${EXP_NEW} (decompiled C, strings, call graph) and, for hw1 claims, the HW1 source function named; for signatures, check parameter count, calling convention and pointer types against the body and its call sites; for renames and removals, the old name must be shown wrong, not merely less good; verdict correct/wrong/unverifiable; precision = correct/(correct+wrong). Policy: >= 0.9 keep (drop the sampled-wrong); 0.7-0.9 downgrade high->medium and drop wrong; < 0.7 keep only 'high' entries you verified (verify up to 40 more). Write ${PROP}/${t.key}.verified.json (same shape + top-level "audit":{sampled,wrong,precision,action}); never edit the producer's file; validate JSON. No Ghidra, no commit.`
}

phase('Propose')
log(`Round 5: ${TASKS.length} producers over ${EXP_NEW}`)
const results = await pipeline(
  TASKS,
  t => agent(COMMON + '\n' + t.prompt + `\nWrite ${PROP}/${t.key}.json (mkdir -p ${PROP}). Return via StructuredOutput with proposals_path=${PROP}/${t.key}.json.`,
             { label: t.label, phase: 'Propose', schema: PROP_SCHEMA, model: 'opus' }),
  (r, t) => agent(verifyPrompt(t, r), { label: 'audit:' + t.key, phase: 'Verify', schema: VERIFY_SCHEMA, model: 'opus' })
             .then(v => ({ key: t.key, produced: r, audit: v })),
)
const done = results.filter(Boolean)
log(`${done.length}/${TASKS.length} round-5 sets produced and audited`)

phase('Reconcile + Apply')
const digest = JSON.stringify(done.map(d => ({ key: d.key, count: d.produced && d.produced.count, precision: d.audit && d.audit.precision, findings: d.produced && d.produced.key_findings }))).slice(0, 16000)
const merged = await agent(`${COMMON}
TASK: RECONCILE round 5. Inputs: the applied files ${APPLIED}; the 54 deferred names D1 created (${REPO}/ghidra/create_functions_r5.json; D1 result: ${D1}); the verified sets ${PROP}/*.verified.json. Produce ${REPO}/ghidra/symbols_round5.json in the cet_symbols.json schema containing ONLY what to apply now: new names, renames and removals the audit upheld (round 4's reconcile report ${REPO}/ghidra/proposals/r4/reconcile_report.md says how removals are applied), confidence raises, and prototypes (same name, signature field). List every rename/removal with both names and the reason in ${PROP}/reconcile_report.md. Rules: data addresses anywhere inside mapped .data/.rdata; function addresses must exist in ${EXP_NEW}/functions.csv (others go to ${PROP}/reconcile_deferred_functions.json); names unique across rounds 1-5 (suffix only when two DIFFERENT functions legitimately share a base name; prefer fixing the wrong one); never override a CET anchor without recording it under "CET differs". Validate JSON. Digest: ${digest}
Return via StructuredOutput: {symbols_path, total, renames: number, removals: number, prototypes: number, conflicts: number, dropped: number, report_path}`,
  { label: 'reconcile:r5', phase: 'Reconcile + Apply', model: 'opus', effort: 'high',
    schema: { type: 'object', properties: { symbols_path: { type: 'string' }, total: { type: 'number' }, renames: { type: 'number' }, removals: { type: 'number' }, prototypes: { type: 'number' }, conflicts: { type: 'number' }, dropped: { type: 'number' }, report_path: { type: 'string' } }, required: ['symbols_path', 'total', 'renames', 'conflicts', 'dropped', 'report_path'] } })

if (!merged || !merged.symbols_path) {
  log('reconcile did not complete; stopping before Apply so it can be resumed')
  return { d1, producers: done.map(d => ({ key: d.key, count: d.produced && d.produced.count, precision: d.audit && d.audit.precision })), merged: null, applied: null }
}

const applied = await agent(`${COMMON}
TASK: APPLY round 5 (you may run Ghidra). Wait until no 'tools/ghidra_headless.sh' process exists; run ONE headless pass (no --reimport), DETACHED with setsid nohup (log ${MAIN}/ghidra/exports/r5_headless.log) and polled every 60 s: ${GHIDRA} --symbols ${merged.symbols_path} --export-name cataclysm_gog_r5 ${EXE}  (decompile ON; lock rules as before; handle removals as round 4 did). Then compute coverage with ${REPO}/tools/binary_map_coverage.py exactly as round 4 did (its --history/--label options; read the Round 4 section of ${REPO}/docs/binary-map.md): real-named functions and bytes overall and in the game region 0x401000-0x5c9fff, per class, per HW1 source file, prototypes applied/degraded/missing, confidence mix, the largest still-unnamed functions with string hints. Append a "## Round 5" section to ${REPO}/docs/binary-map.md with the before/after table and the remaining gaps (R5.x numbering), and add a short verdict there: is another naming round worth its cost (about 3M subagent tokens per round), or should the next work be something else from ${REPO}/docs/PLAN.md? Give the evidence (bytes and functions still unnamed, what kind they are). Update the binary-map rows of ${REPO}/cycle/STATE.md (binary-map-r5 COMPLETE with the numbers; next = round 6 targets OR "naming saturated" per that verdict; keep the rest of the file). Checkpoint: cd ${REPO} && CHECKPOINT_COAUTHOR="Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>" tools/checkpoint.sh "Binary map round 5: deferred functions, last game-region clusters, prototypes, low-confidence re-check" (it refuses forbidden paths itself; it commits on branch r2-apply-coverage, which is correct).
Reconcile result: ${JSON.stringify(merged)}
Return via StructuredOutput: {commit, map_path, named_functions, real_named_functions, total_functions, real_named_bytes_pct, named_bytes_pct_game_region, game_region_fun_left, prototypes_applied, apply_failures, another_round_worth_it: boolean, verdict, top_gaps: [string]}`,
  { label: 'apply:r5', phase: 'Reconcile + Apply', model: 'opus', effort: 'high',
    schema: { type: 'object', properties: { commit: { type: 'string' }, map_path: { type: 'string' }, named_functions: { type: 'number' }, real_named_functions: { type: 'number' }, total_functions: { type: 'number' }, real_named_bytes_pct: { type: 'number' }, named_bytes_pct_game_region: { type: 'number' }, game_region_fun_left: { type: 'number' }, prototypes_applied: { type: 'number' }, apply_failures: { type: 'number' }, another_round_worth_it: { type: 'boolean' }, verdict: { type: 'string' }, top_gaps: { type: 'array', items: { type: 'string' } } }, required: ['commit', 'named_functions', 'total_functions', 'apply_failures', 'another_round_worth_it', 'verdict', 'top_gaps'] } })

return { d1, producers: done.map(d => ({ key: d.key, count: d.produced && d.produced.count, precision: d.audit && d.audit.precision })), merged, applied }
