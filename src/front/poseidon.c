/* Poseidon over the BN254 scalar field (S11 step 11.3, O101), in the circomlib
 * 2.0.5 optimized order of PoseidonEx (poseidon.circom): Ark; 3 full rounds of
 * Sigma, Ark and Mix with M; 1 full round of Sigma, Ark and Mix with P;
 * nRoundsP partial rounds (Sigma on word 0, its round constant, MixS); 3 full
 * rounds of Sigma, Ark and Mix with M; Sigma; the last mix with column 0 of M.
 * Mix gives out[i] = sum over j of M[j][i] * in[j]. This is our own code: only
 * the numbers of poseidon_const.h come from circomlib (O104). */
#include "poseidon.h"

#include "poseidon_const.h"

#define T_MAX 3
#define FULL_HALF 4

typedef struct {
  size_t t;
  size_t partial;
  const char *const *c;
  const char *const *s;
  const char *const *m;
  const char *const *p;
} Params;

static const Params params2 = {2, 56, poseidon_c2, poseidon_s2, poseidon_m2, poseidon_p2};
static const Params params3 = {3, 57, poseidon_c3, poseidon_s3, poseidon_m3, poseidon_p3};

/* r, the BN254 scalar field modulus, by limb (limb 0 is the low 64 bits). */
static const U256 field_r = {
    {UINT64_C(0x43e1f593f0000001), UINT64_C(0x2833e84879b97091), UINT64_C(0xb85045b68181585d),
     UINT64_C(0x30644e72e131a029)}};

static const U256 zero = {{0, 0, 0, 0}};

/* a - r, for a at or above r. */
static U256 sub_r(U256 a) {
  U256 d;
  uint64_t borrow = 0;
  for (int i = 0; i < 4; i++) {
    uint64_t x = a.limb[i] - borrow;
    uint64_t under = a.limb[i] < borrow;
    d.limb[i] = x - field_r.limb[i];
    borrow = under + (x < field_r.limb[i]);
  }
  return d;
}

/* a mod r, for any word: 2^256 is under 6r. */
static U256 reduce(U256 a) {
  while (u256_cmp(&a, &field_r) >= 0) a = sub_r(a);
  return a;
}

/* a + b mod r, for a and b under r: the sum is under 2r, thus under 2^255. */
static U256 add_mod(U256 a, U256 b) {
  U256 s;
  uint64_t carry = 0;
  for (int i = 0; i < 4; i++) {
    uint64_t x = a.limb[i] + carry;
    uint64_t over = x < carry;
    s.limb[i] = x + b.limb[i];
    carry = over + (s.limb[i] < x);
  }
  return u256_cmp(&s, &field_r) >= 0 ? sub_r(s) : s;
}

/* a * b mod r, for a under r: double and add over the bits of b, high bit first. */
static U256 mul_mod(U256 a, U256 b) {
  U256 acc = zero;
  for (int bit = 255; bit >= 0; bit--) {
    acc = add_mod(acc, acc);
    if ((b.limb[bit / 64] >> (bit % 64)) & 1u) acc = add_mod(acc, a);
  }
  return acc;
}

/* The word of 64 lower-case hex digits, big-endian. */
static U256 word_of(const char *hex) {
  U256 w = zero;
  for (int i = 0; i < 64; i++) {
    char c = hex[i];
    uint64_t digit = c <= '9' ? (uint64_t)(c - '0') : (uint64_t)(c - 'a' + 10);
    int at = 63 - i;
    w.limb[at / 16] |= digit << (4 * (at % 16));
  }
  return w;
}

static U256 sigma(U256 x) {
  U256 x2 = mul_mod(x, x);
  return mul_mod(mul_mod(x2, x2), x);
}

static void sigma_all(U256 *st, size_t t) {
  for (size_t j = 0; j < t; j++) st[j] = sigma(st[j]);
}

static void ark(const Params *q, U256 *st, size_t at) {
  for (size_t j = 0; j < q->t; j++) st[j] = add_mod(st[j], word_of(q->c[at + j]));
}

/* out[i] = sum over j of m[j][i] * st[j]; m is row-major. */
static void mix(U256 *st, size_t t, const char *const *m) {
  U256 out[T_MAX];
  for (size_t i = 0; i < t; i++) {
    out[i] = zero;
    for (size_t j = 0; j < t; j++) out[i] = add_mod(out[i], mul_mod(st[j], word_of(m[j * t + i])));
  }
  for (size_t i = 0; i < t; i++) st[i] = out[i];
}

/* MixS of partial round r: word 0 is a row of S over all words, and word i > 0
 * adds word 0 times its S entry. */
static void mix_s(U256 *st, size_t t, const char *const *s, size_t r) {
  size_t base = (2 * t - 1) * r;
  U256 first = zero;
  for (size_t i = 0; i < t; i++) first = add_mod(first, mul_mod(st[i], word_of(s[base + i])));
  for (size_t i = 1; i < t; i++) st[i] = add_mod(st[i], mul_mod(st[0], word_of(s[base + t + i - 1])));
  st[0] = first;
}

int poseidon(U256 *out, const U256 *in, size_t n) {
  if (n != 1 && n != 2) return 0;
  const Params *q = n == 1 ? &params2 : &params3;
  size_t t = q->t;
  U256 st[T_MAX];
  st[0] = zero;
  for (size_t j = 1; j < t; j++) st[j] = reduce(in[j - 1]);
  ark(q, st, 0);
  for (size_t r = 0; r < FULL_HALF - 1; r++) {
    sigma_all(st, t);
    ark(q, st, (r + 1) * t);
    mix(st, t, q->m);
  }
  sigma_all(st, t);
  ark(q, st, FULL_HALF * t);
  mix(st, t, q->p);
  for (size_t r = 0; r < q->partial; r++) {
    st[0] = add_mod(sigma(st[0]), word_of(q->c[(FULL_HALF + 1) * t + r]));
    mix_s(st, t, q->s, r);
  }
  for (size_t r = 0; r < FULL_HALF - 1; r++) {
    sigma_all(st, t);
    ark(q, st, (FULL_HALF + 1) * t + q->partial + r * t);
    mix(st, t, q->m);
  }
  sigma_all(st, t);
  U256 acc = zero;
  for (size_t j = 0; j < t; j++) acc = add_mod(acc, mul_mod(st[j], word_of(q->m[j * t])));
  *out = acc;
  return 1;
}
