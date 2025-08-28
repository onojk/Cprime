#!/usr/bin/env bash
set -euo pipefail
LOG="${1:-cprime64.log}"
OUT="/tmp/pretty.log"
: > "$OUT"
tail -F "$LOG" 2>/dev/null | stdbuf -oL grep --line-buffered '"status":"ok"' | \
while IFS= read -r line; do
  printf '%s\n' "$line" > /tmp/ok.json
  BOX_W_ENV="${BOX_W_ENV:-$(tput cols 2>/dev/null || echo 100)}" ./pretty_last_ok.sh | tee -a "$OUT" >/dev/null
  printf '\n' >> "$OUT"
done
