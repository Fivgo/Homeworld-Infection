# Grow truncated function bodies to a documented end address, then cover the mission region.
#
# Input CSV (ghidra/proposals/r2/missions-r2.body-fix.csv), columns
#   address,name,doc_end,doc_size,ghidra_size,lost_callees,lost_callee_names
# doc_end is EXCLUSIVE: doc_end == address + doc_size, the first byte after the last instruction
# (docs/mission-code-map.md section 7: "code bytes from the entry to the end of the last
# instruction"). The body set here is therefore [address, doc_end - 1].
#
# Args (key=value, any order; a bare first arg is taken as csv=):
#   csv=<path>       the body-fix list (required)
#   report=<csv>     per-row status CSV (default <csv minus .csv>.report.csv)
#   region=<lo>-<hi> region scanned for uncovered runs afterwards (default 0x5d6fe0-0x670a0f)
#   scan=1|0         create functions on uncovered runs of the region (default 1)
#   analyze=1|0      auto-analysis restricted to the changed/new bodies (default 1), with the
#                    analyzers in disable= switched off meanwhile (same default as
#                    CreateFunctions.py). Every function body is snapshotted before it; after it,
#                    newly no-return functions that hold a RET get the flag cleared, deleted
#                    functions are re-created (old name) and shrunk bodies re-grown (re-disassembled)
#   disable=<names>  default "Non-Returning Functions - Discovered,WindowsResourceReference"
#   tag=R3_bodyfix   function tag on every fixed function (R3_uncovered on scan-created ones)
#
# Per row:
#   no function at address           -> re-created there first (detail "recreated"), else failed
#   walk [address, end]: instruction -> next (fall-through overrides on it cleared);
#                        defined data -> cleared (never a function entry), then disassembled;
#                        undefined    -> disassembled (flow restricted to [address, end])
#   another function entry strictly inside (address, end] -> end = that entry - 1 ("clipped")
#   another function's body overlapping the range        -> subtracted ("clipped")
#   body := old body + [address, end]  (Function.setBody)  -> fixed / clipped / skipped / failed
# Uncovered-run scan (after all rows): for every run of the region not inside any function body
#   and not alignment padding, each 16-byte aligned candidate (first non-padding byte of the run,
#   then every 16 bytes) that is (a) a plausible prologue after padding/RET, or (b) the target of a call, of a pointer
#   held in data, or of a jump from another mission-region function, gets a function (unnamed,
#   reason "uncovered-run"). Address tables (first dword points into the region) are skipped.
# Tolerant: bad rows are reported and skipped; the script never aborts the headless run.
#
# @category Homeworld-Infection
# @runtime PyGhidra

import typing
if typing.TYPE_CHECKING:
    from ghidra.ghidra_builtins import *  # noqa: F401,F403

import csv
import os
import re
import time

from ghidra.app.cmd.disassemble import DisassembleCommand
from ghidra.app.cmd.function import CreateFunctionCmd
from ghidra.app.plugin.core.analysis import AutoAnalysisManager
from ghidra.program.model.address import AddressSet
from ghidra.program.model.listing import CommentType, FlowOverride
from ghidra.program.model.symbol import SourceType

MARK = "[R3]"
T0 = time.time()
PADDING = (0x90, 0xCC)
# mission-function first bytes seen in exe 12948 (2,085 kas2c functions): mov eax/ecx/edx,
# [0x00a43ee8]; push imm32; mov dword [0x00c3749c], imm; push imm8. Plus the usual MSVC frames.
PROLOGUES = (
    bytes.fromhex("a1e83ea400"),
    bytes.fromhex("8b0de83ea400"),
    bytes.fromhex("8b15e83ea400"),
    bytes.fromhex("c7059c74c300"),
    bytes.fromhex("558bec"),
    bytes.fromhex("83ec"),
    bytes.fromhex("81ec"),
)
PROLOGUE_FIRST = (0x68, 0x6A, 0x53, 0x55, 0x56, 0x57)


def log(msg):
    println("[FixFunctionBodies +%.1fs] %s" % (time.time() - T0, msg))


def parse_args():
    args = {}
    for i, a in enumerate(list(getScriptArgs())):
        a = str(a)
        if "=" in a:
            k, v = a.split("=", 1)
            args[k.strip().lower()] = v.strip()
        elif i == 0:
            args["csv"] = a
    return args


