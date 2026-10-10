export const meta = {
  name: 'homeworld-infection-m2-r3',
  description: 'M2-R3, stage 2 part A of the port: stage-1 close-out + systemic headers, ETG/particle argument arrays, selection, ship code A (HW1 headers), universe statics + MEX, each ported into src/ and audited in sequence; alongside, a small Ghidra pass for the 3 audited name fixes; then the R3 exit test (sp reaches universe running on Mission01) and STATE.md',
  phases: [
    { title: 'Port', detail: 'five src/ producers in sequence (they share src/ and the build), each followed by its auditor', model: 'opus' },
    { title: 'Ghidra', detail: 'apply the 3 name fixes from ghidra/proposals/m2-r2-name-fixes.json, in parallel with Port', model: 'opus' },
    { title: 'Integrate', detail: 'R3 exit test, summary, STATE.md, checkpoint', model: 'opus' },
  ],
}

// Same two-root layout as M2-R1/R2. REPO is the git worktree this round writes and commits in (branch m2).
// MAIN holds what git never tracks: Ghidra project and exports, original/, reference/.
const MAIN = '/home/evanmedinger/Documents/GameDev/Homeworld-Infection'
const REPO = MAIN + '/.claude/worktrees/m2'
const REF = MAIN + '/reference'
const EXP = MAIN + '/ghidra/exports/cataclysm_gog_r5'
const SCRATCH = MAIN + '/original/scratch-m2'
const HARNESS = SCRATCH + '/m2r2-harness'
const CKPT = `cd ${REPO} && CHECKPOINT_COAUTHOR="Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>" tools/checkpoint.sh`
const GHIDRA = `cd ${REPO} && GHIDRA_PROJECTS_DIR=${MAIN}/ghidra/projects GHIDRA_EXPORTS_DIR=${MAIN}/ghidra/exports tools/ghidra_headless.sh`
const EXE = `${MAIN}/original/game/cataclysm.exe cataclysm_gog`

