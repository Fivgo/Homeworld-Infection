export const meta = {
  name: 'homeworld-infection-m2-r2',
  description: 'M2-R2, stage 1 of the port (formats + data load): Cataclysm identifiers and tweak tables, ship statics, the 0x500 mesh, startup names/audio/FE art/textures, and the recreation side of the checksum oracle, each ported into src/ and audited in sequence; alongside, the re-read of the 1,013 low-confidence IDENTICAL functions; then the stage-1 exit test and STATE.md',
  phases: [
    { title: 'Port', detail: 'five src/ producers in sequence (they share src/ and the build), each followed by its auditor', model: 'opus' },
    { title: 'Re-read', detail: 'delta-map follow-up (decision 4), in parallel with Port; analysis only', model: 'opus' },
    { title: 'Integrate', detail: 'stage-1 exit test, summary, STATE.md, checkpoint', model: 'opus' },
  ],
}

// Same two-root layout as M2-R1. REPO is the git worktree this round writes and commits in (branch m2).
// MAIN holds what git never tracks: Ghidra exports, original/, reference/.
const MAIN = '/home/evanmedinger/Documents/GameDev/Homeworld-Infection'
const REPO = MAIN + '/.claude/worktrees/m2'
const REF = MAIN + '/reference'
const EXP = MAIN + '/ghidra/exports/cataclysm_gog_r5'
const SCRATCH = MAIN + '/original/scratch-m2'
const HARNESS = SCRATCH + '/m2r2-harness'
const CKPT = `cd ${REPO} && CHECKPOINT_COAUTHOR="Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>" tools/checkpoint.sh`

