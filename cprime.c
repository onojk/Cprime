#define _POSIX_C_SOURCE 200809L
#include <gmp.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <errno.h>

/* -------- time helpers -------- */
static double now_ms(void){
  struct timespec ts; clock_gettime(CLOCK_MONOTONIC, &ts);
  return ts.tv_sec*1000.0 + ts.tv_nsec/1e6;
}

/* -------- primality -------- */
static int is_probable_prime(const mpz_t n){
  int r = mpz_probab_prime_p(n, 25);  /* 0 comp, 1 probable, 2 definitely for smalls */
  return r > 0;
}

/* -------- small trial division -------- */
static const unsigned small_primes[] = {
  2,3,5,7,11,13,17,19,23,29,31,37,41,43,47,53,59,61,67,71,73,79,83,89,97,
  101,103,107,109,113,127,131,137,139,149,151,157,163,167,173,179,181,191,193,197,199,
  211,223,227,229,233,239,241,251,257,263,269,271,277,281,283,293,307,311,313,317,331,337,347,349
};
static int trial_division(mpz_t factor, const mpz_t n, unsigned bound){
  for(size_t i=0;i<sizeof(small_primes)/sizeof(small_primes[0]);++i){
    unsigned p = small_primes[i];
    if (p > bound) break;
    if (mpz_divisible_ui_p(n, p)){
      mpz_set_ui(factor, p);
      return 1;
    }
  }
  return 0;
}

/* -------- sieve for p-1 -------- */
static unsigned *sieve_primes_upto(unsigned B, size_t *out_len){
  if (B < 2){ *out_len=0; return NULL; }
  unsigned n = B+1;
  char *is_p = calloc(n,1);
  unsigned *list = malloc(n*sizeof(unsigned));
  size_t len=0;
  for (unsigned i=2;i<=B;i++) is_p[i]=1;
  for (unsigned p=2;p*p<=B;p++) if (is_p[p])
    for (unsigned j=p*p;j<=B;j+=p) is_p[j]=0;
  for (unsigned i=2;i<=B;i++) if (is_p[i]) list[len++]=i;
  free(is_p);
  *out_len = len;
  return list;
}

/* -------- Pollard p-1 stage 1 -------- */
static int pollard_pminus1(mpz_t g, const mpz_t n, unsigned B){
  if (mpz_even_p(n)){ mpz_set_ui(g,2); return 1; }
  size_t plen=0; unsigned *pr = sieve_primes_upto(B, &plen);
  if (!pr){ return 0; }
  mpz_t a, t; mpz_inits(a, t, NULL);
  mpz_set_ui(a, 2);  /* base */
  for (size_t i=0;i<plen;i++){
    unsigned p = pr[i];
    /* raise exponent by highest power p^e <= B */
    unsigned long pe = p;
    while ((unsigned long)pe * p <= B) pe *= p;
    mpz_powm_ui(a, a, pe, n);  /* a = a^(p^e) mod n */
  }
  free(pr);
  mpz_sub_ui(t, a, 1);
  mpz_gcd(g, t, n);
  int ok = (mpz_cmp_ui(g,1)>0 && mpz_cmp(g,n)<0);
  mpz_clears(a, t, NULL);
  return ok;
}

/* -------- RNG for rho -------- */
static void rnd_seed_gmp(gmp_randstate_t st, const mpz_t n){
  unsigned long seed = (unsigned long)time(NULL) ^ 0x9E3779B97F4A7C15ULL;
  seed ^= (unsigned long)mpz_get_ui(n);
  gmp_randinit_default(st);
  gmp_randseed_ui(st, seed);
}