const COMMON = `
PROJECT: Homeworld-Infection: a 1-to-1 recreation of Homeworld: Cataclysm (GOG "Homeworld: Emergence", retail 1.01) for Linux and Windows 11. M2 (reimplementation) follows Option A: the engine is Homeworld 1 (src/ = Gardens of Kadesh 3af31fb) and we port Cataclysm's differences into it. M2-R1 measured the deltas; M2-R2 ported stage 1 (formats + data load; exit test met: the debug ASan engine reaches Main_game_screen on unmodified data, and single player passes the RACE_* stop, stopping at the GenericInterceptor.c (205) assert). This round, M2-R3, is stage 2 part A per ${REPO}/docs/m2-r2-summary.md "Plan for M2-R3 and M2-R4".
USER DECISIONS (binding): src/ base = GoK 3af31fb; Cataclysm behaviour wins per subsystem; 16 Hz universe default; FP/optimisation flags pinned (-O2 -fno-strict-aliasing, no fast-math) before any checksum comparison; Relic ad1bb82 Game/ and Ships/ at src/reference/hw1-ad1bb82/ (reference only); the oracle may call the game's own record/playback functions; DLL toolchain clang-18 + lld-link; LAN interop with stock 1.01 is nice-to-have, not required (do not break the wire format gratuitously). 2026-10-10: the faithfulness defaults already in the tree are ACCEPTED (the 22 dropped HW1 types dormant as ids 200-221; k/tCloakGenerator on DefaultShipHeader where the exe has NULL; crateInit resets 46 entries; the mission-17 texture cut follows the stock exe, not CET; the fq audio code follows the binary). The user will supply the oracle reference recording and two saves LATER: do not block on them; build what does not need them and leave a clear hook where they plug in.
PATHS: the working copy is the git worktree ${REPO} (branch m2). Read docs and write EVERY tracked file there; never write into ${MAIN} directly (blocked). Untracked inputs in ${MAIN}: the named Ghidra export ${EXP}/ (functions.csv, symbols.csv, strings.csv, call_graph.csv, decompiled.c, datatypes.h; a parallel Ghidra stage this round writes a NEW export cataclysm_gog_r6, which only renames 3 functions; keep reading r5), the game ${MAIN}/original/ (game/, game_extracted/cataclysm/), the reference clones ${REF} (read-only; CET headers in ${REF}/CataclysmEngineTweaks). Scratch for anything derived from game data: ${SCRATCH}/ (gitignored); R2's tool outputs are there too (e.g. ${SCRATCH}/m2r2-shipstatic-tools/headers.txt).
READ FIRST: ${REPO}/cycle/STATE.md ("## M2"), ${REPO}/docs/m2-r2-summary.md (your producer's row in the M2-R3 table and "What is left of stage 1"), ${REPO}/docs/CONSTRAINTS.md, then the R2 port reports ${REPO}/docs/m2-r2-*.md and audits ${REPO}/docs/audits/m2-r2-*.md that touch your scope, ${REPO}/docs/formats/*.md, ${REPO}/docs/m2-r1-function-deltas.md + ${REPO}/ghidra/delta_map.json (tools/m2_stage_counts.py lists your scope's functions), ${REPO}/docs/m2-checksum-oracle.md + ${REPO}/docs/m2-r2-oracle.md, ${REPO}/docs/hw1-engine-gameplay.md.

PORTING CONVENTIONS (every src/ producer, as in M2-R2): change src/ in place, smallest faithful change; every Cataclysm-specific change carries a comment "CAT: <what> (<exe function> 0x00xxxxxx)"; IDENTICAL functions stay verbatim (but the delta map is an estimate: compare with the exe body before trusting it, and record any IDENTICAL you find modified as a review entry in ghidra/delta_map.json); new Cataclysm-only files go next to their HW1 siblings with the tree's REULA header style; struct layouts follow docs/formats/*.md and the CET headers, with static asserts on sizes (32-bit layout checks via tools/m2_layout32.sh where the R2 ports set them up). Retire the HW1 type aliases and dormant HW1-only ids in the files you port (summary: 1,387 alias lines outside ShipDefs.h). Keep tools/build.sh --debug and --release green with 0 warnings under -Werror after every subsystem. Never commit; never put game data, build outputs or logs from game data into git.

TEST HARNESS (exists since M2-R2): ${HARNESS}/ (plain: only what a real install has, no workarounds) and ${REPO}/tools/m2_harness.sh (read its usage: modes fe, sp, and the self-test modes statics/meshes/textures/oracle); runs land in ${HARNESS}/runs/<label>/. The isolation harness tools/m2_harness_iso.sh is for experiments only; every DONE WHEN is judged on the PLAIN harness. Never run the GOG game; never inject input; no gamescope.

SESSION RULE (the session can restart without warning): keep your report doc current after EVERY finished subsystem (a "<!-- done: a, b -->" marker at its top) and keep the tree building. RESUME RULE: if your report doc exists, read it and git diff in ${REPO}, keep what is done, finish only what is missing. HARD RULES: do NOT run Ghidra (only the Ghidra stage may); ground every claim in a file you read or a command you ran; your final text is a return value, not a message.
`

const PROD_SCHEMA = { type: 'object', properties: {
  report: { type: 'string' }, files_changed: { type: 'array', items: { type: 'string' } }, summary: { type: 'string' },
  done_when_met: { type: 'boolean' }, harness_milestone: { type: 'string' }, first_stop: { type: 'string' },
  counts: { type: 'object' }, open_questions: { type: 'array', items: { type: 'string' } } },
  required: ['report', 'files_changed', 'summary', 'done_when_met', 'harness_milestone', 'first_stop', 'open_questions'] }
const AUDIT_SCHEMA = { type: 'object', properties: {
  audit_path: { type: 'string' }, sampled: { type: 'number' }, wrong: { type: 'number' }, precision: { type: 'number' },
  fixed_in_place: { type: 'number' }, build_ok: { type: 'boolean' }, verdict: { type: 'string' }, commit: { type: 'string' },
  notes: { type: 'array', items: { type: 'string' } } },
  required: ['audit_path', 'sampled', 'wrong', 'precision', 'build_ok', 'verdict', 'notes'] }

