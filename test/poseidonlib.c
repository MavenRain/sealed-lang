/* The Poseidon(2) library of the deposit tree (S11 step 11.3, O101).
 *
 * build/poseidonlib prints the runtime code of one EVM contract as hex on
 * stdout. The contract reads two calldata words a and b and returns one word,
 * poseidon2 a b of src/front/poseidon.c (t = 3, the PoseidonEx order).
 * test/evmchain.sh puts the code at the library address 0x...f5 in each
 * prestate, and `poseidon2` lowers to a STATICCALL to that address
 * (src/lower.c lower_curve). The tables of src/front/poseidon_const.h (C, S, M
 * and P, in this order) follow the code: the contract copies them to memory
 * with CODECOPY. Three counted loops do the rounds. A full round falls through
 * into the one mix routine. Each sum and product is ADDMOD or MULMOD over r.
 * This is our own code: only the numbers of poseidon_const.h come from
 * circomlib (O104). `make check` builds the tool; test/precompile.sh runs the
 * library. */
#include <stdio.h>
#include <string.h>

#include "front/poseidon_const.h"

#define T 3
#define PARTIAL 57
#define FULL_HALF 4
#define MAX_CODE 4096
#define EIP170 24576

#define COUNT(a) (sizeof(a) / sizeof((a)[0]))

/* Memory: state word j at 32 * j; r at R_AT; at C_PTR and S_PTR the memory
 * address of the next word of C and of the next row of S; the tables from
 * C_BASE. */
#define R_AT 0x60u
#define C_PTR 0x80u
#define S_PTR 0xa0u
#define C_BASE 0x100u
#define S_BASE (C_BASE + 32u * (unsigned)COUNT(poseidon_c3))
#define M_BASE (S_BASE + 32u * (unsigned)COUNT(poseidon_s3))
#define P_BASE (M_BASE + 32u * (unsigned)COUNT(poseidon_m3))
#define DATA_SIZE (P_BASE + 32u * (unsigned)COUNT(poseidon_p3) - C_BASE)

static const char field_r[] = "30644e72e131a029b85045b68181585d2833e84879b9709143e1f593f0000001";

enum {
  ADD = 0x01, SUB = 0x03, ADDMOD = 0x08, MULMOD = 0x09, CALLDATALOAD = 0x35, CODECOPY = 0x39,
  POP = 0x50, MLOAD = 0x51, MSTORE = 0x52, JUMP = 0x56, JUMPI = 0x57, JUMPDEST = 0x5b,
  PUSH1 = 0x60, PUSH2 = 0x61, PUSH32 = 0x7f, DUP1 = 0x80, SWAP1 = 0x90, SWAP2 = 0x91, RETURN = 0xf3
};

enum { L_LOOP1, L_RET1, L_RET2, L_LOOP2, L_LOOP3, L_RET3, L_RET4, L_FULL, L_MIX, L_DATA, LABELS };

/* The code of one pass. label holds the offsets of the first pass, at the
 * offsets of this pass: each jump target is a PUSH2, thus the two agree. */
typedef struct {
  unsigned char byte[MAX_CODE];
  size_t n;
  int ok;
  size_t label[LABELS];
  size_t at[LABELS];
} Code;

static void op(Code *c, unsigned v) {
  if (c->n < MAX_CODE) c->byte[c->n++] = (unsigned char)v;
  else c->ok = 0;
}

static void push(Code *c, unsigned v) {
  if (v > 0xffu) op(c, PUSH2), op(c, v >> 8);
  else op(c, PUSH1);
  op(c, v & 0xffu);
}

static void push_label(Code *c, int k) {
  op(c, PUSH2);
  op(c, (unsigned)(c->label[k] >> 8));
  op(c, (unsigned)(c->label[k] & 0xffu));
}

static unsigned hex_digit(char ch) {
  return ch <= '9' ? (unsigned)(ch - '0') : (unsigned)(ch - 'a' + 10);
}

static void push_word(Code *c, const char *hex) {
  op(c, PUSH32);
  for (int i = 0; i < 32; i++) op(c, 16u * hex_digit(hex[2 * i]) + hex_digit(hex[2 * i + 1]));
}

static void mark(Code *c, int k) {
  c->at[k] = c->n;
  op(c, JUMPDEST);
}

static void load(Code *c, unsigned addr) {
  push(c, addr);
  op(c, MLOAD);
}

static void store(Code *c, unsigned addr) {
  push(c, addr);
  op(c, MSTORE);
}

static void offset(Code *c, unsigned off) {
  if (off > 0u) push(c, off), op(c, ADD);
}

/* The word at PTR := the word at PTR + BY. */
static void advance(Code *c, unsigned ptr, unsigned by) {
  load(c, ptr);
  offset(c, by);
  store(c, ptr);
}

/* x on top of the stack := x^5 mod r. */
static void sigma(Code *c) {
  load(c, R_AT), op(c, DUP1 + 1), op(c, DUP1), op(c, MULMOD); /* x, x^2 */
  load(c, R_AT), op(c, DUP1 + 1), op(c, DUP1), op(c, MULMOD); /* x, x^2, x^4 */
  op(c, SWAP1), op(c, POP);                                   /* x, x^4 */
  load(c, R_AT), op(c, SWAP2), op(c, MULMOD);                 /* x^5 */
}

static void sigma_all(Code *c) {
  for (unsigned j = 0; j < T; j++) {
    load(c, 32u * j);
    sigma(c);
    store(c, 32u * j);
  }
}

/* Word j := word j + C[next + j] mod r, then next := next + T: the round
 * constants are used in table order. */
