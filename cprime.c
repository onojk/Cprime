/* cprime.c  —  Console-only prime/factor CLI with JSON output
 *
 * Features
 * - Subcommands:
 *     prime  <n>
 *     factor <n> [--timeout_ms T] [--p1_B B] [--rho_restarts R] [--rho_iters I]
 * - Outputs a single JSON line with:
 *     { "n": <uint>, "n_str":"<decimal>", "classification":"prime|composite",
 *       "factors":{"p1":e1,"p2":e2,...}, "bits": <int>,
 *       "status":"ok|timeout|error", "params":{...} }
 * - Uses GMP for big integers
 * - Pollard’s Rho (Brent variant) with restarts + iteration caps
 * - Optional small P-1 trial stage via B smoothness bound (--p1_B)
 * - --help and --version flags
 *
 * Build (requires libgmp-dev):
 *   gcc -O3 -march=x86-64 -mtune=generic -pipe -o cprime_cli_demo cprime.c -lgmp -lm
 */

#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <inttypes.h>
#include <math.h>
#include <errno.h>
#include <stdbool.h>
#include <gmp.h>

#ifndef GIT_DESC
#define GIT_DESC "dev"
#endif

/* ---------- util: time ---------- */

static uint64_t now_ms(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (uint64_t)ts.tv_sec * 1000ull + (uint64_t)(ts.tv_nsec / 1000000ull);
}

/* ---------- util: parsing & printing ---------- */

static void die_usage(const char *prog) {
    fprintf(stderr,
        "Usage:\n"
        "  %s --help | -h\n"
        "  %s --version | -V\n"
        "  %s prime  <n>\n"
        "  %s factor <n> [--timeout_ms T] [--p1_B B] [--rho_restarts R] [--rho_iters I]\n"
        "Notes:\n"
        "  - <n> is a non-negative integer (decimal string).\n"
        "  - timeout: T=0 disables time limit; rho_iters=0 => unlimited when T=0.\n",
        prog, prog, prog, prog
    );
}

static int parse_mpz_or_err(mpz_t out, const char *s) {
    if (!s || !*s) return -1;
    // Disallow leading '+' and any sign; treat as unsigned integer.
    for (const char *p=s; *p; ++p) {
        if (*p < '0' || *p > '9') return -2;
    }
    if (mpz_init_set_str(out, s, 10) != 0) return -3;
    return 0;
}

static int bits_of(const mpz_t n) {
    return (int) mpz_sizeinbase(n, 2);
}

static char* mpz_to_cstr(const mpz_t x) {
    // Caller must free
    return mpz_get_str(NULL, 10, x);
}

/* ---------- math helpers ---------- */

static void gcd_mpz(mpz_t r, const mpz_t a, const mpz_t b) {
    mpz_gcd(r, a, b);
}

static bool is_probable_prime(const mpz_t n) {
    // GMP’s BPSW-like probable prime test: reps=25 is customary.
    int r = mpz_probab_prime_p(n, 25);
    return r > 0; // 1 = probably prime, 2 = definitely prime (for small)
}

/* ---------- trial division by small primes (quick win) ---------- */

static const unsigned small_primes[] = {
    2u,3u,5u,7u,11u,13u,17u,19u,23u,29u,31u,37u,41u,43u,47u,53u,59u,61u,
    67u,71u,73u,79u,83u,89u,97u,101u,103u,107u,109u,113u,127u,131u,137u,139u,149u
};
static const size_t small_primes_len = sizeof(small_primes)/sizeof(small_primes[0]);

static bool trial_divide(mpz_t n, mpz_t factor_out) {
    for (size_t i=0; i<small_primes_len; ++i) {
        unsigned p = small_primes[i];
        if (mpz_divisible_ui_p(n, p)) {
            mpz_set_ui(factor_out, p);
            return true;
        }
    }
    return false;
}

/* ---------- Pollard Rho (Brent) ---------- */