// Order: close-out + systemic headers first (everything else compiles against them), then ETG/particles and
// selection (header-level conventions), then ship code A (the current sp stop), universe statics last (needs the ships).
const PORTS = [
  { key: 'closeout-headers', label: 'port:closeout-headers',
    scope: `Stage-1 close-out + systemic headers (M2-R3 producer 1). Close-out, docs/m2-r2-summary.md "What is left of stage 1" items 1-5: the 8-type Cataclysm derelict table (ObjTypes; replace the CAT-PROVISIONAL appends), the GoK quit defect (utyGrowthHeapFree munmap(heap, 0)), the save container and chunk sizes in SaveGame.c (container + chunk sizes only, 32-bit layouts; the full layout waits for the user's saves: leave a documented hook), the 13 + 1 delta-map review entries (record them in ghidra/delta_map.json and re-run tools/m2_stage_counts.py), the 1,705-row Cataclysm string-id enum (strNameToEnum 0x005288f0) so ShipTypeToNiceStr shows the right strings. Systemic headers: SOF_* renumbering (SOF_Crazy 0x200000, hidden 0x8000), MAX_MILITARY_SLOTS 63 + the parade slot enum, AlternativeShips 0xA8, ClampInfo 0x38, StaticCollInfo lengths, CustShipFire at +0x11C; Randy.c to 27 streams (particles on 25), the wall-clock advance in ranStartup / cat_ranResetGameStreams, realtimeelapsed (the oracle prerequisites in docs/m2-checksum-oracle.md).`,
    done: `a quitting harness run returns rc 0 (no utility.c (3484) assert); a static size assert exists for every struct this port changed; the oracle self-test still reports SELFTEST PASS with 27 RNG streams; the delta-map review entries are recorded and the stage counts re-run; fe still reaches Main_game_screen and sp still gets at least as far as before`,
    audit: `Check 30 header values / struct sizes / enum rows against CET's headers and the exe (${EXP} decompiled bodies, strings.csv; the string-id enum against strNameToEnum's table); confirm the RNG stream count and seeding against cat_ranResetGameStreams and ranStartup; re-run fe, sp and the oracle self-test on the plain harness; confirm the quit rc 0; confirm the delta_map.json review entries and that tools/m2_stage_counts.py reproduces the counts the producer reports.` },
  { key: 'etg-particles', label: 'port:etg-particles',
    scope: `ETG + particle argument arrays (producer 2): the one-pointer callable convention (59+ functions: Cataclysm's ETG callables take one pointer to an argument array instead of HW1's variadic/stack arguments), the per-race ETG scripts (etg\\race_*.script), ETG.c and Particle.c (171 / 164 / 2 functions, 21,936 MODIFIED bytes; cat-new 24 / 6,685 B).`,
    done: `every etg\\*.script of the install parses (a self-test mode that parses them all, kept out of the default startup path); ASan 0 in an effect self-test that creates and runs a sample of effects for a number of ticks; fe and sp unchanged or further`,
    audit: `Check the callable convention against 20 decompiled ETG callables in ${EXP} (argument array layout, which index is which), and 10 ETG/particle function bodies against the exe; re-run the ETG parse and effect self-tests under the debug ASan build; confirm every etg script of the install was parsed (count files vs log lines).` },
  { key: 'selection', label: 'port:selection',
    scope: `Selection (producer 3): ioUpdateShipTotals in the 30 selection filters, Select.c and ShipSelect.c (33 / 44 / 4 functions, 14,536 MODIFIED bytes; cat-new 59 / 11,278 B), against the new SOF_* flags from producer 1.`,
    done: `the filters compile against the new flags and match the exe's filter bodies (spot-checked in the report); no HW1 alias left in Select.c and ShipSelect.c; fe and sp unchanged or further`,
    audit: `Compare 20 of the ported filter/selection functions with their decompiled bodies in ${EXP} (flag tests, ioUpdateShipTotals placement, Cataclysm-only branches); confirm no HW1 alias or dormant id remains in both files; rebuild and re-run fe and sp.` },
  { key: 'ships-a', label: 'port:ships-a',
    scope: `Ship code A, the HW1 headers the exe reuses (producer 4): CustShipHeader slot wiring (slots 9, 12-14, 20, 22-34; the exe's header contents in ${SCRATCH}/m2r2-shipstatic-tools/headers.txt), the StaticInit and slot bodies of the HW1 ship headers Cataclysm reuses, GenericInterceptorStaticInit (0x005b1e70, 14 Cataclysm types) first because it is the current sp stop; Ships/* 80 / 45 / 12 functions (15,070 MODIFIED bytes) plus the part of the Ships/* cat-new that belongs to these headers. The 22 Cataclysm-only headers on DefaultShipHeader are M2-R4: keep them on DefaultShipHeader, but make sure nothing asserts on them.`,
    done: `the sp plain-harness run passes GenericInterceptor.c (205) and every later ship StaticInit for Mission01's ships; the statics self-test stays at 143/143 types and 170/173 .shp; fe unchanged`,
    audit: `Check the slot table (all 34 slots for 20 ship types) against headers.txt and the exe; compare 15 StaticInit/slot bodies with their decompiled versions in ${EXP}; re-run sp and the statics self-test on the plain harness and confirm the producer's furthest point.` },
  { key: 'universe-mex', label: 'port:universe-mex',
    scope: `Universe statics + MEX (producer 5): Universe.c (mine statics over 7 races, universeDefaultTeamColors, the derelict loader with the new 8-type table from producer 1), MEX.c (mexLoad with the salvage chunks, mexSetCollisionInfo, collrectmid), the DockStaticInfo 0x50 header and scriptSetDockStatics, Globals.c (38 / 31 / 0 functions, 15,684 MODIFIED bytes; cat-new 20 / 5,549 B).`,
    done: `Mission01 level load completes and sp reaches "universe running" (univUpdateCounter > 0) on the plain harness with ASan 0; if a later subsystem (stage-2 part B: UnivUpdate, command layer, combat, Cataclysm-only ships) stops it first, the report names the exact stop and its M2-R4 owner`,
    audit: `Check the mine statics, team colours, derelict table use, DockStaticInfo layout and the MEX chunk parsing against ${EXP} (30 checks); load every .mex file of the install in a self-test (or the harness) under ASan; re-run sp and confirm the milestone the producer reports.` },
]