/* -------- Brent's Pollard rho (max_iters=0 => unlimited) -------- */
static int rho_brent(mpz_t factor, const mpz_t n, gmp_randstate_t rst,
                     unsigned long max_iters, double deadline_ms)
{
  if (mpz_even_p(n)){ mpz_set_ui(factor, 2); return 1; }
  if (is_probable_prime(n)){ mpz_set(factor, n); return 1; }

  mpz_t y,c,m,g,r,q,x,ys,absdiff;
  mpz_inits(y,c,m,g,r,q,x,ys,absdiff, NULL);

  mpz_urandomm(y, rst, n); if (mpz_cmp_ui(y,0)==0) mpz_set_ui(y,1);
  mpz_urandomm(c, rst, n); if (mpz_cmp_ui(c,0)==0) mpz_set_ui(c,1);
  mpz_set_ui(m, 1UL<<9);  /* block size */

  mpz_set_ui(g,1);
  mpz_set_ui(r,1);
  mpz_set_ui(q,1);

  unsigned long iters = 0;
  while (mpz_cmp_ui(g,1)==0){
    if (deadline_ms>0 && now_ms()>deadline_ms) break;
    mpz_set(x, y);
    /* y = f^r(y), f(u)=u*u + c mod n */
    unsigned long r_ui = mpz_get_ui(r);
    for (unsigned long i=0;i<r_ui;i++){
      mpz_mul(y,y,y); mpz_add(y,y,c); mpz_mod(y,y,n);
      if (max_iters && ++iters>=max_iters) goto done;
      if (deadline_ms>0 && now_ms()>deadline_ms) goto done;
    }
    mpz_set_ui(q,1);
    mpz_t k; mpz_init_set_ui(k,0);
    while (mpz_cmp(k, r) < 0 && mpz_cmp_ui(g,1)==0){
      mpz_set(ys, y);
      unsigned long cnt = mpz_cmp(m, r) < 0 ? mpz_get_ui(m) : mpz_get_ui(r);
      for (unsigned long i=0;i<cnt;i++){
        mpz_mul(y,y,y); mpz_add(y,y,c); mpz_mod(y,y,n);
        mpz_sub(absdiff, x, y); if (mpz_sgn(absdiff)<0) mpz_neg(absdiff, absdiff);
        mpz_mul(q,q,absdiff); mpz_mod(q,q,n);
        if (max_iters && ++iters>=max_iters) { mpz_clear(k); goto done; }
        if (deadline_ms>0 && now_ms()>deadline_ms) { mpz_clear(k); goto done; }
      }
      mpz_gcd(g, q, n);
      mpz_add(k,k,m);
    }
    mpz_mul_ui(r, r, 2);
    if (max_iters && iters>=max_iters) break;
  }

  if (mpz_cmp(g, n)==0){
    do{
      if (deadline_ms>0 && now_ms()>deadline_ms) break;
      mpz_mul(ys,ys,ys); mpz_add(ys,ys,c); mpz_mod(ys,ys,n);
      mpz_sub(absdiff, x, ys); if (mpz_sgn(absdiff)<0) mpz_neg(absdiff, absdiff);
      mpz_gcd(g, absdiff, n);
    }while (mpz_cmp_ui(g,1)==0);
  }

done:;
  int ok = (mpz_cmp_ui(g,1)>0 && mpz_cmp(g,n)<0);
  if (ok) mpz_set(factor, g);
  mpz_clears(y,c,m,g,r,q,x,ys,absdiff, NULL);
  return ok;
}

/* -------- recursive splitter -------- */
static void add_factor(mpz_t **arr, size_t *len, size_t *cap, const mpz_t f){
  if (*len==*cap){ *cap = (*cap?*cap*2:8); *arr = (mpz_t*)realloc(*arr, (*cap)*sizeof(mpz_t)); }
  mpz_init_set((*arr)[(*len)++], f);
}