const COMMON = `
PROJECT: Homeworld-Infection: a 1-to-1 recreation of Homeworld: Cataclysm (GOG "Homeworld: Emergence", retail 1.01) for Linux and Windows 11. M2 (reimplementation) follows Option A: the engine is Homeworld 1 (src/ = Gardens of Kadesh 3af31fb, accepted by the user) and we port Cataclysm's differences into it. M2-R1 measured the deltas; this round, M2-R2, ports STAGE 1: file formats + data load (docs/m2-r1-summary.md "M2-R2 plan").
USER DECISIONS (2026-10-10, binding): (1) src/ base = GoK 3af31fb. (2) Cataclysm behaviour wins, decided per subsystem as it is ported; 16 Hz universe is the default (GoK's 64 Hz only behind an option); floating-point/optimisation flags are pinned (-O2 -fno-strict-aliasing, no fast-math) before any checksum comparison. (3) Relic's ad1bb82 Game/ and Ships/ are in-tree at src/reference/hw1-ad1bb82/ (reference only, not built). (4) The 1,013 low-confidence IDENTICAL functions get re-read, and the R1 audit's reclassifications are applied. (5) The oracle may call the game's own record/playback functions (no OS input is synthesised). (6) DLL toolchain: clang-18 + lld-link first, mingw-w64 as fallback (installing packages needs the user: if mingw is required, say so). (7) LAN interop with stock 1.01 is nice-to-have, not required: do not spend effort on it, but do not break the existing wire format gratuitously.
PATHS: the working copy is the git worktree ${REPO} (branch m2). Read docs and write EVERY tracked file there; never write into ${MAIN} directly (blocked). Untracked inputs in ${MAIN}: the named Ghidra export ${EXP}/ (functions.csv, symbols.csv, strings.csv, call_graph.csv, decompiled.c, datatypes.h), the game ${MAIN}/original/ (game/, game_extracted/cataclysm/), the reference clones ${REF} (read-only; CET headers in ${REF}/CataclysmEngineTweaks). Scratch for anything derived from game data: ${SCRATCH}/ (gitignored).
READ FIRST: ${REPO}/cycle/STATE.md ("## M2"), ${REPO}/docs/m2-r1-summary.md, ${REPO}/docs/CONSTRAINTS.md, then what your task names: ${REPO}/docs/m2-r1-data-deltas.md (the 17 stops, ranked, and the run setup), ${REPO}/docs/formats/*.md, ${REPO}/docs/m2-r1-function-deltas.md + ${REPO}/ghidra/delta_map.json, ${REPO}/docs/m2-r1-bootstrap.md (build), ${REPO}/docs/m2-checksum-oracle.md, ${REPO}/docs/hw1-port-lessons.md, ${REPO}/docs/binary-map.md (Round 5).

PORTING CONVENTIONS (every src/ producer): change src/ in place, smallest faithful change; every Cataclysm-specific change carries a comment "CAT: <what> (<exe function> 0x00xxxxxx)" so the delta stays traceable; IDENTICAL functions stay verbatim; new Cataclysm-only files go next to their HW1 siblings with the REULA header style of the tree; struct layouts follow docs/formats/*.md and the CET headers, with static asserts on sizes where the on-disk/in-exe size matters. Keep tools/build.sh --debug and --release green with 0 warnings under -Werror after every subsystem (fix what you break). Never commit; never put game data, build outputs or logs from game data into git.

TEST HARNESS (shared by all src/ producers): ${HARNESS}/ is a scratch HW_Data setup holding ONLY what a real Cataclysm install has, under the names the game uses (symlinks into ${MAIN}/original/game/ and game_extracted/; none of data-deltas' 15 workaround stages, no mesh down-conversion, no renamed copies), and ${REPO}/tools/m2_harness.sh runs build/debug/hwi-engine headless (SDL_VIDEODRIVER=offscreen, then dummy) with a timeout, no input, a scratch HOME/settings dir under ${HARNESS}/, and copies the log, the file-load trace and any ASan/UBSan reports to ${HARNESS}/runs/<label>/; it prints the furthest milestone reached (e.g. Main_game_screen, level load) and the first stop. The first producer creates both if missing (read how data-deltas ran in docs/m2-r1-data-deltas.md and ${SCRATCH}/runs/data-deltas/runs.txt); later producers reuse and may extend them. Never run the GOG game; never inject input; no gamescope.

SESSION RULE (the session can restart without warning): keep your report doc current after EVERY finished subsystem (a "<!-- done: a, b -->" marker at its top) and keep the tree building. RESUME RULE: if your report doc exists, read it and git diff in ${REPO}, keep what is done, finish only what is missing. HARD RULES: do NOT run Ghidra; ground every claim in a file you read or a command you ran; your final text is a return value, not a message.
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

// Order matters: identifiers first (the other loaders key on the enums), oracle last (it needs a loading engine to be useful).
const PORTS = [
  { key: 'identifiers', label: 'port:identifiers',
    scope: `Cataclysm identifiers and tweak tables (M2-R2 plan producer 2): the 145-type / 7-race / 12-class enums from CET's ShipDefs.h / RaceDefs.h / ClassDefs.h (verify against the exe's string tables in ${EXP}), StrToShipType and the race/class string tables, every TOTAL_NUM_SHIPS-sized table; the script tables aiship, tweak, crates, Tactical, the 12 per-ship parade files, the dropped Dock.script; .mission Include/ExcludeShips; colPick's new format; GatherStatsFor. Also create the TEST HARNESS (see COMMON) first.`,
    done: `the data-deltas stops ranked 2, 3, 5 and 7 are gone and both ASan out-of-bounds writes (StrToShipType = -1 in tweak.script and crates.script) are gone in the harness; level load gets past the RACE_* stop (run27 in data-deltas) or the doc shows exactly which other stage-1 item still blocks it`,
    audit: `Check 30 enum values / string-table entries / table sizes against CET's headers and the exe's own tables in ${EXP} (decompiled StrToShipType and friends, strings.csv). Confirm every array indexed by ship type/race/class is sized by the new enums (grep for the old HW1 constants left behind). Re-run tools/m2_harness.sh and confirm the stops the producer claims are gone, and that no ASan report remains for these tables.` },
  { key: 'shipstatics', label: 'port:shipstatics',
    scope: `Ship statics (producer 3): ShipStaticInfo 0x458, GunStatic 0x11C with Cataclysm's gimbal types, CustShipHeader 0x8C with 34 slots (layout only: the slot bodies are stage 2, so point new slots at safe defaults and say so), the 29 new .shp keys and the 8 dropped ones, Cataclysm's per-race .shp paths. Spec: docs/formats/shipstatic.md; probe: tools/formats/shipstatic_probe.py.`,
    done: `170 of 173 .shp files parse as the probe does (the 3 light-count cases handled like the exe), with a log line per ship proving it; no ship type is silently disabled; static asserts pin the three struct sizes`,
    audit: `Check 30 struct offsets / key handlers against docs/formats/shipstatic.md and the decompiled parser in ${EXP}; run the probe and the engine's parse (via the harness log) on 10 ships of different races and compare field values; confirm the 3 special cases match the exe's behaviour.` },
  { key: 'mesh', label: 'port:mesh',
    scope: `Mesh (producer 1): Cataclysm meshLoad with polygonobject 0x74, the per-vertex 16-byte array at +0x1C wired to the glow/colour path, the .lod naming of .peo, meshObjectFixupUV, MeshAnim. Spec: docs/formats/mesh.md; probe: tools/formats/mesh_probe.py.`,
    done: `all 1,729 .geo/.peo files load in the engine with no ASan report (a small loader test mode or a harness pass that loads every mesh is fine; keep it out of the default startup path), the 21 engine-glow meshes load at startup, and the harness gets past the mesh stop without the down-conversion`,
    audit: `Check the loader's struct layout and every pointer fixup against docs/formats/mesh.md and the decompiled meshLoad in ${EXP} (30 checks); re-run the all-meshes load under the debug (ASan) build and confirm 0 reports; compare vertex/polygon counts for 10 meshes with the probe.` },
  { key: 'startup', label: 'port:startup',
    scope: `Startup names, audio, FE art, textures (producer 4): bigFilePrecedence (Cataclysm.big, the Update.big case), Music4Cat.wxd, SectSpeech.vce / BeastSpeech.vce and Cataclysm's speech LUT reader, the battle-chatter event table, renamed construction/research art, FE callbacks the .fib screens reference; texreg 0x34 entry, trTextureRegister's 4th argument, LL_MaxElements above 12,403. Specs: docs/formats/textures.md, fib.md, docs/m2-r1-data-deltas.md.`,
    done: `the data-deltas stops ranked 4, 6, 8, 9 and 10 are cleared without /noSpeech, textures.ll is fully registered, and the harness reaches Main_game_screen on the unmodified data (if it does not, the doc names the exact remaining stop and its owner)`,
    audit: `Check the texreg layout, the speech LUT format and bigFilePrecedence against the decompiled code in ${EXP} (30 checks); re-run the harness without /noSpeech and confirm the milestone and that no workaround from data-deltas is in the harness dir.` },
  { key: 'oracle', label: 'port:oracle',
    scope: `Checksum oracle, recreation side, and the determinism settings (producer 5; docs/m2-checksum-oracle.md sections 5-6 and the audit fixes marked in it): a -Doracle meson option; the .hwo stream writer at the univUpdate / netcheck points; the comparison tool tools/oracle/hwo_compare.py (first divergent tick, which sub-checksum); the 16 Hz universe as the default (GoK's 64 Hz only behind an option; document it); the pinned flags -O2 -fno-strict-aliasing, no fast-math, in the release build used for oracle runs. Then the GOG-side DLL SKELETON (decision 6): tools/oracle/dll/ with the hook-site table H1-H7 from the design (addresses and expected prologue bytes, with a startup check that refuses to hook if the bytes differ), the .hwo writer, and the calls into the game's own record/playback functions (decision 5), built with clang-18 + lld-link for i686-pc-windows-msvc (or mingw-w64 if clang cannot link it; if mingw is not installed, stop and record it as a user decision). Build it; never load it into the game (the user runs the first live oracle session).`,
    done: `two hwi-engine runs of the same HW1 recorded game (or, if no recording is available, the same deterministic scripted start the design names as the self-test) give byte-identical .hwo streams; the comparison tool reports identical, and reports the right tick on a deliberately perturbed copy; the DLL builds (or the blocker is recorded)`,
    audit: `Check the writer's hook points and record format against docs/m2-checksum-oracle.md; re-run the self-test twice and compare; perturb one record and confirm the tool finds it; check the DLL's hook-site table (addresses, prologue bytes) against ${EXP} and the design (all 7), and that nothing in it injects input. Confirm 16 Hz is the default and the release flags are as decided.` },
]

