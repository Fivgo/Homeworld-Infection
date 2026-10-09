export const meta = {
  name: 'homeworld-infection-binary-map-r3',
  description: 'Round 3 of the cataclysm.exe binary map: fix the truncated mission bodies and create the deferred functions in Ghidra, name the table-reached and game-region FUN_* clusters with audited Opus producers, add HW1 prototypes now that the struct types exist, then one Ghidra apply and a coverage update',
  phases: [
    { title: 'Ghidra prep', detail: 'one Ghidra pass: mission body fix, deferred + uncovered-run function creation, re-export', model: 'opus' },
    { title: 'Propose', detail: 'five incremental producers over the r3a export', model: 'opus' },
    { title: 'Verify', detail: 'one adversarial auditor per producer', model: 'opus' },
    { title: 'Reconcile + Apply', detail: 'merge with rounds 1-2, one headless apply, coverage, commit', model: 'opus' },
  ],
}

// Two roots. REPO is the git worktree this round writes and commits in (branch r2-apply-coverage,
// stacked on round 2; subagents of the background orchestrator cannot write to the main checkout).
// MAIN holds what git never tracks: the Ghidra project, the exports, original/ and reference/.
const MAIN = '/home/evanmedinger/Documents/GameDev/Homeworld-Infection'
const REPO = MAIN + '/.claude/worktrees/r2-apply'
const REF = MAIN + '/reference'
const EXP2 = MAIN + '/ghidra/exports/cataclysm_gog_r2'
const EXP3 = MAIN + '/ghidra/exports/cataclysm_gog_r3a'
const PROP = REPO + '/ghidra/proposals/r3'
const GHIDRA = `cd ${REPO} && GHIDRA_PROJECTS_DIR=${MAIN}/ghidra/projects GHIDRA_EXPORTS_DIR=${MAIN}/ghidra/exports tools/ghidra_headless.sh`
const EXE = `${MAIN}/original/game/cataclysm.exe cataclysm_gog`

