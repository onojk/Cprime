// tau_demo.c  —  classify n by divisor count (τ) and find factors if semiprime.
// build:  gcc -O2 -std=c11 -Wall tau_demo.c -o tau_demo
// usage:  ./tau_demo 1638912102000

#include <stdio.h>
#include <stdint.h>
#include <inttypes.h>

static uint64_t modmul(uint64_t a, uint64_t b, uint64_t m){
    __int128 z = ( __int128)a * b;
    return (uint64_t)(z % m);
}
static uint64_t modpow(uint64_t a, uint64_t e, uint64_t m){
    uint64_t r = 1;
    while(e){
        if(e & 1) r = modmul(r, a, m);
        a = modmul(a, a, m);
        e >>= 1;
    }
    return r;
}
static int miller_rabin(uint64_t n){
    if(n < 2) return 0;
    for(uint64_t p: (uint64_t[]){2,3,5,7,11,13,17}) {
        if(n%p==0) return n==p;
    }
    uint64_t d = n-1, s = 0;
    while((d & 1) == 0){ d >>= 1; ++s; }
    uint64_t bases[] = {2,3,5,7,11,13,17};
    for(size_t i=0;i<sizeof(bases)/sizeof(bases[0]);++i){
        uint64_t a = bases[i];
        if(a % n == 0) continue;
        uint64_t x = modpow(a,d,n);
        if(x==1 || x==n-1) continue;
        int cont = 0;
        for(uint64_t r=1;r<s;++r){
            x = modmul(x,x,n);
            if(x == n-1){ cont = 1; break; }
        }
        if(!cont) return 0;
    }
    return 1; // deterministic for 64-bit
}
static uint64_t prev_prime(uint64_t n){
    if(n<=2) return 0;
    for(uint64_t k=n-1;;--k) if(miller_rabin(k)) return k;
}
static uint64_t next_prime(uint64_t n){
    if(n<=2) return 2;
    for(uint64_t k=n+1;;++k) if(miller_rabin(k)) return k;
}

// returns τ(n); if τ(n)==4 fills *p,*q with the two prime factors (p<=q)
static uint64_t tau_and_semiprime(uint64_t n, uint64_t *p, uint64_t *q){
    if(n==0) return 0;
    if(n==1) return 1;
    if(miller_rabin(n)) return 2;

    uint64_t t = 2; // 1 and n
    uint64_t r = 1;
    while((r+1)*(r+1) <= n) ++r;
    for(uint64_t d=2; d<=r; ++d){
        if(n % d == 0){
            uint64_t e = n / d;
            t += (d==e) ? 1 : 2;
            // early semiprime check
            if(p && q && (d==e ? 0 : 1)){
                if(miller_rabin(d) && miller_rabin(e)){ *p=d; *q=e; }
            }
        }
    }
    return t;
}

int main(int argc, char **argv){
    if(argc<2){ fprintf(stderr,"usage: %s <n>\n", argv[0]); return 1; }
    uint64_t n=0; sscanf(argv[1],"%" SCNu64,&n);

    uint64_t p=0,q=0;
    uint64_t tau = tau_and_semiprime(n,&p,&q);
    printf("n = %" PRIu64 "\n", n);
    printf("tau(n) = %" PRIu64 "  (dot count)\n", tau);

    if(tau==1){
        printf("class: special (n=1)\n");
    } else if(tau==2){
        printf("class: PRIME\n");
    } else if(p && q){
        if(p>q){ uint64_t t=p; p=q; q=t; }
        printf("class: SEMIPRIME  factors = [%" PRIu64 ", %" PRIu64 "]\n", p,q);
    } else {
        printf("class: COMPOSITE/other\n");
    }

    uint64_t below = prev_prime(n);
    uint64_t above = next_prime(n);
    if(below) printf("prev prime < n: %" PRIu64 "\n", below);
    printf("next prime > n: %" PRIu64 "\n", above);
    return 0;
}
