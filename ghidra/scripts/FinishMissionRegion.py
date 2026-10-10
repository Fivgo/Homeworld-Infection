# Finish the mission-code region 0x5d6fe0-0x670a0f after round 3 (docs/binary-map.md R3.6 item 6):
#   1. type the inline switch tables as pointer[N] / byte[N] arrays and give every jump-table
#      dispatch its COMPUTED_JUMP references;
#   2. repair off-cut code: walk every mission function from its entry along verified flows; where
#      a reachable address is not an instruction start (undisassembled bytes, or bytes taken by an
#      instruction that starts 1-3 bytes off the real stream), clear the off-cut instructions and
#      re-disassemble the real stream until it meets existing code again;
#   3. check the call sites whose CALL reference was missing in round 3 (calls=);
#   4. auto-analysis restricted to the changed code plus the functions listed in created= (the
#      CreateFunctions.py report of the same pass), inside FixFunctionBodies.py's snapshot guard
#      (docs/ghidra-round3-prep.md: "Non-Returning Functions - Discovered" runs even when switched
#      off): every function body is snapshotted first; afterwards newly no-return functions that
#      hold a RET get the flag cleared, deleted functions are re-created, shrunk bodies re-grown,
#      and the off-cut repair runs once more.
#
# Why the 4 CALL references were missing (2026-10-09 inspection of the r3 program): the call
# instruction did not exist. At each site the bytes of the real instruction stream were covered by
# an older instruction that starts 1-3 bytes off the stream (e.g. Watch_Mission13_Resourcers:
# real `push 0x6a96934` at 0x00626543 and `call kasJump` at 0x00626548, but an off-cut
# `TEST EAX,0xa0f3e806` sat at 0x00626546). FixFunctionBodies.py's walk cannot disassemble over
# an existing instruction and never slides byte by byte, so it skipped to the off-cut one and left
# 1-4 bytes undisassembled: these are the 56 "undisassembled-bytes" of fix_bodies_r3.report.csv.
#
# Switch tables are found from the code, not from a list: every `jmp dword [reg*4 + T]` and
# `mov r8, byte [reg + T]` whose T lies in the region outside every function body. Lengths:
#   direct table   `cmp reg, N; ja default; jmp [reg*4 + T]`                     -> pointer[N+1]
#   index table    `cmp reg, N; ja; xor edx,edx; mov dl,[reg + I]; jmp [edx*4+T]` -> byte[N+1] at I,
#                  pointer[max(I[0..N]) + 1] at T
# A table whose pointers leave the dispatching function, that overlaps another table, or that
# holds an instruction is reported and left alone.
#
# Args (key=value, any order):
#   report=<csv>     per-item CSV (default <cwd>/ghidra/finish_missions_r4.report.csv);
#                    columns kind,address,name,status,before,after,detail
#   region=<lo>-<hi> default 0x5d6fe0-0x670a0f
#   created=<csv>    CreateFunctions.py report; rows with status "created" join the analysis set
#   calls=<owner>:<target>,...  call sites to check (default: the 4 of docs/ghidra-round3-prep.md)
#   tables=1|0  repair=1|0  analyze=1|0   (default 1 each)
#   disable=<names>  analyzers switched off during the analysis (default as FixFunctionBodies.py)
#   tag=R4_finish    function tag on every function whose code this script changed
#
# Tolerant: bad items are reported and skipped; the script never aborts the headless run.
#
# @category Homeworld-Infection
# @runtime PyGhidra

import typing
if typing.TYPE_CHECKING:
    from ghidra.ghidra_builtins import *  # noqa: F401,F403

import csv
import os
import re
import struct
import time

from ghidra.app.cmd.disassemble import DisassembleCommand
from ghidra.app.cmd.function import CreateFunctionCmd
from ghidra.app.plugin.core.analysis import AutoAnalysisManager
from ghidra.app.util import PseudoDisassembler
from ghidra.program.model.address import AddressSet
from ghidra.program.model.data import ArrayDataType, ByteDataType, PointerDataType
from ghidra.program.model.listing import FlowOverride
from ghidra.program.model.symbol import RefType, SourceType

T0 = time.time()
DEFAULT_CALLS = ("0x00626510:0x00490640,"   # Watch_Mission13_Resourcers -> kasJump
                 "0x006430c0:0x0049a8b0,"   # Watch_Mission16_Corvettes_Dock -> kasfGetDockHost
                 "0x0064ffe0:0x00492ce0,"   # Init_Mission17_Workers -> kasfTeamCount
                 "0x0066aa30:0x00496870")   # Watch_Tutorial1_SpeechEvents_WaitMessage5 -> kasfCameraFocus