def parse_int(v):
    if v is None:
        return None
    s = str(v).strip().lower().replace("`", "").replace("_", "")
    if not s:
        return None
    try:
        return int(s[2:] if s.startswith("0x") else s, 16)
    except ValueError:
        return None


def parse_list(v):
    return [x.strip() for x in str(v or "").split(",") if x.strip()]


def first_line(e):
    s = str(e)
    return (s.splitlines()[0] if s else repr(e))[:200]


def add_detail(row, text):
    row["detail"] = (row["detail"] + ";" if row["detail"] else "") + text


def raw(addr, n):
    try:
        return bytes((x & 0xFF) for x in getBytes(addr, n))
    except Exception:
        return b""


def body_set(fn):
    s = AddressSet()
    s.add(fn.getBody())
    return s


def other_functions_overlapping(aset, self_entry):
    out = []
    it = currentProgram.getFunctionManager().getFunctionsOverlapping(aset)
    while it.hasNext():
        g = it.next()
        if g.getEntryPoint() != self_entry:
            out.append(g)
    return out


def walk_disassemble(lo, hi, stats):
    """Make [lo, hi] instructions. Returns the max address actually covered by instructions."""
    listing = currentProgram.getListing()
    fm = currentProgram.getFunctionManager()
    restricted = AddressSet(lo, hi)
    x = lo
    last = None
    guard = 0
    while x is not None and x.compareTo(hi) <= 0 and guard < 200000:
        guard += 1
        ins = listing.getInstructionContaining(x)
        if ins is not None:
            if ins.isFallThroughOverridden():
                ins.clearFallThroughOverride()
                stats["ft_cleared"] += 1
            if ins.getFlowOverride() == FlowOverride.CALL_RETURN or \
                    (ins.getFlowOverride() != FlowOverride.NONE
                     and ins.getFlowType().isCall()):
                ins.setFlowOverride(FlowOverride.NONE)
                stats["flow_cleared"] += 1
            last = ins.getMaxAddress()
            x = last.next()
            continue
        if fm.getFunctionAt(x) is not None and x != lo:
            break  # never disassemble over another function's entry (caller clips first)
        d = listing.getDefinedDataContaining(x)
        if d is not None:
            try:
                stats["cleared"].append("%s@%s" % (d.getDataType().getName(), d.getAddress()))
                listing.clearCodeUnits(d.getAddress(), d.getMaxAddress(), False)
            except Exception as e:
                stats["errors"].append("clear:%s" % first_line(e))
        try:
            cmd = DisassembleCommand(AddressSet(x, x), restricted, True)
            cmd.applyTo(currentProgram, monitor)
        except Exception as e:
            stats["errors"].append("disasm@%s:%s" % (x, first_line(e)))
        ins = listing.getInstructionAt(x)
        if ins is None:
            # never slide byte by byte (that would make off-cut garbage): resume at the next
            # instruction that already exists in the range, else stop here
            nxt = listing.getInstructionAfter(x)
            if nxt is None or nxt.getAddress().compareTo(hi) > 0:
                stats["undisassembled"] += int(hi.subtract(x)) + 1
                break
            stats["undisassembled"] += int(nxt.getAddress().subtract(x))
            x = nxt.getAddress()
        else:
            stats["disassembled"] += 1
    return last


