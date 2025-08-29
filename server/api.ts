import express from 'express';
import cors from 'cors';
import multer from 'multer';
import { spawnSync } from 'node:child_process';
import { normalizeMany } from '../utils/normalizeN';
import { factorQuick } from './cprime';

const app = express();
app.use(cors());

// text input (allow plain, octet-stream, or anything)
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

// Single quick shot (JSON body); returns wrapper with {ok:true,...}
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
    return res.json({ ok: true, ...out });
  } catch (e: any) {
    return res.status(500).json({ ok: false, error: e.message || 'factor failed' });
  }
});

// === Classic GET shim expected by the UI (/api/factor?n=...) ===
// Returns the inner cprime JSON (bits, classification, factors, n_str, status:"ok", ...)
app.get('/api/factor', (req, res) => {
  try {
    const n = String(req.query.n ?? '');
    if (!n) return res.status(400).json({ ok:false, error:'missing n' });
    const timeout_ms   = Number(req.query.timeout_ms ?? 4000);
    const p1_B         = Number(req.query.p1_B ?? 600000);
    const rho_restarts = Number(req.query.rho_restarts ?? 128);

    const out = factorQuick({ n, timeout_ms, p1_B, rho_restarts });
    // @ts-ignore prefer raw cprime JSON if present
    if (out.json) return res.json(out.json);
    return res.json(out);
  } catch (e: any) {
    return res.status(500).json({ ok:false, error: e.message || 'factor failed' });
  }
});

const port = Number(process.env.PORT || 3000);
app.listen(port, () => console.log('API listening on :' + port));
