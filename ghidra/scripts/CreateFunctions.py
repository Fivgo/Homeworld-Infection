# Create functions Ghidra's auto-analysis missed, from a JSON address list
# (ghidra/create_functions_r2.json), then analyse only the new code.
#
# JSON shape: [{"address": "0x...", "name": "<identifier or empty>",
#               "reason": "mission-init|mission-watch|ship-slot|pointer-table|prologue-scan"}]
# (a {"functions": [...]} or {"symbols": [...]} wrapper is accepted too)
#
# Args (key=value, any order; a bare first arg is taken as json=):
#   json=<path>      the address list (required)
#   report=<csv>     per-entry status CSV (default <json minus .json>.report.csv)
#   analyze=1|0      after creating, run auto-analysis restricted to the new function bodies
#                    (default 1; uses the program's stored analysis options, i.e. the
#                    ConfigureAnalysis profile applied at import)
#   tag=R2_created   function tag put on every function this script creates
#   returning=<hex,...>  functions that DO return although analysis marked them non-returning;
#                    their no-return flag is cleared, every call site's fall-through is
#                    re-disassembled (iterated until stable) and the bodies of the callers plus
#                    every function in `region` are re-fixed. Default 0x00490bb0 =
#                    kasGrowSelectionPtr (HW1 KAS.c:512 returns &Selections[i]->shipList); the
#                    first A1 pass (2026-10-09) had "Non-Returning Functions - Discovered" mark it
#                    no-return, which truncated 620 decompiled call sites and deleted 944 of the
#                    2,085 freshly created mission functions. returning= (empty) turns this off.
#   autoreturn=1|0   after analysis, any function that analysis newly marked no-return although
#                    its body holds a RET is treated like returning= (flag cleared, call sites
#                    re-disassembled), then entries whose function vanished are re-created and
#                    bodies re-fixed, for up to 3 rounds (default 1)
#   region=<lo>-<hi> functions whose bodies are re-fixed after the repair
#                    (default 0x5d6fe0-0x670a0f, the mission-code region)
#   disable=<names>  analyzers switched off while the new code is analysed, restored afterwards
#                    (default "Non-Returning Functions - Discovered,WindowsResourceReference":
#                    the first re-marks the functions above, the second took 1708 s of the
#                    first pass for nothing). disable= (empty) keeps the stored options.
#
# Per entry:
#   function already at the address          -> skipped (exists), never renamed here
#   address inside a pre-existing function   -> skipped (inside), never carved out
#   otherwise: clear a defined-data unit sitting on the entry (executable blocks only),
#   disassemble, create the function (all entries in one batch so bodies stop at each
#   other's entries), fix up bodies, apply the name as the primary name (IMPORTED source)
#   -> created / failed:<reason>
# Tolerant: bad entries are reported and skipped; the script never aborts the headless run.
#
# @category Homeworld-Infection
# @runtime PyGhidra

import typing
if typing.TYPE_CHECKING:
    from ghidra.ghidra_builtins import *  # noqa: F401,F403

import csv
import json
import os
import re
import time

from ghidra.app.cmd.function import CreateFunctionCmd
from ghidra.app.plugin.core.analysis import AutoAnalysisManager
from ghidra.program.model.address import AddressSet
from ghidra.program.model.listing import CommentType
from ghidra.program.model.symbol import SourceType, SymbolUtilities

MARK = "[R2]"
# Functions that return (each ends in a reachable RET; HW1 KAS.c / KASFunc.c bodies return a
# value) but that Ghidra's "Non-Returning Functions - Discovered" marked no-return while
# analysing the mission code on 2026-10-09: kasGrowSelectionPtr (pass 1), kasfShipsCount,
# kasfShipsSelectType, kasfShipsSelectCapital, kasfShipsSelectDamaged, kasfShipsSelectNearby
# (pass 2, although the analyzer had been switched off through setAnalysisOption).
DEFAULT_RETURNING = "0x00490bb0,0x00492d20,0x00495690,0x004956e0,0x004957d0,0x00495a50"
T0 = time.time()


def log(msg):
    println("[CreateFunctions +%.1fs] %s" % (time.time() - T0, msg))


def parse_args():
    args = {}
    for i, a in enumerate(list(getScriptArgs())):
        a = str(a)
        if "=" in a:
            k, v = a.split("=", 1)
            args[k.strip().lower()] = v.strip()
        elif i == 0:
            args["json"] = a
    return args


