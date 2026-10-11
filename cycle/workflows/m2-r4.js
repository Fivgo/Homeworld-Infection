export const meta = {
  name: 'homeworld-infection-m2-r4',
  description: 'M2-R4, stage 2 part B of the port: universe update + mesh state, command layer + formations, combat/docking/salvage, the Cataclysm-only ships, and the rest + Player layout + oracle stage 2, each ported into src/ and audited in sequence; then the stage-2 exit test (Mission01 runs headless with ASan 0 and two runs give byte-identical .hwo streams) and STATE.md',
  phases: [
    { title: 'Port', detail: 'five src/ producers in sequence (they share src/ and the build), each followed by its auditor', model: 'opus' },
    { title: 'Integrate', detail: 'stage-2 exit test, summary, STATE.md, checkpoint', model: 'opus' },
  ],
}

// Same two-root layout as M2-R1..R3. REPO is the git worktree this round writes and commits in (branch m2).
// MAIN holds what git never tracks: Ghidra project and exports, original/, reference/.
const MAIN = '/home/evanmedinger/Documents/GameDev/Homeworld-Infection'
const REPO = MAIN + '/.claude/worktrees/m2'
const REF = MAIN + '/reference'
const EXP = MAIN + '/ghidra/exports/cataclysm_gog_r6'
const SCRATCH = MAIN + '/original/scratch-m2'
const HARNESS = SCRATCH + '/m2r2-harness'
const CKPT = `cd ${REPO} && CHECKPOINT_COAUTHOR="Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>" tools/checkpoint.sh`
const PLAN = `${REPO}/docs/m2-r3-summary.md "Plan for M2-R4"`

