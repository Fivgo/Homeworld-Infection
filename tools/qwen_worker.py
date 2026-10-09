#!/usr/bin/env python3
"""Local Qwen naming worker for Homeworld-Infection.

Names the still-unnamed functions of cataclysm.exe using a local Ollama model, so
bulk binary-map work continues while Claude is rate-limited. Costs zero Claude tokens.

Design (see cycle/STATE.md):
  * Checkpoint = append-only JSONL (ghidra/proposals/qwen/naming.jsonl). Every finished
    function is one line, written + fsynced immediately. A restart skips addresses
    already present, so stopping anywhere loses at most the one in-flight function.
  * Trigger = `run --auto` (systemd timer): only works while Claude is inside a
    usage-limit window (parsed from the "session limit · resets HH:MM" message in
    ~/.claude transcripts) AND the GPU is not being used by the user's game.
  * Output is a PROPOSAL source for the binary-map workflow; Opus audits a sample before
    anything is applied to Ghidra (Qwen never writes to the Ghidra project).

Subcommands:
  run [--auto|--force] [--max-items N] [--max-minutes M]
  status            limit window, GPU gate, queue progress
  export            write ghidra/proposals/qwen-naming.json in the binary-map contract
"""
import argparse, csv, datetime as dt, fcntl, glob, json, os, re, subprocess, sys, time, urllib.request

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
def _newest_export():
    base = os.path.join(ROOT, "ghidra/exports")
    cands = [d for d in glob.glob(os.path.join(base, "cataclysm_gog*")) if os.path.exists(os.path.join(d, "decompiled_index.csv"))]
    return max(cands, key=os.path.getmtime) if cands else os.path.join(base, "cataclysm_gog")
EXP = os.environ.get("HWI_EXPORT_DIR") or _newest_export()
HW1M = os.path.join(ROOT, "ghidra/exports/cataclysm_gog_hw1match/hw1_string_matches.csv")
OUTDIR = os.path.join(ROOT, "ghidra/proposals/qwen")
OUT = os.path.join(OUTDIR, "naming.jsonl")
LOG = os.path.join(OUTDIR, "worker.log")
LOCK = os.path.join(OUTDIR, ".worker.lock")
PROJECTS = os.path.expanduser("~/.claude/projects")
OLLAMA = os.environ.get("OLLAMA_HOST", "http://127.0.0.1:11434")
MODEL = os.environ.get("HWI_QWEN_MODEL", "qwen3.6:27b")
NUM_CTX = int(os.environ.get("HWI_QWEN_CTX", "8192"))
BODY_CAP = 8000          # chars of decompiled C per function (covers ~98% whole)
GPU_BUSY_PCT = 40        # user's rule: treat >40% util (not ours) as "user is gaming"

CLASSES = ["crt", "won", "bink", "rgl", "hw1", "cat-new", "unknown"]
SCHEMA = {
    "type": "object",
    "properties": {
        "name": {"type": "string"},
        "class": {"type": "string", "enum": CLASSES},
        "confidence": {"type": "string", "enum": ["high", "medium", "low"]},
        "hw1_equivalent": {"type": "string"},
        "evidence": {"type": "string"},
    },
    "required": ["name", "class", "confidence", "hw1_equivalent", "evidence"],
}
SYSTEM = """You are a reverse engineer naming functions in the x86 executable of Homeworld: Cataclysm (2000, MSVC 6), a fork of the Homeworld 1 engine whose C source is public. For ONE function you get: its decompiled C, the string literals it references, the names of its callers and callees, and sometimes a candidate Homeworld 1 function found by string matching.
Return ONLY the JSON object. Rules:
- name: a C identifier. If the function is clearly the Homeworld 1 candidate, reuse that name verbatim. If it is a C runtime, Winsock or WON/Titan networking routine, use the conventional name. If you cannot tell, use "" and confidence "low". Never invent Homeworld 1 file names you were not given.
- class: crt (C runtime), won (WON/Titan online SDK), bink (video), rgl (renderer DLL glue), hw1 (a Homeworld 1 engine function), cat-new (Cataclysm-only game code), unknown.
- confidence: high only when strings or the candidate make it unambiguous.
- hw1_equivalent: "File.c:function" only if a candidate was given and matches, else "".
- evidence: under 160 characters, cite the deciding string or callee."""

def log(msg):
    line = f"{dt.datetime.now().isoformat(timespec='seconds')} {msg}"
    print(line, flush=True)
    with open(LOG, "a") as f:
        f.write(line + "\n")

