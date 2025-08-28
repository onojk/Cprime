#!/usr/bin/env bash
set -euo pipefail

# Usage:
#   loop64_luby_mt.sh <threads> <p1_B> <restarts> <cpuset or '-'> <logfile> <N> \
#                     [--iters K] [--schedule S] [--cap ABS] [--timeout SEC] [--extra "..."]

if [[ $# -lt 6 ]]; then
  echo "usage: $0 <threads> <p1_B> <restarts> <cpuset|-> <logfile> <N> [--iters K] [--schedule S] [--cap ABS] [--timeout SEC] [--extra '...']" >&2
  exit 2
fi

T="$1"; P1_B="$2"; RESTARTS="$3"; CPUSET="$4"; LOGFILE="$5"; TARGET="$6"; shift 6

ITERS=75000000
SCHEDULE="luby"
CAP=0
TIMEOUT=0
EXTRA=""

while [[ $# -gt 0 ]]; do
  case "$1" in
    --iters)    ITERS="$2"; shift 2;;
    --schedule) SCHEDULE="$2"; shift 2;;
    --cap)      CAP="$2"; shift 2;;
    --timeout)  TIMEOUT="$2"; shift 2;;
    --extra)    EXTRA="$2"; shift 2;;
    *) echo "unknown arg: $1" >&2; exit 2;;
  esac
done

mkdir -p logs
date +"[loop64_luby_mt] %F %T starting" | tee -a "$LOGFILE"

pids=()
finish() {
  for p in "${pids[@]:-}"; do kill "$p" 2>/dev/null || true; done
}
trap finish INT TERM

for idx in $(seq 0 $((T-1))); do
  tlog="logs/thread_${idx}.log"
  echo "[loop64_luby_mt] spawn thread $idx -> $tlog" | tee -a "$LOGFILE"
  (
    BIND=()
    if [[ "$CPUSET" != "-" ]]; then
      # naive pinning for ranges like 0-7
      CPU=$(awk -v base="$CPUSET" -v i="$idx" 'BEGIN{
        split(base,b,"-"); start=b[1]; end=(length(b[2])?b[2]:b[1]);
        cpu=start + (i % (end-start+1)); printf "%d", cpu;
      }')
      BIND=( taskset -c "$CPU" )
    fi
    exec "${BIND[@]}" ./factor_luby.sh "$TARGET" \
      --p1_B "$P1_B" --restarts "$RESTARTS" --iters "$ITERS" \
      --schedule "$SCHEDULE" ${CAP:+--cap "$CAP"} \
      ${TIMEOUT:+--timeout "$TIMEOUT"} \
      ${EXTRA:+--extra "$EXTRA"} \
      --log "$tlog"
  ) >>"$LOGFILE" 2>&1 &
  pids+=($!)
done

success=0
# Keep waiting until a worker exits with rc==0 (success), or all have failed.
while :; do
  # update pids to those still running
  mapfile -t pids < <(jobs -p)
  ((${#pids[@]})) || break  # none left -> all failed
  if wait -n; rc=$?; then
    # wait -n executed and returned the exit code of one job in $rc
    if [[ $rc -eq 0 ]]; then
      success=1
      echo "[loop64_luby_mt] success detected (one worker exit=0). Stopping others..." | tee -a "$LOGFILE"
      break
    else
      # one worker failed; loop continues to wait on remaining
      :
    fi
  fi
done

finish
if [[ $success -eq 1 ]]; then
  echo "[loop64_luby_mt] DONE: factor found by a worker." | tee -a "$LOGFILE"
  exit 0
else
  echo "[loop64_luby_mt] DONE: all workers finished without success." | tee -a "$LOGFILE"
  exit 1
fi