function portPrompt(p) {
  return `${COMMON}
TASK (M2-R2 port, key ${p.key}): ${p.scope}
DONE WHEN: ${p.done}.
Report: ${REPO}/docs/m2-r2-${p.key}.md (what changed and why, file by file; each Cataclysm delta with its exe function and address; the build results; the harness runs with milestone and first stop; what is left for later stages). Subsystems: split your scope into 3-6 subsystems and record them in the done marker.
Return via StructuredOutput: report, files_changed, summary, done_when_met, harness_milestone, first_stop, counts, open_questions.`
}

function auditPrompt(p, r) {
  return `${COMMON}
TASK: ADVERSARIAL AUDITOR for M2-R2 port "${p.key}" (producer result: ${JSON.stringify(r || {}).slice(0, 2500)}). RESUME RULE: if ${REPO}/docs/audits/m2-r2-${p.key}.md exists and ends with a "Verdict:" line, return its numbers immediately.
1. Build from clean in a separate dir: rm -rf ${SCRATCH}/build-audit-${p.key} && meson setup / compile the debug and release configurations there exactly as tools/build.sh does (read it), 0 warnings under -Werror; record sizes.
2. ${p.audit}
3. Read the full diff of this port (git diff in ${REPO}; the previous ports are already committed): every change outside the producer's scope, every HW1 IDENTICAL function that was edited, every Cataclysm change without a "CAT:" comment is a finding.
4. Re-check the producer's DONE WHEN claim (${p.done}) yourself.
Fix factual errors you prove directly (code or doc; mark doc fixes "[audit: ...]"), keep the build green, and count the fixes. Write ${REPO}/docs/audits/m2-r2-${p.key}.md: what you checked, each finding with evidence, precision = correct/(correct+wrong) over the sampled checks, fixes, and a last line "Verdict: PASS|PASS-WITH-FIXES|FAIL - <reason>".
Then CHECKPOINT this port (even on FAIL, so the next port starts from a recorded state): ${CKPT} "M2-R2 ${p.key}: <verdict word> (<one-line summary>)" (it refuses forbidden paths; if git reports an index.lock, wait 30 s and retry, the re-read stage may be committing). Return the commit hash in "commit".`
}