const COMMON = `
PROJECT: Homeworld-Infection: a 1-to-1 recreation of Homeworld: Cataclysm (GOG "Homeworld: Emergence", retail 1.01) for Linux and Windows 11.
PATHS (important): the working copy for this round is the git worktree ${REPO} (branch r2-apply-coverage). Read docs and write EVERY file there; never write into ${MAIN} directly (writes to the main checkout are blocked). Untracked inputs live only in ${MAIN}: the Ghidra project ${MAIN}/ghidra/projects/cataclysm_gog, the exports ${MAIN}/ghidra/exports/*, the game ${MAIN}/original/, the reference clones ${REF} (read-only). Ghidra runs (Ghidra stages only) use exactly: ${GHIDRA} [options] ${EXE}
Read ${REPO}/cycle/STATE.md first, then ${REPO}/docs/binary-map.md (rounds 1 and 2; the "Round 2" section is authoritative, its R2.6 lists the gaps this round targets), ${REPO}/docs/ghidra-pipeline.md (export formats), ${REPO}/docs/ghidra-round2-prep.md (A2 struct types), ${REPO}/docs/mission-code-map.md, ${REPO}/docs/cet-knowledge.md. Homeworld 1 source: ${REF}/HomeworldSDL/src and ${REF}/gardens-of-kadesh. CET: ${REF}/CataclysmEngineTweaks.

ROUND-2 STATE: 14,384 functions; real names on 7,055 (59.7% of function bytes); game region 65.7% real-named; 2,081 FUN_* left in the game region (549,585 bytes) plus 548 A1-created FUN_* reached through pointer tables. Applied symbol files: ${REPO}/ghidra/symbols_round1.json, symbols_round1_data.json, symbols_round2.json. Round-2 export with all names: ${EXP2}/. 1,735 Cataclysm/HW1 types are in the program under /Cataclysm (A2).

OUTPUT CONTRACT for proposals: ${PROP}/<key>.json = {"source":"<key>","method":"...","done_subsystems":[...],"proposals":[{"address":"0x00xxxxxx","name":"identifierOrEmpty","kind":"function|data","class":"crt|won|bink|rgl|hw1:<File.c>|cat-new|lib:<name>|unknown","confidence":"high|medium|low","evidence":"<=200 chars","hw1_equivalent":"HomeworldSDL/src/Game/File.c:func or empty","signature":"optional C prototype using /Cataclysm type names"}]}. Names are C identifiers; HW1 names verbatim when the function is the HW1 one; Cataclysm-only functions cat_<subsystem>Verb; never rename a CET anchor or an applied round-1/2 name without high-confidence evidence (say so in evidence). Validate JSON with python3 before finishing.

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
  bodies_fixed: { type: 'number' }, mission_region_covered_pct: { type: 'number' },
  summary: { type: 'string' }, problems: { type: 'array', items: { type: 'string' } } },
  required: ['export_dir', 'functions_created', 'functions_total', 'bodies_fixed', 'summary', 'problems'] }

phase('Ghidra prep')
log('Ghidra prep B1: mission body fix, deferred functions, uncovered mission runs')
const b1 = await agent(`${COMMON}
TASK (Ghidra stage B1, you may run Ghidra): make Ghidra's function bodies and entries match what rounds 1-2 established, then re-export. RESUME FIRST: if ${EXP3}/decompiled_index.csv exists and ${EXP3}/logs/headless.out ends with the export 'done' line, the headless pass already completed: skip to step 5 and report from the export and the report CSVs (derive counts by comparing ${EXP2}/functions.csv with ${EXP3}/functions.csv where a report is missing). Reuse any of the files below that a cut-off attempt already wrote.
1. Body fix list: ${REPO}/ghidra/proposals/r2/missions-r2.body-fix.csv (984 mission functions whose Ghidra size is below the kas2c size documented in ${REPO}/docs/mission-code-map.md; columns address,name,doc_end,doc_size,ghidra_size,lost_callees,lost_callee_names).
2. Write ${REPO}/ghidra/scripts/FixFunctionBodies.py in the style of ${REPO}/ghidra/scripts/CreateFunctions.py (same headless API conventions, Ghidra 12.1.4 PyGhidra): for each row, disassemble from the current body end to doc_end (clearing only undefined/data that blocks it, never another function's entry), then set the body to [address, doc_end] (Function.setBody with an AddressSet, or CreateFunctionCmd with an explicit body) unless another function entry lies strictly inside that range (then stop at that entry and report "clipped"). Count fixed/clipped/skipped/failed; write ${REPO}/ghidra/fix_bodies_r3.report.csv. After the body fix, scan the mission region 0x5d6fe0-0x670a0f for remaining runs not covered by any function body that start with a plausible prologue or are the target of a call/jump/pointer from mission code, and create unnamed functions there (record them in the report with reason "uncovered-run").
3. Deferred functions: ${REPO}/ghidra/proposals/r2/reconcile_deferred_functions.json (105 audited round-2 names at addresses that are code but not function entries; fields address,name,reason,confidence,evidence). Write ${REPO}/ghidra/create_functions_r3.json in CreateFunctions' input format ([{address,name,reason}], reason "r2-deferred") from it.
4. Run ONE headless pass on the existing project: ${GHIDRA} --create-functions ${REPO}/ghidra/create_functions_r3.json --post-script "FixFunctionBodies.py csv=${REPO}/ghidra/proposals/r2/missions-r2.body-fix.csv report=${REPO}/ghidra/fix_bodies_r3.report.csv" --export-name cataclysm_gog_r3a ${EXE}  (decompile ON, do NOT pass --reimport; adapt the --post-script argument syntax to what tools/ghidra_headless.sh and FixFunctionBodies.py actually accept; if the post-script runs after analysis is too late for the new bodies to be analysed, run the body fix as the first of two passes instead, but never two at once). Wait first until no 'tools/ghidra_headless.sh' process exists; on a project-lock error wait 60 s and retry up to 10 times. DETACH IT so a session restart cannot kill it: setsid nohup bash -c '<command>' > ${MAIN}/ghidra/exports/r3a_headless.log 2>&1 &  then poll every 60 s until no 'tools/ghidra_headless.sh' process exists. If the script's start check never fires under PyGhidra (round 2 A2 hit this), fix it and re-run.
5. Measure: functions before/after, bodies fixed/clipped, bytes of 0x5d6fe0-0x670a0f now inside function bodies (union of address+size; round 2 was 320,296 of 629,296 = 50.9%), how many deferred names landed as function names, decompile ok/failed. Write ${REPO}/docs/ghidra-round3-prep.md (section "B1") with those numbers and any failures verbatim.
Return via StructuredOutput: export_dir=${EXP3}, functions_created, functions_total (after), bodies_fixed, mission_region_covered_pct, summary, problems.`,
  { label: 'ghidra:B1-bodies-and-entries', phase: 'Ghidra prep', schema: GHIDRA_SCHEMA, model: 'opus', effort: 'high' })

const B1 = JSON.stringify(b1).slice(0, 1200)
const TASKS = [
  { key: 'table-targets', label: 'propose:table-targets', prompt: `TASK (key table-targets): name the functions reached only through pointer tables. Inputs: ${EXP3}/functions.csv, decompiled.c, strings.csv, call_graph.csv (B1 result: ${B1}); ${REPO}/ghidra/create_functions_r2.json (reason pointer-table) and ${REPO}/ghidra/create_functions_r2.report.csv. Targets: (a) the 548 A1-created functions still FUN_* (543 pointer-table targets + 5 found by analysis); find which table each sits in by reading the .data/.rdata dwords around its referencing pointer in ${MAIN}/original/game/cataclysm.exe (.data VA 0x74a000 = file offset 0x34a000, .rdata VA 0x72a000 = 0x32a000) and name it by its table's role (FE callback tables, chat command tables, ship/AI dispatch tables, KAS tables) and its body; largest first: 0x004a13f0 (colorScheme, 'Bad shiptype in %s'), chat 0x004c41c0 / 0x004ba280, FE draw 0x004573d0 / 0x0048cf30. (b) The 111 in the WON/Magick range get a library class with the round-2 libs-unknown method (${REPO}/ghidra/proposals/r2/libs-unknown.verified.json). (c) The unnamed functions B1 created in the mission region ("uncovered-run" in ${REPO}/ghidra/fix_bodies_r3.report.csv): name helpers reached from one mission mission<NN>_<verb>, kas runtime pieces per mission-code-map.md. Subsystems: fe-tables, chat, ship-ai-tables, other-tables, won-magick, mission-uncovered; write after each.` },
  { key: 'game-ai-ships', label: 'propose:game-ai-ships', prompt: `TASK (key game-ai-ships): fill the AI and ship gaps (docs/binary-map.md R2.5/R2.6) from ${EXP3}: the skirmish AI cluster under cat_aisPlay (0x0042fd50 4,691 bytes, 0x00432760, 0x00432df0), the sensors manager (cat_sm* callers: 0x00506720, 0x005053a0, 0x0050fd60, 0x00510e60, 0x004e8850, 0x004ee120), HW1 AIMoves.c (270/120 matched), AIUtilities.c (113/11), AITeam.c (80/16), Dock.c (113/18), CommandLayer.c (144/68), formation 0x00481ef0 / 0x00484310, and ship code: Leech 0x005b6f30, RepairDroid 0x005c32f0 (droidUpdateArray), 0x005c24e0 (three *SpecialTarget callers), and the 0-match ship files Ships/CloakGenerator.c, DDDFrigate.c, RepairCorvette.c, ResearchShip.c (say so when a ship does not exist in Cataclysm). Method: from named neighbours in call_graph.csv, compare decompiled bodies with the HW1 function (control flow, constants, string literals). Subsystems: skirmish-ai, sensors, aimoves, aiutil-aiteam, dock, commandlayer, formation, ship-code; write after each.` },
  { key: 'game-render-ui', label: 'propose:game-render-ui', prompt: `TASK (key game-render-ui): fill the presentation gaps (docs/binary-map.md R2.5/R2.6) from ${EXP3}: clouds/nebulae (HW1 Clouds.c, Nebulae.c 71/6; 0x00436aa0 caller cloudStartup, 0x00439130 cloudSetFog, 0x00438090, 0x00438c30), the main-view renderer (callers rndMainViewRenderFunction/rndRenderTask: 0x0057bec0, 0x0057d2c0, 0x0057ce00, 0x005a7310, 0x005312b0, 0x0057a410), NIS.c (159/30), Options.c (154/37), ShipSelect.c (68/10), mainrgn.c (93/20), utility.c (98/25), the dev console 0x0045a700, and the 0-match files PlugScreen.c, Color.c, Clipper.c, B-Spline.c, Hash.c, NavLights.c, Tracking.c, Twiddle.c, Volume.c, Tweak.c, BMP.c. Method as for the other gap producers: named neighbours, body comparison with the HW1 function. Subsystems: clouds-nebulae, mainview, nis, options, shipselect, mainrgn, utility, console, zero-match-files; write after each.` },
  { key: 'game-sim-mp', label: 'propose:game-sim-mp', prompt: `TASK (key game-sim-mp): fill the simulation/session gaps (docs/binary-map.md R2.5/R2.6) from ${EXP3}: MultiplayerGame.c (233/78), SaveGame.c (180/77; chunk writers and loaders, the version 0x10ad0027 users), SinglePlayer.c (107/35), Universe.c (93/25), crates 0x00460ed0 (HW1 Crates.c, 'Got %d RU's from crate'), ETG.c (244/167), and the densest remaining FUN_* bands 0x005axxxx (162), 0x0051xxxx (128), 0x0041xxxx (125), 0x004dxxxx (111): attribute each band's functions to HW1 files by callers/callees and strings first, then name. Also check the universe.world[] offset question (+0x3D9B0 derived vs CET's +0x3DA30; ${REPO}/docs/ghidra-round2-prep.md) against the decompiled users and report which is right with evidence (an open_question if unresolved). Subsystems: multiplayer, savegame, singleplayer, universe, crates, etg, band-5a, band-51, band-41, band-4d; write after each.` },
  { key: 'prototypes', label: 'propose:prototypes', prompt: `TASK (key prototypes): give named functions real prototypes now that A2 imported the types (/Cataclysm in the program; the declarations under ${MAIN}/ghidra/exports/headers_r2b or the worktree copy named in ${REPO}/docs/ghidra-round2-prep.md; layouts verified there). Scope: functions in ${REPO}/ghidra/symbols_round1.json and symbols_round2.json whose class is hw1:* and whose entry has no "signature" or was applied degraded (round 1: 6,408 without, 1,006 degraded; round 2: 880 without). For each, take the declaration from the HW1 source (${REF}/HomeworldSDL/src, the .h prototype or the .c definition), translate HW1 types to the /Cataclysm type names (sdword->sdword etc. as the A2 header set defines them), and check against the decompiled body in ${EXP3}/decompiled.c that the parameter count and the obvious pointer/value uses agree (cdecl vs stdcall by the caller's stack cleanup in call sites); drop or mark low where the Cataclysm body differs. Emit proposals that repeat the CURRENT applied name exactly with kind "function" and the "signature" field set (no renames in this set; note that in method). Subsystems: one per HW1 directory group: Game-a-f, Game-g-m, Game-n-s, Game-t-z, Ships, other; write after each.` },
]

