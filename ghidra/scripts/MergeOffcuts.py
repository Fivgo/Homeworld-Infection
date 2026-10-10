# Remove off-cut function objects and give their code back to the parent function.
#
# An off-cut is a FUN_* "function" whose entry is really a branch target inside another
# function: the parent jumps into it, it has no CALL reference, and its code runs on the parent's
# frame. Reconcile lists these in the top-level `removals` list of a round symbol file
# (ghidra/symbols_round5.json), because ImportSymbolsJson.py cannot express "no function here":
#   {"address": "0x...", "old_name": "FUN_...", "action": "remove-function-merge-into-parent",
#    "parent": "<name>", "parent_address": "0x...", "reason": "..."}
#
# Per entry, the script checks:
#   - a function starts exactly at `address` and still carries `old_name` (a default FUN_* name);
#   - a function starts at `parent_address` and is named `parent`;
#   - no CALL reference reaches `address`, and at least one jump reference into `address` comes
#     from the parent's body (the off-cut evidence reconcile cited);
# and, when all hold, removes the function object at `address` (code, labels and references are
# kept), then re-grows the parent with CreateFunctionCmd.fixupFunctionBody. If the new parent body
# does not cover both its old body and the off-cut's old body, the union is set explicitly.
#
# Args (key=value):
#   json=<path>    round symbol file with a `removals` list (required)
#   report=<csv>   per-entry CSV (default <json minus .json>.merge_offcuts.report.csv)
#   dry=1|0        check and report only (default 0)
#
# Tolerant: per-entry errors are reported; the script never aborts the headless run.
#
# @category Homeworld-Infection
# @runtime PyGhidra

import typing
if typing.TYPE_CHECKING:
    from ghidra.ghidra_builtins import *  # noqa: F401,F403

import csv
import json
import os
import time

from ghidra.app.cmd.function import CreateFunctionCmd
from ghidra.program.model.address import AddressSet

T0 = time.time()
FIELDS = ["address", "old_name", "offcut_size", "parent_address", "parent", "parent_size_before",
          "parent_size_after", "jumps_from_parent", "calls_in", "status", "detail"]


def log(msg):
    println("[MergeOffcuts +%.1fs] %s" % (time.time() - T0, msg))


def parse_args():
    args = {}
    for a in list(getScriptArgs()):
        a = str(a)
        if "=" in a:
            k, v = a.split("=", 1)
            args[k.strip().lower()] = v.strip()
    return args


def first_line(e):
    s = str(e)
    return (s.splitlines()[0] if s else repr(e))[:200]


def body_size(fn):
    return int(fn.getBody().getNumAddresses()) if fn is not None else 0


def handle(item, fm, refm, dry):
    row = {k: "" for k in FIELDS}
    row["address"] = item.get("address", "")
    row["old_name"] = item.get("old_name", "")
    row["parent"] = item.get("parent", "")
    row["parent_address"] = item.get("parent_address", "")
    if item.get("action") != "remove-function-merge-into-parent":
        row["status"], row["detail"] = "skipped", "action=%s" % item.get("action")
        return row
    a = toAddr(row["address"])
    pa = toAddr(row["parent_address"])
    if a is None or pa is None:
        row["status"], row["detail"] = "failed", "bad address"
        return row
    fn = fm.getFunctionAt(a)
    par = fm.getFunctionAt(pa)
    if fn is None:
        row["status"], row["detail"] = "skipped", "no function at address"
        return row
    if fn.getName() != row["old_name"] or not fn.getName().startswith("FUN_"):
        row["status"], row["detail"] = "skipped", "name is %s, not the default %s" % (
            fn.getName(), row["old_name"])
        return row
    if par is None or par.getName() != row["parent"]:
        row["status"], row["detail"] = "skipped", "parent at %s is %s" % (
            pa, par.getName() if par is not None else "none")
        return row
    row["offcut_size"] = body_size(fn)
    row["parent_size_before"] = body_size(par)
    calls, jumps_par, other = 0, 0, []
    for r in refm.getReferencesTo(a):
        t = r.getReferenceType()
        src = r.getFromAddress()
        if t.isCall():
            calls += 1
        elif t.isJump() or t.isFlow():
            if par.getBody().contains(src):
                jumps_par += 1
            else:
                other.append("%s<-%s" % (t, src))
    row["calls_in"], row["jumps_from_parent"] = calls, jumps_par
    if calls or jumps_par == 0:
        row["status"] = "skipped"
        row["detail"] = "not an off-cut by its references: calls=%d jumps_from_parent=%d other=%s" % (
            calls, jumps_par, ",".join(other[:4]))
        return row
    if dry:
        row["status"], row["detail"] = "dry-ok", ",".join(other[:4])
        return row
    old_par = AddressSet(par.getBody())
    old_cut = AddressSet(fn.getBody())
    if not fm.removeFunction(a):
        row["status"], row["detail"] = "failed", "removeFunction returned false"
        return row
    par = fm.getFunctionAt(pa)
    ok = CreateFunctionCmd.fixupFunctionBody(currentProgram, par, monitor)
    want = AddressSet(old_par).union(old_cut)
    detail = "fixup=%s" % ok
    if not par.getBody().contains(want):
        par.setBody(AddressSet(par.getBody()).union(want))
        detail += "; body set to union explicitly"
    row["parent_size_after"] = body_size(par)
    row["status"] = "merged"
    row["detail"] = detail + (("; other flow refs: " + ",".join(other[:4])) if other else "")
    return row


def main():
    args = parse_args()
    path = args.get("json")
    if not path or not os.path.isfile(path):
        log("no json= file: %r" % path)
        return
    report = args.get("report") or (path[:-5] if path.endswith(".json") else path) + \
        ".merge_offcuts.report.csv"
    dry = args.get("dry", "0") == "1"
    with open(path) as f:
        data = json.load(f)
    items = data.get("removals", []) if isinstance(data, dict) else []
    fm = currentProgram.getFunctionManager()
    refm = currentProgram.getReferenceManager()
    rows = []
    for it in items:
        try:
            row = handle(it, fm, refm, dry)
        except Exception as e:  # noqa: BLE001
            row = {k: "" for k in FIELDS}
            row.update(address=it.get("address", ""), status="failed", detail=first_line(e))
        rows.append(row)
        log("%s %s -> %s: %s (%s)" % (row["address"], row["old_name"], row["parent"],
                                      row["status"], row["detail"]))
    with open(report, "w", newline="") as f:
        w = csv.DictWriter(f, fieldnames=FIELDS)
        w.writeheader()
        w.writerows(rows)
    log("items=%d merged=%d report=%s" % (
        len(rows), sum(1 for r in rows if r["status"] == "merged"), report))


main()
