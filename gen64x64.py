import random, time, json

# Miller–Rabin: deterministic for <2^64; strong SPRP otherwise
def is_probable_prime(n:int)->bool:
    if n < 2: return False
    small = (2,3,5,7,11,13,17,19,23,29,31,37)
    for p in small:
        if n % p == 0: return n == p
    d, s = n-1, 0
    while d % 2 == 0:
        d //= 2; s += 1
    for a in (2,325,9375,28178,450775,9780504,1795265022):
        a %= n
        if a in (0,1,n-1): continue
        x = pow(a, d, n)
        if x in (1, n-1): continue
        for _ in range(s-1):
            x = (x*x) % n
            if x == n-1: break
        else:
            return False
    return True

def rand_prime(bits:int)->int:
    while True:
        x = random.getrandbits(bits) | (1<<(bits-1)) | 1
        if is_probable_prime(x): return x

if __name__ == "__main__":
    p = rand_prime(64)
    q = rand_prime(64)
    if q == p:
        q = rand_prime(64)
    print(p*q)