static void factor_rec(mpz_t n, mpz_t **out, size_t *len, size_t *cap,
                       double deadline_ms, unsigned p1_B,
                       unsigned long rho_restarts, unsigned long rho_max_iters)
{
  if (mpz_cmp_ui(n,1)==0) return;
  if (is_probable_prime(n)){ add_factor(out,len,cap,n); return; }

  mpz_t f; mpz_init(f);

  /* small trial division first */
  if (trial_division(f, n, 349)){
    mpz_t q; mpz_init(q); mpz_divexact(q, n, f);
    factor_rec(f, out, len, cap, deadline_ms, p1_B, rho_restarts, rho_max_iters);
    factor_rec(q, out, len, cap, deadline_ms, p1_B, rho_restarts, rho_max_iters);
    mpz_clears(f,q,NULL); return;
  }

  /* perfect square? */
  mpz_t r; mpz_init(r);
  mpz_sqrt(r, n); mpz_mul(r, r, r);
  if (mpz_cmp(r, n)==0){
    mpz_sqrt(r, n);
    factor_rec(r, out, len, cap, deadline_ms, p1_B, rho_restarts, rho_max_iters);
    factor_rec(r, out, len, cap, deadline_ms, p1_B, rho_restarts, rho_max_iters);
    mpz_clears(f,r,NULL); return;
  }
  mpz_clear(r);

  if (deadline_ms>0 && now_ms()>deadline_ms){ mpz_clear(f); return; }

  /* p-1 stage-1 (cheap win if smooth) */
  if (p1_B){
    if (pollard_pminus1(f, n, p1_B)){
      mpz_t q; mpz_init(q); mpz_divexact(q, n, f);
      factor_rec(f, out, len, cap, deadline_ms, p1_B, rho_restarts, rho_max_iters);
      factor_rec(q, out, len, cap, deadline_ms, p1_B, rho_restarts, rho_max_iters);
      mpz_clears(f,q,NULL); return;
    }
  }

  /* Brent rho with restarts */
  gmp_randstate_t rst; rnd_seed_gmp(rst, n);
  int found = 0;
  unsigned long tries = rho_restarts? rho_restarts : 1;
  for (unsigned long t=0; t<tries && !found; ++t){
    double dl = deadline_ms;
    if (deadline_ms>0 && now_ms()>deadline_ms) break;
    found = rho_brent(f, n, rst, rho_max_iters, dl);
  }
  gmp_randclear(rst);

  if (found){
    mpz_t q; mpz_init(q); mpz_divexact(q, n, f);
    factor_rec(f, out, len, cap, deadline_ms, p1_B, rho_restarts, rho_max_iters);
    factor_rec(q, out, len, cap, deadline_ms, p1_B, rho_restarts, rho_max_iters);
    mpz_clears(f,q,NULL); return;
  }

  mpz_clear(f);  /* give up for now */
}

/* -------- json helpers -------- */
static void print_json_factors(mpz_t *fs, size_t len){
  int *used = calloc(len, sizeof(int));
  printf("\"factors\":{");
  int first=1;
  for (size_t i=0;i<len;i++){
    if (used[i]) continue;
    int mult=1;
    for (size_t j=i+1;j<len;j++){
      if (!used[j] && mpz_cmp(fs[i], fs[j])==0){ used[j]=1; mult++; }
    }
    char *s = mpz_get_str(NULL,10,fs[i]);
    if (!first) printf(",");
    printf("\"%s\": %d", s, mult);
    free(s);
    first=0;
  }
  printf("}");
  free(used);
}
static void free_factors(mpz_t *fs, size_t len){ for(size_t i=0;i<len;i++) mpz_clear(fs[i]); free(fs); }

/* -------- CLI -------- */
static void usage(const char* prog){
  fprintf(stderr,
    "Usage:\n"
    "  %s prime  <n>\n"
    "  %s factor <n> [--timeout_ms T] [--p1_B B] [--rho_restarts R] [--rho_iters I]\n"
    "Notes: T=0 => no time limit; I=0 => unlimited rho iters when T=0.\n", prog, prog);
}

static int parse_mpz_or_err(mpz_t out, const char* s){
  if (!s || !*s) return -1;
  /* base 0 allows 0x.. etc */
  if (mpz_init_set_str(out, s, 0) != 0) return -1;
  return 0;
}

static void cmd_prime(const char* nstr){
  mpz_t n; if (parse_mpz_or_err(n, nstr)!=0){
    printf("{\"error\":\"invalid_integer\"}\n"); return;
  }
  int bits = mpz_sizeinbase(n,2);
  char *ns = mpz_get_str(NULL,10,n);
  if (is_probable_prime(n)){
    printf("{\"n\": %s, \"n_str\":\"%s\", \"classification\":\"prime\", \"factors\":{\"%s\":1}, \"bits\": %d}\n", ns, nstr, nstr, bits);
  }else{
    printf("{\"n\": %s, \"n_str\":\"%s\", \"classification\":\"composite\", \"bits\": %d}\n", ns, nstr, bits);
  }
  free(ns); mpz_clear(n);
}

