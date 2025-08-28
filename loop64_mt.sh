#!/usr/bin/env bash
set -euo pipefail
cd "$(dirname "$0")"

T="${1:-$(nproc)}"
P1B="${2:-500000}"
RRE="${3:-96}"
CPUSET="${4:-}"         # e.g. "0-7"
LOG="${5:-cprime64.log}"

echo "$(date -u +%FT%TZ) START loop64_mt T=$T p1_B=$P1B rho_restarts=$RRE cpuset='${CPUSET}'" | tee -a "$LOG"

while :; do
  N="$(python3 gen64x64.py)"
  ts="$(date -u +%FT%TZ)"
  echo "$ts NEW_N bits=$(python3 - <<PY
n=int("$N"); print(n.bit_length())
PY
) N=$N" | tee -a "$LOG"

  if [[ -n "$CPUSET" ]]; then
    out="$(./factor_mt.sh "$N" "$T" "$P1B" "$RRE" "$CPUSET")"
  else
    out="$(./factor_mt.sh "$N" "$T" "$P1B" "$RRE")"
  fi

  echo "$out" | tee -a "$LOG"

  # quick success/failure note
  if echo "$out" | grep -q '"status":"ok"'; then
    echo "$ts SUCCESS" | tee -a "$LOG"
  else
    echo "$ts NO_SUCCESS" | tee -a "$LOG"
  fi
done