const COMMON = `
PROJECT: Homeworld-Infection: a 1-to-1 recreation of Homeworld: Cataclysm (GOG "Homeworld: Emergence", retail 1.01) for Linux and Windows 11. M2 (reimplementation) follows Option A: the engine is Homeworld 1 (src/ = Gardens of Kadesh 3af31fb) and we port Cataclysm's differences into it. M2-R1 measured the deltas; M2-R2 ported stage 1 (formats + data load; exit test met: the debug ASan engine reaches Main_game_screen on unmodified data, and single player passes the RACE_* stop, stopping at the GenericInterceptor.c (205) assert). M2-R3 ported stage 2 part A (exit test met: sp on Mission01 reaches the running universe, 1,248 ticks, ASan 0, with the provisional CAT_HW1_MISSION_SCRIPTS 0; every quit returns rc 0). This round, M2-R4, is stage 2 part B per ${REPO}/docs/m2-r3-summary.md "Plan for M2-R4" (your producer's row) and "What is left of stage 2 part A" (the part-A items folded into your scope).
USER DECISIONS (binding): src/ base = GoK 3af31fb; Cataclysm behaviour wins per subsystem; 16 Hz universe default; FP/optimisation flags pinned (-O2 -fno-strict-aliasing, no fast-math) before any checksum comparison; Relic ad1bb82 Game/ and Ships/ at src/reference/hw1-ad1bb82/ (reference only); the oracle may call the game's own record/playback functions; DLL toolchain clang-18 + lld-link; LAN interop with stock 1.01 is nice-to-have, not required (do not break the wire format gratuitously). 2026-10-10: the faithfulness defaults already in the tree are ACCEPTED (the 22 dropped HW1 types dormant as ids 200-221; k/tCloakGenerator on DefaultShipHeader where the exe has NULL; crateInit resets 46 entries; the mission-17 texture cut follows the stock exe, not CET; the fq audio code follows the binary). The user will supply the oracle reference recording and two saves LATER: do not block on them; build what does not need them and leave a clear hook where they plug in.
PATHS: the working copy is the git worktree ${REPO} (branch m2). Read docs and write EVERY tracked file there; never write into ${MAIN} directly (blocked). Untracked inputs in ${MAIN}: the named Ghidra export ${EXP}/ (cataclysm_gog_r6, the newest: r5 plus 3 audited renames; functions.csv, symbols.csv, strings.csv, call_graph.csv, decompiled.c, datatypes.h), the game ${MAIN}/original/ (game/, game_extracted/cataclysm/), the reference clones ${REF} (read-only; CET headers in ${REF}/CataclysmEngineTweaks). Scratch for anything derived from game data: ${SCRATCH}/ (gitignored); R2's tool outputs are there too (e.g. ${SCRATCH}/m2r2-shipstatic-tools/headers.txt).
READ FIRST: ${REPO}/cycle/STATE.md ("## M2"), ${REPO}/docs/m2-r3-summary.md (your row in the M2-R4 table, "What is left of stage 2 part A", the harness facts under the exit test), ${REPO}/docs/m2-r2-summary.md, ${REPO}/docs/CONSTRAINTS.md, then the R2/R3 port reports ${REPO}/docs/m2-r2-*.md, ${REPO}/docs/m2-r3-*.md and their audits ${REPO}/docs/audits/ that touch your scope, ${REPO}/docs/formats/*.md, ${REPO}/docs/m2-r1-function-deltas.md + ${REPO}/ghidra/delta_map.json (tools/m2_stage_counts.py lists your scope's functions), ${REPO}/docs/m2-checksum-oracle.md + ${REPO}/docs/m2-r2-oracle.md, ${REPO}/docs/hw1-engine-gameplay.md.

PORTING CONVENTIONS (every src/ producer, as in M2-R2): change src/ in place, smallest faithful change; every Cataclysm-specific change carries a comment "CAT: <what> (<exe function> 0x00xxxxxx)"; IDENTICAL functions stay verbatim (but the delta map is an estimate: compare with the exe body before trusting it, and record any IDENTICAL you find modified as a review entry in ghidra/delta_map.json); new Cataclysm-only files go next to their HW1 siblings with the tree's REULA header style; struct layouts follow docs/formats/*.md and the CET headers, with static asserts on sizes (32-bit layout checks via tools/m2_layout32.sh where the R2 ports set them up). Retire the HW1 type aliases and dormant HW1-only ids in the files you port (summary: 1,387 alias lines outside ShipDefs.h). Keep tools/build.sh --debug and --release green with 0 warnings under -Werror after every subsystem. Never commit; never put game data, build outputs or logs from game data into git.

TEST HARNESS (exists since M2-R2): ${HARNESS}/ (plain: only what a real install has, no workarounds) and ${REPO}/tools/m2_harness.sh (read its usage: modes fe, sp, and the self-test modes statics/meshes/textures/oracle); runs land in ${HARNESS}/runs/<label>/. The isolation harness tools/m2_harness_iso.sh is for experiments only; every DONE WHEN is judged on the PLAIN harness. Harness facts from R3: sp needs --timeout of about 300 s or more, because the intro animatic plays in real time (with 60 s the run dies inside aviPlay, rc 137); after about 816 ticks GoK's /debugScreenshots hook calls singlePlayerNextLevel() every frame (Cataclysm's next-level flow is stage 6), so judge sp milestones by univUpdateCounter and ASan, not by that loop. Never run the GOG game; never inject input; no gamescope.

SESSION RULE (the session can restart without warning): keep your report doc current after EVERY finished subsystem (a "<!-- done: a, b -->" marker at its top) and keep the tree building. RESUME RULE: if your report doc exists, read it and git diff in ${REPO}, keep what is done, finish only what is missing. HARD RULES: do NOT run Ghidra (no Ghidra stage this round); ground every claim in a file you read or a command you ran; your final text is a return value, not a message.
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

// Order follows docs/m2-r3-summary.md: universe update first (everything runs through it), the flag renumbering at the
// start of #2 (systemic), combat/docking/salvage before the Cataclysm-only ships that use it, the rest + oracle last.
const PORTS = [
  { key: 'univupdate', label: 'port:univupdate',
    scope: `Universe update + mesh state (M2-R4 producer 1, row 1 of ${PLAN}): UnivUpdate.c (52 / 46 / 0, 30,978 MODIFIED B; cat-new 51 / 15,092 B); the per-vertex glow runtime (cat_univShipMeshStateCreate, cat_shGlowUpdate, the bullet-hit glow); the effect's +0x80 mesh state and +0x84 block and the per-ship mesh-state helpers cat_shipNozzleInfoGet, cat_meshObjectMatrixGet/Concat (part-A item 3); univDeleteDeadShip with MiningBaseDied's list removals and the mothership death-sequence trigger.`,
    done: `sp on Mission01 runs at least as far as R3 (universe running, univUpdateCounter at least 1,248 within the same timeout) with ASan 0 on the new UnivUpdate.c; the etg effect self-test still passes; every IDENTICAL body of UnivUpdate.c was compared with the exe and the differences recorded as delta-map reviews`,
    audit: `Compare 25 UnivUpdate.c / glow / mesh-state bodies with their decompiled versions in ${EXP} (update order per tick, the death and deletion paths, the +0x80/+0x84 layouts); re-run sp (--timeout 340) and the etg self-test on the plain harness; confirm univUpdateCounter and ASan 0.` },
  { key: 'command', label: 'port:command',
    scope: `Command layer + formations (producer 2, row 2 of ${PLAN}). FIRST the specialFlags / specialFlags2 renumbering to the exe's bits (part-A item 1: FriendlyStatus 0x40000, Hyperspacing 0x200000, CircularGuard into specialFlags 0x40000000, specialFlags2 0x8 and 0x2000, SPECIAL_CatSalvager, the kamikaze bits 0x6), with static checks, updating every reader. Then CommandLayer.c, CommandWrap.c, Formation.c (87 / 65 / 1, 51,539 MODIFIED B; cat-new 41 / 11,084 B); the Cataclysm command orders 11-17 and formation types 9/10 (move them out of ShipSelect.c's CAT_COMMAND_* / CAT_FORMATION_* stand-ins); SPECIAL_2_CatBeingBuilt (external build); the HW1 functions with no exe counterpart that CommandWrap still calls.`,
    done: `every specialFlags/specialFlags2 bit sits at the exe's value with a static check and no reader uses an old value; sp runs as far as before with ASan 0; fe unchanged`,
    audit: `Check every renumbered flag against the exe (decompiled tests and sets in ${EXP}, at least 20 sites) and grep for readers still using HW1 values; compare 20 CommandLayer/CommandWrap/Formation bodies with the exe; re-run sp (--timeout 340) and fe.` },
  { key: 'combat', label: 'port:combat',
    scope: `Combat, docking, salvage (producer 3, row 3 of ${PLAN}): Collision.c, Physics.c, Clamp.c, Gun.c (damage by WEAPON_* / GUNSIZE_* and the gun weapon-type word), Damage.c, Attack.c, Dock.c (Cataclysm dock types, the sCarrier doors), LaunchMgr.c (module slots, Beast systems off), ResCollect.c (asteroid break pieces) (48 / 56 / 5, 34,228 MODIFIED B; cat-new 42 / 20,039 B). The salvage runtime (Salvage.c's provisional entries, cat_salvageTargetAllowed, cat_salvageResetGlobals) and the deletion of SalCapCorvette.c's dormant HW1 salvage code and its readers; the Beast infection weapon runtime (BeastWeapon.c's provisional parts); kamikaze steering/hit; cat_blastCreate / cat_blastReset (part-A item 4 for these).`,
    done: `SalCapCorvetteHeader allocates the exe's 0x30 bytes; no HW1 salvage reader is left; the provisional salvage/Beast-weapon entries are real ports or carry a CAT-TODO naming their stage; sp runs as far as before with ASan 0`,
    audit: `Compare 25 bodies (collision, damage, docking, launch, salvage, Beast weapon, kamikaze, blast) with the exe in ${EXP}; check the gun weapon-type word and the dock types against the exe; confirm no HW1 salvage reader is left (grep) and the 0x30 header; re-run sp (--timeout 340) and the statics self-test.` },
  { key: 'ships-b', label: 'port:ships-b',
    scope: `Ship code B, the Cataclysm-only ships (producer 4, row 4 of ${PLAN}): the 22 headers now on DefaultShipHeader (Acolyte, Mimic, Leech, ACV, MCV, RammingFrigate, MultiBeamFrigate, HiveFrigate, Swarmer, Dreadnought, ModuleShip, Worker, RepairDroid, Processor, Sentinel, CruiseMissile, RKO, Slipgate, Crystal, dBeastMothership, NomadMoon, NomadFieldEmitter) with their StaticInit and slot bodies from the exe (header contents in ${SCRATCH}/m2r2-shipstatic-tools/headers.txt), and the cloak-generator NULL headers as the exe has them (the accepted default keeps DefaultShipHeader where the exe has NULL: keep that and say why); the rest of Ships/* cat-new: the sMothership big gun (cat_bigGun*), dissolve, the disable/EMP/drift functions, cat_madAnimationFinished; move the provisional ship-state queries (the six Mimic queries, cat_shipIsSentinelFieldActive, cat_leechIsLeeching, cat_acvLinkState, the catDisabled setters) to their home files; delete the 22 dormant HW1 types (ids 200-221), DefenseFighter.c and the dormant RepairCorvette header (Ships/* 61 / 64 / 12, 18,603 MODIFIED B; cat-new 422 / 78,155 B, of which R3 ported or hooked 97).`,
    done: `every ship type uses the exe's header (Mission01's ships and all 145 types in the statics self-test, with the StaticInits run under ASan); no HW1-only ship type is left; statics 143/143 and 170/173 .shp unchanged; sp runs as far as before with ASan 0`,
    audit: `Check the 22 headers slot by slot against headers.txt and the exe (all slots for at least 10 of them) and 20 StaticInit/slot bodies against ${EXP}; confirm the 22 dormant types, DefenseFighter.c and the RepairCorvette header are gone with no reader left; re-run the statics self-test and sp (--timeout 340).` },
  { key: 'rest-oracle', label: 'port:rest-oracle',
    scope: `The rest of stage 2, the Player layout, oracle stage 2 and tools (producer 5, row 5 of ${PLAN}): Clouds.c, Nebulae.c, Camera*.c, Stats.c (fight code, GameStats layout), ResearchAPI.c (upgrades: catUpgradeFlags, cat_playerUpgradesReset), Crates.c (cratesRewardPlayer), Captaincy.c, Alliance.c, Tactics.c, Eval.c, Bounties.c, Blobs.c, Region.c, Memory.c, Task.c (cat_taskRealTimeElapsed, time compression in place of universeTurbo), HS.c, FlightMan.c, the math files (341 / 63 / 6, 33,483 MODIFIED B; cat-new 155 / 30,655 B). The Universe/Player layout with exe-size asserts (0x3D9BC / 0x6530), the FOW pointer at Player+0x6500 and the upgrades at +0x6504 (part-A item 2); scriptSetShipsCB's open deltas and unitCapCanCreateShip with ConsMgr.c's 0x30-byte shipinprogress (part-A item 5). Oracle stage 2: Ship health offset and Player layout in the checksum; a HEADLESS Mission01 oracle run (sp with the virtual clock and /oracleDump, no real-time animatic: use /disableAVI if the engine has it or add a harness switch, and keep the /debugScreenshots next-level loop out of it), wired into tools/m2_harness.sh as a mode; the save tick-0 compare stays a documented hook at saveCatLoadHook (the user's saves come later). Tools: the delta_map.py report sample filter, a classify rerun on ${EXP}, moving the 21 UI cat-new functions to stage 3 in m2_stage_counts.py.`,
    done: `two headless Mission01 oracle runs of a few thousand ticks (state the count) with ASan 0 give byte-identical .hwo streams (hwo_compare reports identical; a perturbed copy is caught at the right tick); the Universe/Player size asserts hold in the 32-bit layout check; fe and the self-tests unchanged`,
    audit: `Re-run the headless Mission01 oracle mode twice yourself and compare the streams (and once perturbed); check the Player/Universe layout (20 offsets) and the new checksum terms against the exe in ${EXP} and docs/m2-checksum-oracle.md; compare 15 bodies from the listed files with the exe; confirm the tools changes reproduce (m2_stage_counts.py).` },
]