static void cmd_factor(const char* nstr, long timeout_ms, unsigned p1_B,
                       unsigned long rho_restarts, unsigned long rho_iters)
{
  mpz_t n; if (parse_mpz_or_err(n, nstr)!=0){
    printf("{\"error\":\"invalid_integer\"}\n"); return;
  }
  int bits = mpz_sizeinbase(n,2);
  char *ns = mpz_get_str(NULL,10,n);

  if (is_probable_prime(n)){
    printf("{\"n\": %s, \"n_str\":\"%s\", \"classification\":\"prime\", \"factors\":{\"%s\":1}, \"bits\": %d, \"status\":\"ok\", \"params\":{\"timeout_ms\": %ld}}\n",
           ns, nstr, nstr, bits, timeout_ms);
    free(ns); mpz_clear(n); return;
  }

  double deadline = (timeout_ms<=0) ? -1.0 : (now_ms() + timeout_ms);

  mpz_t *fs=NULL; size_t len=0, cap=0;
  mpz_t ncopy; mpz_init_set(ncopy, n);

  /* defaults */
  if (!p1_B) p1_B = 200000;                      /* cheap-ish */
  if (!rho_restarts) rho_restarts = 16;          /* a few polynomials */
  if (!rho_iters) rho_iters = (timeout_ms<=0 ? 0 : 5000000UL); /* unlimited when no timeout */

  factor_rec(ncopy, &fs, &len, &cap, deadline, p1_B, rho_restarts, rho_iters);

  /* verify */
  mpz_t prod; mpz_init_set_ui(prod,1);
  for (size_t i=0;i<len;i++) mpz_mul(prod, prod, fs[i]);
  int success = (len>0 && mpz_cmp(prod, n)==0);

  if (success){
    printf("{\"n\": %s, \"n_str\":\"%s\", \"classification\":\"composite\", ", ns, nstr);
    print_json_factors(fs, len);
    printf(", \"bits\": %d, \"status\":\"ok\", \"params\":{\"timeout_ms\": %ld, \"p1_B\": %u, \"rho_restarts\": %lu, \"rho_iters\": %lu}}\n",
           bits, timeout_ms, p1_B, rho_restarts, rho_iters);
  }else{
    const char* s = (timeout_ms<=0 ? "noresult" : "timeout");
    printf("{\"n\": %s, \"n_str\":\"%s\", \"classification\":\"composite\", \"bits\": %d, \"status\":\"%s\", \"params\":{\"timeout_ms\": %ld, \"p1_B\": %u, \"rho_restarts\": %lu, \"rho_iters\": %lu}}\n",
           ns, nstr, bits, s, timeout_ms, p1_B, rho_restarts, rho_iters);
  }

  free(ns);
  mpz_clears(n, ncopy, prod, NULL);
  if (fs) free_factors(fs, len);
}

int main(int argc, char** argv){
  if (argc<3){ usage(argv[0]); return 1; }
  const char* cmd = argv[1];
  const char* nstr = argv[2];

  long timeout_ms = 3000;
  unsigned p1_B = 200000;
  unsigned long rho_restarts = 16;
  unsigned long rho_iters = 0;

  for (int i=3;i<argc;i++){
    if (!strcmp(argv[i],"--timeout_ms") && i+1<argc) timeout_ms = atol(argv[++i]);
    else if (!strcmp(argv[i],"--p1_B") && i+1<argc)  p1_B = (unsigned)strtoul(argv[++i],NULL,10);
    else if (!strcmp(argv[i],"--rho_restarts") && i+1<argc) rho_restarts = strtoul(argv[++i],NULL,10);
    else if (!strcmp(argv[i],"--rho_iters") && i+1<argc)    rho_iters    = strtoul(argv[++i],NULL,10);
  }

  if (!strcmp(cmd,"prime"))       { cmd_prime(nstr); }
  else if (!strcmp(cmd,"factor")) { cmd_factor(nstr, timeout_ms, p1_B, rho_restarts, rho_iters); }
  else { usage(argv[0]); return 1; }
  return 0;
}