function verifyPrompt(t, r) {
  return `${COMMON}
TASK: ADVERSARIAL AUDITOR for ${PROP}/${t.key}.json (producer summary: ${JSON.stringify(r && r.summary || '')}). RESUME RULE: if ${PROP}/${t.key}.verified.json exists with an "audit" object, return its numbers immediately.
Sample 30 proposals (python random.seed(13); stratified, at least 10 'high'); establish the truth independently from ${EXP3} (decompiled C, strings, call graph) and, for hw1 claims, the HW1 source function named; for signatures, check parameter count, calling convention and pointer types against the body and its call sites; verdict correct/wrong/unverifiable; precision = correct/(correct+wrong). Policy: >= 0.9 keep (drop the sampled-wrong); 0.7-0.9 downgrade high->medium and drop wrong; < 0.7 keep only 'high' entries you verified (verify up to 40 more). Write ${PROP}/${t.key}.verified.json (same shape + top-level "audit":{sampled,wrong,precision,action}); never edit the producer's file; validate JSON. No Ghidra, no commit.`
}

phase('Propose')
log(`Round 3: ${TASKS.length} producers over ${EXP3}`)
const results = await pipeline(
  TASKS,
  t => agent(COMMON + '\n' + t.prompt + `\nWrite ${PROP}/${t.key}.json (mkdir -p ${PROP}). Return via StructuredOutput with proposals_path=${PROP}/${t.key}.json.`,
             { label: t.label, phase: 'Propose', schema: PROP_SCHEMA, model: 'opus' }),
  (r, t) => agent(verifyPrompt(t, r), { label: 'audit:' + t.key, phase: 'Verify', schema: VERIFY_SCHEMA, model: 'opus' })
             .then(v => ({ key: t.key, produced: r, audit: v })),
)
const done = results.filter(Boolean)
log(`${done.length}/${TASKS.length} round-3 sets produced and audited`)

