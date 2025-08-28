#!/usr/bin/env bash
set -euo pipefail
# find newest log with a success
src="$(find logs runs -type f -name '*.log' 2>/dev/null -print0 \
  | xargs -0r grep -l '"status":"ok"' 2>/dev/null \
  | xargs -r ls -t 2>/dev/null | head -1 || true)"
[[ -z "${src:-}" ]] && { echo "No success found yet."; exit 2; }
grep -a '"status":"ok"' "$src" | tail -1 > /tmp/ok.json
cat /tmp/ok.json
echo
# extract N, p, q (jq optional)
if command -v jq >/dev/null; then
  N="$(jq -r '.n_str // (.n|tostring)' /tmp/ok.json)"
  read p q < <(jq -r '.factors | keys[]' /tmp/ok.json | sort -n | head -2)
else
  N="$(sed -n 's/.*"n_str":"\([0-9]\+\)".*/\1/p' /tmp/ok.json)"
  read p q < <(grep -o '"[0-9]\{2,\}": *1' /tmp/ok.json | cut -d'"' -f2 | sort -n | head -2 | tr '\n' ' ')
fi
echo "N=$N"
echo "p=$p"
echo "q=$q"