# ---------------------------------------------------------------- limit window
def claude_limit_state(lookback_h=6):
    """Return (limited, reset_local, seen_at) from GENUINE rate-limit events only.

    A real limit is recorded by Claude Code as a synthetic assistant message with
    isApiErrorMessage=true and a top-level quotaLimits dict
    {"status":"rejected","rateLimitType":"five_hour","resetsAt":<epoch>, ...}.
    Text that merely quotes "session limit · resets ..." (task notifications, tool output,
    docs) is ignored, because only the parsed top-level field counts. Claude Code records
    no 'approaching limit' warning (verified 2026-10-08 over a week of transcripts: every
    quotaLimits record is status=rejected), so this is the only machine-readable signal."""
    cutoff = time.time() - lookback_h * 3600
    now = time.time()
    best = None  # (resetsAt, seen_ts)
    marker = None  # epoch of the newest '[Usage limit reached' marker
    last_ok = None  # epoch of the newest real assistant message
    for dirpath, _dirs, files in os.walk(PROJECTS):
        for fn in files:
            if not fn.endswith(".jsonl"):
                continue
            p = os.path.join(dirpath, fn)
            try:
                if os.path.getmtime(p) < cutoff:
                    continue
                with open(p, "rb") as f:
                    f.seek(max(0, os.path.getsize(p) - 4_000_000))
                    tail = f.read().decode("utf-8", "replace")
            except OSError:
                continue
            for line in tail.splitlines():
                # (a) hard 429 with an exact reset time
                if "isApiErrorMessage" in line and "quotaLimits" in line:
                    try:
                        o = json.loads(line)
                    except ValueError:
                        continue
                    q = o.get("quotaLimits")
                    if (o.get("isApiErrorMessage") is True and isinstance(q, dict)
                            and q.get("status") == "rejected" and isinstance(q.get("resetsAt"), (int, float))):
                        if best is None or q["resetsAt"] > best[0]:
                            best = (q["resetsAt"], o.get("timestamp", ""))
                    continue
                # (b) the in-turn marker Claude Code injects as a user message when a session runs dry
                #     ("[Usage limit reached; a short grace allowance remains, then this turn is cut ...]");
                #     no reset time is given, so treat it as "limited" until a later REAL assistant message appears.
                if '"[Usage limit reached' in line and '"type": "user"' in line or ('"type":"user"' in line and '"[Usage limit reached' in line):
                    ts = _ts(line)
                    if ts and (marker is None or ts > marker):
                        marker = ts
                    continue
                if '"type": "assistant"' in line or '"type":"assistant"' in line:
                    if '"<synthetic>"' in line or '"isApiErrorMessage": true' in line or '"isApiErrorMessage":true' in line:
                        continue
                    ts = _ts(line)
                    if ts and (last_ok is None or ts > last_ok):
                        last_ok = ts
    limited_429 = best is not None and now < best[0]
    limited_marker = marker is not None and (last_ok is None or marker > last_ok) and (now - marker) < 5 * 3600
    reset = dt.datetime.fromtimestamp(best[0]).astimezone() if best else None
    seen = None
    src = best[1] if best else None
    if marker and (best is None or marker > _epoch(best[1])):
        seen = dt.datetime.fromtimestamp(marker).astimezone()
        reset = reset if limited_429 else None
    elif src:
        try:
            seen = dt.datetime.fromisoformat(src.replace("Z", "+00:00")).astimezone()
        except ValueError:
            pass
    return (limited_429 or limited_marker), reset, seen


def _ts(line):
    m = re.search(r'"timestamp"\s*:\s*"([^"]+)"', line)
    return _epoch(m.group(1)) if m else None


def _epoch(iso):
    try:
        return dt.datetime.fromisoformat(iso.replace("Z", "+00:00")).timestamp()
    except Exception:
        return None

# ---------------------------------------------------------------- GPU gate
def game_running():
    """True if a Wine/Proton game (or gamescope) is running: the user's game wins."""
    try:
        out = subprocess.run(["pgrep", "-af", r"\.exe|gamescope|proton"], capture_output=True, text=True).stdout
    except OSError:
        return False
    for ln in out.splitlines():
        low = ln.lower()
        if any(k in low for k in ("wineserver", "winedevice", "services.exe", "explorer.exe /desktop", "plugplay", "rpcss", "pgrep")):
            continue
        if ".exe" in low or "gamescope" in low:
            return True
    return False

def gpu_util():
    try:
        out = subprocess.run(["nvidia-smi", "--query-gpu=utilization.gpu", "--format=csv,noheader,nounits"],
                             capture_output=True, text=True, timeout=10).stdout.strip()
        return int(out.splitlines()[0])
    except Exception:
        return 0

def ollama_busy_with_us():
    try:
        with urllib.request.urlopen(OLLAMA + "/api/ps", timeout=5) as r:
            return any(m.get("name", "").startswith(MODEL.split(":")[0]) for m in json.load(r).get("models", []))
    except Exception:
        return False

