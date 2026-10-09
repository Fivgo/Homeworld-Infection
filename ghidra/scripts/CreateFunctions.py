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


def main():
    args = parse_args()
    path = args.get("json")
    if not path or not os.path.isfile(path):
        log("address list not found, skipping: %s" % path)
        return
    report = args.get("report") or (re.sub(r"\.json$", "", path) + ".report.csv")
    analyze = args.get("analyze", "1") in ("1", "true", "yes")
    tag = args.get("tag", "R2_created")
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
    if analyze and created:
        bodies = AddressSet()
        for a, _ in created:
            bodies.add(fm.getFunctionAt(a).getBody())
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

    # 6. final per-entry state (analysis may have grown bodies)
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
