// cprime_rho.c — minimal Pollard's Rho for up to 64-bit N
// Build: gcc -O3 -march=x86-64 -mtune=generic -pipe -o cprime_rho cprime_rho.c -lm
// Usage: ./cprime_rho --n <uint64> [--iters K] [--restarts R] [--verbose]

#include <stdio.h>
#include <stdint.h>
#include <inttypes.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

static uint64_t mul_mod(uint64_t a, uint64_t b, uint64_t m){
    __uint128_t z = (__uint128_t)a * b;
    return (uint64_t)(z % m);
}
static uint64_t add_mod(uint64_t a, uint64_t b, uint64_t m){
    uint64_t s = a + b;
    if (s < a) s -= m;
    s %= m;
    return s;
}
static uint64_t pow_mod(uint64_t a, uint64_t e, uint64_t m){
    uint64_t r = 1 % m;
    while (e){
        if (e & 1) r = mul_mod(r, a, m);
        a = mul_mod(a, a, m);
        e >>= 1;
    }
    return r;
}
static uint64_t gcd_u64(uint64_t a, uint64_t b){
    while (b){ uint64_t t = a % b; a = b; b = t; }
    return a;
}
static int is_probable_prime(uint64_t n){
    if (n < 2) return 0;
    static const uint64_t small[] = {2,3,5,7,11,13,17,19,23,0};
    for (int i=0; small[i]; ++i){ if (n % small[i] == 0) return n == small[i]; }
    // Deterministic Miller–Rabin for 64-bit
    uint64_t d = n-1, s = 0;
    while ((d & 1) == 0){ d >>= 1; ++s; }
    uint64_t bases[] = {2, 3, 5, 7, 11, 13, 17};
    for (size_t i=0;i<sizeof(bases)/sizeof(bases[0]);++i){
        uint64_t a = bases[i] % n; if (a==0) return 1;
        uint64_t x = pow_mod(a, d, n);
        if (x==1 || x==n-1) continue;
        int cont = 0;
        for (uint64_t r=1; r<s; ++r){
            x = mul_mod(x, x, n);
            if (x == n-1){ cont = 1; break; }
        }
        if (!cont) return 0;
    }
    return 1;
}
static uint64_t f(uint64_t x, uint64_t c, uint64_t n){
    // x^2 + c mod n
    return add_mod(mul_mod(x,x,n), c, n);
}
static uint64_t pollard_rho(uint64_t n, uint64_t iters, uint64_t seed_c){
    if ((n & 1) == 0) return 2;
    uint64_t c = seed_c | 1ULL; // odd c
    uint64_t x = 2, y = 2, d = 1;
    for (uint64_t i=0; i<iters; ++i){
        x = f(x,c,n);
        y = f(f(y,c,n),c,n);
        uint64_t diff = x>y ? x-y : y-x;
        d = gcd_u64(diff, n);
        if (d > 1 && d < n) return d;
    }
    return 1;
}

int main(int argc, char** argv){
    uint64_t n = 0, iters = 50000, restarts = 32;
    int verbose = 0;
    for (int i=1;i<argc;i++){
        if (!strcmp(argv[i],"--n") && i+1<argc) n = strtoull(argv[++i],NULL,10);
        else if (!strcmp(argv[i],"--iters") && i+1<argc) iters = strtoull(argv[++i],NULL,10);
        else if (!strcmp(argv[i],"--restarts") && i+1<argc) restarts = strtoull(argv[++i],NULL,10);
        else if (!strcmp(argv[i],"--verbose")) verbose = 1;
        else {
            fprintf(stderr,"Usage: %s --n <uint64> [--iters K] [--restarts R] [--verbose]\n", argv[0]);
            return 2;
        }
    }
    if (n==0){ fprintf(stderr,"Error: --n required\n"); return 2; }
    if (is_probable_prime(n)){ printf("prime %" PRIu64 "\n", n); return 0; }

    srand((unsigned)time(NULL));
    for (uint64_t r=0; r<restarts; ++r){
        uint64_t c = (rand() % 0xffffffffu) | 1u;
        uint64_t d = pollard_rho(n, iters, c);
        if (d != 1 && d != n){
            uint64_t p = d, q = n / d;
            if (p > q){ uint64_t t=p; p=q; q=t; }
            printf("factors %" PRIu64 " %" PRIu64 "\n", p, q);
            return 0;
        }
        if (verbose) fprintf(stderr,"[restart %llu] no factor\n",(unsigned long long)r);
    }
    printf("unknown\n");
    return 1;
}
