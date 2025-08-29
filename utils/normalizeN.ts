export type NormItem = { nDec: string; bits: number; source: string; note?: string };

const b10 = (x: bigint) => x.toString(10);
const bitsOf = (nDec: string) => (nDec === '0' ? 0 : BigInt(nDec).toString(2).length);

function decOnly(s: string) {
  return /^[0-9]{2,}$/.test(s);
}
function hexLooseToBig(s: string) {
  let h = s.trim().toLowerCase();
  h = h.replace(/^0x/, '').replace(/[^0-9a-f]/g, ''); // drop colons/spaces
  if (!h) throw new Error('no hex digits');
  return BigInt('0x' + h);
}
function parsePow(s: string) {
  // e.g. 2^127-1, 3^100+7
  const m = s.replace(/\s+/g, '').match(/^([0-9]+)\^([0-9]+)([+\-]([0-9]+))?$/);
  if (!m) return null;
  const a = BigInt(m[1]), k = BigInt(m[2]);
  const c = m[3] ? (m[3][0] === '-' ? -BigInt(m[4]) : BigInt(m[4])) : 0n;
  let n = 1n;
  for (let i = 0n; i < k; i++) n *= a;
  n += c;
  return n;
}
function parseMul(s: string) {
  // supports dec or hex terms: 123*456  or  0xabc * 0xdef
  const m = s.replace(/\s+/g, '').match(/^([0-9a-fx:]+)\*([0-9a-fx:]+)$/i);
  if (!m) return null;
  const a = m[1].match(/^[0-9]+$/) ? BigInt(m[1]) : hexLooseToBig(m[1]);
  const b = m[2].match(/^[0-9]+$/) ? BigInt(m[2]) : hexLooseToBig(m[2]);
  return a * b;
}
function b64urlToBigInt(n: string) {
  const pad = '==='.slice((n.length + 3) % 4);
  const b64 = n.replace(/-/g, '+').replace(/_/g, '/') + pad;
  const buf = Buffer.from(b64, 'base64');
  let hex = buf.toString('hex').replace(/^0+/, '');
  if (!hex) hex = '0';
  return BigInt('0x' + hex);
}

export function normalizeMany(input: string): NormItem[] {
  const s = (input || '').trim();
  const out: NormItem[] = [];
  if (!s) throw new Error('empty input');

  // JWK JSON (kty=RSA)
  try {
    const j = JSON.parse(s);
    if (j && j.kty === 'RSA' && typeof j.n === 'string') {
      const n = b64urlToBigInt(j.n);
      const nDec = b10(n);
      out.push({ nDec, bits: bitsOf(nDec), source: 'jwk' });
      return out;
    }
  } catch { /* not JSON */ }

  // PEM/cert/ssh go to server extractor (we let API handle it)
  if (/-----BEGIN|ssh-rsa|CERTIFICATE/.test(s)) {
    throw new Error('PEM/SSH/cert provided: send as file or text to /api/normalize-file or /api/normalize-text');
  }

  // p*q
  const mul = parseMul(s);
  if (mul !== null) {
    const nDec = b10(mul);
    out.push({ nDec, bits: bitsOf(nDec), source: 'expr' });
    return out;
  }

  // 2^k±c
  const pow = parsePow(s);
  if (pow !== null) {
    const nDec = b10(pow);
    out.push({ nDec, bits: bitsOf(nDec), source: 'pow' });
    return out;
  }

  // 0x-hex or colon-hex or bare hex
  if (/^0x[0-9a-f]+$/i.test(s) || /^[0-9a-f: ]+$/i.test(s)) {
    const n = hexLooseToBig(s);
    const nDec = b10(n);
    out.push({ nDec, bits: bitsOf(nDec), source: 'hex' });
    return out;
  }

  // pure decimal
  if (decOnly(s)) {
    out.push({ nDec: s.replace(/^0+/, '') || '0', bits: bitsOf(s), source: 'decimal' });
    return out;
  }

  throw new Error('Unrecognized format. Try decimal, 0x-hex/colon-hex, p*q, 2^k±c, JWK, PEM/SSH/cert.');
}
