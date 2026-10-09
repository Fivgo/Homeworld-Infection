# Export an analysed program to flat files for the decompilation / port effort.
#
# Args (key=value):
#   outdir=<dir>       required in practice (tools/ghidra_headless.sh passes it);
#                      default <scripts dir>/../exports/<program name>
#   decompile=1|0      write decompiled.c (default 1)
#   threads=N          parallel decompiler workers (default: min(16, cpu count))
#   timeout=S          per-function decompile timeout in seconds (default 60)
#   types=1|0          write datatypes.h via DataTypeWriter (default 1)
#
# Output files (all UTF-8; CSV is RFC4180 via python csv; strings are escaped C-style):
#   program.json         metadata: hashes, image base, language, blocks, analysis options, counts
#   functions.csv        address,name,size,calls_in,calls_out,... (one row per non-external function)
#   call_graph.csv       caller -> callee edges (callee may be EXTERNAL:LIB::name)
#   strings.csv          address,string,xref_count,first_xref_function,... (defined string data)
#   imports.txt          external libraries -> symbols, ordinals, IAT slot, referencing code
#   exports.txt          entry points / exported symbols
#   symbols.csv          every non-default symbol (address,name,namespace,type,source,primary)
#   symbols_summary.txt  human-readable counts (incl. functions / FUN_* per memory block)
#   decompiled.c         every function, address-sorted, each preceded by a marker line
#   decompiled_index.csv address,name,status,line_start,line_end (1-based, into decompiled.c)
#   datatypes.h          program data type manager as C (structures/typedefs applied so far)
#
# @category Homeworld-Infection
# @runtime PyGhidra

import typing
if typing.TYPE_CHECKING:
    from ghidra.ghidra_builtins import *  # noqa: F401,F403

import csv
import json
import os
import queue
import threading
import time

from java.io import FileWriter
from ghidra.app.decompiler import DecompInterface, DecompileOptions
from ghidra.framework import Application
from ghidra.program.model.data import DataTypeWriter
from ghidra.program.model.symbol import SourceType, SymbolType
from ghidra.util.task import TaskMonitor

T0 = time.time()


def log(msg):
    println("[ExportAnalysis +%.1fs] %s" % (time.time() - T0, msg))


def parse_args():
    args = {}
    for a in list(getScriptArgs()):
        a = str(a)
        if "=" in a:
            k, v = a.split("=", 1)
            args[k.strip().lower()] = v.strip()
    return args


ARGS = parse_args()
PROG = currentProgram
LISTING = PROG.getListing()
FM = PROG.getFunctionManager()
REFMGR = PROG.getReferenceManager()
ST = PROG.getSymbolTable()
MEM = PROG.getMemory()


def default_outdir():
    try:
        sd = os.path.dirname(str(getSourceFile().getAbsolutePath()))
        return os.path.normpath(os.path.join(sd, "..", "exports", PROG.getName()))
    except Exception:
        return os.path.join(os.getcwd(), "exports", PROG.getName())


OUT = ARGS.get("outdir") or default_outdir()
os.makedirs(OUT, exist_ok=True)


def p(name):
    return os.path.join(OUT, name)


def esc(s):
    """C-style escape so every string fits on one CSV line."""
    out = []
    for ch in s:
        o = ord(ch)
        if ch == "\\":
            out.append("\\\\")
        elif ch == "\n":
            out.append("\\n")
        elif ch == "\r":
            out.append("\\r")
        elif ch == "\t":
            out.append("\\t")
        elif o < 0x20 or o == 0x7f:
            out.append("\\x%02x" % o)
        else:
            out.append(ch)
    return "".join(out)


def hexaddr(a):
    return "0x%08x" % a.getOffset()


def block_name(a):
    b = MEM.getBlock(a)
    return b.getName() if b is not None else ""


def func_label(f):
    return f.getName(True) if f is not None else ""


# ------------------------------------------------------------------ functions
def collect_functions():
    funcs = [f for f in FM.getFunctions(True) if not f.isExternal()]
    log("functions: %d" % len(funcs))
    return funcs


