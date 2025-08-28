#!/usr/bin/env bash
set -euo pipefail
cd "$(dirname "$0")"

T="${1:-$(nproc)}"          # threads/workers
P1B="${2:-600000}"          # Pollard p-1 bound
RRE="${3:-128}"             # rho restarts per worker
CPUSET="${4:-}"             # e.g. 0-7 or leave blank
CAP_S="${5:-90}"            # fail-fast cap per round (seconds)
LOG="${6:-cprime64.log}"    # log file

ts(){ date -u +%FT%TZ; }

gen_semiprime_64x64(){  # pure-Python 64b primes (no sympy)
python3 - <<'PY'
import random
def is_probable_prime(n:int)->bool:
    if n<2: return False
    small=(2,3,5,7,11,13,17,19,23,29,31,37)
    for p in small:
        if n%p==0: return n==p
    # deterministic MR for 64-bit
    d=n-1; s=0
    while d%2==0: d//=2; s+=1
    for a in (2,325,9375,28178,450775,9780504,1795265022):
        if a%n==0: continue
        x=pow(a,d,n)
        if x==1 or x==n-1: continue
        for _ in range(s-1):
            x=pow(x,2,n)
            if x==n-1: break
        else:
            return False
    return True

def rand_prime64():
    while True:
        n = (random.getrandbits(64) | (1<<63) | 1)
        if is_probable_prime(n): return n

p=rand_prime64(); q=rand_prime64()
while q==p: q=rand_prime64()
print(p*q)
PY
}

mkdir -p runs
round=0
echo "$(ts) START lotto T=$T P1B=$P1B RRE=$RRE CAP_S=${CAP_S}s CPUSET='${CPUSET:-all}'" | tee -a "$LOG"

while :; do
  round=$((round+1))
  N="$(gen_semiprime_64x64)"
  echo "$(ts) ROUND=$round NEW_N bits=127 N=$N" | tee -a "$LOG"

  if timeout -k 2s "${CAP_S}s" ./factor_mt.sh "$N" "$T" "$P1B" "$RRE" "${CPUSET:-}" | tee -a "$LOG"; then
    echo "$(ts) ROUND=$round RESULT=SUCCESS" | tee -a "$LOG"
  else
    echo "$(ts) ROUND=$round RESULT=CAP_HIT_RETRY" | tee -a "$LOG"
  fi
done
