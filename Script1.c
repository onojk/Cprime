#include <stdint.h>
#include <stdio.h>

// --- Luby restart helper ---
static uint64_t luby(unsigned k){
    unsigned p = 1;
    while (p <= k) p <<= 1;
    p >>= 1;
    if (k == p - 1) return (uint64_t)(p >> 1);
    return luby(k - (p >> 1) + 1);
}

// --- Mock rho_try for demo ---
// Replace this with your actual Brent/Pollard rho loop, limited by 'budget'.
static int rho_try(unsigned seed, unsigned poly, uint64_t budget){
    printf("rho_try(seed=%u, poly=%u, budget=%llu)\n",
           seed, poly, (unsigned long long)budget);
    // TODO: Insert actual rho work here
    return 0; // 0 = not found, 1 = found
}

int main(void){
    uint64_t baseK    = 75000000ULL; // base rho_iters
    unsigned restarts = 16;          // rho_restarts

    for (unsigned i=1; i<=restarts; ++i){
        uint64_t budget = baseK * luby(i);
        if (rho_try(i, i, budget)){
            puts("Factor found!");
            break;
        }
    }
    return 0;
}