# ------------------------------------------------------------------ strings
def collect_strings():
    rows = []
    func_strings = {}
    it = LISTING.getDefinedData(True)
    while it.hasNext():
        d = it.next()
        if not d.hasStringValue():
            continue
        try:
            val = d.getValue()
            sval = str(val) if val is not None else ""
        except Exception:
            sval = ""
        addr = d.getAddress()
        direct_funcs, data_refs, indirect_funcs = [], 0, []
        for ref in getReferencesTo(addr):
            frm = ref.getFromAddress()
            f = getFunctionContaining(frm)
            if f is not None:
                if f not in direct_funcs:
                    direct_funcs.append(f)
            else:
                data_refs += 1
                # one hop: a pointer table entry -> who references the pointer?
                for ref2 in getReferencesTo(frm):
                    f2 = getFunctionContaining(ref2.getFromAddress())
                    if f2 is not None and f2 not in indirect_funcs:
                        indirect_funcs.append(f2)
        refs_total = len(getReferencesTo(addr))
        for f in direct_funcs:
            func_strings.setdefault(f.getEntryPoint().getOffset(), []).append(sval)
        rows.append({
            "address": hexaddr(addr),
            "string": esc(sval),
            "xref_count": refs_total,
            "first_xref_function": func_label(direct_funcs[0]) if direct_funcs else
                                   (("~" + func_label(indirect_funcs[0])) if indirect_funcs else ""),
            "section": block_name(addr),
            "datatype": d.getDataType().getName(),
            "length": d.getLength(),
            "xref_functions": "|".join("%s@%s" % (f.getName(True), hexaddr(f.getEntryPoint()))
                                       for f in direct_funcs),
            "data_refs": data_refs,
            "indirect_functions": "|".join("%s@%s" % (f.getName(True), hexaddr(f.getEntryPoint()))
                                           for f in indirect_funcs),
        })
    log("strings: %d" % len(rows))
    return rows, func_strings


def write_strings(rows):
    cols = ["address", "string", "xref_count", "first_xref_function", "section", "datatype",
            "length", "xref_functions", "data_refs", "indirect_functions"]
    with open(p("strings.csv"), "w", newline="", encoding="utf-8") as fh:
        w = csv.DictWriter(fh, fieldnames=cols)
        w.writeheader()
        for r in rows:
            w.writerow(r)


def write_functions(funcs, func_strings):
    cols = ["address", "name", "size", "calls_in", "calls_out", "call_sites_in", "is_thunk",
            "thunk_target", "source", "calling_convention", "param_count", "varargs",
            "no_return", "section", "strings_referenced", "tags", "signature"]
    edges = []
    with open(p("functions.csv"), "w", newline="", encoding="utf-8") as fh:
        w = csv.writer(fh)
        w.writerow(cols)
        for f in funcs:
            entry = f.getEntryPoint()
            callers = f.getCallingFunctions(TaskMonitor.DUMMY)
            callees = f.getCalledFunctions(TaskMonitor.DUMMY)
            call_sites = 0
            for ref in REFMGR.getReferencesTo(entry):
                if ref.getReferenceType().isCall():
                    call_sites += 1
            thunk_target = ""
            if f.isThunk():
                t = f.getThunkedFunction(True)
                if t is not None:
                    thunk_target = ("EXTERNAL:" if t.isExternal() else "") + t.getName(True)
            for c in callees:
                edges.append((f, c))
            w.writerow([
                hexaddr(entry), f.getName(True), f.getBody().getNumAddresses(),
                len(callers), len(callees), call_sites, int(f.isThunk()), thunk_target,
                str(f.getSymbol().getSource()), f.getCallingConventionName(),
                f.getParameterCount(), int(f.hasVarArgs()), int(f.hasNoReturn()),
                block_name(entry), len(func_strings.get(entry.getOffset(), [])),
                "|".join(sorted(t.getName() for t in f.getTags())),
                esc(f.getPrototypeString(False, False)),
            ])
    with open(p("call_graph.csv"), "w", newline="", encoding="utf-8") as fh:
        w = csv.writer(fh)
        w.writerow(["caller_address", "caller", "callee_address", "callee"])
        for a, b in edges:
            if b.isExternal():
                w.writerow([hexaddr(a.getEntryPoint()), a.getName(True), "",
                            "EXTERNAL:" + b.getName(True)])
            else:
                w.writerow([hexaddr(a.getEntryPoint()), a.getName(True),
                            hexaddr(b.getEntryPoint()), b.getName(True)])
    log("functions.csv + call_graph.csv (%d edges)" % len(edges))
    return len(edges)