phase('Reconcile + Apply')
const digest = JSON.stringify(done.map(d => ({ key: d.key, count: d.produced && d.produced.count, precision: d.audit && d.audit.precision, findings: d.produced && d.produced.key_findings }))).slice(0, 16000)
const merged = await agent(`${COMMON}
TASK: RECONCILE round 3. Inputs: the applied files ${REPO}/ghidra/symbols_round1.json, symbols_round1_data.json, symbols_round2.json; the 105 deferred names B1 created (${REPO}/ghidra/create_functions_r3.json; B1 result: ${B1}); the verified sets ${PROP}/*.verified.json. Produce ${REPO}/ghidra/symbols_round3.json in the cet_symbols.json schema containing ONLY what to apply now: new names, renames of earlier names where a round-3 set gives higher-confidence evidence (list every rename with both names and the reason in ${PROP}/reconcile_report.md), and prototypes from the prototypes set (same name, signature field; check that ImportSymbolsJson.py applies a signature when the name is unchanged, and say in the report if it does not). Rules: data addresses anywhere inside mapped .data/.rdata; function addresses must exist in ${EXP3}/functions.csv (others go to ${PROP}/reconcile_deferred_functions.json for round 4); names unique across rounds 1-3 (suffix only when two DIFFERENT functions legitimately share a base name; prefer fixing the wrong one); never override a CET anchor without recording it under "CET differs". Validate JSON. Digest: ${digest}
Return via StructuredOutput: {symbols_path, total, renames: number, prototypes: number, conflicts: number, dropped: number, report_path}`,
  { label: 'reconcile:r3', phase: 'Reconcile + Apply', model: 'opus', effort: 'high',
    schema: { type: 'object', properties: { symbols_path: { type: 'string' }, total: { type: 'number' }, renames: { type: 'number' }, prototypes: { type: 'number' }, conflicts: { type: 'number' }, dropped: { type: 'number' }, report_path: { type: 'string' } }, required: ['symbols_path', 'total', 'renames', 'conflicts', 'dropped', 'report_path'] } })