def fix_one(row, fn_name_before):
    """Grow one function's body to [address, doc_end - 1]. Mutates row; returns changed body."""
    fm = currentProgram.getFunctionManager()
    listing = currentProgram.getListing()
    a = toAddr(row["_off"])
    end_off = row["_end"] - 1
    f = fm.getFunctionAt(a)
    if f is None:
        try:
            if listing.getInstructionAt(a) is None:
                DisassembleCommand(AddressSet(a, a), None, True).applyTo(currentProgram, monitor)
            CreateFunctionCmd(None, a, None, SourceType.DEFAULT).applyTo(currentProgram, monitor)
        except Exception as e:
            add_detail(row, "recreate-error:%s" % first_line(e))
        f = fm.getFunctionAt(a)
        if f is None:
            row["status"] = "failed"
            add_detail(row, "no-function-at-entry")
            return None
        add_detail(row, "recreated")
        if fn_name_before and f.getName().startswith("FUN_") and not fn_name_before.startswith("FUN_"):
            try:
                f.setName(fn_name_before, SourceType.IMPORTED)
            except Exception as e:
                add_detail(row, "rename-error:%s" % first_line(e))
    if not row["old_size"]:
        row["old_size"] = str(f.getBody().getNumAddresses())
    # clip at the next function entry strictly inside (address, end]
    clipped = []
    it = fm.getFunctions(a.next(), True)
    if it.hasNext():
        g = it.next()
        if g.getEntryPoint().getOffset() <= end_off:
            clipped.append("entry:%s@%s" % (g.getName(), g.getEntryPoint()))
            end_off = g.getEntryPoint().getOffset() - 1
    end = toAddr(end_off)
    stats = {"ft_cleared": 0, "flow_cleared": 0, "cleared": [], "errors": [], "disassembled": 0,
             "undisassembled": 0}
    last = walk_disassemble(a, end, stats)
    # an instruction straddling the documented end belongs to this function
    if last is not None and last.compareTo(end) > 0:
        nxt = fm.getFunctionContaining(last)
        if nxt is None or nxt.getEntryPoint() == a:
            end = last
    want = AddressSet(a, end)
    others = other_functions_overlapping(want, a)
    for g in others:
        clipped.append("body:%s@%s" % (g.getName(), g.getEntryPoint()))
        want = want.subtract(g.getBody())
    new_body = body_set(f).union(want)
    if stats["cleared"]:
        add_detail(row, "cleared-data:%s" % "|".join(stats["cleared"][:5]))
    if stats["ft_cleared"] or stats["flow_cleared"]:
        add_detail(row, "overrides-cleared:%d" % (stats["ft_cleared"] + stats["flow_cleared"]))
    if stats["undisassembled"]:
        add_detail(row, "undisassembled-bytes:%d" % stats["undisassembled"])
    if stats["errors"]:
        add_detail(row, "errors:%s" % "|".join(stats["errors"][:3]))
    if clipped:
        add_detail(row, "clipped-at:%s" % "|".join(clipped[:3]))
    if body_set(f).contains(want) and not row["status"]:
        row["status"] = "skipped"
        add_detail(row, "already-covers")
        row["new_size"] = str(f.getBody().getNumAddresses())
        return None
    try:
        f.setBody(new_body)
    except Exception as e:
        row["status"] = "failed"
        add_detail(row, "setBody:%s" % first_line(e))
        return None
    row["new_size"] = str(f.getBody().getNumAddresses())
    if not row["status"] or row["status"] == "skipped":
        row["status"] = "clipped" if clipped else "fixed"
    return f


def region_coverage(lo, hi):
    fm = currentProgram.getFunctionManager()
    reg = AddressSet(toAddr(lo), toAddr(hi))
    cov = AddressSet()
    it = fm.getFunctionsOverlapping(reg)
    while it.hasNext():
        cov.add(it.next().getBody())
    cov = cov.intersect(reg)
    return cov, reg


def looks_like_table(addr, lo, hi):
    b = raw(addr, 4)
    if len(b) < 4:
        return False
    v = b[0] | (b[1] << 8) | (b[2] << 16) | (b[3] << 24)
    return lo <= v <= hi


def prologue_ok(addr):
    b = raw(addr, 6)
    if not b or b[0] in PADDING:
        return False
    if any(b.startswith(p) for p in PROLOGUES):
        return True
    return b[0] in PROLOGUE_FIRST


def preceded_by_stop(addr):
    """True if the code before addr cannot fall into it (padding, RET, JMP, or nothing)."""
    listing = currentProgram.getListing()
    p = addr.previous()
    if p is None:
        return True
    b = raw(p, 1)
    if b and b[0] in PADDING:
        return True
    ins = listing.getInstructionContaining(p)
    if ins is not None:
        return not ins.hasFallthrough()
    b3 = raw(addr.subtract(3), 3)
    return bool(b) and (b[0] == 0xC3 or (len(b3) == 3 and b3[0] == 0xC2))


def ref_reason(addr, lo, hi, prev_fn_entry):
    rm = currentProgram.getReferenceManager()
    fm = currentProgram.getFunctionManager()
    listing = currentProgram.getListing()
    for ref in rm.getReferencesTo(addr):
        rt = ref.getReferenceType()
        src = ref.getFromAddress()
        if rt.isCall():
            return "call-from:%s" % src
        if rt.isData() and listing.getInstructionAt(src) is None:
            return "pointer-from:%s" % src
        if rt.isJump() and lo <= src.getOffset() <= hi:
            g = fm.getFunctionContaining(src)
            if g is not None and g.getEntryPoint() != prev_fn_entry:
                return "jump-from:%s" % src
    return ""