# ------------------------------------------------------------------ imports / exports
def write_imports():
    em = PROG.getExternalManager()
    n = 0
    with open(p("imports.txt"), "w", encoding="utf-8") as fh:
        fh.write("# library :: symbol  [ordinal]  iat=<slot address>  code_refs=<n>\n")
        for lib in sorted(em.getExternalLibraryNames()):
            locs = list(em.getExternalLocations(lib))
            fh.write("%s  (%d symbols, path=%s)\n" % (lib, len(locs), em.getExternalLibraryPath(lib)))
            entries = []
            for loc in locs:
                sym = loc.getSymbol()
                iat, code_refs = [], 0
                for ref in sym.getReferences():
                    frm = ref.getFromAddress()
                    if LISTING.getInstructionAt(frm) is not None:
                        code_refs += 1
                    else:
                        iat.append(hexaddr(frm))
                ordinal = ""
                orig = loc.getOriginalImportedName()
                if orig and orig.startswith("Ordinal_"):
                    ordinal = orig
                entries.append("  %s :: %s%s  iat=%s  code_refs=%d\n" % (
                    lib, loc.getLabel(), ("  [%s]" % ordinal) if ordinal else "",
                    ",".join(iat) or "-", code_refs))
            for e in sorted(entries):
                fh.write(e)
            n += len(locs)
    log("imports.txt (%d symbols)" % n)
    return n


def write_exports():
    n = 0
    with open(p("exports.txt"), "w", encoding="utf-8") as fh:
        fh.write("# address  name  (external entry points: PE exports + program entry)\n")
        it = ST.getExternalEntryPointIterator()
        rows = []
        while it.hasNext():
            a = it.next()
            s = ST.getPrimarySymbol(a)
            rows.append((a.getOffset(), "%s  %s\n" % (hexaddr(a), s.getName(True) if s else "")))
        for _, line in sorted(rows):
            fh.write(line)
            n += 1
    log("exports.txt (%d)" % n)
    return n


# ------------------------------------------------------------------ symbols
def write_symbols():
    by_source, by_type = {}, {}
    n = 0
    with open(p("symbols.csv"), "w", newline="", encoding="utf-8") as fh:
        w = csv.writer(fh)
        w.writerow(["address", "name", "namespace", "type", "source", "primary"])
        it = ST.getAllSymbols(True)
        while it.hasNext():
            s = it.next()
            src = str(s.getSource())
            typ = str(s.getSymbolType())
            by_source[src] = by_source.get(src, 0) + 1
            by_type[typ] = by_type.get(typ, 0) + 1
            if s.getSource() == SourceType.DEFAULT or s.isExternal():
                continue
            a = s.getAddress()
            if not a.isMemoryAddress():
                continue
            w.writerow([hexaddr(a), s.getName(), s.getParentNamespace().getName(True), typ, src,
                        int(s.isPrimary())])
            n += 1
    log("symbols.csv (%d non-default)" % n)
    return by_source, by_type, n


# ------------------------------------------------------------------ decompile
def make_decompiler(timeout):
    ifc = DecompInterface()
    opts = DecompileOptions()
    opts.grabFromProgram(PROG)
    ifc.setOptions(opts)
    ifc.toggleCCode(True)
    ifc.toggleSyntaxTree(False)
    ifc.setSimplificationStyle("decompile")
    if not ifc.openProgram(PROG):
        raise RuntimeError("decompiler failed to open program: " + str(ifc.getLastMessage()))
    return ifc