if (!merged || !merged.symbols_path) {
  log('reconcile did not complete; stopping before Apply so it can be resumed')
  return { b1, producers: done.map(d => ({ key: d.key, count: d.produced && d.produced.count, precision: d.audit && d.audit.precision })), merged: null, applied: null }
}

const applied = await agent(`${COMMON}
TASK: APPLY round 3 (you may run Ghidra). Wait until no 'tools/ghidra_headless.sh' process exists; run ONE headless pass (no --reimport), DETACHED with setsid nohup (log ${MAIN}/ghidra/exports/r3_headless.log) and polled every 60 s: ${GHIDRA} --symbols ${merged.symbols_path} --export-name cataclysm_gog_r3 ${EXE}  (decompile ON; lock rules as before). Then compute coverage with ${REPO}/tools/binary_map_coverage.py exactly as round 2 did (read its usage and the Round 2 section of ${REPO}/docs/binary-map.md): real-named functions and bytes overall and in the game region 0x401000-0x5c9fff, the mission region 0x5d6fe0-0x670a0f (names AND body coverage), per class, per HW1 source file, prototypes applied/degraded/missing, the 60 largest still-unnamed functions with string hints. Append a "## Round 3" section to ${REPO}/docs/binary-map.md with the before/after table and ranked gaps for round 4 (R3.x numbering as in round 2), and update the binary-map rows of ${REPO}/cycle/STATE.md (binary-map-r3 COMPLETE with the numbers, next = round 4 targets; keep the rest of the file). Checkpoint: cd ${REPO} && CHECKPOINT_COAUTHOR="Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>" tools/checkpoint.sh "Binary map round 3: mission bodies, table targets, game-region clusters, prototypes" (it refuses forbidden paths itself; it commits on branch r2-apply-coverage, which is correct).
Reconcile result: ${JSON.stringify(merged)}
Return via StructuredOutput: {commit, map_path, named_functions, real_named_functions, total_functions, real_named_bytes_pct, named_bytes_pct_game_region, mission_region_body_pct, prototypes_applied, apply_failures, top_gaps: [string]}`,
  { label: 'apply:r3', phase: 'Reconcile + Apply', model: 'opus', effort: 'high',
    schema: { type: 'object', properties: { commit: { type: 'string' }, map_path: { type: 'string' }, named_functions: { type: 'number' }, real_named_functions: { type: 'number' }, total_functions: { type: 'number' }, real_named_bytes_pct: { type: 'number' }, named_bytes_pct_game_region: { type: 'number' }, mission_region_body_pct: { type: 'number' }, prototypes_applied: { type: 'number' }, apply_failures: { type: 'number' }, top_gaps: { type: 'array', items: { type: 'string' } } }, required: ['commit', 'named_functions', 'total_functions', 'apply_failures', 'top_gaps'] } })

return { b1, producers: done.map(d => ({ key: d.key, count: d.produced && d.produced.count, precision: d.audit && d.audit.precision })), merged, applied }