static void brent_rho(mpz_t factor, const mpz_t n, gmp_randstate_t rng,
                      uint64_t max_iters)
{
    // Precondition: n is odd composite (best-effort), factor result 1 or non-trivial.
    mpz_set_ui(factor, 1);
    if (mpz_even_p(n)) { mpz_set_ui(factor, 2); return; }

    mpz_t y, c, m, g, r, q, x, ys, tmp, absdiff;
    mpz_inits(y,c,m,g,r,q,x,ys,tmp,absdiff, NULL);

    // y in [1..n-1], c in [1..n-1], m in [1..n-1]
    mpz_urandomm(y, rng, n);
    do { mpz_urandomm(c, rng, n); } while (mpz_cmp_ui(c,0) == 0);
    mpz_set_ui(m, 128);

    mpz_set_ui(g, 1);
    mpz_set_ui(r, 1);
    mpz_set_ui(q, 1);

    uint64_t iters = 0;
    while (mpz_cmp_ui(g,1) == 0) {
        mpz_set(x, y);
        for (mp_bitcnt_t i = 0; i < mpz_get_ui(r); ++i) {
            // y = (y*y + c) % n
            mpz_mul(y,y,y);
            mpz_add(y,y,c);
            mpz_mod(y,y,n);
            if (max_iters && ++iters >= max_iters) goto done; // give up
        }




        mpz_set_ui(g,1);
        mpz_set_ui(q,1);

        mpz_t k;
        mpz_init_set_ui(k,0);

        while (mpz_cmp(k, r) < 0 && mpz_cmp_ui(g,1) == 0) {
            mp_bitcnt_t lim = mpz_get_ui(m);
            mp_bitcnt_t i = 0;

            mpz_set(ys, y);
            for (; i < lim && mpz_cmp(k, r) < 0; ++i) {
                // y = (y*y + c) % n
                mpz_mul(y,y,y);
                mpz_add(y,y,c);
                mpz_mod(y,y,n);

                // q = q * |x - y| % n
                mpz_sub(absdiff, x, y);
                if (mpz_sgn(absdiff) < 0) mpz_neg(absdiff, absdiff);
                mpz_mul(q,q,absdiff);
                mpz_mod(q,q,n);

                if (max_iters && ++iters >= max_iters) { mpz_clear(k); goto done; }
                mpz_add_ui(k, k, 1);
            }
            gcd_mpz(g, q, n);
            if (mpz_cmp_ui(g,1) == 1) { mpz_clear(k); break; }

            mpz_set(y, ys);
            for (mp_bitcnt_t j = 0; j < mpz_get_ui(m) && mpz_cmp_ui(g,1) == 0 && mpz_cmp(k,r) < 0; ++j) {
                // y = (y*y + c) % n
                mpz_mul(y,y,y);
                mpz_add(y,y,c);
                mpz_mod(y,y,n);
                mpz_sub(absdiff, x, y);
                if (mpz_sgn(absdiff) < 0) mpz_neg(absdiff, absdiff);
                gcd_mpz(g, absdiff, n);
                if (max_iters && ++iters >= max_iters) { mpz_clear(k); goto done; }
                mpz_add_ui(k, k, 1);
            }
            mpz_clear(k);
        }
        mpz_mul_ui(r, r, 2);
    }

    if (mpz_cmp(g, n) == 0) {
        // fallback: try many single steps to find nontrivial gcd
        do {
            // ys = (ys*ys + c) % n
            mpz_mul(ys,ys,ys);
            mpz_add(ys,ys,c);
            mpz_mod(ys,ys,n);
            mpz_sub(absdiff, x, ys);
            if (mpz_sgn(absdiff) < 0) mpz_neg(absdiff, absdiff);
            gcd_mpz(g, absdiff, n);
        } while (mpz_cmp_ui(g,1) == 0);
    }
done:
    if (mpz_cmp_ui(g,1) > 0 && mpz_cmp(g, n) < 0) mpz_set(factor, g);
    mpz_clears(y,c,m,g,r,q,x,ys,tmp,absdiff, NULL);
}