function portPrompt(p) {
  return `${COMMON}
TASK (M2-R4 port, key ${p.key}): ${p.scope}
DONE WHEN: ${p.done}.
Report: ${REPO}/docs/m2-r4-${p.key}.md (what changed and why, file by file; each Cataclysm delta with its exe function and address; the build results; the plain-harness runs with milestone and first stop; delta-map review entries you added; what is left and its owner). Subsystems: split your scope into 3-6 subsystems and record them in the done marker.
Return via StructuredOutput: report, files_changed, summary, done_when_met, harness_milestone, first_stop, counts, open_questions.`
}

function auditPrompt(p, r) {
  return `${COMMON}
TASK: ADVERSARIAL AUDITOR for M2-R4 port "${p.key}" (producer result: ${JSON.stringify(r || {}).slice(0, 2500)}). RESUME RULE: if ${REPO}/docs/audits/m2-r4-${p.key}.md exists and ends with a "Verdict:" line, return its numbers immediately.
1. Build from clean in a separate dir: rm -rf ${SCRATCH}/build-audit-${p.key} && meson setup / compile the debug and release configurations there exactly as tools/build.sh does (read it), 0 warnings under -Werror; record sizes.
2. ${p.audit}
3. Read the full diff of this port (git diff in ${REPO}; earlier ports are already committed): every change outside the producer's scope, every edited function the delta map calls IDENTICAL without a review entry, every Cataclysm change without a "CAT:" comment is a finding.
4. Re-check the producer's DONE WHEN claim (${p.done}) yourself on the plain harness.
Fix factual errors you prove directly (code or doc; mark doc fixes "[audit: ...]"), keep the build green, and count the fixes. Write ${REPO}/docs/audits/m2-r4-${p.key}.md: what you checked, each finding with evidence, precision = correct/(correct+wrong) over the sampled checks, fixes, and a last line "Verdict: PASS|PASS-WITH-FIXES|FAIL - <reason>".
Then CHECKPOINT this port (even on FAIL, so the next port starts from a recorded state): ${CKPT} "M2-R4 ${p.key}: <verdict word> (<one-line summary>)" (it refuses forbidden paths; on an index.lock, wait 30 s and retry). Return the commit hash in "commit".`
}

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

