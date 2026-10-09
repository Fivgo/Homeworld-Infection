# Set auto-analysis options for a 32-bit MSVC6-era Windows game (Homeworld: Cataclysm).
# Run as a -preScript: options set before auto-analysis are honoured by the headless analyzer.
#
# Args (optional, key=value):  profile=msvc6 (default) | fast | default (leave Ghidra defaults)
#                              fast = msvc6 minus the expensive analyzers (decompiler-driven
#                              param/switch/convention analysis, stack, AIF, FID, DWARF):
#                              for quick triage and for huge symbolised ground-truth builds
#                              strmin=4      ASCII-string minimum length (LEN_4..LEN_n)
#                              resref=0|1    WindowsResourceReference analyzer (default 0 = off:
#                                     200 s for 2 bookmarks on cataclysm.exe)
#                              noexec=.cms_t[,name...]  quarantine memory blocks: clear their
#                                     execute flag and set the program option
#                                     Disassembler."Restrict Disassembly to Executable Memory",
#                                     so neither flow-following nor the instruction/function-start
#                                     finders disassemble them (applied in every profile).
#                                     Intended for cataclysm.exe's high-entropy .cms_t
#                                     (SecuROM-style) section; see docs/ghidra-pipeline.md.
# @category Homeworld-Infection
# @runtime PyGhidra

import typing
if typing.TYPE_CHECKING:
    from ghidra.ghidra_builtins import *  # noqa: F401,F403

ARGS = {}
for a in list(getScriptArgs()):
    if "=" in a:
        k, v = a.split("=", 1)
        ARGS[k.strip().lower()] = v.strip()

profile = ARGS.get("profile", "msvc6")
strmin = int(ARGS.get("strmin", "4"))
noexec = [n.strip() for n in ARGS.get("noexec", "").split(",") if n.strip()]

if noexec:
    from ghidra.program.model.listing import Program
    RESTRICT = "Restrict Disassembly to Executable Memory"  # Disassembler.RESTRICT_DISASSEMBLY_TO_EXECUTE_MEMORY_PROPERTY
    mem = currentProgram.getMemory()
    done = []
    for name in noexec:
        blk = mem.getBlock(name)
        if blk is None:
            println("[ConfigureAnalysis] WARNING noexec: no memory block named %r (blocks: %s)"
                    % (name, ", ".join(b.getName() for b in mem.getBlocks())))
            continue
        was = blk.isExecute()
        blk.setExecute(False)
        currentProgram.getBookmarkManager().setBookmark(
            blk.getStart(), "Note", "pipeline",
            "noexec: execute flag cleared before analysis (was %s); block not disassembled" % was)
        done.append("%s(was x=%s)" % (name, was))
    if done:
        # restrict=0 is test-only: it shows that clearing the execute flag alone does NOT stop
        # disassembly (docs/ghidra-pipeline.md G19)
        if ARGS.get("restrict", "1") not in ("0", "false", "no"):
            currentProgram.getOptions(Program.DISASSEMBLER_PROPERTIES).setBoolean(RESTRICT, True)
        println("[ConfigureAnalysis] noexec: %s; %s=%s" % (
            ", ".join(done), RESTRICT,
            currentProgram.getOptions(Program.DISASSEMBLER_PROPERTIES).getBoolean(RESTRICT, False)))