/* ---------- P-1 (very light, stage 1 only) ---------- */

static void pollard_p1_stage1(mpz_t factor, const mpz_t n, unsigned long B) {
    mpz_set_ui(factor, 1);
    if (B < 5) return;
    mpz_t a, d, t;
    mpz_inits(a,d,t,NULL);
    mpz_set_ui(a, 2);

    for (unsigned long p=2; p<=B; ++p) {
        // raise a to highest power of p <= B
        unsigned long e = 1;
        unsigned long pe = p;
        while (pe * p <= B) { pe *= p; ++e; }
        // a = a^(p^e) mod n
        mpz_powm_ui(a, a, pe, n);
    }

    // d = gcd(a-1, n)
    mpz_sub_ui(t, a, 1);
    mpz_gcd(d, t, n);
    if (mpz_cmp_ui(d,1)>0 && mpz_cmp(d, n)<0) mpz_set(factor, d);
    mpz_clears(a,d,t,NULL);
}

/* ---------- factor recursion ---------- */

typedef struct {
    mpz_t *p;  // array of primes
    int   *e;  // exponents
    size_t len, cap;
} factor_list;

static void fl_init(factor_list *fl) {
    fl->p = NULL; fl->e = NULL; fl->len = 0; fl->cap = 0;
}
static void fl_free(factor_list *fl) {
    if (fl->p) {
        for (size_t i=0;i<fl->len;i++) mpz_clear(fl->p[i]);
        free(fl->p);
    }
    free(fl->e);
    fl->p = NULL; fl->e = NULL; fl->len = fl->cap = 0;
}
static void fl_push(factor_list *fl, const mpz_t prime, int exp) {
    // merge if exists
    for (size_t i=0;i<fl->len;i++) {
        if (mpz_cmp(fl->p[i], prime) == 0) { fl->e[i] += exp; return; }
    }
    if (fl->len == fl->cap) {
        size_t ncap = fl->cap? fl->cap*2 : 8;
        fl->p = (mpz_t*)realloc(fl->p, ncap*sizeof(mpz_t));
        fl->e = (int*)realloc(fl->e, ncap*sizeof(int));
        for (size_t j=fl->cap;j<ncap;j++) mpz_init(fl->p[j]);
        fl->cap = ncap;
    }
    mpz_set(fl->p[fl->len], prime);
    fl->e[fl->len] = exp;
    fl->len++;
}

typedef struct {
    uint64_t timeout_ms;   // 0 => no timeout
    uint64_t start_ms;
    unsigned long p1_B;    // 0 => skip
    uint64_t rho_restarts;
    uint64_t rho_iters;    // per-restart iteration cap (0 => unlimited if no timeout)
} factor_params;

static int factor_rec(mpz_t n, factor_list *out, gmp_randstate_t rng, const factor_params *fp);

/* Split n into (d, n/d) using methods; returns 1 if split found, 0 otherwise */
static int find_split(mpz_t d, const mpz_t n, gmp_randstate_t rng, const factor_params *fp) {
    mpz_set_ui(d, 0);

    // 0) Trial divide small primes quickly
    mpz_t f;
    mpz_init(f);
    mpz_t nn; mpz_init_set(nn, n);
    if (trial_divide(nn, f)) { mpz_set(d, f); mpz_clears(f,nn,NULL); return 1; }
    mpz_clears(f,nn,NULL);

    // 1) Optional Pollard P-1 stage 1
    if (fp->p1_B > 0) {
        mpz_t p1d; mpz_init(p1d);
        pollard_p1_stage1(p1d, n, fp->p1_B);
        if (mpz_cmp_ui(p1d,1)>0 && mpz_cmp(p1d,n)<0) { mpz_set(d, p1d); mpz_clear(p1d); return 1; }
        mpz_clear(p1d);
    }

    // 2) Pollard Rho (Brent) with restarts
    uint64_t restarts = fp->rho_restarts ? fp->rho_restarts : 256;
    for (uint64_t r=0; r<restarts; ++r) {
        if (fp->timeout_ms && now_ms() - fp->start_ms >= fp->timeout_ms) break;
        mpz_t g; mpz_init(g);
        uint64_t itcap = fp->rho_iters;
        if (fp->timeout_ms && itcap == 0) itcap = 5000000ull; // sensible default under timeout regime
        brent_rho(g, n, rng, itcap);
        if (mpz_cmp_ui(g,1)>0 && mpz_cmp(g,n)<0) { mpz_set(d, g); mpz_clear(g); return 1; }
        mpz_clear(g);
    }
    return 0;
}