const REREAD = `${COMMON}
TASK (M2-R2 re-read, user decision 4; analysis only, do NOT edit src/): finish the function delta map.
1. Read, by hand, both bodies (Cataclysm decompiled C in ${EXP}/decompiled.c; HW1 source in ${REPO}/src/reference/hw1-ad1bb82/ and the GoK version in ${REPO}/src/) of every IDENTICAL function with confidence low in ${REPO}/ghidra/delta_map.json (1,013; 51 were read in R1) and set class, change line, confidence and reviewed=true. Work by HW1 file, write the JSON after each file group (done_subsystems), and use ${REPO}/tools/delta_map.py to extract bodies.
2. Apply the R1 audit's reclassifications (docs/audits/m2-r1-function-deltas.md): the kasfTimer* family -> MODIFIED (hash-keyed timers), the 10 SpaceObjRegistryGetShip callers -> IDENTICAL, and the linker-folded-name, callee-arity and deletion-counted-as-REWRITTEN cases it lists; re-check the two suspect binary-map names (aieCheckEnemyNearby 0x00404090, RemoveShipFromFormation 0x00447950) and, if wrong, record the correct name as a proposal in ${REPO}/ghidra/proposals/m2-r2-name-fixes.json (binary-map proposal schema; no Ghidra run).
3. Fix tools/delta_map.py where the re-read shows a systematic misclassification (linker-folded names, hashed keys, arity), re-run it, and make sure hand reviews survive a re-run (reviewed entries keep their class).
4. Recompute the per-file table in ${REPO}/docs/m2-r1-function-deltas.md and the per-stage counts (the "Delta counts per M2-R2 stage" table of ${REPO}/docs/m2-r1-summary.md) with a committed script ${REPO}/tools/m2_stage_counts.py (R1 used /tmp/hwi-integrate/stages.py, which may be gone: rebuild its method from the summary's description). Add a section "M2-R2 re-read" to docs/m2-r1-function-deltas.md: how many changed class, the new totals, the new IDENTICAL agreement estimate.
Write ${REPO}/docs/m2-r2-reread.md (done marker; numbers). Return via StructuredOutput: report, files_changed, summary, done_when_met (all 1,013 read), harness_milestone "n/a", first_stop "n/a", counts {read, changed_class, identical, modified, rewritten}, open_questions.`

const REREAD_AUDIT = (r) => `${COMMON}
TASK: ADVERSARIAL AUDITOR for the M2-R2 delta-map re-read (producer result: ${JSON.stringify(r || {}).slice(0, 2000)}). RESUME RULE: if ${REPO}/docs/audits/m2-r2-reread.md exists and ends with a "Verdict:" line, return its numbers immediately.
Sample 40 of the re-read functions (python random.seed(29); at least 15 whose class changed, 15 still IDENTICAL): read both bodies yourself and give your own class; precision = agreement. Check the reclassifications from the R1 audit were applied, that tools/m2_stage_counts.py reproduces both tables exactly, and that a re-run of tools/delta_map.py keeps hand-reviewed classes. Fix proven errors (mark doc fixes "[audit: ...]"). Write ${REPO}/docs/audits/m2-r2-reread.md ending "Verdict: PASS|PASS-WITH-FIXES|FAIL - <reason>". Then checkpoint: ${CKPT} "M2-R2 re-read: <verdict word> (<one-line summary>)" (on an index.lock, wait 30 s and retry). build_ok = true if you did not touch src/. Return the commit hash in "commit".`

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

