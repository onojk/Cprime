#!/usr/bin/env bash
set -euo pipefail
cd "$(dirname "$0")"
N="$(python3 gen64x64.py)"
echo "N=${N}"
./cprime factor "${N}" --timeout_ms 0 --p1_B 200000 --rho_restarts 64 --rho_iters 0 | tee -a cprime64.log