def scan_uncovered(lo, hi, tag, rows):
    fm = currentProgram.getFunctionManager()
    listing = currentProgram.getListing()
    cov, reg = region_coverage(lo, hi)
    gaps = reg.subtract(cov)
    created = []
    runs = 0
    for rng in list(gaps.getAddressRanges()):
        s, e = rng.getMinAddress(), rng.getMaxAddress()
        b = raw(s, int(rng.getLength()))
        if b and all(x in PADDING for x in b):
            continue
        runs += 1
        prev = fm.getFunctionContaining(s.previous()) if s.previous() is not None else None
        prev_entry = prev.getEntryPoint() if prev is not None else None
        cands = [s]
        o = (s.getOffset() + 15) & ~15
        while o <= e.getOffset():
            if o != s.getOffset():
                cands.append(toAddr(o))
            o += 16
        for c in cands:
            if fm.getFunctionContaining(c) is not None:
                continue  # covered by a function created earlier in this scan
            off = c.getOffset()
            # skip leading padding of the run
            while off <= e.getOffset() and raw(toAddr(off), 1) and raw(toAddr(off), 1)[0] in PADDING:
                off += 1
            if off > e.getOffset():
                continue
            c = toAddr(off)
            if fm.getFunctionContaining(c) is not None or looks_like_table(c, lo, hi):
                continue
            why = ref_reason(c, lo, hi, prev_entry)
            if why and off % 16 != 0:
                why = ""  # all 2,085 mission entries are 16-aligned; others are case labels/tails
            if not why:
                if off % 16 == 0 and prologue_ok(c) and preceded_by_stop(c):
                    why = "prologue:%s" % raw(c, 6).hex()
            if not why:
                continue
            if listing.getDefinedDataContaining(c) is not None:
                if not why.startswith("prologue") and not why.startswith("call"):
                    continue
                d = listing.getDefinedDataContaining(c)
                try:
                    listing.clearCodeUnits(d.getAddress(), d.getMaxAddress(), False)
                except Exception:
                    continue
            row = {"address": "0x%08x" % off, "name": "", "reason": "uncovered-run", "status": "",
                   "old_size": "0", "new_size": "", "doc_end": "", "detail": why}
            try:
                if listing.getInstructionAt(c) is None:
                    DisassembleCommand(AddressSet(c, c), None, True).applyTo(currentProgram, monitor)
                if listing.getInstructionAt(c) is None:
                    row["status"] = "failed"
                    add_detail(row, "no-instruction")
                else:
                    CreateFunctionCmd(None, c, None, SourceType.DEFAULT).applyTo(currentProgram, monitor)
            except Exception as ex:
                add_detail(row, "create-error:%s" % first_line(ex))
            f = fm.getFunctionAt(c)
            if f is None:
                row["status"] = "failed"
                add_detail(row, "create-failed")
            else:
                row["status"] = "created"
                row["name"] = f.getName()
                row["new_size"] = str(f.getBody().getNumAddresses())
                f.addTag(tag)
                try:
                    listing.setComment(c, CommentType.PLATE,
                                       "%s created by FixFunctionBodies.py (uncovered-run: %s)"
                                       % (MARK, why))
                except Exception:
                    pass
                created.append(f)
            rows.append(row)
            prev_entry = c
    log("uncovered scan: %d non-padding runs, %d functions created" % (runs, len(created)))
    return created


def set_analyzers(names, value):
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
    return prev


def take_snapshot():
    snap = {}
    for fn in currentProgram.getFunctionManager().getFunctions(True):
        if fn.isExternal():
            continue
        snap[fn.getEntryPoint().getOffset()] = (fn.getName(), fn.getSymbol().getSource(),
                                                body_set(fn))
    return snap


