import { normalizeMany } from '../utils/normalizeN';

const samples = [
  // decimal
  '247333061729551307576997189786129830309',
  // 0x-hex
  '0xf7e75fdc469067ffdc4e847c51f452df',
  // colon-hex (OpenSSL dump style)
  'f7:e7:5f:dc:46:90:67:ff:dc:4e:84:7c:51:f4:52:df',
  // expression: p*q
  '15148685386245244409 * 16327031384130905101',
  // expression: 2^k ± c
  '2^127-1',
  // ssh-rsa (short fake example; replace with a real public key line if you have one)
  'ssh-rsa AAAAB3NzaC1yc2EAAAADAQABAAABAQC7',
  // JWK (toy)
  '{"kty":"RSA","n":"sXh","e":"AQAB"}'
];

for (const s of samples) {
  try {
    const out = normalizeMany(s);
    console.log('INPUT:', s);
    for (const o of out) console.log(' ->', o.source, 'bits=', o.bits, 'nDec=', o.nDec.slice(0,40)+'...');
  } catch (e:any) {
    console.log('INPUT:', s, 'ERROR:', e.message);
  }
}