function portPrompt(p) {
  return `${COMMON}
TASK (M2-R3 port, key ${p.key}): ${p.scope}
DONE WHEN: ${p.done}.
Report: ${REPO}/docs/m2-r3-${p.key}.md (what changed and why, file by file; each Cataclysm delta with its exe function and address; the build results; the plain-harness runs with milestone and first stop; delta-map review entries you added; what is left and its owner). Subsystems: split your scope into 3-6 subsystems and record them in the done marker.
Return via StructuredOutput: report, files_changed, summary, done_when_met, harness_milestone, first_stop, counts, open_questions.`
}

function auditPrompt(p, r) {
  return `${COMMON}
TASK: ADVERSARIAL AUDITOR for M2-R3 port "${p.key}" (producer result: ${JSON.stringify(r || {}).slice(0, 2500)}). RESUME RULE: if ${REPO}/docs/audits/m2-r3-${p.key}.md exists and ends with a "Verdict:" line, return its numbers immediately.
1. Build from clean in a separate dir: rm -rf ${SCRATCH}/build-audit-${p.key} && meson setup / compile the debug and release configurations there exactly as tools/build.sh does (read it), 0 warnings under -Werror; record sizes.
2. ${p.audit}
3. Read the full diff of this port (git diff in ${REPO}; earlier ports are already committed): every change outside the producer's scope, every edited function the delta map calls IDENTICAL without a review entry, every Cataclysm change without a "CAT:" comment is a finding.
4. Re-check the producer's DONE WHEN claim (${p.done}) yourself on the plain harness.
Fix factual errors you prove directly (code or doc; mark doc fixes "[audit: ...]"), keep the build green, and count the fixes. Write ${REPO}/docs/audits/m2-r3-${p.key}.md: what you checked, each finding with evidence, precision = correct/(correct+wrong) over the sampled checks, fixes, and a last line "Verdict: PASS|PASS-WITH-FIXES|FAIL - <reason>".
Then CHECKPOINT this port (even on FAIL, so the next port starts from a recorded state): ${CKPT} "M2-R3 ${p.key}: <verdict word> (<one-line summary>)" (it refuses forbidden paths; on an index.lock, wait 30 s and retry: the Ghidra stage may be committing). Return the commit hash in "commit".`
}