MAX_RESYNC = 64  # bytes decoded past a hole before giving up on meeting existing code
FIELDS = ["kind", "address", "name", "status", "before", "after", "detail"]


def log(msg):
    println("[FinishMissionRegion +%.1fs] %s" % (time.time() - T0, msg))


def parse_args():
    args = {}
    for a in list(getScriptArgs()):
        a = str(a)
        if "=" in a:
            k, v = a.split("=", 1)
            args[k.strip().lower()] = v.strip()
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


def fmt(d):
    out = []
    for k, v in sorted(d.items()):
        if isinstance(v, (list, tuple, set)):
            v = "%d[%s]" % (len(v), " ".join(str(x) for x in list(v)[:8]))
        out.append("%s=%s" % (k, v))
    return ", ".join(out)


def first_line(e):
    s = str(e)
    return (s.splitlines()[0] if s else repr(e))[:200]


def raw(addr, n):
    try:
        return bytes((x & 0xFF) for x in getBytes(addr, n))
    except Exception:
        return b""


def body_set(fn):
    s = AddressSet()
    s.add(fn.getBody())
    return s


def region_functions(lo, hi):
    fm = currentProgram.getFunctionManager()
    out = []
    for fn in fm.getFunctions(toAddr(lo), True):
        if fn.getEntryPoint().getOffset() > hi:
            break
        out.append(fn)
    return out


def region_coverage(lo, hi):
    fm = currentProgram.getFunctionManager()
    reg = AddressSet(toAddr(lo), toAddr(hi))
    cov = AddressSet()
    it = fm.getFunctionsOverlapping(reg)
    while it.hasNext():
        cov.add(it.next().getBody())
    return cov.intersect(reg), reg


def undisassembled(fn):
    """Bytes of the body covered neither by an instruction nor by defined data."""
    listing = currentProgram.getListing()
    n = 0
    for rng in fn.getBody().getAddressRanges():
        x = rng.getMinAddress()
        end = rng.getMaxAddress()
        while x is not None and x.compareTo(end) <= 0:
            cu = listing.getCodeUnitContaining(x)
            if cu is None:
                x = x.next()
                continue
            if listing.getInstructionContaining(x) is None and \
                    listing.getDefinedDataContaining(x) is None:
                n += 1
                x = x.next()
                continue
            x = cu.getMaxAddress().next()
    return n


# ----------------------------------------------------------------------------------------------
# 1. switch tables

def find_tables(lo, hi, rows):
    """Raw-byte scan of the region for jump-table dispatches whose table lies outside every body."""
    fm = currentProgram.getFunctionManager()
    cov, reg = region_coverage(lo, hi)
    blob = raw(toAddr(lo), hi - lo + 1)
    found = []

    def outside(t):
        return lo <= t <= hi and not cov.contains(toAddr(t))

    for m in re.finditer(rb"\xff\x24[\x85\x8d\x95\x9d\xad\xb5\xbd](....)", blob, re.S):
        t = struct.unpack("<I", m.group(1))[0]
        if outside(t):
            found.append({"table": t, "kind": "ptr", "site": lo + m.start(),
                          "reg": (blob[m.start() + 2] >> 3) & 7})
    for m in re.finditer(rb"\x8a([\x80-\xbf])(....)", blob, re.S):
        if (m.group(1)[0] & 7) == 4:
            continue
        t = struct.unpack("<I", m.group(2))[0]
        if outside(t):
            found.append({"table": t, "kind": "byte", "site": lo + m.start(),
                          "reg": m.group(1)[0] & 7})
    # the dispatch must be a real instruction of a function (not bytes inside data)
    listing = currentProgram.getListing()
    keep = []
    for t in found:
        f = fm.getFunctionContaining(toAddr(t["site"]))
        ins = listing.getInstructionAt(toAddr(t["site"]))
        if f is None:
            rows.append({"kind": "switch-table", "address": "0x%08x" % t["table"], "name": "",
                         "status": "skipped", "detail": "dispatch 0x%08x outside any function"
                         % t["site"]})
            continue
        if ins is None:
            # an off-cut instruction may hide it; the repair pass runs after the tables, so accept
            # the raw pattern when the bytes sit inside the function body
            t["detail_pre"] = "dispatch-not-an-instruction-yet"
        t["fn"] = f
        keep.append(t)
    keep.sort(key=lambda x: x["table"])
    return keep