def gpu_free_for_us():
    if game_running():
        return False, "a game is running"
    u = gpu_util()
    if u > GPU_BUSY_PCT and not ollama_busy_with_us():
        return False, f"GPU at {u}% (not ours)"
    return True, f"GPU at {u}%"

# ---------------------------------------------------------------- inputs
def load_inputs():
    idx = list(csv.DictReader(open(os.path.join(EXP, "decompiled_index.csv"))))
    lines = open(os.path.join(EXP, "decompiled.c"), encoding="utf-8", errors="replace").read().split("\n")
    body = {}
    for r in idx:
        if r["status"] == "ok":
            a, b = int(r["line_start"]), int(r["line_end"])
            body[r["address"].lower()] = "\n".join(lines[a - 1:b])
    names = {r["address"].lower(): r["name"] for r in csv.DictReader(open(os.path.join(EXP, "functions.csv")))}
    strs = {}
    for r in csv.DictReader(open(os.path.join(EXP, "strings.csv"))):
        for x in (r.get("xref_functions") or "").split("|"):
            if "@" in x:
                strs.setdefault(x.split("@")[1].lower(), []).append(r["string"])
    callees, callers = {}, {}
    for r in csv.DictReader(open(os.path.join(EXP, "call_graph.csv"))):
        callees.setdefault(r["caller_address"].lower(), []).append(r["callee"])
        callers.setdefault(r["callee_address"].lower(), []).append(r["caller"])
    hw1 = {}
    if os.path.exists(HW1M):
        for r in csv.DictReader(open(HW1M)):
            if r["status"] in ("proposed", "weak"):
                hw1[r["address"].lower()] = r
    queue = [a for a, n in names.items() if n.startswith("FUN_") and a in body]
    # most nameable first: has a HW1 candidate, then strings, then named callees, then size
    def score(a):
        named_callees = sum(1 for c in callees.get(a, []) if not c.startswith("FUN_"))
        return (a in hw1, len(strs.get(a, [])) > 0, min(named_callees, 5), len(body[a]))
    queue.sort(key=score, reverse=True)
    return body, names, strs, callees, callers, hw1, queue

def done_addresses():
    seen = set()
    if os.path.exists(OUT):
        for ln in open(OUT):
            try:
                seen.add(json.loads(ln)["address"].lower())
            except Exception:
                pass
    return seen

def build_prompt(a, body, strs, callees, callers, hw1):
    b = body[a]
    if len(b) > BODY_CAP:
        b = b[:BODY_CAP] + "\n/* ... truncated ... */"
    s = list(dict.fromkeys(strs.get(a, [])))[:25]
    ce = list(dict.fromkeys(callees.get(a, [])))[:15]
    cr = list(dict.fromkeys(callers.get(a, [])))[:10]
    parts = [f"Function at {a}.", "Decompiled C:\n" + b]
    parts.append("Strings referenced: " + (json.dumps(s) if s else "none"))
    parts.append("Callees: " + (", ".join(ce) if ce else "none"))
    parts.append("Callers: " + (", ".join(cr) if cr else "none"))
    if a in hw1:
        h = hw1[a]
        parts.append(f"Homeworld 1 candidate (string match, score {h['score']}, {h['status']}): "
                     f"{h['proposed']} in {h['hw1_location']}; evidence: {h['evidence']}")
    else:
        parts.append("Homeworld 1 candidate: none")
    return "\n\n".join(parts)

def ask(prompt):
    req = {
        "model": MODEL, "stream": False, "think": False, "format": SCHEMA, "keep_alive": "5m",
        "options": {"num_ctx": NUM_CTX, "temperature": 0.1, "num_predict": 400},
        "messages": [{"role": "system", "content": SYSTEM}, {"role": "user", "content": prompt}],
    }
    r = urllib.request.Request(OLLAMA + "/api/chat", data=json.dumps(req).encode(),
                               headers={"Content-Type": "application/json"})
    with urllib.request.urlopen(r, timeout=300) as resp:
        out = json.load(resp)
    return json.loads(out["message"]["content"]), out

IDENT = re.compile(r"^[A-Za-z_][A-Za-z0-9_]{0,80}$")

