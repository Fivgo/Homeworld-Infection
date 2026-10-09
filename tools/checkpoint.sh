#!/usr/bin/env bash
# checkpoint.sh "message" - commit all tracked project work so a rate limit can never lose it.
# Refuses to commit game binaries/data, third-party clones, the Wine prefix or Ghidra databases.
set -euo pipefail
ROOT="$(cd "$(dirname "$(readlink -f "$0")")/.." && pwd)"; cd "$ROOT"
MSG="${1:-checkpoint}"
# refresh the snapshot of the local Qwen worker's progress into the resume doc
QS="$(python3 tools/qwen_worker.py status 2>/dev/null | sed 's/^/    /' || true)"
python3 - "$QS" <<'PY'
import re,sys,datetime
p="cycle/STATE.md"; s=open(p).read()
block="<!-- qwen-status -->\n```\n"+sys.argv[1]+"\n```\n<!-- /qwen-status -->"
s=re.sub(r"<!-- qwen-status -->.*?<!-- /qwen-status -->", lambda m: block, s, flags=re.S)
s=re.sub(r"Last checkpoint: .*", "Last checkpoint: "+datetime.datetime.now().strftime("%Y-%m-%d %H:%M %Z").strip(), s)
open(p,"w").write(s)
PY
git add -A
if git diff --cached --name-only | grep -E '^(original|reference|prefix)/|^ghidra/(exports|projects)/|\.(exe|big|dll|vce|wxd)$'; then
  echo "REFUSING: forbidden path staged (see above)"; git reset -q; exit 1
fi
if git diff --cached --quiet; then echo "nothing to checkpoint"; exit 0; fi
git -c user.name=evanmedinger -c user.email=thefiveegos@gmail.com commit -q -m "$MSG" -m "${CHECKPOINT_COAUTHOR:-Co-Authored-By: Claude Fable 5.1 <noreply@anthropic.com>}"
git log --oneline -1
