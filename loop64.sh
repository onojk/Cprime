#!/usr/bin/env bash
set -euo pipefail
cd "$(dirname "$0")"
while :; do
  N="$(python3 gen64x64.py)"
  ts="$(date -u +%Y-%m-%dT%H:%M:%SZ)"
  echo "${ts} START N=${N}" | tee -a cprime64.log
  ./cprime factor "${N}" --timeout_ms 0 --p1_B 300000 --rho_restarts 96 --rho_iters 0 | tee -a cprime64.log
  echo "${ts} DONE" | tee -a cprime64.log
done
