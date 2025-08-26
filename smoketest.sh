#!/usr/bin/env bash
set -euo pipefail

BIN=./cprime_cli_demo
pass=0; fail=0

check_seq () {
  local name="$1"; shift
  local expect="$1"; shift
  local got="$("$BIN" "$@" | tr '\n' ' ' | sed 's/ $//')"
  if [[ "$got" == "$expect" ]]; then
    echo "PASS: $name"
    ((pass++))
  else
    echo "FAIL: $name"
    echo "  expect: $expect"
    echo "  got   : $got"
    ((fail++))
  fi
}

# Test 1: Luby multipliers (1..15) with K=1
check_seq "luby K=1 N=15" \
"1 1 2 1 1 2 4 1 1 2 1 1 2 4 8" \
--rho_iters 1 --rho_restarts 15 --schedule luby

# Test 2: Luby budgets with K=100 for first 10
check_seq "luby K=100 N=10 budgets" \
"100 100 200 100 100 200 400 100 100 200" \
--rho_iters 100 --rho_restarts 10 --schedule luby

# Test 3: Doubling with K=10, cap 80 (budgets)
# Sequence: 10 10 20 40 80 80 80 80 80 80
check_seq "doubling K=10 cap=80 N=10" \
"10 10 20 40 80 80 80 80 80 80" \
--rho_iters 10 --rho_restarts 10 --schedule doubling --cap 80

# Test 4: Fixed always K
check_seq "fixed K=7 N=8" \
"7 7 7 7 7 7 7 7" \
--rho_iters 7 --rho_restarts 8 --schedule fixed

echo
echo "Summary: PASS=$pass FAIL=$fail"
if [[ $fail -gt 0 ]]; then exit 1; fi