def run(args):
    os.makedirs(OUTDIR, exist_ok=True)
    lockf = open(LOCK, "w")
    try:
        fcntl.flock(lockf, fcntl.LOCK_EX | fcntl.LOCK_NB)
    except BlockingIOError:
        print("another worker is running; exiting")
        return 0
    if args.auto:
        limited, reset, _ = claude_limit_state()
        if not limited:
            print("auto: Claude is not rate-limited; Opus has the work. exiting")
            return 0
    ok, why = gpu_free_for_us()
    if not ok:
        log(f"yield: {why}")
        return 0
    body, names, strs, callees, callers, hw1, queue = load_inputs()
    seen = done_addresses()
    todo = [a for a in queue if a not in seen]
    log(f"start model={MODEL} ctx={NUM_CTX} queue={len(queue)} done={len(seen)} todo={len(todo)} "
        f"mode={'auto' if args.auto else 'force'}")
    t_end = time.time() + args.max_minutes * 60
    n = 0
    with open(OUT, "a") as out:
        for a in todo:
            if n >= args.max_items or time.time() > t_end:
                break
            if n % 10 == 0 and n:
                if args.auto and not claude_limit_state()[0]:
                    log("Claude's limit has reset; handing back to Opus")
                    break
                ok, why = gpu_free_for_us()
                if not ok:
                    log(f"yield mid-batch: {why}")
                    break
            t0 = time.time()
            try:
                res, raw = ask(build_prompt(a, body, strs, callees, callers, hw1))
            except Exception as e:
                log(f"error {a}: {e!r}")
                time.sleep(2)
                continue
            name = res.get("name", "").strip()
            if name and not IDENT.match(name):
                res["confidence"], name = "low", ""
            h = hw1.get(a)
            if h and h["status"] == "weak" and name == h["proposed"] and res.get("confidence") == "high":
                res["confidence"] = "medium"   # echoing a weak string match is not 'high' (calibration, 2026-10-08)
            rec = {"address": a, "name": name, "class": res.get("class", "unknown"),
                   "confidence": res.get("confidence", "low"), "hw1_equivalent": res.get("hw1_equivalent", ""),
                   "evidence": res.get("evidence", "")[:200], "model": MODEL,
                   "ts": dt.datetime.now().isoformat(timespec="seconds"),
                   "ms": int((time.time() - t0) * 1000), "eval_tokens": raw.get("eval_count")}
            out.write(json.dumps(rec) + "\n")
            out.flush()
            os.fsync(out.fileno())
            n += 1
    log(f"stop: named {n} this run; total done {len(seen) + n}/{len(queue)}")
    return 0

def status(_args):
    limited, reset, seen_at = claude_limit_state()
    ok, why = gpu_free_for_us()
    total = done = 0
    try:
        _b, names, *_ = None, None
    except Exception:
        pass
    done = len(done_addresses())
    try:
        idx = list(csv.DictReader(open(os.path.join(EXP, "decompiled_index.csv"))))
        total = sum(1 for r in idx if r["name"].startswith("FUN_") and r["status"] == "ok")
    except OSError:
        pass
    print(f"claude limited : {limited}" + (f" (reset {reset:%H:%M}, seen {seen_at:%H:%M})" if reset else ""))
    print(f"gpu gate       : {'free' if ok else 'BLOCKED'} ({why})")
    print(f"model          : {MODEL} ctx={NUM_CTX}")
    print(f"progress       : {done}/{total} functions proposed -> {OUT}")
    return 0

def export(_args):
    recs = [json.loads(l) for l in open(OUT)] if os.path.exists(OUT) else []
    from collections import Counter
    dup = Counter(r["name"] for r in recs if r["name"])
    props = []
    for r in recs:
        if r["name"] and dup[r["name"]] > 1:
            r = dict(r, confidence="low", evidence=f"[dup x{dup[r['name']]}: audit which address owns this name] " + r["evidence"])
        cls = r["class"]
        if cls == "hw1" and r.get("hw1_equivalent"):
            cls = "hw1:" + r["hw1_equivalent"].split(":")[0]
        props.append({"address": r["address"], "name": r["name"], "kind": "function", "class": cls,
                      "confidence": r["confidence"], "evidence": r["evidence"],
                      "hw1_equivalent": r.get("hw1_equivalent", ""), "signature": ""})
    doc = {"source": "qwen-naming", "method": f"Local {MODEL} via Ollama, one call per unnamed function with decompiled C, referenced strings, callers/callees and the HW1 string-match candidate; JSON-schema constrained output; unaudited until Opus samples it.", "proposals": props}
    p = os.path.join(ROOT, "ghidra/proposals/qwen-naming.json")
    json.dump(doc, open(p, "w"), indent=1)
    print(f"wrote {len(props)} proposals -> {p}")
    return 0

if __name__ == "__main__":
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    sp = ap.add_subparsers(dest="cmd", required=True)
    r = sp.add_parser("run")
    g = r.add_mutually_exclusive_group()
    g.add_argument("--auto", action="store_true", help="only while Claude is rate-limited (timer mode)")
    g.add_argument("--force", action="store_true", help="run regardless of Claude's limit state")
    r.add_argument("--max-items", type=int, default=10_000)
    r.add_argument("--max-minutes", type=float, default=10)
    sp.add_parser("status")
    sp.add_parser("export")
    a = ap.parse_args()
    sys.exit({"run": run, "status": status, "export": export}[a.cmd](a))