def parse_int(v):
    if v is None:
        return None
    if isinstance(v, int):
        return v
    s = str(v).strip().lower().replace("`", "").replace("_", "")
    if not s:
        return None
    try:
        return int(s[2:] if s.startswith("0x") else s, 16)
    except ValueError:
        return None


def clean_name(name):
    name = re.sub(r"\s+", "_", str(name or "").strip())
    return SymbolUtilities.replaceInvalidChars(name, True) if name else ""


def first_line(e):
    s = str(e)
    return (s.splitlines()[0] if s else repr(e))[:200]


def add_detail(row, text):
    row["detail"] = (row["detail"] + ";" if row["detail"] else "") + text


def apply_name(func, name):
    """Make `name` the function's name (IMPORTED). Never clobbers a USER_DEFINED name."""
    if not name:
        return "no-name"
    if func.getName() == name:
        return "same-name"
    if func.getSymbol().getSource() == SourceType.USER_DEFINED:
        currentProgram.getSymbolTable().createLabel(func.getEntryPoint(), name, SourceType.IMPORTED)
        return "kept-user-name"
    func.setName(name, SourceType.IMPORTED)
    return "named"


def parse_list(v):
    return [x.strip() for x in str(v or "").split(",") if x.strip()]


def repair_returning(addrs, region):
    """Clear a wrong no-return flag and restore the flow it cut. Returns (stats, functions to fix)."""
    listing = currentProgram.getListing()
    fm = currentProgram.getFunctionManager()
    rm = currentProgram.getReferenceManager()
    stats = {"flag_cleared": 0, "callsites": 0, "fallthrough_overrides": 0, "redisassembled": 0}
    fix = {}
    for off in addrs:
        f = fm.getFunctionAt(toAddr(off))
        if f is None:
            log("returning=0x%08x: no function there, skipped" % off)
            continue
        if f.hasNoReturn():
            f.setNoReturn(False)
            stats["flag_cleared"] += 1
            log("returning: cleared no-return on %s@%s" % (f.getName(), f.getEntryPoint()))
        done = set()
        for rnd in range(25):
            new = 0
            for ref in list(rm.getReferencesTo(f.getEntryPoint())):
                if not ref.getReferenceType().isCall():
                    continue
                src = ref.getFromAddress()
                if src in done:
                    continue
                ins = listing.getInstructionAt(src)
                if ins is None:
                    continue
                done.add(src)
                stats["callsites"] += 1
                if ins.isFallThroughOverridden():
                    ins.clearFallThroughOverride()
                    stats["fallthrough_overrides"] += 1
                ft = ins.getMaxAddress().next()
                if ft is not None and listing.getInstructionAt(ft) is None \
                        and listing.getDefinedDataContaining(ft) is None:
                    try:
                        disassemble(ft)
                    except Exception as e:
                        log("disassemble %s failed: %s" % (ft, first_line(e)))
                    if listing.getInstructionAt(ft) is not None:
                        stats["redisassembled"] += 1
                        new += 1
                caller = fm.getFunctionContaining(src)
                if caller is not None:
                    fix[caller.getEntryPoint()] = caller
            if new == 0:
                break
    if region:
        lo, hi = region
        for fn in fm.getFunctions(toAddr(lo), True):
            if fn.getEntryPoint().getOffset() > hi:
                break
            fix[fn.getEntryPoint()] = fn
    log("returning repair: %s; %d functions to re-fix" % (stats, len(fix)))
    return stats, fix


def has_ret(fn):
    for ins in currentProgram.getListing().getInstructions(fn.getBody(), True):
        if ins.getMnemonicString().upper().startswith("RET"):
            return True
    return False


def noreturn_set():
    return set(fn.getEntryPoint().getOffset()
               for fn in currentProgram.getFunctionManager().getFunctions(True) if fn.hasNoReturn())


def create_batch(pairs):
    """Disassemble + batch-create functions at [(addr, row)]; returns the addresses that got one."""
    listing = currentProgram.getListing()
    fm = currentProgram.getFunctionManager()
    entries = AddressSet()
    for a, row in pairs:
        if listing.getInstructionAt(a) is None:
            try:
                disassemble(a)
            except Exception:
                pass
        if listing.getInstructionAt(a) is not None and fm.getFunctionContaining(a) is None:
            entries.add(a)
    if not entries.isEmpty():
        try:
            CreateFunctionCmd(entries, SourceType.DEFAULT).applyTo(currentProgram, monitor)
        except Exception as e:
            log("batch create raised %s" % first_line(e))
    out = []
    for a, row in pairs:
        if fm.getFunctionAt(a) is None and fm.getFunctionContaining(a) is None \
                and listing.getInstructionAt(a) is not None:
            try:
                CreateFunctionCmd(None, a, None, SourceType.DEFAULT).applyTo(currentProgram, monitor)
            except Exception:
                pass
        if fm.getFunctionAt(a) is not None:
            out.append(a)
    return out


