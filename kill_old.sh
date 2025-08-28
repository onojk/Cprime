#!/usr/bin/env bash
set -euo pipefail
pkill -f loop64_mt.sh || true
pkill -9 -f "./cprime factor" || true
pkill -9 -f cprime_cli_demo || true
echo "Killed stray cprime/loop64 processes."