def bound_before(site, reg_bits):
    """(N, address) of `cmp reg, N` + `ja` within 24 bytes before site, else (None, None)."""
    for back in range(2, 24):
        a = site - back
        b = raw(toAddr(a), 12)
        if len(b) < 12:
            continue
        if b[0] == 0x83 and b[1] == (0xF8 | reg_bits) and (b[3] == 0x77 or b[3:5] == b"\x0f\x87"):
            return b[2], a
        if b[0] == 0x81 and b[1] == (0xF8 | reg_bits) and (b[6] == 0x77 or b[6:8] == b"\x0f\x87"):
            return struct.unpack_from("<I", b, 2)[0], a
        if reg_bits == 0 and b[0] == 0x3D and (b[5] == 0x77 or b[5:7] == b"\x0f\x87"):
            return struct.unpack_from("<I", b, 1)[0], a
    return None, None


def size_tables(tables):
    for t in tables:
        if t["kind"] == "ptr":
            idx = [x for x in tables if x["kind"] == "byte" and 0 < t["site"] - x["site"] <= 8]
            if idx:
                t["indexed_by"] = idx[0]
                continue
        n, at = bound_before(t["site"], t["reg"])
        t["count"] = n + 1 if n is not None and n < 4096 else None
        t["bound"] = at
    for t in tables:
        if "indexed_by" in t:
            bt = t["indexed_by"]
            if bt.get("count"):
                t["count"] = max(raw(toAddr(bt["table"]), bt["count"])) + 1
                t["bound"] = bt["bound"]
            else:
                t["count"] = None
                t["bound"] = None


def type_tables(tables, rows, changed, targets_by_fn):
    listing = currentProgram.getListing()
    rm = currentProgram.getReferenceManager()
    st = currentProgram.getSymbolTable()
    fm = currentProgram.getFunctionManager()
    taken = {}
    stats = {"typed": 0, "already": 0, "failed": 0, "refs_added": 0, "ptr": 0, "byte": 0,
             "entries": 0}
    for t in tables:
        a = toAddr(t["table"])
        row = {"kind": "switch-table", "address": "0x%08x" % t["table"],
               "name": "switchdataD_%08x" % t["table"], "status": "", "before": "", "after": "",
               "detail": "%s dispatch@0x%08x in %s@%s" % (t["kind"], t["site"], t["fn"].getName(),
                                                          t["fn"].getEntryPoint())}
        rows.append(row)
        if t.get("detail_pre"):
            row["detail"] += ";" + t["detail_pre"]
        n = t.get("count")
        if not n:
            row["status"] = "failed"
            row["detail"] += ";no-bound-check-found"
            stats["failed"] += 1
            continue
        esz = 4 if t["kind"] == "ptr" else 1
        size = n * esz
        end = a.add(size - 1)
        problems = []
        for o in range(t["table"], t["table"] + size):
            if o in taken:
                problems.append("overlaps-table@0x%08x" % taken[o])
                break
        if listing.getInstructions(AddressSet(a, end), True).hasNext():
            problems.append("instruction-inside")
        if fm.getFunctionsOverlapping(AddressSet(a, end)).hasNext():
            problems.append("inside-a-function-body")
        entries = []
        if t["kind"] == "ptr":
            b = raw(a, size)
            entries = [struct.unpack_from("<I", b, 4 * i)[0] for i in range(n)]
            for v in entries:
                g = fm.getFunctionContaining(toAddr(v)) if v else None
                if g is None or g.getEntryPoint() != t["fn"].getEntryPoint():
                    problems.append("entry-0x%08x-outside-%s" % (v, t["fn"].getName()))
                    break
        if problems:
            row["status"] = "failed"
            row["detail"] += ";" + "|".join(problems)
            stats["failed"] += 1
            continue
        d0 = listing.getDataAt(a)
        row["before"] = d0.getDataType().getName() if d0 is not None and d0.isDefined() else "undefined"
        dt = ArrayDataType(PointerDataType() if esz == 4 else ByteDataType(), n, esz)
        if d0 is not None and d0.isDefined() and d0.getDataType().isEquivalent(dt):
            row["status"] = "already"
            stats["already"] += 1
        else:
            try:
                listing.clearCodeUnits(a, end, False)
                listing.createData(a, dt)
                row["status"] = "typed"
                stats["typed"] += 1
            except Exception as e:
                row["status"] = "failed"
                row["detail"] += ";createData:%s" % first_line(e)
                stats["failed"] += 1
                continue
        stats[t["kind"]] += 1
        for o in range(t["table"], t["table"] + size):
            taken[o] = t["table"]
        d1 = listing.getDataAt(a)
        row["after"] = d1.getDataType().getName() if d1 is not None else ""
        row["detail"] += ";count=%d;bound@%s" % (n, "0x%08x" % t["bound"] if t.get("bound") else "?")
        if t.get("indexed_by"):
            row["detail"] += ";indexed-by=0x%08x" % t["indexed_by"]["table"]
        name = "switchdataD_%08x" % t["table"]
        try:
            if not any(s.getName() == name for s in st.getSymbols(a)):
                st.createLabel(a, name, SourceType.ANALYSIS)
        except Exception as e:
            row["detail"] += ";label:%s" % first_line(e)
        site = toAddr(t["site"])
        # DATA reference from the dispatch to the table, COMPUTED_JUMP to every distinct target
        have = set((r.getToAddress().getOffset(), str(r.getReferenceType()))
                   for r in rm.getReferencesFrom(site))
        added = 0
        try:
            if (t["table"], "DATA") not in have and (t["table"], "READ") not in have:
                rm.addMemoryReference(site, a, RefType.DATA, SourceType.ANALYSIS, 0)
            if t["kind"] == "ptr":
                stats["entries"] += n
                for v in sorted(set(entries)):
                    if (v, "COMPUTED_JUMP") not in have:
                        rm.addMemoryReference(site, toAddr(v), RefType.COMPUTED_JUMP,
                                              SourceType.ANALYSIS, 0)
                        added += 1
                targets_by_fn.setdefault(t["fn"].getEntryPoint().getOffset(), set()).update(entries)
        except Exception as e:
            row["detail"] += ";refs:%s" % first_line(e)
        if added:
            row["detail"] += ";computed-jump-refs-added=%d" % added
            stats["refs_added"] += added
            changed.add(t["fn"].getEntryPoint().getOffset())
    return stats