static int factor_rec(mpz_t n, factor_list *out, gmp_randstate_t rng, const factor_params *fp) {
    // base cases
    if (mpz_cmp_ui(n, 1) == 0) return 0;
    if (is_probable_prime(n)) {
        fl_push(out, n, 1);
        return 0;
    }

    // Timeout?
    if (fp->timeout_ms && now_ms() - fp->start_ms >= fp->timeout_ms) {
        return -1; // signal timeout
    }

    // find a nontrivial factor d
    mpz_t d; mpz_init(d);
    int ok = find_split(d, n, rng, fp);
    if (!ok) { mpz_clear(d); return -2; } // failed to split (likely timeout/iteration cap)

    // split n = d * m
    mpz_t m; mpz_init(m);
    mpz_divexact(m, n, d);

    // recurse on d and m
    int r1 = factor_rec(d, out, rng, fp);
    if (r1 < 0) { mpz_clears(d,m,NULL); return r1; }
    int r2 = factor_rec(m, out, rng, fp);
    if (r2 < 0) { mpz_clears(d,m,NULL); return r2; }

    mpz_clears(d,m,NULL);
    return 0;
}

/* ---------- JSON output helpers ---------- */

static void print_json_header(const mpz_t n) {
    char *ns = mpz_to_cstr(n);
    gmp_printf("{\"n\": %Zd, \"n_str\":\"%s\", ", n, ns);
    free(ns);
}

static void print_factors_json(const factor_list *fl) {
    // factors as object with string keys (primes) -> exponents
    printf("\"factors\":{");
    for (size_t i=0;i<fl->len;i++) {
        char *ps = mpz_to_cstr(fl->p[i]);
        printf("%s\"%s\": %d", (i? ",": ""), ps, fl->e[i]);
        free(ps);
    }
    printf("}");
}

static void sort_factors(factor_list *fl) {
    // simple insertion sort by numeric value ascending (tiny lists)
    for (size_t i=1;i<fl->len;i++) {
        size_t j=i;
        while (j>0 && mpz_cmp(fl->p[j-1], fl->p[j]) > 0) {
            // swap p
            mpz_t tmp; mpz_init_set(tmp, fl->p[j-1]);
            mpz_set(fl->p[j-1], fl->p[j]);
            mpz_set(fl->p[j], tmp);
            mpz_clear(tmp);
            // swap e
            int te = fl->e[j-1]; fl->e[j-1] = fl->e[j]; fl->e[j] = te;
            --j;
        }
    }
}

/* ---------- main ---------- */

