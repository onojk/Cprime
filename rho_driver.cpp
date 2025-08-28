#include <cstdint>
#include <cstdio>

// --- Luby restart helper ---
static uint64_t luby(unsigned k) {
    // 1-indexed Luby sequence
    unsigned p = 1;
    while (p <= k) p <<= 1;
    p >>= 1;
    if (k == p - 1) return p >> 1;
    return luby(k - (p >> 1) + 1);
}

// --- Mock rho_try for demo ---
bool rho_try(unsigned seed, unsigned poly, uint64_t budget) {
    printf("rho_try(seed=%u, poly=%u, budget=%lu)\n", seed, poly, budget);
    // do your Brent loop here, limited by 'budget'
    return false;
}

int main() {
    uint64_t baseK = 75000000;  // e.g. from CLI arg: --rho_iters
    unsigned restarts = 16;     // e.g. from CLI arg: --rho_restarts

    for (unsigned i = 1; i <= restarts; ++i) {
        uint64_t budget = baseK * luby(i);
        if (rho_try(i, i, budget)) {
            printf("Factor found!\n");
            break;
        }
    }
    return 0;
}