# ----------------------------------------------------------------------------------------------
# 2. off-cut repair

def flow_succ(ins, extra):
    """Successor addresses of a reachable instruction (fall-through + jump flows, not calls)."""
    out = []
    ft = ins.getFallThrough()
    ftype = ins.getFlowType()
    is_call_insn = ins.getMnemonicString().upper().startswith("CALL")
    if ft is not None:
        out.append(ft)
    elif not is_call_insn:
        pass  # RET, JMP, and tail-call JMPs (CALL_RETURN override): nothing falls through
    elif ftype.isCall() and not ftype.isTerminal():
        out.append(ins.getMaxAddress().next())
    elif ftype.isCall():
        # CALL_TERMINATOR: follow only when no callee is no-return (stale flag left by A1)
        callees = [r.getToAddress() for r in ins.getReferencesFrom() if r.getReferenceType().isCall()]
        fm = currentProgram.getFunctionManager()
        if callees and all(fm.getFunctionAt(c) is not None and not fm.getFunctionAt(c).hasNoReturn()
                           for c in callees):
            out.append(ins.getMaxAddress().next())
    if ftype.isJump():
        for f in ins.getFlows():
            out.append(f)
        for r in ins.getReferencesFrom():
            if r.getReferenceType().isJump():
                out.append(r.getToAddress())
    return out


def resync(a, limit_hi):
    """Decode the real stream from a until it meets an existing instruction start.
    Returns (end_exclusive_offset, n_instructions) or (None, why)."""
    listing = currentProgram.getListing()
    pd = PseudoDisassembler(currentProgram)
    x = a
    n = 0
    while True:
        if x != a and listing.getInstructionAt(x) is not None:
            return x.getOffset(), n
        if x.getOffset() > limit_hi:
            return x.getOffset(), n
        if x.subtract(a) > MAX_RESYNC:
            return None, "no-resync-within-%d" % MAX_RESYNC
        try:
            pi = pd.disassemble(x)
        except Exception as e:
            return None, "bad-decode@%s:%s" % (x, first_line(e))
        if pi is None:
            return None, "bad-decode@%s" % x
        n += 1
        x = x.add(pi.getLength())


