#!/usr/bin/env bash
set -euo pipefail

# Usage:
#   factor_luby.sh <N> [--p1_B B] [--restarts N] [--iters K] [--schedule S] [--cap CAP]
#                  [--timeout SEC] [--extra "..."] [--log FILE]
#
# Strategy:
# - Generate budgets via cprime_cli_demo (Luby/Doubling/Fixed)
# - For each budget, run ./cprime with --rho_restarts 1 --rho_iters <budget>
# - Capture output; EXIT 0 ONLY IF output shows a factor; otherwise CONTINUE

if [[ $# -lt 1 ]]; then
  echo "usage: $0 <N> ..." >&2
  exit 2
fi

TARGET="$1"; shift
P1_B=600000
RESTARTS=256
ITERS=75000000
SCHEDULE="luby"
CAP=0
TIMEOUT=0
EXTRA=""
LOG=""

while [[ $# -gt 0 ]]; do
  case "$1" in
    --p1_B)       P1_B="$2"; shift 2;;
    --restarts)   RESTARTS="$2"; shift 2;;
    --iters)      ITERS="$2"; shift 2;;
    --schedule)   SCHEDULE="$2"; shift 2;;
    --cap)        CAP="$2"; shift 2;;
    --timeout)    TIMEOUT="$2"; shift 2;;
    --extra)      EXTRA="$2"; shift 2;;
    --log)        LOG="$2"; shift 2;;
    *) echo "unknown arg: $1" >&2; exit 2;;
  esac
done

GEN=( ./cprime_cli_demo --rho_iters "$ITERS" --rho_restarts "$RESTARTS" --schedule "$SCHEDULE" )
if [[ "$CAP" != "0" ]]; then GEN+=( --cap "$CAP" ); fi

i=0
while read -r BUDGET; do
  i=$((i+1))
  [[ -z "$BUDGET" ]] && continue
  echo "[factor_luby] attempt #$i budget=$BUDGET" >&2

  CMD=( ./cprime_strict.sh factor "$TARGET" --timeout_ms 0 --p1_B "$P1_B" --rho_restarts 1 --rho_iters "$BUDGET" )
  if [[ -n "$EXTRA" ]]; then
    # shellcheck disable=SC2206
    EXTRA_ARR=( $EXTRA )
    CMD+=( "${EXTRA_ARR[@]}" )
  fi

  tmp="$(mktemp)"; trap 'rm -f "$tmp"' EXIT
  if [[ -n "$LOG" ]]; then
    if [[ "$TIMEOUT" != "0" ]]; then
      ( timeout "${TIMEOUT}s" "${CMD[@]}" ) >"$tmp" 2>&1 | tee -a "$LOG"
      rc=${PIPESTATUS[0]}
    else
      ( "${CMD[@]}" ) >"$tmp" 2>&1 | tee -a "$LOG"
      rc=${PIPESTATUS[0]}
    fi
  else
    if [[ "$TIMEOUT" != "0" ]]; then
      timeout "${TIMEOUT}s" "${CMD[@]}" >"$tmp" 2>&1
      rc=$?
    else
      "${CMD[@]}" >"$tmp" 2>&1
      rc=$?
    fi
  fi

  out="$(cat "$tmp")"

  # Success detection
  if echo "$out" | grep -Eqi '"status"\s*:\s*"found"|"factors"\s*:'; then
    echo "[factor_luby] success (JSON indicates factor) on attempt #$i" >&2
    exit 0
  fi
  if echo "$out" | grep -Eqi '\bfactor\b|\bFOUND\b'; then
    echo "[factor_luby] success (text indicates factor) on attempt #$i" >&2
    exit 0
  fi
  if echo "$out" | grep -Eqi 'gcd[^0-9]*=[[:space:]]*[2-9]'; then
    echo "[factor_luby] success (nontrivial gcd detected) on attempt #$i" >&2
    exit 0
  fi

  # Explicit noresult -> keep going
  if echo "$out" | grep -Eqi '"status"\s*:\s*"noresult"'; then
    continue
  fi

  # If we get here: no factor evidence, continue
  continue

done < <("${GEN[@]}")

echo "[factor_luby] exhausted $RESTARTS attempts without success" >&2
exit 1