# Option names verified against Ghidra 12.1.4 getCurrentAnalysisOptionsAndValues() on a PE32.
# Names that do not exist for a given program are reported and skipped (they are
# per-analyzer and only registered when the analyzer applies to the program).
WANT = [
    # find code the entry-point/call-graph walk misses (MSVC6 switch tables, callbacks, vtables)
    ("Aggressive Instruction Finder", "true"),
    # recover params/returns via decompiler. Ghidra enables it by default only for a PE with
    # fewer than 2 MiB of addresses (DecompilerFunctionAnalyzer.getDefaultEnablement); cataclysm.exe
    # has ~9 MB of blocks, so without this line it would be off (docs/ghidra-pipeline.md 4)
    ("Decompiler Parameter ID", "true"),
    ("Decompiler Parameter ID.Analysis Decompiler Timeout (sec)", "90"),
    # function-start byte patterns (x86win_patterns.xml), incl. after code/data
    ("Function Start Search", "true"),
    ("Function Start Pre Search", "true"),
    ("Function Start Search After Code", "true"),
    ("Function Start Search After Data", "true"),
    # Win32 API parameter names/types pushed back into callers
    ("WindowsPE x86 Propagate External Parameters", "true"),
    # MSVC CRT / library recognition (vsOlder_x86.fidbf etc.)
    ("Function ID", "true"),
    ("Demangler Microsoft", "true"),
    # vtables / jump tables / pointer tables (KAS, ETG and AI dispatch use them)
    ("Create Address Tables", "true"),
    ("Data Reference.Switch Table References", "true"),
    ("Reference.Switch Table References", "true"),
    # more strings -> more anchors for the HW1 string-xref matcher
    ("ASCII Strings", "true"),
    ("ASCII Strings.Minimum String Length", "LEN_%d" % strmin),
    ("Data Reference.Minimum String Length", str(strmin)),
    ("Reference.Minimum String Length", str(strmin)),
    # stack params as real variables help the decompiler dump
    ("Stack.Create Param Variables", "true"),
    ("Variadic Function Signature Override", "true"),
    # RTTI/EH: these options are registered only for a PE that Ghidra identifies as MSVC/Clang
    # (RttiAnalyzer/PEExceptionAnalyzer.canAnalyze -> PEUtil.isVisualStudioOrClangPe), not
    # because RTTI was found. cataclysm.exe does carry RTTI: 266 type descriptors (WON, Crypto++,
    # Magick++, std) on the first real run (docs/ghidra-pipeline.md 6.7)
    ("Windows x86 PE RTTI Analyzer", "true"),
    ("Windows x86 PE Exception Handling", "true"),
    # PDB: none shipped; don't hit symbol servers
    ("PDB Universal.Search untrusted symbol servers", "false"),
    # Resource-ID references (LoadString/LoadIcon ids -> .rsrc). On cataclysm.exe this analyzer
    # alone took 199.8 s of a 347 s auto-analysis and produced 2 bookmarks (its .rsrc is one
    # icon). Off by default; resref=1 turns it back on (e.g. for a DLL with real resources).
    ("WindowsResourceReference", "true" if ARGS.get("resref", "0") in ("1", "true", "yes") else "false"),
]

FAST_OFF = [
    ("Decompiler Parameter ID", "false"),
    ("Decompiler Switch Analysis", "false"),
    ("Call Convention ID", "false"),
    ("Stack", "false"),
    ("Aggressive Instruction Finder", "false"),
    ("Function ID", "false"),
    ("DWARF", "false"),
    ("Variadic Function Signature Override", "false"),
]
if profile == "fast":
    off = dict(FAST_OFF)
    WANT = [(n, off.get(n, v)) for n, v in WANT] + \
           [(n, v) for n, v in FAST_OFF if n not in dict(WANT)]

if profile == "default":
    println("[ConfigureAnalysis] profile=default: leaving Ghidra defaults")
else:
    current = getCurrentAnalysisOptionsAndValues(currentProgram)
    applied, skipped = 0, []
    for name, value in WANT:
        if not current.containsKey(name):
            skipped.append(name)
            continue
        setAnalysisOption(currentProgram, name, value)
        applied += 1
    after = getCurrentAnalysisOptionsAndValues(currentProgram)
    mismatched = [n for n, v in WANT if after.containsKey(n) and str(after.get(n)).lower() != v.lower()]
    println("[ConfigureAnalysis] profile=%s applied=%d skipped(not registered for this program)=%s"
            % (profile, applied, ", ".join(skipped) if skipped else "none"))
    if mismatched:
        println("[ConfigureAnalysis] WARNING read-back mismatch: %s"
                % ", ".join("%s=%s" % (n, after.get(n)) for n in mismatched))