def fix_hole(fn, a, limit_hi, detail):
    """Clear the off-cut code over the real stream starting at a, re-disassemble it."""
    listing = currentProgram.getListing()
    fm = currentProgram.getFunctionManager()
    rm = currentProgram.getReferenceManager()
    end_x, n = resync(a, limit_hi)
    if end_x is None:
        detail.append("hole@%s:%s" % (a, n))
        return False
    lo_c, hi_c = a, toAddr(end_x - 1)
    offcut = []
    it = listing.getCodeUnits(AddressSet(a, hi_c), True)
    first = listing.getCodeUnitContaining(a)
    units = []
    if first is not None and first.getAddress().compareTo(a) < 0:
        units.append(first)
    while it.hasNext():
        units.append(it.next())
    for cu in units:
        if cu.getMinAddress().compareTo(lo_c) < 0:
            lo_c = cu.getMinAddress()
        if cu.getMaxAddress().compareTo(hi_c) > 0:
            hi_c = cu.getMaxAddress()
        if listing.getInstructionAt(cu.getMinAddress()) is not None:
            src = ["%s<-%s" % (r.getReferenceType(), r.getFromAddress())
                   for r in rm.getReferencesTo(cu.getMinAddress())][:2]
            offcut.append("%s'%s'%s" % (cu.getMinAddress(), cu, ("refs:" + ",".join(src)) if src else ""))
    for x in (lo_c,):
        g = fm.getFunctionAt(x)
        if g is not None and x != a:
            detail.append("hole@%s:function-entry-%s-in-the-way" % (a, g.getName()))
            return False
    it2 = fm.getFunctions(lo_c.next(), True)
    if it2.hasNext():
        g = it2.next()
        if g.getEntryPoint().compareTo(hi_c) <= 0:
            detail.append("hole@%s:function-entry-%s-in-the-way" % (a, g.getName()))
            return False
    try:
        listing.clearCodeUnits(lo_c, hi_c, False)
        DisassembleCommand(AddressSet(a, a), AddressSet(lo_c, hi_c), True).applyTo(currentProgram,
                                                                                   monitor)
    except Exception as e:
        detail.append("hole@%s:disasm:%s" % (a, first_line(e)))
        return False
    ok = listing.getInstructionAt(a) is not None
    detail.append("%s@%s:%dB/%dins%s" % ("fixed" if ok else "unfixed", a, end_x - a.getOffset(), n,
                                          (";offcut=" + "|".join(offcut[:3])) if offcut else ""))
    return ok


def repair_function(fn, extra_targets, stats):
    """Walk fn from its entry; repair every reachable address that is not an instruction start.
    Returns a detail list (empty when nothing was wrong)."""
    listing = currentProgram.getListing()
    body = body_set(fn)
    hi = body.getMaxAddress().getOffset()
    detail = []
    for rnd in range(6):
        seen = set()
        work = [fn.getEntryPoint()]
        for v in sorted(extra_targets or ()):
            work.append(toAddr(v))
        fixes = 0
        unresolved = 0
        while work:
            a = work.pop()
            o = a.getOffset()
            if o in seen:
                continue
            seen.add(o)
            if not body.contains(a):
                continue  # leaves the body (tail call, region boundary): not this script's business
            ins = listing.getInstructionAt(a)
            if ins is None:
                if fix_hole(fn, a, hi, detail):
                    fixes += 1
                    ins = listing.getInstructionAt(a)
                else:
                    unresolved += 1
                    continue
            if ins.isFallThroughOverridden():
                ins.clearFallThroughOverride()
                stats["ft_cleared"] += 1
            # only on CALL instructions: the 41 CALL_RETURN overrides on tail-call JMPs of the
            # region (e.g. Init_Mission03_PatrolPirates_Stop `JMP kasfStop`) are correct
            if ins.getFlowOverride() != FlowOverride.NONE and \
                    ins.getMnemonicString().upper().startswith("CALL"):
                stats["flow_cleared_at"].append("%s:%s" % (ins.getAddress(), ins.getFlowOverride()))
                ins.setFlowOverride(FlowOverride.NONE)
                stats["flow_cleared"] += 1
            for s in flow_succ(ins, extra_targets):
                if s is not None and s.getOffset() not in seen:
                    work.append(s)
        stats["holes_fixed"] += fixes
        if fixes == 0:
            stats["unresolved"] += unresolved
            break
    # grow the body over any repaired code that left it (straddling instructions)
    return detail


def trim_padding_tail(fn):
    """Drop alignment padding (0x90/0xCC) left undisassembled at the end of a body, e.g. the byte
    an off-cut instruction straddling doc_end had pulled in (Watch_Mission16_Corvettes_Dock)."""
    listing = currentProgram.getListing()
    body = body_set(fn)
    last = None
    for ins in listing.getInstructions(body, False):
        b = raw(ins.getAddress(), ins.getLength())
        if ins.getLength() == 1 and b and b[0] in (0x90, 0xCC):
            continue  # NOP/INT3 alignment disassembled at the tail
        last = ins.getMaxAddress()
        break
    if last is None:
        return 0
    tail = body.subtract(AddressSet(body.getMinAddress(), last))
    if tail.isEmpty():
        return 0
    for rng in tail.getAddressRanges():
        b = raw(rng.getMinAddress(), int(rng.getLength()))
        if not b or any(x not in (0x90, 0xCC) for x in b):
            return 0
        if listing.getDefinedDataContaining(rng.getMinAddress()) is not None:
            return 0
    try:
        fn.setBody(body.subtract(tail))
    except Exception as e:
        log("trim %s failed: %s" % (fn.getName(), first_line(e)))
        return 0
    return int(tail.getNumAddresses())


