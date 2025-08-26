#!/usr/bin/env bash
set -euo pipefail
cd "$(dirname "$0")"

T="${T:-$(nproc)}"          # threads
P1B="${P1B:-300000}"        # Pollard p-1 bound per worker
RRE="${RRE:-96}"            # rho restarts per worker
CPUSET="${CPUSET:-}"        # e.g. "0-7" to pin

log="cprime64_mt.log"

while :; do
  N="$(python3 gen64x64.py)"
  ts="$(date -u +%Y-%m-%dT%H:%M:%SZ)"
  echo "$ts START T=$T P1B=$P1B RRE=$RRE N=$N" | tee -a "$log"
  ./factor_mt.sh "$N" "$T" "$P1B" "$RRE" "$CPUSET" | tee -a "$log"
  echo "$ts DONE" | tee -a "$log"
done
