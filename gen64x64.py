import random

# Deterministic MR bases for n < 2^64
_BASES = (2, 325, 9375, 28178, 450775, 9780504, 1795265022)

def is_probable_prime(n: int) -> bool:
    if n < 2:
        return False
    small = (2,3,5,7,11,13,17,19,23,29,31,37)
    for p in small:
        if n % p == 0:
            return n == p
    d = n - 1
    s = (d & -d).bit_length() - 1
    d >>= s
    for a in _BASES:
        a %= n
        if a in (0,1,n-1): 
            continue
        x = pow(a, d, n)
        if x in (1, n-1):
            continue
        for _ in range(s-1):
            x = (x*x) % n
            if x == n-1:
                break
        else:
            return False
    return True

def random_prime(bits: int) -> int:
    assert bits >= 2
    while True:
        n = random.getrandbits(bits)
        n |= (1 << (bits-1)) | 1   # force top bit + odd
        if is_probable_prime(n):
            return n

if __name__ == "__main__":
    p = random_prime(64)
    q = random_prime(64)
    print(p*q)
