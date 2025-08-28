#!/usr/bin/env bash
# Robust smoketest: runs all cases, aggregates results
set -u

BIN=./cprime_cli_demo
pass=0; fail=0

have_timeout() { command -v timeout >/dev/null 2>&1; }

check () {
  local name="$1"; shift
  local expect="$1"; shift
  local got rc
  if have_timeout; then
    got=$(timeout 5s "$BIN" "$@" 2>&1 | tr '\n' ' ' | sed 's/ $//'); rc=$?
  else
    got=$("$BIN" "$@" 2>&1 | tr '\n' ' ' | sed 's/ $//'); rc=$?
  fi
  if [ $rc -ne 0 ]; then
    echo "FAIL: $name"; echo "  exit code: $rc"; echo "  stderr/stdout: $got"; ((fail++)); return 0
  fi
  if [ "$got" = "$expect" ]; then echo "PASS: $name"; ((pass++))
  else echo "FAIL: $name"; echo "  expect: $expect"; echo "  got   : $got"; ((fail++)); fi
  return 0
}

check "luby K=1 N=15" "1 1 2 1 1 2 4 1 1 2 1 1 2 4 8" --rho_iters 1 --rho_restarts 15 --schedule luby
check "luby K=100 N=10 budgets" "100 100 200 100 100 200 400 100 100 200" --rho_iters 100 --rho_restarts 10 --schedule luby
check "doubling K=10 cap=80 N=10" "10 10 20 40 80 80 80 80 80 80" --rho_iters 10 --rho_restarts 10 --schedule doubling --cap 80
check "fixed K=7 N=8" "7 7 7 7 7 7 7 7" --rho_iters 7 --rho_restarts 8 --schedule fixed

echo; echo "Summary: PASS=$pass FAIL=$fail"; [ "$fail" -eq 0 ] || exit 1