def decompile_all(funcs, threads, timeout):
    work = [f for f in funcs if not f.isThunk()]
    results = {}
    q = queue.Queue()
    for f in work:
        q.put(f)
    lock = threading.Lock()
    done = [0]
    errors = []

    def worker():
        try:
            ifc = make_decompiler(timeout)
        except Exception as e:
            with lock:
                errors.append(str(e))
            return
        try:
            while True:
                try:
                    f = q.get_nowait()
                except queue.Empty:
                    return
                status, text = "ok", ""
                try:
                    res = ifc.decompileFunction(f, timeout, TaskMonitor.DUMMY)
                    if res is not None and res.decompileCompleted() and res.getDecompiledFunction():
                        text = str(res.getDecompiledFunction().getC())
                    else:
                        status = "failed"
                        msg = res.getErrorMessage() if res is not None else "no result"
                        text = "/* DECOMPILE FAILED: %s */\n" % (str(msg).strip().replace("*/", "* /"))
                except Exception as e:
                    status = "error"
                    text = "/* DECOMPILE ERROR: %s */\n" % str(e).replace("*/", "* /")
                with lock:
                    results[f.getEntryPoint().getOffset()] = (f, status, text)
                    done[0] += 1
                    if done[0] % 500 == 0:
                        log("decompiled %d/%d" % (done[0], len(work)))
        finally:
            ifc.dispose()

    ts = [threading.Thread(target=worker, name="decomp-%d" % i) for i in range(max(1, threads))]
    for t in ts:
        t.start()
    for t in ts:
        t.join()
    if errors:
        log("decompiler worker errors: %s" % "; ".join(errors[:3]))

    stats = {"ok": 0, "failed": 0, "error": 0, "thunk": 0}
    line = 1
    with open(p("decompiled.c"), "w", encoding="utf-8") as fc, \
            open(p("decompiled_index.csv"), "w", newline="", encoding="utf-8") as fi:
        wi = csv.writer(fi)
        wi.writerow(["address", "name", "status", "line_start", "line_end"])
        hdr = ("/* Ghidra %s decompilation of %s (sha256 %s)\n"
               " * Every function is preceded by a marker line:\n"
               " *   //==== FUNCTION <address> <name> size=<bytes> status=<ok|failed|error|thunk>\n"
               " * Split with:  csplit -z decompiled.c '/^\\/\\/==== FUNCTION/' '{*}'\n"
               " */\n\n") % (Application.getApplicationVersion(), PROG.getName(),
                            PROG.getExecutableSHA256())
        fc.write(hdr)
        line += hdr.count("\n")
        for f in funcs:
            off = f.getEntryPoint().getOffset()
            if f.isThunk():
                t = f.getThunkedFunction(True)
                status = "thunk"
                text = "/* thunk -> %s */\n" % (t.getName(True) if t is not None else "?")
            elif off in results:
                _, status, text = results[off]
            else:
                status, text = "error", "/* not decompiled (worker failure) */\n"
            stats[status] = stats.get(status, 0) + 1
            marker = "//==== FUNCTION %s %s size=%d status=%s\n" % (
                hexaddr(f.getEntryPoint()), f.getName(True), f.getBody().getNumAddresses(), status)
            if not text.endswith("\n"):
                text += "\n"
            block = marker + text + "\n"
            start = line
            fc.write(block)
            line += block.count("\n")
            wi.writerow([hexaddr(f.getEntryPoint()), f.getName(True), status, start, line - 1])
    log("decompiled.c: %s" % ", ".join("%s=%d" % kv for kv in sorted(stats.items())))
    return stats


def write_datatypes():
    fw = FileWriter(p("datatypes.h"))
    try:
        dtw = DataTypeWriter(PROG.getDataTypeManager(), fw, False)
        dtw.write(PROG.getDataTypeManager(), TaskMonitor.DUMMY)
    finally:
        fw.close()
    log("datatypes.h")


