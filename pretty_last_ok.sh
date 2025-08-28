#!/usr/bin/env bash
set -euo pipefail
cd "$(dirname "$0")"

# If we don't already have a success line, find the newest and save it.
if [[ ! -s /tmp/ok.json ]]; then
  src="$( (find logs runs -type f -name '*.log' -print0 2>/dev/null; \
            printf '%s\0' cprime64.log /tmp/cprime64.lotto.out 2>/dev/null || true) \
          | xargs -0r grep -l '"status":"ok"' 2>/dev/null \
          | xargs -r ls -t 2>/dev/null | head -1 || true)"
  [[ -z "${src:-}" ]] && { echo "No success found yet."; exit 2; }
  grep -a '"status":"ok"' "$src" | tail -1 > /tmp/ok.json
fi

# Parse
if command -v jq >/dev/null; then
  N="$(jq -r '.n_str // (.n|tostring)' /tmp/ok.json)"
  mapfile -t ks < <(jq -r '.factors | keys[]' /tmp/ok.json | sort -n | head -2)
  BITS="$(jq -r '.bits // empty' /tmp/ok.json)"
else
  N="$(sed -n 's/.*"n_str":"\([0-9]\+\)".*/\1/p' /tmp/ok.json)"
  read -r -a ks < <(grep -o '"[0-9]\{2,\}": *1' /tmp/ok.json | cut -d'"' -f2 | sort -n | head -2)
  BITS=""
fi

# Fill bits if missing
if [[ -z "${BITS:-}" ]]; then
  BITS="$(python3 - <<'PY' 2>/dev/null || true
import json
try:
  j=json.load(open('/tmp/ok.json')); n=int(j.get('n_str') or j.get('n') or 0)
  print(n.bit_length() if n else "")
except Exception: print("")
PY
)"
fi

p="${ks[0]:-}"; q="${ks[1]:-}"

# Width & ASCII divider
W="${BOX_W_ENV:-$(tput cols 2>/dev/null || echo 100)}"
printf -v DIV '%*s' "$W" ''; DIV="${DIV// /-}"

echo "$DIV"
echo "64x64 SEMIPRIME FACTORED"
echo "bits=${BITS}  host=$(hostname -s)  when=$(date -Is)"
echo "$DIV"
echo "N:"
echo "$N" | fold -w "$W" -s
echo "$DIV"
echo "p = $p"
echo "q = $q"
echo "$DIV"