def restore_snapshot(snap):
    """Re-create deleted functions and re-grow shrunk bodies to their snapshot bodies."""
    fm = currentProgram.getFunctionManager()
    listing = currentProgram.getListing()
    st = {"recreated": 0, "recreate_failed": 0, "regrown": 0, "regrow_partial": 0,
          "regrow_failed": 0}
    for off in sorted(snap):
        name, src, body = snap[off]
        a = toAddr(off)
        f = fm.getFunctionAt(a)
        if f is None:
            try:
                if listing.getInstructionAt(a) is None:
                    DisassembleCommand(AddressSet(a, a), None, True).applyTo(currentProgram, monitor)
                CreateFunctionCmd(None, a, None, SourceType.DEFAULT).applyTo(currentProgram, monitor)
            except Exception:
                pass
            f = fm.getFunctionAt(a)
            if f is None:
                st["recreate_failed"] += 1
                log("restore: could not re-create %s@0x%08x" % (name, off))
                continue
            st["recreated"] += 1
            if f.getName() != name and src != SourceType.DEFAULT:
                try:
                    f.setName(name, src)
                except Exception as e:
                    log("restore: rename %s failed: %s" % (name, first_line(e)))
            log("restore: re-created %s@0x%08x" % (f.getName(), off))
        if body_set(f).contains(body):
            continue
        stats = {"ft_cleared": 0, "flow_cleared": 0, "cleared": [], "errors": [],
                 "disassembled": 0, "undisassembled": 0}
        for rng in list(body.getAddressRanges()):
            walk_disassemble(rng.getMinAddress(), rng.getMaxAddress(), stats)
        want = AddressSet(body)
        for g in other_functions_overlapping(want, a):
            want = want.subtract(g.getBody())
        try:
            f.setBody(body_set(f).union(want))
        except Exception as e:
            st["regrow_failed"] += 1
            log("restore: setBody %s@0x%08x failed: %s" % (name, off, first_line(e)))
            continue
        if body_set(f).contains(body):
            st["regrown"] += 1
        else:
            st["regrow_partial"] += 1
            log("restore: %s@0x%08x regrown only partly (%d of %d bytes)"
                % (name, off, body.intersect(f.getBody()).getNumAddresses(), body.getNumAddresses()))
    return st


def noreturn_set():
    return set(fn.getEntryPoint().getOffset()
               for fn in currentProgram.getFunctionManager().getFunctions(True) if fn.hasNoReturn())


def has_ret(fn):
    for ins in currentProgram.getListing().getInstructions(fn.getBody(), True):
        if ins.getMnemonicString().upper().startswith("RET"):
            return True
    return False