phase('Port')
log('M2-R4: 5 src/ ports in sequence (univupdate, command, combat, ships-b, rest-oracle)')
const all = (await portChain()).filter(Boolean)

phase('Integrate')
const digest = JSON.stringify(all.map(d => ({ key: d.key, done: d.produced && d.produced.done_when_met, milestone: d.produced && d.produced.harness_milestone, first_stop: d.produced && d.produced.first_stop, counts: d.produced && d.produced.counts, questions: d.produced && d.produced.open_questions, audit: d.audit && { precision: d.audit.precision, verdict: d.audit.verdict, commit: d.audit.commit } }))).slice(0, 20000)
const integrated = await agent(`${COMMON}
TASK: INTEGRATE M2-R4. Digest: ${digest}
1. STAGE-2 EXIT TEST (docs/m2-r3-summary.md "R4 integrate"), from a clean build on the PLAIN harness: Mission01 runs a few thousand ticks headless with ASan 0; two hwi-engine runs of it give byte-identical .hwo streams (the headless Mission01 oracle mode); fe still reaches Main_game_screen; the self-tests (statics, meshes, textures, etg, oracle) unchanged or better; the release oracle self-test reports SELFTEST PASS; quitting runs return rc 0; tools/m2_layout32.sh OK. Run them yourself and report each with its run label. Also tools/build.sh --debug and --release from clean, 0 warnings.
2. Write ${REPO}/docs/m2-r4-summary.md in the shape of docs/m2-r3-summary.md: per port (what landed, done-when met or not, audit precision/verdict, commit), the delta map after the round (tools/m2_stage_counts.py), the exit-test table, what is left of stage 2 (if anything, with owners), and the plan for M2-R5: stage 3 UI and presentation (docs/m2-r1-summary.md stage table: FE flow, render/GL, sound/speech, NIS, the stage-3 items the R2-R4 reports deferred: billboard/fog/lens-flare renderers, status bars and picking, the ship info panel, speech/UI hooks, fquant.c/fescreen UBSan, the 1,705-string UI readers), split into rounds of 5 producers + 5 auditors + integrate, with counts, done-whens and an exit test; remaining stage-2 items first if any.
3. Update ${REPO}/cycle/STATE.md "## M2": an "M2-R4" subsection (COMPLETE with date and numbers, links), the M2-R5 plan in one table, and the M2 decisions (keep the open ones: the oracle reference recording, the two saves, the M0 sign-off, the first live oracle session; add anything this round surfaced, with the default the tree uses). Keep the rest of the file; the orchestrator updates the workflow table row.
4. git status: nothing from original/, reference/, build/ or game data staged or untracked outside .gitignore. Then: ${CKPT} "M2-R4: stage 2 part B (universe update, command layer, combat/docking/salvage, Cataclysm-only ships, rest + Player layout + oracle stage 2)"
Return via StructuredOutput.`,
  { label: 'integrate:m2-r4', phase: 'Integrate', model: 'opus', effort: 'high',
    schema: { type: 'object', properties: {
      commit: { type: 'string' }, stage2_exit_met: { type: 'boolean' }, unmet: { type: 'array', items: { type: 'string' } },
      milestone: { type: 'string' }, first_stop: { type: 'string' }, build_ok: { type: 'boolean' },
      m2r5_plan: { type: 'array', items: { type: 'string' } }, user_decisions: { type: 'array', items: { type: 'string' } }, summary: { type: 'string' } },
      required: ['commit', 'stage2_exit_met', 'unmet', 'milestone', 'first_stop', 'build_ok', 'm2r5_plan', 'user_decisions', 'summary'] } })

return { items: all.map(d => ({ key: d.key, done: d.produced && d.produced.done_when_met, milestone: d.produced && d.produced.harness_milestone, verdict: d.audit && d.audit.verdict, commit: d.audit && d.audit.commit })), integrated }