def repair_region(lo, hi, targets_by_fn, rows, changed, label):
    stats = {"holes_fixed": 0, "unresolved": 0, "ft_cleared": 0, "flow_cleared": 0,
             "functions_fixed": 0, "undis_before": 0, "undis_after": 0, "padding_trimmed": 0,
             "flow_cleared_at": []}
    for fn in region_functions(lo, hi):
        u0 = undisassembled(fn)
        h0 = stats["holes_fixed"]
        detail = repair_function(fn, targets_by_fn.get(fn.getEntryPoint().getOffset()), stats)
        if detail or u0:
            t = trim_padding_tail(fn)
            if t:
                detail.append("trimmed-padding-tail=%d" % t)
                stats["padding_trimmed"] += t
        u1 = undisassembled(fn) if (detail or u0) else 0
        stats["undis_before"] += u0
        stats["undis_after"] += u1
        if detail or u0 or u1:
            if stats["holes_fixed"] > h0:
                stats["functions_fixed"] += 1
                changed.add(fn.getEntryPoint().getOffset())
            rows.append({"kind": "offcut-repair" + label, "address": str(fn.getEntryPoint()),
                         "name": fn.getName(), "status": "fixed" if stats["holes_fixed"] > h0
                         and u1 == 0 else ("partial" if stats["holes_fixed"] > h0 else "unfixed"),
                         "before": "undisassembled=%d" % u0, "after": "undisassembled=%d" % u1,
                         "detail": ";".join(detail)[:900]})
    return stats


# ----------------------------------------------------------------------------------------------
# 3. call checks

def check_calls(specs, rows):
    listing = currentProgram.getListing()
    fm = currentProgram.getFunctionManager()
    rm = currentProgram.getReferenceManager()
    res = {"ok": 0, "ref_added": 0, "missing": 0}
    for spec in specs:
        if ":" not in spec:
            continue
        owner, tgt = parse_int(spec.split(":")[0]), parse_int(spec.split(":")[1])
        f = fm.getFunctionAt(toAddr(owner))
        t = fm.getFunctionAt(toAddr(tgt))
        row = {"kind": "call-ref", "address": "0x%08x" % owner,
               "name": "%s->%s" % (f.getName() if f else "?", t.getName() if t else "?"),
               "status": "", "before": "", "after": "", "detail": ""}
        rows.append(row)
        if f is None:
            row["status"], row["detail"] = "failed", "no-owner"
            res["missing"] += 1
            continue
        lo = f.getEntryPoint()
        n = int(body_set(f).getMaxAddress().subtract(lo)) + 1
        b = raw(lo, n)
        sites = []
        for i in range(max(0, len(b) - 4)):
            if b[i] == 0xE8:
                rel = struct.unpack_from("<i", b, i + 1)[0]
                if (owner + i + 5 + rel) & 0xFFFFFFFF == tgt:
                    sites.append(lo.add(i))
        if not sites:
            row["status"], row["detail"] = "failed", "no-e8-call-to-target-in-body"
            res["missing"] += 1
            continue
        det = []
        good = 0
        for s in sites:
            ins = listing.getInstructionAt(s)
            if ins is None:
                c = listing.getInstructionContaining(s)
                det.append("%s:no-instruction%s" % (s, (" (inside %s '%s')" % (c.getAddress(), c))
                                                     if c else ""))
                continue
            refs = [r for r in ins.getReferencesFrom()
                    if r.getReferenceType().isCall() and r.getToAddress().getOffset() == tgt]
            if not refs:
                try:
                    rm.addMemoryReference(s, toAddr(tgt), RefType.UNCONDITIONAL_CALL,
                                          SourceType.ANALYSIS, 0)
                    det.append("%s:call-ref-added" % s)
                    res["ref_added"] += 1
                    good += 1
                except Exception as e:
                    det.append("%s:addref:%s" % (s, first_line(e)))
            else:
                det.append("%s:'%s' ok" % (s, ins))
                good += 1
        row["status"] = "ok" if good == len(sites) else "failed"
        res["ok" if good == len(sites) else "missing"] += 1
        row["detail"] = ";".join(det)
    return res


# ----------------------------------------------------------------------------------------------
# 4. guarded analysis (snapshot/restore as FixFunctionBodies.py)