int main(int argc, char **argv) {
    const char *prog = argv[0] ? argv[0] : "cprime_cli_demo";

    if (argc >= 2 && (!strcmp(argv[1],"--help") || !strcmp(argv[1],"-h"))) {
        die_usage(prog);
        return 2;
    }
    if (argc >= 2 && (!strcmp(argv[1],"--version") || !strcmp(argv[1],"-V"))) {
        printf("{\"version\":\"%s\"}\n", GIT_DESC);
        return 0;
    }

    if (argc < 3) {
        die_usage(prog);
        return 2;
    }

    const char *cmd = argv[1];
    const char *n_str = argv[2];

    mpz_t N;
    int perr = parse_mpz_or_err(N, n_str);
    if (perr != 0) {
        fprintf(stderr, "{\"ok\":false,\"error\":\"bad_n\",\"code\":%d}\n", perr);
        return 3;
    }

    int rc = 0;

    if (!strcmp(cmd, "prime")) {
        bool isp = is_probable_prime(N);
        int bits = bits_of(N);
        print_json_header(N);
        printf("\"classification\":\"%s\", ", isp? "prime":"composite");
        printf("\"factors\":{");
        if (isp) {
            char *ps = mpz_to_cstr(N);
            printf("\"%s\":1", ps);
            free(ps);
        }
        printf("}, ");
        printf("\"bits\": %d}\n", bits);
        mpz_clear(N);
        return 0;
    }

    if (!strcmp(cmd, "factor")) {
        // defaults
        factor_params fp = {
            .timeout_ms   = 0,
            .start_ms     = now_ms(),
            .p1_B         = 200000,      // small but helpful default
            .rho_restarts = 256,
            .rho_iters    = 5000000      // per restart; 0 => unlimited if no timeout
        };

        // parse flags
        for (int i=3; i<argc; ++i) {
            if (!strcmp(argv[i], "--timeout_ms") && i+1<argc) {
                fp.timeout_ms = strtoull(argv[++i], NULL, 10);
            } else if (!strcmp(argv[i], "--p1_B") && i+1<argc) {
                fp.p1_B = strtoul(argv[++i], NULL, 10);
            } else if (!strcmp(argv[i], "--rho_restarts") && i+1<argc) {
                fp.rho_restarts = strtoull(argv[++i], NULL, 10);
            } else if (!strcmp(argv[i], "--rho_iters") && i+1<argc) {
                fp.rho_iters = strtoull(argv[++i], NULL, 10);
            } else {
                fprintf(stderr, "{\"ok\":false,\"error\":\"bad_flag\",\"arg\":\"%s\"}\n", argv[i]);
                mpz_clear(N);
                return 2;
            }
        }

        // Quick classification
        int bits = bits_of(N);
        bool isp = is_probable_prime(N);

        factor_list fl; fl_init(&fl);

        char status[16]; strcpy(status, "ok");
        if (isp) {
            // prime: factors = {N:1}
            fl_push(&fl, N, 1);
        } else {
            // composite: attempt to factor
            gmp_randstate_t rng; gmp_randinit_default(rng);
            // seed: time-based (acceptable here)
            mpz_t seed; mpz_init(seed);
            mpz_set_ui(seed, (unsigned long) (now_ms() & 0xffffffffu));
            gmp_randseed(rng, seed);
            mpz_clear(seed);

            mpz_t ncopy; mpz_init_set(ncopy, N);
            int fr = factor_rec(ncopy, &fl, rng, &fp);
            mpz_clear(ncopy);
            gmp_randclear(rng);

            if (fr == -1) { strcpy(status, "timeout"); }
            else if (fr < 0) { strcpy(status, "error"); }
        }

        sort_factors(&fl);

        // JSON output
        print_json_header(N);
        printf("\"classification\":\"%s\", ", isp? "prime":"composite");
        print_factors_json(&fl);
        printf(", \"bits\": %d, \"status\":\"%s\", \"params\":{", bits, status);
        printf("\"timeout_ms\": %" PRIu64 ", ", fp.timeout_ms);
        printf("\"p1_B\": %lu, ", fp.p1_B);
        printf("\"rho_restarts\": %" PRIu64 ", ", fp.rho_restarts);
        printf("\"rho_iters\": %" PRIu64 "}", fp.rho_iters);
        printf("}\n");

        fl_free(&fl);
        mpz_clear(N);
        return rc;
    }

    // unknown subcommand
    die_usage(prog);
    mpz_clear(N);
    return 2;
}

