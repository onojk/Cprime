#!/usr/bin/env bash
set -euo pipefail
cd "$(dirname "$0")"

N="${1:?usage: factor_mt.sh <N> [threads] [p1_B] [rho_restarts_per_worker] [cpuset]}" 
T="${2:-$(nproc)}"
P1B="${3:-300000}"
RRE="${4:-64}"
CPUSET="${5:-}"   # e.g. "0-7" to pin

tmpdir="$(mktemp -d -t cprime_mt.XXXXXX)"
pfile="$tmpdir/pids"; touch "$pfile"

cleanup(){
  while read -r p; do kill "$p" 2>/dev/null || true; done <"$pfile" || true
  rm -rf "$tmpdir"
}
trap cleanup EXIT

launch() {
  i="$1"; out="$tmpdir/out.$i"
  # small jitter to decorrelate random walks
  sleep "0.$(( (RANDOM%90)+10 ))"
  cmd=( ./cprime factor "$N" --timeout_ms 0 --p1_B "$P1B" --rho_restarts "$RRE" --rho_iters 0 )
  if [[ -n "$CPUSET" ]]; then
    taskset -c "$CPUSET" "${cmd[@]}" >"$out" 2>&1 &
  else
    "${cmd[@]}" >"$out" 2>&1 &
  fi
  echo $! >> "$pfile"
}

for i in $(seq 1 "$T"); do launch "$i"; done

# poll for the first success
while :; do
  for f in "$tmpdir"/out.*; do
    [[ -e "$f" ]] || continue
    if grep -q '"status":"ok"' "$f"; then
      cat "$f"
      exit 0
    fi
  done
  sleep 0.2
done