const NAMEFIX = `${COMMON}
TASK (Ghidra stage, you MAY run Ghidra; this is the only stage that does): apply the 3 audited name fixes in ${REPO}/ghidra/proposals/m2-r2-name-fixes.json (0x00404090 aieCheckEnemyNearby -> aieCheckHealthLow, 0x00404150 aieCheckEnemyNotNearby -> aieCheckHealthHigh, 0x004aeb10 -> meshObjectFixupPacoUV; with their signatures). RESUME FIRST: if ${MAIN}/ghidra/exports/cataclysm_gog_r6/decompiled_index.csv exists and its symbols.csv shows the 3 new names, skip to step 4.
1. Write ${REPO}/ghidra/symbols_m2r2_namefix.json in the cet_symbols.json schema of ${REPO}/ghidra/symbols_round5.json (same target block; 3 function entries with name, signature and a class=/source note), and check no other function already has those names (${EXP}/functions.csv).
2. Run ONE headless pass, detached and polled exactly as the binary-map rounds did (read ${REPO}/docs/binary-map.md Round 5 and ${REPO}/docs/ghidra-round5-prep.md for the command form and the gotchas: wait until no tools/ghidra_headless.sh process exists, but do not wait on pgrep -f matching your own shell; wait for the wrapper's rc line in the log): setsid nohup bash -c '${GHIDRA} --symbols ${REPO}/ghidra/symbols_m2r2_namefix.json --export-name cataclysm_gog_r6 ${EXE}' > ${MAIN}/ghidra/exports/r6_headless.log 2>&1 &  (decompile ON, never --reimport; if the wrapper's reuse mode would re-apply anything beyond these 3 symbols, say what and confirm it is idempotent). No analysis pass is needed; if one runs over mission code, the FixFunctionBodies snapshot guard rule from docs/ghidra-round4-prep.md applies.
3. Check: the 3 names and prototypes in the new export's symbols.csv/functions.csv, apply report 3/3, no other name changed (diff r5 vs r6 functions.csv names), decompile counts.
4. Update every tracked reference to the old names: ${REPO}/ghidra/delta_map.json (name fields; keep classes), docs that cite them (grep ${REPO}/docs), and add a short "M2-R3 name fixes" note to ${REPO}/docs/binary-map.md (the three renames, the export cataclysm_gog_r6 as the newest). Do NOT edit src/ (the ports own it).
5. Checkpoint: ${CKPT} "Ghidra: 3 audited name fixes (aieCheckHealthLow/High, meshObjectFixupPacoUV); export cataclysm_gog_r6" (on an index.lock, wait 30 s and retry: a port auditor may be committing).
Return via StructuredOutput: report (the binary-map note path), files_changed, summary, done_when_met (3/3 applied, no other name changed), harness_milestone "n/a", first_stop "n/a", counts {applied, failed}, open_questions.`

async function portChain() {
  const out = []
  for (const p of PORTS) {
    const r = await agent(portPrompt(p), { label: p.label, phase: 'Port', schema: PROD_SCHEMA, model: 'opus', effort: 'high' })
    const v = await agent(auditPrompt(p, r), { label: 'audit:' + p.key, phase: 'Port', schema: AUDIT_SCHEMA, model: 'opus' })
    out.push({ key: p.key, produced: r, audit: v })
    log(`${p.key}: done_when=${r && r.done_when_met} milestone=${r && r.harness_milestone} verdict=${v && v.verdict}`)
  }
  return out
}