def walk_disassemble(lo, hi, stats):
    listing = currentProgram.getListing()
    fm = currentProgram.getFunctionManager()
    restricted = AddressSet(lo, hi)
    x = lo
    guard = 0
    while x is not None and x.compareTo(hi) <= 0 and guard < 200000:
        guard += 1
        ins = listing.getInstructionContaining(x)
        if ins is not None:
            x = ins.getMaxAddress().next()
            continue
        if fm.getFunctionAt(x) is not None and x != lo:
            break
        if listing.getDefinedDataContaining(x) is not None:
            x = listing.getDefinedDataContaining(x).getMaxAddress().next()
            continue
        try:
            DisassembleCommand(AddressSet(x, x), restricted, True).applyTo(currentProgram, monitor)
        except Exception:
            pass
        if listing.getInstructionAt(x) is None:
            nxt = listing.getInstructionAfter(x)
            if nxt is None or nxt.getAddress().compareTo(hi) > 0:
                break
            x = nxt.getAddress()


def take_snapshot():
    snap = {}
    for fn in currentProgram.getFunctionManager().getFunctions(True):
        if fn.isExternal():
            continue
        snap[fn.getEntryPoint().getOffset()] = (fn.getName(), fn.getSymbol().getSource(),
                                                body_set(fn))
    return snap


def restore_snapshot(snap):
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
        for rng in list(body.getAddressRanges()):
            walk_disassemble(rng.getMinAddress(), rng.getMaxAddress(), {})
        want = AddressSet(body)
        it = fm.getFunctionsOverlapping(want)
        while it.hasNext():
            g = it.next()
            if g.getEntryPoint() != a:
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


def set_analyzers(names, value):
    prev = {}
    cur = getCurrentAnalysisOptionsAndValues(currentProgram)
    for n in names:
        if not cur.containsKey(n):
            log("analyzer option not registered: %s" % n)
            continue
        prev[n] = str(cur.get(n))
        setAnalysisOption(currentProgram, n, value)
    return prev


def guarded_analysis(entries, disable, rows):
    fm = currentProgram.getFunctionManager()
    bodies = AddressSet()
    for o in sorted(entries):
        g = fm.getFunctionAt(toAddr(o))
        if g is not None:
            bodies.add(g.getBody())
    if bodies.isEmpty():
        log("analysis: nothing to analyse")
        return {}
    snapshot = take_snapshot()
    nr_before = noreturn_set()
    log("snapshot of %d function bodies taken; analysing %d bytes in %d functions ..."
        % (len(snapshot), bodies.getNumAddresses(), len(entries)))
    prev = set_analyzers(disable, "false")
    try:
        mgr = AutoAnalysisManager.getAnalysisManager(currentProgram)
        mgr.initializeOptions()
        mgr.reAnalyzeAll(bodies)
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
        rows.append({"kind": "guard", "address": "0x%08x" % o, "name": g.getName(),
                     "status": "noreturn-cleared", "before": "no_return=1", "after": "no_return=0",
                     "detail": "analysis flagged a function holding a RET"})
    rs = restore_snapshot(snapshot)
    new_fns = [fn for fn in fm.getFunctions(True)
               if not fn.isExternal() and fn.getEntryPoint().getOffset() not in snapshot]
    rs["new_functions_by_analysis"] = len(new_fns)
    for fn in new_fns:
        log("analysis created %s@%s (%d bytes)" % (fn.getName(), fn.getEntryPoint(),
                                                   fn.getBody().getNumAddresses()))
        rows.append({"kind": "guard", "address": str(fn.getEntryPoint()), "name": fn.getName(),
                     "status": "created-by-analysis", "before": "",
                     "after": "size=%d" % fn.getBody().getNumAddresses(), "detail": ""})
    rs["noreturn_cleared"] = len(flipped)
    rs["noreturn_new_kept"] = len(noreturn_set() - nr_before)
    gone = [o for o in snapshot if fm.getFunctionAt(toAddr(o)) is None]
    rs["still_gone"] = len(gone)
    shrunk = 0
    for o, (name, src, body) in snapshot.items():
        g = fm.getFunctionAt(toAddr(o))
        if g is not None and not body_set(g).contains(body):
            shrunk += 1
    rs["still_shrunk"] = shrunk
    log("snapshot restore: %s" % fmt(rs))
    rows.append({"kind": "guard", "address": "", "name": "snapshot-restore", "status": "done",
                 "before": "functions=%d" % len(snapshot), "after": "functions=%d" % fm.getFunctionCount(),
                 "detail": fmt(rs)})
    return rs


# ----------------------------------------------------------------------------------------------

