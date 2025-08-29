import express from 'express';
import cors from 'cors';
import multer from 'multer';
import { spawnSync } from 'node:child_process';
import { normalizeMany } from '../utils/normalizeN';
import { factorQuick } from './cprime';

const app = express();
app.use(cors());
const textBody = express.text({ type: ['text/plain','application/octet-stream','*/*'], limit: '5mb' });
const upload   = multer({ limits: { fileSize: 10 * 1024 * 1024 } });

function hexToDec(hex: string): string {
  const h = hex.replace(/^0x/i, '').replace(/^0+/, '') || '0';
  return BigInt('0x' + h).toString(10);
}
function extractViaScript(buf: Buffer) {
  const sh = spawnSync('bash', ['server/extract_n.sh'], { input: buf });
  if (sh.status !== 0) throw new Error((sh.stderr?.toString() || 'extract failed').trim());
  const hex = sh.stdout.toString().trim();
  const nDec = hexToDec(hex);
  const bits = BigInt(nDec).toString(2).length;
  return { nDec, bits, source: 'pem' as const };
}

app.get('/api/health', (_req, res) => res.json({ ok: true }));

app.post('/api/normalize-text', textBody, (req, res) => {
  const body = (req.body ?? '').toString();
  try {
    if (!body) return res.status(400).json({ ok:false, error:'empty input' });
    if (/-----BEGIN|ssh-rsa|CERTIFICATE/.test(body)) {
      const item = extractViaScript(Buffer.from(body));
      return res.json({ ok: true, items: [item] });
    }
    const items = normalizeMany(body);
    return res.json({ ok: true, items });
  } catch (e: any) {
    return res.status(400).json({ ok: false, error: e.message || 'parse error' });
  }
});

app.post('/api/normalize-file', upload.single('file'), (req, res) => {
  try {
    if (!req.file) return res.status(400).json({ ok: false, error: 'no file' });
    const item = extractViaScript(req.file.buffer);
    return res.json({ ok: true, items: [item] });
  } catch (e: any) {
    return res.status(400).json({ ok: false, error: e.message || 'extract error' });
  }
});

// single quick shot
app.post('/api/factor-quick', express.json(), (req, res) => {
  try {
    const { n, timeout_ms, p1_B, rho_restarts } = req.body || {};
    if (!n) return res.status(400).json({ ok: false, error: 'missing n' });
    const out = factorQuick({
      n: String(n),
      timeout_ms: timeout_ms ?? 4000,
      p1_B: p1_B ?? 600000,
      rho_restarts: rho_restarts ?? 128,
    });
    return res.json({ ok:true, ...out });
  } catch (e: any) {
    return res.status(500).json({ ok: false, error: e.message || 'factor failed' });
  }
});

// lotto wrapper: keep trying until budget is used or factors found
app.post('/api/lotto128_factor', express.json(), async (req, res) => {
  try {
    const { n } = req.body || {};
    let { budget_ms, per_try_ms, rho_restarts } = req.body || {};
    if (!n) return res.status(400).json({ ok:false, error:'missing n' });

    budget_ms     = Math.max(1000, Number(budget_ms ?? 15000));
    per_try_ms    = Math.max(500,  Math.min(Number(per_try_ms ?? 2000), budget_ms));
    rho_restarts  = Number(rho_restarts ?? 128);

    const t0 = Date.now();
    let tries = 0;

    while (Date.now() - t0 < budget_ms) {
      const out:any = factorQuick({
        n: String(n),
        timeout_ms: per_try_ms,
        p1_B: 600000,
        rho_restarts,
      });
      tries++;

      // unify success detection across engines
      let p:string|undefined, q:string|undefined;

      // (a) cprime_cli_demo style JSON: {classification:'composite', factors:{ "p":1, "q":1 }, status:'ok'}
      const j:any = out?.json ?? out; // some wrappers put JSON at top level
      if (j && j.factors && typeof j.factors === 'object') {
        const ks = Object.keys(j.factors).sort((a,b)=>BigInt(a)<BigInt(b)?-1:1);
        if (ks.length >= 2) { p = ks[0]; q = ks[1]; }
      }

      // (b) legacy style: {found:true, p:'...', q:'...'}
      if (!p && (j?.found === true) && j.p && j.q) { p = j.p; q = j.q; }

      if (p && q) {
        return res.json({
          ok: true, engine: 'lotto-128', found: true,
          n: String(n), p, q, tries_total: tries, elapsed_ms: Date.now() - t0
        });
      }
    }

    return res.json({
      ok: true, engine: 'lotto-128', found: false,
      n: String(n), tries_total: tries, elapsed_ms: Date.now() - t0
    });
  } catch (e:any) {
    return res.status(500).json({ ok:false, error: e.message || 'lotto failed' });
  }
});

const port = Number(process.env.PORT || 3000);
app.listen(port, () => console.log('API listening on :' + port));
