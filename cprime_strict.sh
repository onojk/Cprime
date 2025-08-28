#!/usr/bin/env bash
# Wrapper for ./cprime that enforces strict exit codes:
# - 0 only if output clearly shows a factor
# - 1 if "noresult" or nothing found
# - 2 on invocation error

set -euo pipefail
if [[ $# -lt 1 ]]; then
  echo "usage: $0 <cprime args...>" >&2
  exit 2
fi

tmp="$(mktemp)"; trap 'rm -f "$tmp"' EXIT

# Run cprime, capture stdout+stderr
if ! ./cprime "$@" >"$tmp" 2>&1; then
  # even if ./cprime exits non-zero, we still parse; if it shows a factor we'll return 0
  true
fi

cat "$tmp"

out="$(cat "$tmp")"

# Success signals (JSON or text)
if echo "$out" | grep -Eqi '"status"\s*:\s*"found"|"factors"\s*:|\bfactor\b|\bFOUND\b|gcd[^0-9]*=[[:space:]]*[2-9]'; then
  exit 0
fi

# Explicit noresult
if echo "$out" | grep -Eqi '"status"\s*:\s*"noresult"'; then
  exit 1
fi

# Default: nothing conclusive -> not found
exit 1
