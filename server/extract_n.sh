#!/usr/bin/env bash
set -euo pipefail
tmp="$(mktemp -d)"; in="$tmp/in.bin"; pem="$tmp/in.pem"
trap 'rm -rf "$tmp"' EXIT

cat > "$in"

# Heuristics
if head -c 20 "$in" | grep -q '^ssh-rsa '; then
  # ssh-rsa line -> PKCS8 PEM via ssh-keygen, then extract
  mv "$in" "$tmp/in.pub"
  ssh-keygen -e -m PKCS8 -f "$tmp/in.pub" > "$pem" 2>/dev/null
elif grep -q '-----BEGIN ' "$in"; then
  cp "$in" "$pem"
else
  # Maybe DER cert or DER SubjectPublicKeyInfo
  # Try to convert DER cert to PEM
  if openssl x509 -inform DER -in "$in" -noout >/dev/null 2>&1; then
    openssl x509 -inform DER -in "$in" -out "$pem" >/dev/null 2>&1
  else
    # Assume DER SPKI -> wrap through pkey to PEM
    openssl pkey -pubin -inform DER -in "$in" -out "$pem" 2>/dev/null || {
      echo "unsupported binary format" >&2; exit 2;
    }
  fi
fi

# Try common extract paths (OpenSSL 1.1 / 3.0 variants)
# 1) X.509 cert PEM
if grep -q 'BEGIN CERTIFICATE' "$pem"; then
  m="$(openssl x509 -noout -modulus -in "$pem" 2>/dev/null | sed 's/^Modulus=//')"
  if [ -n "${m:-}" ]; then echo "$m"; exit 0; fi
fi

# 2) RSA public/private PEM
m="$(openssl rsa -pubin -in "$pem" -noout -modulus 2>/dev/null | sed 's/^Modulus=//')"
if [ -n "${m:-}" ]; then echo "$m"; exit 0; fi
m="$(openssl rsa -in "$pem" -noout -modulus 2>/dev/null | sed 's/^Modulus=//')"
if [ -n "${m:-}" ]; then echo "$m"; exit 0; fi

# 3) Generic SubjectPublicKeyInfo -> DER -> rsa -pubin
if openssl pkey -pubin -in "$pem" -outform DER -out "$tmp/spki.der" 2>/dev/null; then
  m="$(openssl rsa -pubin -inform DER -in "$tmp/spki.der" -noout -modulus 2>/dev/null | sed 's/^Modulus=//')"
  if [ -n "${m:-}" ]; then echo "$m"; exit 0; fi
fi

echo "failed to extract RSA modulus" >&2
exit 2