def set_analyzers(names, value):
    """Set boolean analyzers; returns {name: previous value string} for the ones that exist."""
    prev = {}
    if not names:
        return prev
    cur = getCurrentAnalysisOptionsAndValues(currentProgram)
    for n in names:
        if not cur.containsKey(n):
            log("analyzer option not registered: %s" % n)
            continue
        prev[n] = str(cur.get(n))
        setAnalysisOption(currentProgram, n, value)
    after = getCurrentAnalysisOptionsAndValues(currentProgram)
    log("analyzers %s" % ", ".join("%s: %s -> %s" % (n, prev[n], after.get(n)) for n in prev))
    return prev


def main():
    args = parse_args()
    path = args.get("json")
    if not path or not os.path.isfile(path):
        log("address list not found, skipping: %s" % path)
        return
    report = args.get("report") or (re.sub(r"\.json$", "", path) + ".report.csv")
    analyze = args.get("analyze", "1") in ("1", "true", "yes")
    tag = args.get("tag", "R2_created")
    returning = [o for o in (parse_int(x) for x in parse_list(args.get("returning", DEFAULT_RETURNING)))
                 if o is not None]
    region = None
    rg = args.get("region", "0x5d6fe0-0x670a0f")
    if rg and "-" in rg:
        lo_s, hi_s = rg.split("-", 1)
        if parse_int(lo_s) is not None and parse_int(hi_s) is not None:
            region = (parse_int(lo_s), parse_int(hi_s))
    autoreturn = args.get("autoreturn", "1") in ("1", "true", "yes")
    disable = parse_list(args.get("disable",
                                  "Non-Returning Functions - Discovered,WindowsResourceReference"))
    try:
        with open(path, "r", encoding="utf-8") as fh:
            doc = json.load(fh)
    except Exception as e:
        log("could not parse %s (%s), skipping" % (path, first_line(e)))
        return
    if isinstance(doc, dict):
        doc = doc.get("functions") or doc.get("symbols") or []

    mem = currentProgram.getMemory()
    listing = currentProgram.getListing()
    fm = currentProgram.getFunctionManager()
    before = fm.getFunctionCount()
    rep_stats, refix = ({}, {})
    if returning:
        rep_stats, refix = repair_returning(returning, region)
    rows, todo, seen = [], [], set()
    for i, ent in enumerate(doc):
        row = {"address": "", "name": "", "reason": "", "status": "", "function": "",
               "body_bytes": "", "detail": ""}
        rows.append(row)
        if not isinstance(ent, dict):
            row["status"], row["detail"] = "failed", "bad-entry #%d" % i
            continue
        row["address"] = str(ent.get("address") or "")
        row["name"] = clean_name(ent.get("name"))
        row["reason"] = str(ent.get("reason") or "")
        off = parse_int(ent.get("address"))
        if off is None:
            row["status"], row["detail"] = "failed", "bad-address"
            continue
        row["address"] = "0x%08x" % off
        if off in seen:
            row["status"], row["detail"] = "skipped", "duplicate-entry"
            continue
        seen.add(off)
        try:
            addr = toAddr(off)
        except Exception:
            addr = None
        if addr is None or not mem.contains(addr):
            row["status"], row["detail"] = "failed", "not-in-memory"
            continue
        f = fm.getFunctionAt(addr)
        if f is not None:
            row["status"], row["detail"], row["function"] = "skipped", "exists", f.getName(True)
            continue
        outer = fm.getFunctionContaining(addr)
        if outer is not None:
            row["status"] = "skipped"
            row["detail"] = "inside:%s@%s" % (outer.getName(True), outer.getEntryPoint())
            continue
        todo.append((addr, row))
    log("entries=%d to-create=%d functions-before=%d" % (len(rows), len(todo), before))

    # 1. disassemble every entry first, so the batch create below sees all the code
    for addr, row in todo:
        block = mem.getBlock(addr)
        if block is None or not block.isExecute():
            row["status"], row["detail"] = "failed", "not-executable-block"
            continue
        if listing.getInstructionAt(addr) is None:
            d = listing.getDefinedDataContaining(addr)
            if d is not None:
                try:
                    dname = d.getDataType().getName()
                    listing.clearCodeUnits(d.getAddress(), d.getMaxAddress(), False)
                    add_detail(row, "cleared-data:%s" % dname)
                except Exception as e:
                    row["status"] = "failed"
                    add_detail(row, "clear-data-failed:%s" % first_line(e))
                    continue
            try:
                disassemble(addr)
            except Exception as e:
                add_detail(row, "disassemble-error:%s" % first_line(e))
        if listing.getInstructionAt(addr) is None:
            row["status"] = "failed"
            add_detail(row, "no-instruction-after-disassembly")
    live = [(a, r) for a, r in todo if r["status"] == ""]
    log("disassembled; %d entries have code" % len(live))

    # 2. create all functions in one batch (bodies then stop at each other's entry points)
    entries = AddressSet()
    for a, _ in live:
        entries.add(a)
    if not entries.isEmpty():
        try:
            CreateFunctionCmd(entries, SourceType.DEFAULT).applyTo(currentProgram, monitor)
        except Exception as e:
            log("batch create raised %s; falling back to one by one" % first_line(e))
    for a, row in live:
        if fm.getFunctionAt(a) is None:
            cmd = CreateFunctionCmd(None, a, None, SourceType.DEFAULT)
            ok = False
            try:
                ok = cmd.applyTo(currentProgram, monitor)
            except Exception as e:
                add_detail(row, "create-error:%s" % first_line(e))
            if not ok or fm.getFunctionAt(a) is None:
                row["status"] = "failed"
                add_detail(row, "create-failed:%s" % (cmd.getStatusMsg() or "no function at entry"))
    created = [(a, r) for a, r in live if r["status"] == "" and fm.getFunctionAt(a) is not None]

    # 3. fix up bodies now that every new entry exists
    fixed = 0
    for a, _ in created:
        try:
            if CreateFunctionCmd.fixupFunctionBody(currentProgram, fm.getFunctionAt(a), monitor):
                fixed += 1
        except Exception:
            pass
    log("created=%d (fixupFunctionBody ok on %d)" % (len(created), fixed))
    created_set = set(a for a, _ in created)
    refixed = 0
    if returning:
        # second repair sweep: the new functions' code may hold more cut call sites
        _, more = repair_returning(returning, region)
        refix.update(more)
        for ep, fn in list(refix.items()):
            if ep in created_set:
                continue
            try:
                fn = fm.getFunctionAt(ep)
                if fn is not None and CreateFunctionCmd.fixupFunctionBody(currentProgram, fn, monitor):
                    refixed += 1
            except Exception:
                pass
        log("re-fixed %d of %d existing function bodies after the returning repair"
            % (refixed, len(refix)))

    # 4. names, tags, comments
    for a, row in created:
        f = fm.getFunctionAt(a)
        try:
            add_detail(row, apply_name(f, row["name"]))
        except Exception as e:
            add_detail(row, "name-failed:%s" % first_line(e))
        f.addTag(tag)
        try:
            old = listing.getComment(CommentType.PLATE, a)
            line = "%s created by CreateFunctions.py (%s)" % (MARK, row["reason"])
            if not old or line not in old:
                listing.setComment(a, CommentType.PLATE, (old + "\n" + line) if old else line)
        except Exception:
            pass
        row["status"] = "created"

    # 5. auto-analysis restricted to the new code
    nr_before = noreturn_set()
    funcs_before = set(fn.getEntryPoint().getOffset() for fn in fm.getFunctions(True))
    if analyze and (created or refix):
        bodies = AddressSet()
        for a, _ in created:
            bodies.add(fm.getFunctionAt(a).getBody())
        for ep in refix:
            fn = fm.getFunctionAt(ep)
            if fn is not None:
                bodies.add(fn.getBody())
        prev = set_analyzers(disable, "false")
        try:
            mgr = AutoAnalysisManager.getAnalysisManager(currentProgram)
            mgr.initializeOptions()
            mgr.reAnalyzeAll(bodies)
            log("analysing %d bytes of new code ..." % bodies.getNumAddresses())
            mgr.startAnalysis(monitor)
            mgr.waitForAnalysis(None, monitor)
            log("analysis done")
        except Exception as e:
            log("analysis raised %s" % first_line(e))
        for n, v in prev.items():
            setAnalysisOption(currentProgram, n, v)
        if prev:
            log("analyzers restored: %s" % ", ".join("%s=%s" % kv for kv in prev.items()))
    for off in returning:
        fn = fm.getFunctionAt(toAddr(off))
        if fn is not None:
            if fn.hasNoReturn():
                log("WARNING %s was marked no-return again by analysis (autoreturn repairs it)"
                    % fn.getName())
            log("returning check: %s no_return=%s calls_in=%d" % (
                fn.getName(), fn.hasNoReturn(),
                sum(1 for r in currentProgram.getReferenceManager().getReferencesTo(fn.getEntryPoint())
                    if r.getReferenceType().isCall())))

    # 5b. undo wrong no-return discoveries made by that analysis, re-create what they deleted
    if analyze and autoreturn and (created or refix):
        for rnd in range(3):
            flipped = [o for o in noreturn_set() - nr_before
                       if has_ret(fm.getFunctionAt(toAddr(o)))]
            # every function that existed before the analysis (created now, by an earlier pass, or
            # by Ghidra) and is gone: ClearFlowAndRepair deletes unreferenced functions whose
            # entry flow it cut. Rows give the name; else the label left at the entry keeps it.
            by_off = dict((a.getOffset(), r) for a, r in created)
            for r in rows:
                o = parse_int(r.get("address"))
                if o is not None and o not in by_off:
                    by_off[o] = r
            gone = [(toAddr(o), by_off.get(o) or {"name": "", "detail": ""})
                    for o in sorted(funcs_before) if fm.getFunctionAt(toAddr(o)) is None]
            if not flipped and not gone:
                break
            log("autoreturn round %d: wrongly no-return %s; %d created functions gone"
                % (rnd + 1, ", ".join("%s@0x%08x" % (fm.getFunctionAt(toAddr(o)).getName(), o)
                                      for o in flipped) or "none", len(gone)))
            more = {}
            if flipped:
                _, more = repair_returning(flipped, region)
            back = create_batch(gone)
            for a in back:
                row = next(r for x, r in gone if x == a)
                f = fm.getFunctionAt(a)
                try:
                    apply_name(f, row["name"])
                except Exception:
                    pass
                f.addTag(tag)
                add_detail(row, "recreated-round%d" % (rnd + 1))
            if more or back:
                _, more2 = repair_returning(returning + flipped, region)
                more.update(more2)
                n = 0
                for ep in list(more):
                    fn = fm.getFunctionAt(ep)
                    try:
                        if fn is not None and CreateFunctionCmd.fixupFunctionBody(currentProgram, fn,
                                                                                  monitor):
                            n += 1
                    except Exception:
                        pass
                log("autoreturn round %d: re-created %d of %d, re-fixed %d bodies"
                    % (rnd + 1, len(back), len(gone), n))
            returning = returning + [o for o in flipped if o not in returning]

    # 6. final per-entry state (analysis may have grown bodies)
    for r in rows:
        if r["status"] == "skipped" and r["detail"] == "exists":
            o = parse_int(r["address"])
            if o is not None and fm.getFunctionAt(toAddr(o)) is None:
                r["status"] = "failed"
                add_detail(r, "existing-function-gone-after-analysis")
    for a, row in created:
        f = fm.getFunctionAt(a)
        if f is None:
            row["status"] = "failed"
            add_detail(row, "function-gone-after-analysis")
            continue
        row["function"] = f.getName(True)
        row["body_bytes"] = str(f.getBody().getNumAddresses())
    tot, by_reason = {}, {}
    for r in rows:
        tot[r["status"] or "?"] = tot.get(r["status"] or "?", 0) + 1
        k = "%s/%s" % (r["reason"] or "?", r["status"] or "?")
        by_reason[k] = by_reason.get(k, 0) + 1
    log("results: %s" % ", ".join("%s:%d" % kv for kv in sorted(tot.items())))
    log("by reason: %s" % ", ".join("%s:%d" % kv for kv in sorted(by_reason.items())))
    log("functions before=%d after=%d" % (before, fm.getFunctionCount()))
    for r in rows:
        if r["status"] == "failed":
            log("FAILED %s %s %s: %s" % (r["address"], r["reason"], r["name"], r["detail"]))
    try:
        d = os.path.dirname(report)
        if d:
            os.makedirs(d, exist_ok=True)
        with open(report, "w", newline="", encoding="utf-8") as fh:
            w = csv.DictWriter(fh, fieldnames=["address", "name", "reason", "status", "function",
                                               "body_bytes", "detail"])
            w.writeheader()
            for r in rows:
                w.writerow({k: r.get(k, "") for k in w.fieldnames})
        log("report: %s" % report)
    except Exception as e:
        log("could not write report %s: %s" % (report, first_line(e)))


main()
