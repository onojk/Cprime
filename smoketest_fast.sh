#!/usr/bin/env bash
set -u  # keep unset-var guard, but avoid -e/-o pipefail so we can count failures

pass=0; fail=0

ok()  { printf 'PASS: %s\n' "$1"; ((pass++)); }
bad() { printf 'FAIL: %s\n  expect: %s\n  got   : %s\n' "$1" "$2" "$3"; ((fail++)); }

# Helpers (no jq)
has() { grep -Fq "$2" <<<"$1"; return $?; }

# 1) prime 13
name="prime 13"
out="$(./cprime_cli_demo prime 13 2>&1)"; rc=$?
if (( rc == 0 )) && has "$out" '"classification":"prime"'; then
  ok "$name"
else
  bad "$name" '"classification":"prime"' "$out"
fi

# 2) prime known 64-bit prime
name="prime 18446744073709551557 (64-bit)"
P64=18446744073709551557
out="$(./cprime_cli_demo prime "$P64" 2>&1)"; rc=$?
if (( rc == 0 )) && has "$out" '"classification":"prime"' && has "$out" '"bits": 64'; then
  ok "$name"
else
  bad "$name" '"classification":"prime" and "bits": 64' "$out"
fi

# 3) factor known 64-bit semiprime (p=4294967291, q=4294967279)
name="factor 18446743979220271189 (rho)"
N=18446743979220271189
out="$(./cprime_cli_demo factor "$N" --timeout_ms 5000 --rho_restarts 256 2>&1)"; rc=$?
if (( rc == 0 )) && has "$out" '"classification":"composite"' && has "$out" '"4294967291": 1' && has "$out" '"4294967279": 1'; then
  ok "$name"
else
  bad "$name" 'composite with factors 4294967291 and 4294967279' "$out"
fi

echo
printf 'Summary: PASS=%d FAIL=%d\n' "$pass" "$fail"
exit $(( fail == 0 ? 0 : 1 ))
