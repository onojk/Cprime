#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* ---------- Helpers ---------- */
static uint64_t luby(unsigned i){
    unsigned k = 1;
    while (((1u<<k) - 1u) < i) k++;
    if (i == ((1u<<k) - 1u)) return 1u<<(k-1);
    return luby(i - (1u<<(k-1)) + 1u);
}

/* prints budgets; stub for your real rho work */
static int rho_try(unsigned seed, unsigned poly, unsigned long long budget, int verbose){
    if (verbose) printf("rho_try(seed=%u, poly=%u, budget=%llu)\n", seed, poly, budget);
    else         printf("%llu\n", budget);
    return 0;
}

/* ---------- CLI parsing ---------- */
typedef enum { SCH_LUBY=0, SCH_DOUBLING=1, SCH_FIXED=2 } schedule_t;
typedef struct {
    unsigned long long baseK;   // --rho_iters
    unsigned            restarts; // --rho_restarts
    schedule_t          schedule; // --schedule
    int                 verbose;  // --verbose
    unsigned long long cap;      // --cap (ABSOLUTE budget cap)
    int                 show_help;
} opts_t;

static void usage(const char* prog){
    fprintf(stderr,
"Usage: %s --rho_iters K --rho_restarts N [--schedule luby|doubling|fixed]\n"
"           [--cap M] [--verbose]\n", prog);
}

static int parse_opts(int argc, char** argv, opts_t* o){
    o->baseK=75000000ULL; o->restarts=16; o->schedule=SCH_LUBY;
    o->verbose=0; o->cap=0; o->show_help=0;
    for (int i=1;i<argc;i++){
        if (!strcmp(argv[i],"--rho_iters")    && i+1<argc) o->baseK=strtoull(argv[++i],NULL,10);
        else if (!strcmp(argv[i],"--rho_restarts")&&i+1<argc) o->restarts=(unsigned)strtoul(argv[++i],NULL,10);
        else if (!strcmp(argv[i],"--schedule")&&i+1<argc){
            const char*s=argv[++i];
            if(!strcmp(s,"luby"))o->schedule=SCH_LUBY;
            else if(!strcmp(s,"doubling"))o->schedule=SCH_DOUBLING;
            else if(!strcmp(s,"fixed"))o->schedule=SCH_FIXED;
            else { fprintf(stderr,"Unknown schedule: %s\n",s); return 0; }
        } else if (!strcmp(argv[i],"--cap")   && i+1<argc) o->cap=strtoull(argv[++i],NULL,10);
        else if (!strcmp(argv[i],"--verbose")) o->verbose=1;
        else if (!strcmp(argv[i],"-h")||!strcmp(argv[i],"--help")) o->show_help=1;
        else { fprintf(stderr,"Unknown arg: %s\n", argv[i]); return 0; }
    }
    return 1;
}

/* ---------- Main ---------- */
int main(int argc, char** argv){
    opts_t opt; if(!parse_opts(argc,argv,&opt)){ usage(argv[0]); return 2; }
    if(opt.show_help){ usage(argv[0]); return 0; }

    for(unsigned i=1;i<=opt.restarts;i++){
        unsigned long long mult = 1ULL;
        switch(opt.schedule){
            case SCH_LUBY:     mult = luby(i); break;
            case SCH_DOUBLING: mult = (i<=1)?1ULL:(1ULL<<(i-2)); break; // 1,1,2,4,8,...
            case SCH_FIXED:    mult = 1ULL; break;
        }
        unsigned long long budget = opt.baseK * mult;

        // ABSOLUTE cap: clamp budget directly to --cap, if provided.
        if (opt.cap && budget > opt.cap) budget = opt.cap;

        if (rho_try(i,i,budget,opt.verbose)) { if(opt.verbose) puts("Factor found!"); break; }
    }
    return 0;
}