def main():
    args = parse_args()
    path = args.get("csv")
    if not path or not os.path.isfile(path):
        log("body-fix CSV not found, skipping: %s" % path)
        return
    report = args.get("report") or (re.sub(r"\.csv$", "", path) + ".report.csv")
    lo, hi = 0x5d6fe0, 0x670a0f
    rg = args.get("region", "")
    if rg and "-" in rg:
        a, b = rg.split("-", 1)
        if parse_int(a) is not None and parse_int(b) is not None:
            lo, hi = parse_int(a), parse_int(b)
    scan = args.get("scan", "1") in ("1", "true", "yes")
    analyze = args.get("analyze", "1") in ("1", "true", "yes")
    tag = args.get("tag", "R3_bodyfix")
    disable = parse_list(args.get("disable",
                                  "Non-Returning Functions - Discovered,WindowsResourceReference"))
    fm = currentProgram.getFunctionManager()
    before = fm.getFunctionCount()
    cov0, reg = region_coverage(lo, hi)
    log("functions-before=%d region 0x%08x-0x%08x covered=%d of %d"
        % (before, lo, hi, cov0.getNumAddresses(), reg.getNumAddresses()))

    rows = []
    with open(path, "r", encoding="utf-8") as fh:
        for i, r in enumerate(csv.DictReader(fh)):
            row = {"address": str(r.get("address") or ""), "name": str(r.get("name") or ""),
                   "reason": "body-fix", "status": "", "old_size": "", "new_size": "",
                   "doc_end": str(r.get("doc_end") or ""), "detail": ""}
            rows.append(row)
            off, end = parse_int(r.get("address")), parse_int(r.get("doc_end"))
            if off is None or end is None or end <= off:
                row["status"], row["detail"] = "failed", "bad-row #%d" % i
                continue
            row["_off"], row["_end"] = off, end
    body_rows = [r for r in rows if "_off" in r]
    body_rows.sort(key=lambda r: r["_off"])
    names = {}
    for r in body_rows:
        f = fm.getFunctionAt(toAddr(r["_off"]))
        names[r["_off"]] = f.getName() if f is not None else r["name"]
    log("rows=%d valid=%d" % (len(rows), len(body_rows)))

    # 1. grow every body
    changed = []
    for n, r in enumerate(body_rows):
        try:
            f = fix_one(r, names.get(r["_off"]))
        except Exception as e:
            r["status"] = "failed"
            add_detail(r, "exception:%s" % first_line(e))
            f = None
        if f is not None:
            f.addTag(tag)
            changed.append(f.getEntryPoint())
        if n % 200 == 199:
            log("... %d rows done" % (n + 1))
    log("body pass: %s" % summarise(body_rows))

    # 2. uncovered runs
    new_funcs = scan_uncovered(lo, hi, "R3_uncovered", rows) if scan else []

    # 3. analysis restricted to the changed code, then re-assert
    nr_before = noreturn_set()
    if analyze and (changed or new_funcs):
        bodies = AddressSet()
        for ep in changed:
            g = fm.getFunctionAt(ep)
            if g is not None:
                bodies.add(g.getBody())
        for g in new_funcs:
            if fm.getFunctionAt(g.getEntryPoint()) is not None:
                bodies.add(g.getBody())
        snapshot = take_snapshot()
        log("snapshot of %d function bodies taken" % len(snapshot))
        prev = set_analyzers(disable, "false")
        try:
            mgr = AutoAnalysisManager.getAnalysisManager(currentProgram)
            mgr.initializeOptions()
            mgr.reAnalyzeAll(bodies)
            log("analysing %d bytes ..." % bodies.getNumAddresses())
            mgr.startAnalysis(monitor)
            mgr.waitForAnalysis(None, monitor)
            log("analysis done")
        except Exception as e:
            log("analysis raised %s" % first_line(e))
        for n_, v in prev.items():
            setAnalysisOption(currentProgram, n_, v)
        flipped = [o for o in noreturn_set() - nr_before
                   if fm.getFunctionAt(toAddr(o)) is not None and has_ret(fm.getFunctionAt(toAddr(o)))]
        for o in flipped:
            g = fm.getFunctionAt(toAddr(o))
            g.setNoReturn(False)
            log("cleared wrong no-return on %s@0x%08x" % (g.getName(), o))
        # restore: "Non-Returning Functions - Discovered" runs even when switched off (it threw a
        # ConcurrentModificationException and still re-flagged kasGrowSelectionPtr/kasfShipsCount
        # in the 2026-10-09 stand-in run), and its ClearFlowAndRepair then cut 14 mission bodies
        # outside the CSV and deleted one function. Every function's pre-analysis body is put back.
        rs = restore_snapshot(snapshot)
        log("snapshot restore: %s" % ", ".join("%s=%s" % kv for kv in sorted(rs.items())))
        # re-assert the CSV rows (normally a no-op after the restore)
        reasserted = 0
        for r in body_rows:
            if r["status"] not in ("fixed", "clipped"):
                continue
            g = fm.getFunctionAt(toAddr(r["_off"]))
            want = AddressSet(toAddr(r["_off"]), toAddr(r["_end"] - 1))
            if g is not None and body_set(g).contains(want):
                r["new_size"] = str(g.getBody().getNumAddresses())
                continue
            st = r["status"]
            r["status"] = ""
            try:
                f = fix_one(r, names.get(r["_off"]))
            except Exception as e:
                f = None
                add_detail(r, "reassert-exception:%s" % first_line(e))
            if r["status"] in ("skipped", ""):
                r["status"] = st
            add_detail(r, "reasserted")
            reasserted += 1
        log("re-asserted %d bodies after analysis" % reasserted)
    log("final: %s" % summarise(body_rows))

    cov1, _ = region_coverage(lo, hi)
    after = fm.getFunctionCount()
    log("functions before=%d after=%d; region covered before=%d after=%d of %d (%.1f%%)"
        % (before, after, cov0.getNumAddresses(), cov1.getNumAddresses(), reg.getNumAddresses(),
           100.0 * cov1.getNumAddresses() / max(1, reg.getNumAddresses())))
    for r in rows:
        if r["status"] == "failed":
            log("FAILED %s %s: %s" % (r["address"], r["name"], r["detail"]))
    try:
        d = os.path.dirname(report)
        if d:
            os.makedirs(d, exist_ok=True)
        with open(report, "w", newline="", encoding="utf-8") as fh:
            w = csv.DictWriter(fh, fieldnames=["address", "name", "reason", "status", "old_size",
                                               "new_size", "doc_end", "detail"])
            w.writeheader()
            for r in rows:
                w.writerow({k: r.get(k, "") for k in w.fieldnames})
        log("report: %s" % report)
    except Exception as e:
        log("could not write report %s: %s" % (report, first_line(e)))


def summarise(rows):
    tot = {}
    for r in rows:
        tot[r["status"] or "?"] = tot.get(r["status"] or "?", 0) + 1
    return ", ".join("%s:%d" % kv for kv in sorted(tot.items()))


main()