async function rereadChain() {
  const r = await agent(REREAD, { label: 'reread:delta-map', phase: 'Re-read', schema: PROD_SCHEMA, model: 'opus' })
  const v = await agent(REREAD_AUDIT(r), { label: 'audit:reread', phase: 'Re-read', schema: AUDIT_SCHEMA, model: 'opus' })
  return { key: 'reread', produced: r, audit: v }
}

phase('Port')
log('M2-R2: 5 src/ ports in sequence (identifiers, shipstatics, mesh, startup, oracle) + the delta-map re-read in parallel')
const [ports, reread] = await parallel([() => portChain(), () => rereadChain()])
const all = [...(ports || []), reread].filter(Boolean)

phase('Integrate')
const digest = JSON.stringify(all.map(d => ({ key: d.key, done: d.produced && d.produced.done_when_met, milestone: d.produced && d.produced.harness_milestone, first_stop: d.produced && d.produced.first_stop, counts: d.produced && d.produced.counts, questions: d.produced && d.produced.open_questions, audit: d.audit && { precision: d.audit.precision, verdict: d.audit.verdict, commit: d.audit.commit } }))).slice(0, 20000)
const integrated = await agent(`${COMMON}
TASK: INTEGRATE M2-R2. Digest: ${digest}
1. STAGE-1 EXIT TEST (docs/m2-r1-summary.md): from a clean build, build/debug/hwi-engine (ASan) reaches Main_game_screen on the unmodified Cataclysm data in the harness (${HARNESS}, none of data-deltas' workarounds, no mesh down-conversion) and gets past the RACE_* level-load stop. Run tools/m2_harness.sh yourself and report the milestone, the first stop and any ASan/UBSan report. Also tools/build.sh --release, 0 warnings.
2. Write ${REPO}/docs/m2-r2-summary.md: per port (what landed, done-when met or not, audit precision/verdict, commit), the re-read's new totals, the exit-test result, what is left of stage 1 (if anything) and the plan for M2-R3: if stage 1 is not finished, the remaining stage-1 items first; then stage 2 "universe and ships" split over two rounds of about 10 agents (docs/m2-r1-summary.md), starting with the systemic header changes (ETG/particle argument arrays, ioUpdateShipTotals, SOF_*/enum renumbering) and the CustShipHeader slot bodies left by shipstatics; use the re-read's per-stage counts.
3. Update ${REPO}/cycle/STATE.md "## M2": an "M2-R2" subsection (COMPLETE with date and numbers, links), the M2-R3 plan in one table, and under the M2 decisions: mark decisions 1-5, 7 and 9 of the R1 list as answered (the user's 2026-10-10 answers are in COMMON above), keep 6 (first reference recording: which skirmish map/players) and 8 (two saves from the M0 play-test) open, and add anything this round surfaced (e.g. a mingw install, the first live oracle session the user runs). Keep the rest of the file; the orchestrator updates the workflow table row.
4. git status: nothing from original/, reference/, build/ or game data staged or untracked outside .gitignore. Then: ${CKPT} "M2-R2: stage 1 formats + data load (identifiers, ship statics, mesh 0x500, startup/audio/textures, oracle recreation side), delta-map re-read"
Return via StructuredOutput.`,
  { label: 'integrate:m2-r2', phase: 'Integrate', model: 'opus', effort: 'high',
    schema: { type: 'object', properties: {
      commit: { type: 'string' }, stage1_exit_met: { type: 'boolean' }, milestone: { type: 'string' }, first_stop: { type: 'string' },
      build_ok: { type: 'boolean' }, identical: { type: 'number' }, modified: { type: 'number' }, rewritten: { type: 'number' },
      m2r3_plan: { type: 'array', items: { type: 'string' } }, user_decisions: { type: 'array', items: { type: 'string' } }, summary: { type: 'string' } },
      required: ['commit', 'stage1_exit_met', 'milestone', 'first_stop', 'build_ok', 'm2r3_plan', 'user_decisions', 'summary'] } })

return { items: all.map(d => ({ key: d.key, done: d.produced && d.produced.done_when_met, milestone: d.produced && d.produced.harness_milestone, verdict: d.audit && d.audit.verdict, commit: d.audit && d.audit.commit })), integrated }
