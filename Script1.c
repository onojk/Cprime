#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* ---------- Helpers ---------- */

// Luby sequence (1-indexed). Depth O(log k), fine here.
static uint64_t luby(unsigned k){
    unsigned p = 1;
    while (p <= k) p <<= 1;
    p >>= 1;
    if (k == p - 1) return (uint64_t)(p >> 1);
    return luby(k - (p >> 1) + 1);
}

static uint64_t min_u64(uint64_t a, uint64_t b){ return a < b ? a : b; }

/* ---------- Mock attempt ----------
   Replace with your Brent Pollard-rho limited by 'budget'.
   Returns 0 = not found, 1 = found.
*/
static int rho_try(unsigned seed, unsigned poly, uint64_t budget, int verbose){
    if (verbose){
        printf("rho_try(seed=%u, poly=%u, budget=%llu)\n",
               seed, poly, (unsigned long long)budget);
    } else {
        printf("%llu\n", (unsigned long long)budget);
    }
    return 0;
}

/* ---------- CLI parsing ---------- */
typedef enum { SCH_LUBY=0, SCH_DOUBLING=1, SCH_FIXED=2 } schedule_t;

typedef struct {
    uint64_t baseK;         // --rho_iters
    unsigned restarts;      // --rho_restarts
    schedule_t schedule;    // --schedule {luby|doubling|fixed}
    int verbose;            // --verbose
    uint64_t cap;           // optional --cap for doubling (max multiplier)
} opts_t;

static void usage(const char* prog){
    fprintf(stderr,
        "Usage: %s --rho_iters K --rho_restarts N [--schedule luby|doubling|fixed]\n"
        "           [--cap M] [--verbose]\n"
        "\n"
        "Prints per-restart budgets (or detailed calls with --verbose).\n"
        "Examples:\n"
        "  %s --rho_iters 75000000 --rho_restarts 16 --schedule luby --verbose\n"
        "  %s --rho_iters 1000 --rho_restarts 10 --schedule doubling --cap 16\n",
        prog, prog, prog);
}

static int parse_opts(int argc, char** argv, opts_t* o){
    o->baseK = 75000000ULL;
    o->restarts = 16;
    o->schedule = SCH_LUBY;
    o->verbose = 0;
    o->cap = 0; // 0 => no cap

    for (int i=1; i<argc; ++i){
        if (!strcmp(argv[i], "--rho_iters") && i+1<argc){
            o->baseK = strtoull(argv[++i], NULL, 10);
        } else if (!strcmp(argv[i], "--rho_restarts") && i+1<argc){
            o->restarts = (unsigned)strtoul(argv[++i], NULL, 10);
        } else if (!strcmp(argv[i], "--schedule") && i+1<argc){
            const char* s = argv[++i];
            if (!strcmp(s, "luby")) o->schedule = SCH_LUBY;
            else if (!strcmp(s, "doubling")) o->schedule = SCH_DOUBLING;
            else if (!strcmp(s, "fixed")) o->schedule = SCH_FIXED;
            else { fprintf(stderr,"Unknown schedule: %s\n", s); return 0; }
        } else if (!strcmp(argv[i], "--cap") && i+1<argc){
            o->cap = strtoull(argv[++i], NULL, 10);
        } else if (!strcmp(argv[i], "--verbose")){
            o->verbose = 1;
        } else if (!strcmp(argv[i], "-h") || !strcmp(argv[i], "--help")){
            usage(argv[0]); return 0;
        } else {
            fprintf(stderr, "Unknown arg: %s\n", argv[i]);
            return 0;
        }
    }
    return 1;
}

/* ---------- Main driver ---------- */
int main(int argc, char** argv){
    opts_t opt;
    if (!parse_opts(argc, argv, &opt)){
        usage(argv[0]);
        return 2;
    }

    for (unsigned i=1; i<=opt.restarts; ++i){
        uint64_t mult = 1;
        switch (opt.schedule){
            case SCH_LUBY:
                mult = luby(i);
                break;
            case SCH_DOUBLING: {
                // powers-of-two windows => multipliers: 1,1,2,4,8,... then capped
                unsigned p = 1, level = 0;
                while ((p<<1) <= i){ p <<= 1; ++level; }
                mult = (level==0) ? 1ULL : (1ULL << (level-1));
                if (opt.cap) mult = min_u64(mult, opt.cap);
            } break;
            case SCH_FIXED:
                mult = 1;
                break;
        }
        uint64_t budget = opt.baseK * mult;
        if (rho_try(i, i, budget, opt.verbose)){
            if (opt.verbose) puts("Factor found!");
            break;
        }
    }
    return 0;
}