static void ark(Code *c) {
  for (unsigned j = 0; j < T; j++) {
    load(c, R_AT);
    load(c, C_PTR);
    offset(c, 32u * j);
    op(c, MLOAD);
    load(c, 32u * j);
    op(c, ADDMOD);
    store(c, 32u * j);
  }
  advance(c, C_PTR, 32u * T);
}

/* Push the table word at BASE + OFF times state word J, mod r. BASE is at
 * stack position Q before the push of r, or at the memory word S_PTR when Q
 * is 0. */
static void term(Code *c, unsigned q, unsigned off, unsigned j) {
  load(c, R_AT);
  if (q > 0u) op(c, DUP1 + q);
  else load(c, S_PTR);
  offset(c, off);
  op(c, MLOAD);
  load(c, 32u * j);
  op(c, MULMOD);
}

/* The mix routine: word i := sum over j of M[j][i] * word j (src/front/poseidon.c
 * mix). The stack holds the return address and, on top, the base of M. Each
 * new word stays on the stack until the T words are done. */
static void mix(Code *c) {
  for (unsigned i = 0; i < T; i++) {
    unsigned base = 1u + i;
    term(c, base, 32u * i, 0);
    for (unsigned j = 1; j < T; j++) {
      load(c, R_AT), op(c, SWAP1);
      term(c, base + 2u, 32u * (j * T + i), j);
      op(c, ADDMOD);
    }
  }
  for (unsigned i = T; i > 0u; i--) store(c, 32u * (i - 1u));
  op(c, POP);
  op(c, JUMP);
}

/* One partial round: word 0 := sigma(word 0) + C[next] mod r, then MixS with
 * the row of S at S_PTR (src/front/poseidon.c mix_s). */
static void partial(Code *c) {
  load(c, 0);
  sigma(c);
  load(c, R_AT), op(c, SWAP1);
  load(c, C_PTR), op(c, MLOAD);
  op(c, ADDMOD);
  store(c, 0);
  advance(c, C_PTR, 32u);
  term(c, 0, 0, 0);
  for (unsigned i = 1; i < T; i++) {
    load(c, R_AT), op(c, SWAP1);
    term(c, 0, 32u * i, i);
    op(c, ADDMOD);
  }
  for (unsigned i = 1; i < T; i++) {
    load(c, R_AT);
    term(c, 0, 32u * (T + i - 1u), 0);
    load(c, 32u * i);
    op(c, ADDMOD);
    store(c, 32u * i);
  }
  store(c, 0);
  advance(c, S_PTR, 32u * (2u * T - 1u));
}

/* Jump to routine K with the table base BASE; the routine returns to RET. */
static void call(Code *c, int k, unsigned base, int ret) {
  push_label(c, ret);
  push(c, base);
  push_label(c, k);
  op(c, JUMP);
  mark(c, ret);
}

/* The end of a counted loop: count - 1, back to label K while it is not 0,
 * then drop the count. */
static void loop_end(Code *c, int k) {
  push(c, 1);
  op(c, SWAP1), op(c, SUB);
  op(c, DUP1);
  push_label(c, k);
  op(c, JUMPI);
  op(c, POP);
}

static void emit(Code *c) {
  c->n = 0;
  c->ok = 1;
  push_word(c, field_r), store(c, R_AT);
  push(c, C_BASE), store(c, C_PTR);
  push(c, S_BASE), store(c, S_PTR);
  push(c, DATA_SIZE), push_label(c, L_DATA), push(c, C_BASE), op(c, CODECOPY);
  push(c, 0), op(c, CALLDATALOAD), store(c, 32u);
  push(c, 32u), op(c, CALLDATALOAD), store(c, 64u);
  ark(c);
  push(c, FULL_HALF - 1);
  mark(c, L_LOOP1);
  call(c, L_FULL, M_BASE, L_RET1);
  loop_end(c, L_LOOP1);
  call(c, L_FULL, P_BASE, L_RET2);
  push(c, PARTIAL);
  mark(c, L_LOOP2);
  partial(c);
  loop_end(c, L_LOOP2);
  push(c, FULL_HALF - 1);
  mark(c, L_LOOP3);
  call(c, L_FULL, M_BASE, L_RET3);
  loop_end(c, L_LOOP3);
  sigma_all(c);
  call(c, L_MIX, M_BASE, L_RET4);
  push(c, 32u), push(c, 0), op(c, RETURN);
  mark(c, L_FULL);
  sigma_all(c);
  ark(c);
  mark(c, L_MIX);
  mix(c);
  c->at[L_DATA] = c->n;
}

static void put_table(const char *const *words, size_t n) {
  for (size_t i = 0; i < n; i++) fputs(words[i], stdout);
}

int main(void) {
  static Code code;
  /* The t = 2 tables are for poseidon1, which has no EVM form. */
  (void)poseidon_c2, (void)poseidon_s2, (void)poseidon_m2, (void)poseidon_p2;
  emit(&code);
  memcpy(code.label, code.at, sizeof code.label);
  emit(&code);
  if (!code.ok || memcmp(code.label, code.at, sizeof code.label) != 0 || code.n + DATA_SIZE > EIP170) {
    fprintf(stderr, "poseidonlib: the code does not fit (%zu bytes of code)\n", code.n);
    return 1;
  }
  for (size_t i = 0; i < code.n; i++) printf("%02x", code.byte[i]);
  put_table(poseidon_c3, COUNT(poseidon_c3));
  put_table(poseidon_s3, COUNT(poseidon_s3));
  put_table(poseidon_m3, COUNT(poseidon_m3));
  put_table(poseidon_p3, COUNT(poseidon_p3));
  putchar('\n');
  return 0;
}