def main():
    args = parse_args()
    report = args.get("report") or os.path.join(os.getcwd(), "ghidra", "finish_missions_r4.report.csv")
    lo, hi = 0x5d6fe0, 0x670a0f
    rg = args.get("region", "")
    if rg and "-" in rg:
        a, b = rg.split("-", 1)
        if parse_int(a) is not None and parse_int(b) is not None:
            lo, hi = parse_int(a), parse_int(b)
    on = lambda k: args.get(k, "1") in ("1", "true", "yes")  # noqa: E731
    tag = args.get("tag", "R4_finish")
    disable = parse_list(args.get("disable",
                                  "Non-Returning Functions - Discovered,WindowsResourceReference"))
    fm = currentProgram.getFunctionManager()
    rows = []
    changed = set()
    targets_by_fn = {}
    before = fm.getFunctionCount()
    cov0, reg = region_coverage(lo, hi)
    log("functions-before=%d region 0x%08x-0x%08x covered=%d of %d"
        % (before, lo, hi, cov0.getNumAddresses(), reg.getNumAddresses()))

    # 1. switch tables
    tstats = {}
    if on("tables"):
        tables = find_tables(lo, hi, rows)
        size_tables(tables)
        tstats = type_tables(tables, rows, changed, targets_by_fn)
        runs = set()
        gaps = reg.subtract(cov0)
        for t in tables:
            r = gaps.getRangeContaining(toAddr(t["table"]))
            if r is not None:
                runs.add(r.getMinAddress().getOffset())
        tstats["runs"] = len(runs)
        tstats["found"] = len(tables)
        log("switch tables: %s" % ", ".join("%s=%s" % kv for kv in sorted(tstats.items())))

    # 2. off-cut repair
    rstats = {}
    if on("repair"):
        rstats = repair_region(lo, hi, targets_by_fn, rows, changed, "")
        log("off-cut repair: %s" % fmt(rstats))

    # 3. call checks (before analysis; re-checked after)
    cstats = check_calls(parse_list(args.get("calls", DEFAULT_CALLS)), rows)
    log("call checks: %s" % cstats)

    # 4. guarded analysis over changed code + functions created earlier in this pass
    created = set()
    cpath = args.get("created", "")
    if cpath and os.path.isfile(cpath):
        with open(cpath, "r", encoding="utf-8") as fh:
            for r in csv.DictReader(fh):
                if r.get("status") == "created" and parse_int(r.get("address")) is not None:
                    created.add(parse_int(r.get("address")))
        log("created= lists %d created functions" % len(created))
    elif cpath:
        log("created= file not found: %s" % cpath)
    gstats = {}
    rstats2 = {}
    if on("analyze") and (changed or created):
        gstats = guarded_analysis(changed | created, disable, rows)
        if on("repair"):
            rstats2 = repair_region(lo, hi, targets_by_fn, rows, changed, "-post-analysis")
            log("off-cut repair after analysis: %s" % fmt(rstats2))
        cstats2 = check_calls(parse_list(args.get("calls", DEFAULT_CALLS)), rows)
        log("call checks after analysis: %s" % cstats2)
    for o in changed:
        g = fm.getFunctionAt(toAddr(o))
        if g is not None:
            g.addTag(tag)

    cov1, _ = region_coverage(lo, hi)
    after = fm.getFunctionCount()
    rows.append({"kind": "summary", "address": "0x%08x-0x%08x" % (lo, hi), "name": "region",
                 "status": "done", "before": "covered=%d" % cov0.getNumAddresses(),
                 "after": "covered=%d" % cov1.getNumAddresses(),
                 "detail": "of %d bytes; functions %d->%d; tables %s; repair %s; repair-post %s; guard %s"
                 % (reg.getNumAddresses(), before, after, fmt(tstats), fmt(rstats), fmt(rstats2),
                    fmt(gstats))})
    log("functions before=%d after=%d; region covered before=%d after=%d of %d (%.1f%%)"
        % (before, after, cov0.getNumAddresses(), cov1.getNumAddresses(), reg.getNumAddresses(),
           100.0 * cov1.getNumAddresses() / max(1, reg.getNumAddresses())))
    for r in rows:
        if r.get("status") in ("failed", "unfixed"):
            log("FAILED %s %s %s: %s" % (r["kind"], r["address"], r["name"], r.get("detail", "")))
    try:
        d = os.path.dirname(report)
        if d:
            os.makedirs(d, exist_ok=True)
        with open(report, "w", newline="", encoding="utf-8") as fh:
            w = csv.DictWriter(fh, fieldnames=FIELDS)
            w.writeheader()
            for r in rows:
                w.writerow({k: r.get(k, "") for k in FIELDS})
        log("report: %s" % report)
    except Exception as e:
        log("could not write report %s: %s" % (report, first_line(e)))


main()