# ------------------------------------------------------------------ main
def main():
    log("program=%s outdir=%s" % (PROG.getName(), OUT))
    funcs = collect_functions()
    srows, func_strings = collect_strings()
    write_strings(srows)
    n_edges = write_functions(funcs, func_strings)
    n_imports = write_imports()
    n_exports = write_exports()
    by_source, by_type, n_syms = write_symbols()

    want_types = ARGS.get("types", "1") not in ("0", "false", "no")
    if want_types:
        try:
            write_datatypes()
        except Exception as e:
            log("datatypes.h failed: %s" % e)

    dstats = {}
    t_dec = 0.0
    if ARGS.get("decompile", "1") not in ("0", "false", "no"):
        threads = int(ARGS.get("threads", str(min(16, os.cpu_count() or 4))))
        timeout = int(ARGS.get("timeout", "60"))
        t1 = time.time()
        dstats = decompile_all(funcs, threads, timeout)
        t_dec = time.time() - t1
        log("decompile wall time %.1fs with %d threads" % (t_dec, threads))
    else:
        for stale in ("decompiled.c", "decompiled_index.csv"):  # never leave an old dump behind
            if os.path.exists(p(stale)):
                os.remove(p(stale))

    # ---- counts for summary / program.json
    n_default = sum(1 for f in funcs if f.getSymbol().getSource() == SourceType.DEFAULT)
    n_thunk = sum(1 for f in funcs if f.isThunk())
    src_counts = {}
    for f in funcs:
        s = str(f.getSymbol().getSource())
        src_counts[s] = src_counts.get(s, 0) + 1
    tag_counts = {}
    for f in funcs:
        for t in f.getTags():
            tag_counts[t.getName()] = tag_counts.get(t.getName(), 0) + 1
    fid = 0
    bit = PROG.getBookmarkManager().getBookmarksIterator()
    bm_counts = {}
    while bit.hasNext():
        b = bit.next()
        key = "%s/%s" % (b.getTypeString(), b.getCategory())
        bm_counts[key] = bm_counts.get(key, 0) + 1
        if b.getCategory() == "Function ID Analyzer":
            fid += 1
    strings_with_xrefs = sum(1 for r in srows if r["xref_count"])
    # functions per memory block: on cataclysm.exe this separates game code (.text) from the
    # SecuROM-style .cms_t blob (docs/binary-fingerprint.md) so junk there is visible at a glance
    blk_funcs, blk_default = {}, {}
    for f in funcs:
        bn = block_name(f.getEntryPoint())
        blk_funcs[bn] = blk_funcs.get(bn, 0) + 1
        if f.getSymbol().getSource() == SourceType.DEFAULT:
            blk_default[bn] = blk_default.get(bn, 0) + 1
    blocks = []
    for b in MEM.getBlocks():
        blocks.append({"name": b.getName(), "start": hexaddr(b.getStart()), "end": hexaddr(b.getEnd()),
                       "size": b.getSize(), "perm": ("r" if b.isRead() else "-") +
                       ("w" if b.isWrite() else "-") + ("x" if b.isExecute() else "-"),
                       "initialized": bool(b.isInitialized()),
                       "functions": blk_funcs.get(b.getName(), 0),
                       "functions_default_named": blk_default.get(b.getName(), 0)})
    opts = getCurrentAnalysisOptionsAndValues(PROG)
    meta = {
        "ghidra_version": str(Application.getApplicationVersion()),
        "program": PROG.getName(),
        "executable_path": PROG.getExecutablePath(),
        "executable_format": PROG.getExecutableFormat(),
        "sha256": PROG.getExecutableSHA256(),
        "md5": PROG.getExecutableMD5(),
        "image_base": hexaddr(PROG.getImageBase()),
        "language": str(PROG.getLanguageID()),
        "compiler_spec": str(PROG.getCompilerSpec().getCompilerSpecID()),
        "compiler": PROG.getCompiler(),
        "memory_blocks": blocks,
        "counts": {
            "functions": len(funcs), "functions_default_named": n_default,
            "functions_thunk": n_thunk, "functions_by_source": src_counts,
            "function_tags": tag_counts, "fid_bookmarks": fid,
            "strings": len(srows), "strings_with_xrefs": strings_with_xrefs,
            "call_edges": n_edges, "imports": n_imports, "exports": n_exports,
            "symbols_non_default": n_syms, "symbols_by_source": by_source,
            "symbols_by_type": by_type, "bookmarks": bm_counts, "decompile": dstats,
        },
        "decompile_seconds": round(t_dec, 1),
        "export_seconds": round(time.time() - T0, 1),
        "analysis_options": {str(k): str(opts.get(k)) for k in opts.keySet()},
    }
    with open(p("program.json"), "w", encoding="utf-8") as fh:
        json.dump(meta, fh, indent=1, sort_keys=True)

    with open(p("symbols_summary.txt"), "w", encoding="utf-8") as fh:
        fh.write("program        %s\n" % PROG.getName())
        fh.write("sha256         %s\n" % PROG.getExecutableSHA256())
        fh.write("image base     %s   language %s / %s   compiler %s\n" % (
            hexaddr(PROG.getImageBase()), PROG.getLanguageID(),
            PROG.getCompilerSpec().getCompilerSpecID(), PROG.getCompiler()))
        fh.write("ghidra         %s\n\n" % Application.getApplicationVersion())
        fh.write("memory blocks:\n")
        for b in blocks:
            fh.write("  %-10s %s-%s %9d %s %-4s  functions %6d  FUN_* %6d\n" % (
                b["name"], b["start"], b["end"], b["size"], b["perm"],
                "init" if b["initialized"] else "bss", b["functions"], b["functions_default_named"]))
        fh.write("\nfunctions      %d  (default-named FUN_*: %d, thunks: %d)\n"
                 % (len(funcs), n_default, n_thunk))
        for k, v in sorted(src_counts.items()):
            fh.write("  name source %-14s %d\n" % (k, v))
        for k, v in sorted(tag_counts.items()):
            fh.write("  tag %-21s %d\n" % (k, v))
        fh.write("  FID bookmarks         %d\n" % fid)
        fh.write("call edges     %d\n" % n_edges)
        fh.write("strings        %d  (with xrefs: %d)\n" % (len(srows), strings_with_xrefs))
        fh.write("imports        %d   exports/entry points %d\n" % (n_imports, n_exports))
        fh.write("symbols        non-default %d\n" % n_syms)
        for k, v in sorted(by_source.items()):
            fh.write("  source %-14s %d\n" % (k, v))
        for k, v in sorted(by_type.items()):
            fh.write("  type   %-14s %d\n" % (k, v))
        fh.write("bookmarks\n")
        for k, v in sorted(bm_counts.items()):
            fh.write("  %-40s %d\n" % (k, v))
        if dstats:
            fh.write("decompile      %s  (%.1fs)\n"
                     % (", ".join("%s=%d" % kv for kv in sorted(dstats.items())), t_dec))
    log("done: %s" % OUT)


main()