async function nameFix() {
  const r = await agent(NAMEFIX, { label: 'ghidra:name-fixes', phase: 'Ghidra', schema: PROD_SCHEMA, model: 'opus' })
  return { key: 'ghidra-name-fixes', produced: r, audit: null }
}

phase('Port')
log('M2-R3: 5 src/ ports in sequence (closeout-headers, etg-particles, selection, ships-a, universe-mex) + the Ghidra name-fix pass in parallel')
const [ports, namefix] = await parallel([() => portChain(), () => nameFix()])
const all = [...(ports || []), namefix].filter(Boolean)

phase('Integrate')
const digest = JSON.stringify(all.map(d => ({ key: d.key, done: d.produced && d.produced.done_when_met, milestone: d.produced && d.produced.harness_milestone, first_stop: d.produced && d.produced.first_stop, counts: d.produced && d.produced.counts, questions: d.produced && d.produced.open_questions, audit: d.audit && { precision: d.audit.precision, verdict: d.audit.verdict, commit: d.audit.commit } }))).slice(0, 20000)
const integrated = await agent(`${COMMON}
TASK: INTEGRATE M2-R3. Digest: ${digest}
1. R3 EXIT TEST (docs/m2-r2-summary.md): from a clean build, on the PLAIN harness: sp reaches "universe running" (univUpdateCounter > 0) on Mission01 with ASan 0; fe still reaches Main_game_screen; the self-tests (statics, meshes, textures, the new ETG/effect ones, oracle) unchanged or better; the release oracle self-test reports SELFTEST PASS; a quitting run returns rc 0. Run them yourself (tools/m2_harness.sh) and report milestone, first stop and any ASan/UBSan report. Also tools/build.sh --release, 0 warnings.
2. Write ${REPO}/docs/m2-r3-summary.md: per port (what landed, done-when met or not, audit precision/verdict, commit), the Ghidra name fixes, the exit-test result, what is left of stage 2 part A (if anything) and the plan for M2-R4 (docs/m2-r2-summary.md's M2-R4 table, adjusted by what R3 found; the remaining part-A items first if any; stage-2 exit: Mission01 runs a few thousand ticks headless with ASan 0 and two runs give byte-identical .hwo streams). Use tools/m2_stage_counts.py for counts.
3. Update ${REPO}/cycle/STATE.md "## M2": an "M2-R3" subsection (COMPLETE with date and numbers, links), the M2-R4 plan in one table, and the M2 decisions: record the 2026-10-10 answers (faithfulness defaults accepted; Ghidra name fixes run; the recording and two saves come later, not blocking), keep open only what is still open (the oracle reference recording, the two saves, the M0 sign-off, the first live oracle session), and add anything this round surfaced. Keep the rest of the file; the orchestrator updates the workflow table row.
4. git status: nothing from original/, reference/, build/ or game data staged or untracked outside .gitignore. Then: ${CKPT} "M2-R3: stage 2 part A (close-out + systemic headers, ETG/particles, selection, ship code A, universe statics + MEX); Ghidra name fixes"
Return via StructuredOutput.`,
  { label: 'integrate:m2-r3', phase: 'Integrate', model: 'opus', effort: 'high',
    schema: { type: 'object', properties: {
      commit: { type: 'string' }, r3_exit_met: { type: 'boolean' }, milestone: { type: 'string' }, first_stop: { type: 'string' },
      build_ok: { type: 'boolean' }, m2r4_plan: { type: 'array', items: { type: 'string' } },
      user_decisions: { type: 'array', items: { type: 'string' } }, summary: { type: 'string' } },
      required: ['commit', 'r3_exit_met', 'milestone', 'first_stop', 'build_ok', 'm2r4_plan', 'user_decisions', 'summary'] } })

return { items: all.map(d => ({ key: d.key, done: d.produced && d.produced.done_when_met, milestone: d.produced && d.produced.harness_milestone, verdict: d.audit && d.audit.verdict, commit: d.audit && d.audit.commit })), integrated }
