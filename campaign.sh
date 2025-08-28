#!/usr/bin/env bash
set -euo pipefail

# Usage (defaults are sane; override via env or flags):
#   ./campaign.sh --n 1384... --threads 8 --cpus 0-7 \
#       --p1_B 600000 --restarts 256 --iters 75000000 \
#       --schedule luby --cap 800000000 --timeout 90
#
# It will loop rounds until loop64_luby_mt.sh returns 0 (true success).
# On each failed round, it gently scales K, CAP, and R.

# -------- defaults --------
N=""; T=8; CPUSET="0-7"; P1_B=600000
R=256                 # restarts per worker
K=75000000            # base per-restart budget
CAP=800000000         # absolute per-restart cap
SCHED="luby"          # {luby|doubling|fixed}
TO=90                 # per-attempt timeout (seconds)
EXTRA=""              # extra args for cprime

# -------- parse flags --------
while [[ $# -gt 0 ]]; do
  case "$1" in
    --n) N="$2"; shift 2;;
    --threads) T="$2"; shift 2;;
    --cpus) CPUSET="$2"; shift 2;;
    --p1_B) P1_B="$2"; shift 2;;
    --restarts) R="$2"; shift 2;;
    --iters) K="$2"; shift 2;;
    --cap) CAP="$2"; shift 2;;
    --schedule) SCHED="$2"; shift 2;;
    --timeout) TO="$2"; shift 2;;
    --extra) EXTRA="$2"; shift 2;;
    *) echo "unknown arg: $1" >&2; exit 2;;
  esac
done

if [[ -z "$N" ]]; then
  echo "usage: $0 --n <integer> [--threads T] [--cpus set] [--p1_B B] [--restarts R] [--iters K] [--cap CAP] [--schedule S] [--timeout TO] [--extra '...']" >&2
  exit 2
fi

mkdir -p runs
ROUND=0

cleanup() { ./kill_old.sh || true; }
trap cleanup INT TERM

while :; do
  ROUND=$((ROUND+1))
  ts=$(date +"%Y%m%d_%H%M%S")
  LOG="runs/run_${ts}_round${ROUND}.log"

  echo "[campaign] round=${ROUND} N=${N}"
  echo "[campaign] T=${T} CPUSET=${CPUSET} P1_B=${P1_B}"
  echo "[campaign] R=${R} K=${K} CAP=${CAP} SCHED=${SCHED} TO=${TO}"
  echo "[campaign] log=${LOG}"
  echo

  set +e
  ./loop64_luby_mt.sh "${T}" "${P1_B}" "${R}" "${CPUSET}" "${LOG}" "${N}" \
    --iters "${K}" --schedule "${SCHED}" --cap "${CAP}" --timeout "${TO}" \
    ${EXTRA:+--extra "${EXTRA}"}
  rc=$?
  set -e

  if [[ $rc -eq 0 ]]; then
    echo "[campaign] SUCCESS on round ${ROUND}"
    exit 0
  fi

  echo "[campaign] No success; scaling parameters..."

  # --- scaling policy (gentle ramps) ---
  # 1) bump K every round (+25M)
  K=$(( K + 25000000 ))

  # 2) every 2 rounds, bump CAP (+200M)
  if (( ROUND % 2 == 0 )); then
    CAP=$(( CAP + 200000000 ))
  fi

  # 3) at rounds 2,4, bump restarts up to 512 max
  if (( ROUND == 2 || ROUND == 4 )); then
    if (( R < 512 )); then R=$(( R * 2 )); fi
  fi

  # 4) optional: flip schedule after 6 rounds to doubling for bigger chunks
  if (( ROUND == 6 )); then SCHED="doubling"; fi

  # quick cool-down & cleanup
  sleep 2
  ./kill_old.sh || true
done
