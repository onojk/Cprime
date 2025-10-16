import { spawnSync } from 'node:child_process';
import fs from 'node:fs';
const J = (s:string)=>{ try{return JSON.parse(s);}catch{return null;} };

export function factorQuick(opts:{ n:string; timeout_ms?:number; p1_B?:number; rho_restarts?:number; }){
  const n = String(opts.n);
  const timeout_ms = Math.max(1, Number(opts.timeout_ms ?? 4000));
  const rho_restarts = Number(opts.rho_restarts ?? 128);

  // prefer ./cprime (new or old)
  if (fs.existsSync('./cprime')) {
    // try new-style with "factor"
    let p = spawnSync('./cprime', [
      'factor', n, '--timeout_ms', String(timeout_ms),
      '--p1_B', String(opts.p1_B ?? 600000),
      '--rho_restarts', String(rho_restarts), '--rho_iters','0'
    ], { encoding:'utf8' });
    // if it complains, try old-style argv + stdin fallbacks
    if (p.status !== 0 && /Unknown arg: factor|Usage: \.\/cprime /.test((p.stderr||''))) {
      const cap = Math.max(1, Math.floor(timeout_ms/1000));
      const base = ['--rho_iters','0','--rho_restarts', String(rho_restarts),'--cap', String(cap)];
      p = spawnSync('./cprime', [...base, n], { encoding:'utf8' });
      if (p.status !== 0) p = spawnSync('./cprime', base, { input:n+'\n', encoding:'utf8' });
    }
    if (p.status === 0) { const j=J(p.stdout); return j?{ok:true,json:j}:{ok:true,raw:p.stdout.trim()}; }
  }
  // fallback demo binary
  if (fs.existsSync('./cprime_cli_demo')) {
    const cap = Math.max(1, Math.floor(timeout_ms/1000));
    const base = ['--rho_iters','0','--rho_restarts', String(rho_restarts),'--cap', String(cap)];
    let p = spawnSync('./cprime_cli_demo', [...base, n], { encoding:'utf8' });
    if (p.status !== 0) p = spawnSync('./cprime_cli_demo', base, { input:n+'\n', encoding:'utf8' });
    if (p.status === 0) { const j=J(p.stdout); return j?{ok:true,json:j}:{ok:true,raw:p.stdout.trim()}; }
  }
  throw new Error('no factor backend found (need ./cprime or ./cprime_cli_demo)');
}
