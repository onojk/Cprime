#!/usr/bin/env bash
set -euo pipefail
cd "$(dirname "$0")"

# leave a couple cores free for cooling / desktop
NPROC=$(nproc); T=$(( NPROC>2 ? NPROC-2 : 1 ))
P1B="${P1B:-600000}"
RRE="${RRE:-128}"
CAP_S="${CAP_S:-90}"         # per-attempt cap (seconds) -> fail-fast
CPUSET="${CPUSET:-0-$((T-1))}"
LOG="${LOG:-cprime64.log}"

# build quietly
make -j >/dev/null 2>&1 || true

echo "$(date -u +%FT%TZ) [watchdog] start T=$T P1B=$P1B RRE=$RRE CAP_S=$CAP_S CPUSET=$CPUSET LOG=$LOG"

while :; do
  # kill any stragglers from previous runs
  pkill -f 'factor_mt.sh|./cprime factor' >/dev/null 2>&1 || true
  # run one capped campaign; loop64_mt.sh understands CAP_S as last arg
  ./loop64_mt.sh "$T" "$P1B" "$RRE" "$CPUSET" "$LOG" "$CAP_S" || true
  echo "$(date -u +%FT%TZ) [watchdog] loop exited; restarting in 3s"
  sleep 3
done
