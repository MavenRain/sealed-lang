/* The evaluator, the conversion test, quote and the printer (see
   front/core.h). SHARED by the tcc kits. */
#include "front/core.h"
#include "keccak.h"

#include <string.h>

#define PRINT_DEPTH_MAX 200u
#define PRINT_NAMES_MAX PRINT_DEPTH_MAX
#define VARIES 0xffu

static const char *const OP_NAMES[] = {
  [OP_NAT] = "Nat", [OP_FLAG] = "Flag", [OP_U256] = "U256", [OP_ADDR] = "Addr", [OP_UNIT] = "Unit", [OP_PROD] = "Prod",
  [OP_SUM] = "Sum", [OP_OPTION] = "Option", [OP_LIST] = "List",
  [OP_FAMILY] = "?", [OP_UNIT_VAL] = "unit", [OP_FLAG_YES] = "flagYes",
  [OP_FLAG_NO] = "flagNo", [OP_PAIR] = "pair", [OP_INL] = "inl", [OP_INR] = "inr",
  [OP_NONE] = "none", [OP_SOME] = "some", [OP_NIL] = "nil", [OP_CONS] = "cons",
  [OP_PACK] = "pack", [OP_CTOR] = "?", [OP_FIRST] = "first",
  [OP_SECOND] = "second", [OP_EITHER] = "either", [OP_OPTION_ELIM] = "option",
  [OP_PURE] = "pure", [OP_MAP] = "map", [OP_BIND] = "bind", [OP_FOLD_NAT] = "fold",
  [OP_FOLD_LIST] = "fold", [OP_FOLD_FAMILY] = "fold", [OP_UNFOLD] = "unfold",
  [OP_FILTER] = "filter", [OP_WITNESS] = "witness", [OP_PAYLOAD] = "payload",
  [OP_NAT_ADD] = "natAdd", [OP_NAT_SUB] = "natSub",
  [OP_NAT_EQ] = "natEq", [OP_NAT_LE] = "natLe",
  [OP_FLAG_IF] = "flagIf", [OP_U256_ADD] = "u256Add", [OP_U256_SUB] = "u256Sub",
  [OP_U256_LE] = "u256Le",
  [OP_U256_EQ] = "u256Eq", [OP_TO_U256] = "toU256",
  [OP_NAT_DIV] = "natDiv", [OP_NAT_MOD] = "natMod", [OP_HASH] = "hash", [OP_ECRECOVER] = "ecrecover",
  [OP_EC_ADD] = "ecAdd", [OP_EC_MUL] = "ecMul", [OP_EC_PAIRING] = "ecPairing", [OP_ADDR_TO_U256] = "addrToU256",
  [OP_ADDR_EQ] = "addrEq", [OP_PROJ] = "?", [OP_KMAP] = "Map", [OP_KMAP_OF] = "mapOf",
  [OP_KMAP_GET] = "mapGet", [OP_KMAP_SET] = "mapSet",
  [OP_INTERVAL] = "I", [OP_TRANSP] = "transp",
  [OP_SYSTEM] = "system", [OP_HCOMP] = "hcomp"
};

/* The number of arguments of each operation. VARIES: it comes from the
   family tables. */
static const unsigned char OP_ARITY[] = {
  [OP_NAT] = 0, [OP_FLAG] = 0, [OP_U256] = 0, [OP_ADDR] = 0, [OP_UNIT] = 0, [OP_PROD] = 2, [OP_SUM] = 2,
  [OP_OPTION] = 1, [OP_LIST] = 1, [OP_FAMILY] = VARIES,
  [OP_UNIT_VAL] = 0, [OP_FLAG_YES] = 0, [OP_FLAG_NO] = 0, [OP_PAIR] = 2,
  [OP_INL] = 1, [OP_INR] = 1, [OP_NONE] = 0, [OP_SOME] = 1, [OP_NIL] = 0,
  [OP_CONS] = 2, [OP_PACK] = 2, [OP_CTOR] = VARIES, [OP_FIRST] = 1,
  [OP_SECOND] = 1, [OP_EITHER] = 3, [OP_OPTION_ELIM] = 3, [OP_PURE] = 1,
  [OP_MAP] = 2, [OP_BIND] = 2, [OP_FOLD_NAT] = 3, [OP_FOLD_LIST] = 3,
  [OP_FOLD_FAMILY] = VARIES, [OP_UNFOLD] = 3, [OP_FILTER] = 2, [OP_WITNESS] = 1,
  [OP_PAYLOAD] = 1, [OP_NAT_ADD] = 2, [OP_NAT_SUB] = 2,
  [OP_NAT_EQ] = 2, [OP_NAT_LE] = 2, [OP_FLAG_IF] = 3, [OP_U256_ADD] = 2,
  [OP_U256_SUB] = 2, [OP_U256_LE] = 2,
  [OP_U256_EQ] = 2, [OP_TO_U256] = 1, [OP_ADDR_EQ] = 2,
  [OP_NAT_DIV] = 2, [OP_NAT_MOD] = 2, [OP_HASH] = 2, [OP_ECRECOVER] = 4,
  [OP_EC_ADD] = 2, [OP_EC_MUL] = 2, [OP_EC_PAIRING] = 1, [OP_ADDR_TO_U256] = 1,
  [OP_PROJ] = 1, [OP_KMAP] = 2, [OP_KMAP_OF] = VARIES, [OP_KMAP_GET] = 3, [OP_KMAP_SET] = 4,
  [OP_INTERVAL] = 0, [OP_TRANSP] = 3,
  [OP_SYSTEM] = VARIES, [OP_HCOMP] = 4
};

typedef enum {
  SCRUT_VALUE,
  SCRUT_TRAP,
  SCRUT_NEUTRAL
} Scrut;

typedef struct {
  const Value **items;
  uint32_t count;
  uint32_t cap;
} ValBuf;

/* A list value: its cons cells, then the end (nil, a trap or a neutral). */
typedef struct {
  const Value *const *cells;
  uint32_t count;
  const Value *tail;
} Spine;

static const Value *oom(Machine *m) {
  diag_fail(m->diag, "OOM", m->def, "the arena limit is reached");
  return NULL;
}

static const Value *internal(Machine *m, const char *what) {
  diag_fail(m->diag, "INTERNAL", m->def, "the evaluator found %s", what);
  return NULL;
}

static int spend(Machine *m, uint64_t steps) {
  if (m->fuel >= steps) {
    m->fuel -= steps;
    return 1;
  }
  m->fuel = 0;
  return diag_fail(m->diag, "EVAL_FUEL", m->def, "the evaluation takes more than %u steps", EVAL_FUEL_STEPS);
}

static int enter(Machine *m) {
  if (m->depth < EVAL_DEPTH_LIMIT) {
    m->depth++;
    return 1;
  }
  return diag_fail(m->diag, "EVAL_DEPTH", m->def, "the evaluation nests deeper than %u levels", EVAL_DEPTH_LIMIT);
}

static Value *new_value(Machine *m, ValKind kind) {
  Value *v = arena_alloc(m->arena, sizeof *v);
  if (v == NULL) {
    oom(m);
    return NULL;
  }
  v->kind = kind;
  v->tag = CLOS_CORE;
  return v;
}

const Value *val_nat(Machine *m, uint64_t n) {
  Value *v = new_value(m, VAL_NAT);
  if (v != NULL)
    v->nat = n;
  return v;
}

const Value *val_word(Machine *m, uint64_t sort, const U256 *word) {
  Value *v = new_value(m, VAL_WORD);
  if (v != NULL) {
    v->nat = sort;
    v->word = word;
  }
  return v;
}

static const Value *val_trap(Machine *m) {
  return new_value(m, VAL_TRAP);
}

static const Value *overflow(Machine *m) {
  m->overflowed = 1;
  return val_trap(m);
}

const Value *val_var(Machine *m, uint32_t level) {
  Value *v = new_value(m, VAL_VAR);
  if (v != NULL)
    v->nat = level;
  return v;
}

const Value *val_univ(Machine *m, uint64_t level) {
  Value *v = new_value(m, VAL_UNIV);
  if (v != NULL)
    v->nat = level;
  return v;
}

/* The interval is the free De Morgan algebra (R4 (a)). A value is a join
   of conjunctions of literals, in a canonical order (length, then the
   literals), with absorption. ~~i = i, and ~ swaps meet and join. There is
   no rule i /\ ~i = i0: the interval is not Boolean. */

/* 1 when each literal of A is in B. */
static int conj_subset(const IConj *a, const IConj *b) {
  uint32_t i = 0;
  uint32_t j = 0;
  while (i < a->count && j < b->count && a->lits[i] >= b->lits[j]) {
    i += a->lits[i] == b->lits[j];
    j++;
  }
  return i == a->count;
}

static int conj_cmp(const IConj *a, const IConj *b) {
  uint32_t i;
  if (a->count != b->count)
    return a->count < b->count ? -1 : 1;
  for (i = 0; i < a->count; i++)
    if (a->lits[i] != b->lits[i])
      return a->lits[i] < b->lits[i] ? -1 : 1;
  return 0;
}

static int conj_union(Machine *m, const IConj *a, const IConj *b, IConj *out) {
  uint32_t *lits = arena_alloc(m->arena, ((size_t)a->count + b->count + 1u) * sizeof *lits);
  uint32_t i = 0;
  uint32_t j = 0;
  uint32_t k = 0;
  if (lits == NULL) {
    oom(m);
    return 0;
  }
  while (i < a->count || j < b->count) {
    int take_a = j == b->count || (i < a->count && a->lits[i] <= b->lits[j]);
    int take_b = i == a->count || (j < b->count && b->lits[j] <= a->lits[i]);
    lits[k++] = take_a ? a->lits[i] : b->lits[j];
    i += take_a;
    j += take_b;
  }
  out->lits = lits;
  out->count = k;
  return 1;
}

/* The normal form of the join of the N conjunctions CS: absorption, then
   the canonical order. */
static const Value *ival_make(Machine *m, const IConj *cs, uint32_t n) {
  IConj *out = arena_alloc(m->arena, ((size_t)n + 1u) * sizeof *out);
  Value *v;
  uint32_t i;
  uint32_t j;
  uint32_t k = 0;
  int drop;
  if (out == NULL)
    return oom(m);
  for (i = 0; i < n; i++) {
    drop = 0;
    for (j = 0; j < n && !drop; j++)
      drop = j != i && conj_subset(&cs[j], &cs[i]) && (cs[j].count < cs[i].count || j < i);
    for (j = k; j > 0 && !drop && conj_cmp(&out[j - 1u], &cs[i]) > 0; j--)
      out[j] = out[j - 1u];
    out[j] = drop ? out[j] : cs[i];
    k += !drop;
  }
  v = new_value(m, VAL_IVAL);
  if (v == NULL)
    return NULL;
  v->conj = out;
  v->conj_count = k;
  return v;
}

static const Value *ival_lit(Machine *m, uint32_t lit) {
  uint32_t *lits = arena_alloc(m->arena, sizeof *lits);
  IConj c;
  if (lits == NULL)
    return oom(m);
  lits[0] = lit;
  c.lits = lits;
  c.count = 1;
  return ival_make(m, &c, 1);
}

const Value *val_ivar(Machine *m, uint32_t level) {
  return ival_lit(m, level * 2u);
}

const Value *val_iend(Machine *m, int side) {
  IConj c;
  c.lits = NULL;
  c.count = 0;
  return ival_make(m, &c, side != 0 ? 1u : 0u);
}

static const Value *ival_join(Machine *m, const Value *a, const Value *b) {
  IConj *cs;
  uint32_t i;
  if (a == NULL || b == NULL)
    return NULL;
  cs = arena_alloc(m->arena, ((size_t)a->conj_count + b->conj_count + 1u) * sizeof *cs);
  if (cs == NULL)
    return oom(m);
  for (i = 0; i < a->conj_count; i++)
    cs[i] = a->conj[i];
  for (i = 0; i < b->conj_count; i++)
    cs[a->conj_count + i] = b->conj[i];
  return ival_make(m, cs, a->conj_count + b->conj_count);
}

static const Value *ival_meet(Machine *m, const Value *a, const Value *b) {
  IConj *cs;
  uint32_t i;
  uint32_t j;
  if (a == NULL || b == NULL)
    return NULL;
  cs = arena_alloc(m->arena, ((size_t)a->conj_count * b->conj_count + 1u) * sizeof *cs);
  if (cs == NULL)
    return oom(m);
  for (i = 0; i < a->conj_count; i++)
    for (j = 0; j < b->conj_count; j++)
      if (!conj_union(m, &a->conj[i], &b->conj[j], &cs[i * b->conj_count + j]))
        return NULL;
  return ival_make(m, cs, a->conj_count * b->conj_count);
}

/* De Morgan: ~(c1 \/ c2) = ~c1 /\ ~c2 and ~(l1 /\ l2) = ~l1 \/ ~l2. */
static const Value *ival_neg(Machine *m, const Value *a) {
  const Value *out = val_iend(m, 1);
  const Value *flipped;
  uint32_t i;
  uint32_t j;
  if (a == NULL)
    return NULL;
  for (i = 0; i < a->conj_count && out != NULL; i++) {
    flipped = val_iend(m, 0);
    for (j = 0; j < a->conj[i].count; j++)
      flipped = ival_join(m, flipped, ival_lit(m, a->conj[i].lits[j] ^ 1u));
    out = ival_meet(m, out, flipped);
  }
  return out;
}

static int ival_equal(const Value *a, const Value *b) {
  uint32_t i;
  if (a->conj_count != b->conj_count)
    return 0;
  for (i = 0; i < a->conj_count; i++)
    if (conj_cmp(&a->conj[i], &b->conj[i]) != 0)
      return 0;
  return 1;
}

int ival_is_end(const Value *v, int side) {
  if (v == NULL || v->kind != VAL_IVAL)
    return 0;
  return side != 0 ? v->conj_count == 1 && v->conj[0].count == 0 : v->conj_count == 0;
}

/* A literal and its negation are adjacent in a sorted conjunction, and the
   literal is the even one. */
static int conj_empty_face(const IConj *c) {
  uint32_t j;
  for (j = 1; j < c->count; j++)
    if ((c->lits[j - 1u] ^ 1u) == c->lits[j])
      return 1;
  return 0;
}

const Value *face_norm(Machine *m, const Value *phi) {
  IConj *cs;
  uint32_t i;
  uint32_t k = 0;
  if (phi == NULL)
    return NULL;
  cs = arena_alloc(m->arena, ((size_t)phi->conj_count + 1u) * sizeof *cs);
  if (cs == NULL)
    return oom(m);
  for (i = 0; i < phi->conj_count; i++)
    if (!conj_empty_face(&phi->conj[i]))
      cs[k++] = phi->conj[i];
  return ival_make(m, cs, k);
}

const Value *face_join(Machine *m, const Value *a, const Value *b) {
  return face_norm(m, ival_join(m, a, b));
}

const Value *face_meet(Machine *m, const Value *a, const Value *b) {
  return face_norm(m, ival_meet(m, a, b));
}

const Value *ival_face(Machine *m, const Value *r, uint32_t level, int side) {
  /* The literal that the map makes true, and the one it makes false. */
  uint32_t keep = level * 2u + (side != 0 ? 0u : 1u);
  uint32_t kill = keep ^ 1u;
  IConj *cs;
  uint32_t *lits;
  uint32_t i;
  uint32_t j;
  uint32_t n;
  uint32_t k = 0;
  int dead;
  if (r == NULL)
    return NULL;
  cs = arena_alloc(m->arena, ((size_t)r->conj_count + 1u) * sizeof *cs);
  if (cs == NULL)
    return oom(m);
  for (i = 0; i < r->conj_count; i++) {
    lits = arena_alloc(m->arena, ((size_t)r->conj[i].count + 1u) * sizeof *lits);
    if (lits == NULL)
      return oom(m);
    dead = 0;
    n = 0;
    for (j = 0; j < r->conj[i].count; j++) {
      dead |= r->conj[i].lits[j] == kill;
      lits[n] = r->conj[i].lits[j];
      n += r->conj[i].lits[j] != keep && r->conj[i].lits[j] != kill;
    }
    cs[k].lits = lits;
    cs[k].count = n;
    k += !dead;
  }
  return ival_make(m, cs, k);
}

int env_face(Machine *m, const Env *env, const IConj *c, const Env **out) {
  const Value **vals;
  const Value *v;
  const Env *e;
  uint32_t n = 0;
  uint32_t i;
  uint32_t j;
  for (e = env; e != NULL; e = e->next)
    n++;
  vals = arena_alloc(m->arena, ((size_t)n + 1u) * sizeof *vals);
  if (vals == NULL) {
    oom(m);
    return 0;
  }
  for (e = env, i = 0; e != NULL; e = e->next, i++) {
    v = e->value;
    for (j = 0; j < c->count && v != NULL && v->kind == VAL_IVAL; j++)
      v = ival_face(m, v, c->lits[j] >> 1u, (c->lits[j] & 1u) == 0u);
    if (v == NULL)
      return 0;
    vals[i] = v;
  }
  *out = NULL;
  for (i = n; i > 0; i--) {
    *out = env_push(m, *out, vals[i - 1u]);
    if (*out == NULL)
      return 0;
  }
  return 1;
}

const Value *val_op(Machine *m, ValKind kind, Op op, uint32_t inst, uint32_t field, const Value *const *args, uint32_t argc) {
  const Value **copy;
  Value *v;
  uint32_t i;
  for (i = 0; i < argc; i++)
    if (args[i] == NULL)
      return NULL;
  copy = arena_alloc(m->arena, ((size_t)argc + 1u) * sizeof *copy);
  if (copy == NULL)
    return oom(m);
  v = new_value(m, kind);
  if (v == NULL)
    return NULL;
  for (i = 0; i < argc; i++)
    copy[i] = args[i];
  v->op = op;
  v->inst = inst;
  v->field = field;
  v->args = copy;
  v->argc = argc;
  return v;
}

static const Value *make(Machine *m, ValKind kind, Op op, uint32_t inst, uint32_t argc, const Value *a, const Value *b, const Value *c) {
  const Value *args[3];
  args[0] = a;
  args[1] = b;
  args[2] = c;
  return val_op(m, kind, op, inst, 0, args, argc);
}

const Value *val_make(Machine *m, Op op, uint32_t inst, uint32_t argc, const Value *a, const Value *b, const Value *c) {
  return make(m, VAL_OP, op, inst, argc, a, b, c);
}

static const Value *constant(Machine *m, Op op) {
  return make(m, VAL_OP, op, 0, 0, NULL, NULL, NULL);
}

static const Value *flag(Machine *m, int yes) {
  return constant(m, yes ? OP_FLAG_YES : OP_FLAG_NO);
}

const Value *val_arrow(Machine *m, const Value *dom, const Value *cod) {
  Value *v;
  if (dom == NULL || cod == NULL)
    return NULL;
  v = new_value(m, VAL_PI);
  if (v == NULL)
    return NULL;
  v->dom = dom;
  v->arg = cod;
  return v;
}

const Value *val_closure(Machine *m, ValKind kind, const char *name, const Value *dom, const Env *env, const Core *body) {
  Value *v;
  if (body == NULL || (dom == NULL && kind != VAL_LAM && kind != VAL_PLAM))
    return NULL;
  v = new_value(m, kind);
  if (v == NULL)
    return NULL;
  v->name = name;
  v->dom = dom;
  v->env = env;
  v->body = body;
  return v;
}

int val_is(const Value *v, Op op) {
  return v != NULL && v->kind == VAL_OP && v->op == op;
}

const Env *env_push(Machine *m, const Env *env, const Value *value) {
  Env *e;
  if (value == NULL)
    return NULL;
  e = arena_alloc(m->arena, sizeof *e);
  if (e == NULL) {
    oom(m);
    return NULL;
  }
  e->value = value;
  e->next = env;
  return e;
}

static const Value *env_lookup(Machine *m, const Env *env, uint32_t index) {
  uint32_t i;
  for (i = 0; i < index && env != NULL; i++)
    env = env->next;
  if (env == NULL)
    return internal(m, "a variable outside its scope");
  return env->value;
}

const Value *def_value(Machine *m, uint32_t index) {
  DefInfo *d;
  if (index >= m->def_count)
    return internal(m, "an unknown definition");
  d = &m->defs[index];
  if (d->value == NULL)
    d->value = eval_core(m, NULL, d->body);
  return d->value;
}

static Scrut scrut_of(const Value *v) {
  switch (v->kind) {
  case VAL_TRAP:
    return SCRUT_TRAP;
  case VAL_VAR:
  case VAL_APP:
  case VAL_STUCK:
  case VAL_PAPP:
    return SCRUT_NEUTRAL;
  case VAL_NAT:
  case VAL_WORD:
  case VAL_UNIV:
  case VAL_LAM:
  case VAL_PI:
  case VAL_SIGMA:
  case VAL_OP:
  case VAL_PLAM:
  case VAL_PATHP:
  case VAL_IVAL:
    return SCRUT_VALUE;
  }
  return SCRUT_VALUE;
}

/* 1 when the scrutinee S stops the operation: a trap gives the trap, a
   neutral S gives the stuck operation. */
static int blocked(Machine *m, const Value *s, Op op, uint32_t inst, uint32_t field, const Value *const *a, uint32_t n, const Value **out) {
  switch (scrut_of(s)) {
  case SCRUT_TRAP:
    *out = s;
    return 1;
  case SCRUT_NEUTRAL:
    *out = val_op(m, VAL_STUCK, op, inst, field, a, n);
    return 1;
  case SCRUT_VALUE:
    return 0;
  }
  return 0;
}

static void buf_init(ValBuf *buf) {
  buf->items = NULL;
  buf->count = 0;
  buf->cap = 0;
}

static int buf_push(Machine *m, ValBuf *buf, const Value *v) {
  const Value **grown;
  uint32_t cap;
  uint32_t i;
  if (v == NULL)
    return 0;
  if (buf->count < buf->cap) {
    buf->items[buf->count++] = v;
    return 1;
  }
  cap = buf->cap == 0 ? 16u : buf->cap * 2u;
  grown = arena_alloc(m->arena, (size_t)cap * sizeof *grown);
  if (grown == NULL) {
    oom(m);
    return 0;
  }
  for (i = 0; i < buf->count; i++)
    grown[i] = buf->items[i];
  buf->items = grown;
  buf->cap = cap;
  buf->items[buf->count++] = v;
  return 1;
}

static int spine_of(Machine *m, const Value *xs, Spine *out) {
  ValBuf cells;
  buf_init(&cells);
  while (val_is(xs, OP_CONS)) {
    if (!buf_push(m, &cells, xs) || !spend(m, 1))
      return 0;
    xs = xs->args[1];
  }
  out->cells = cells.items;
  out->count = cells.count;
  out->tail = xs;
  return xs != NULL;
}

static const Value *build_list(Machine *m, const Value *const *items, uint32_t count, const Value *tail) {
  const Value *acc = tail;
  uint32_t i;
  for (i = count; i > 0 && acc != NULL; i--)
    acc = make(m, VAL_OP, OP_CONS, 0, 2, items[i - 1u], acc, NULL);
  return acc;
}

/* The end of a list result: nil stays nil, a trap stays a trap, a neutral
   end gives OP FIRST END. */
static const Value *list_tail(Machine *m, const Value *tail, Op op, uint32_t inst, const Value *first) {
  switch (scrut_of(tail)) {
  case SCRUT_TRAP:
    return tail;
  case SCRUT_NEUTRAL:
    return make(m, VAL_STUCK, op, inst, 2, first, tail, NULL);
  case SCRUT_VALUE:
    break;
  }
  return val_is(tail, OP_NIL) ? tail : internal(m, "a list that does not end in nil");
}

const Value *apply_value(Machine *m, const Value *fn, const Value *arg) {
  Value *v;
  if (fn == NULL || arg == NULL)
    return NULL;
  switch (fn->kind) {
  case VAL_LAM:
    return closure_apply(m, fn, arg);
  case VAL_TRAP:
    return fn;
  case VAL_VAR:
  case VAL_APP:
  case VAL_STUCK:
  case VAL_PAPP:
    v = new_value(m, VAL_APP);
    if (v == NULL)
      return NULL;
    v->dom = fn;
    v->arg = arg;
    return v;
  case VAL_NAT:
  case VAL_WORD:
  case VAL_UNIV:
  case VAL_PI:
  case VAL_SIGMA:
  case VAL_OP:
  case VAL_PLAM:
  case VAL_PATHP:
  case VAL_IVAL:
    break;
  }
  return internal(m, "an application of a value that is not a function");
}

static const Value *kan_apply(Machine *m, const Value *k, const Value *r);

const Value *closure_apply(Machine *m, const Value *binder, const Value *arg) {
  const Env *env;
  if (binder == NULL || arg == NULL)
    return NULL;
  if (binder->tag != CLOS_CORE)
    return kan_apply(m, binder, arg);
  if (binder->body == NULL)
    return binder->arg;
  env = env_push(m, binder->env, arg);
  if (env == NULL)
    return NULL;
  return eval_core(m, env, binder->body);
}

const Value *path_apply(Machine *m, const Value *p, const Value *r, const Value *a, const Value *b) {
  Value *v;
  const Value **ends;
  if (p == NULL || r == NULL)
    return NULL;
  if (p->kind == VAL_PLAM)
    return closure_apply(m, p, r);
  if (p->kind == VAL_TRAP)
    return p;
  if (scrut_of(p) != SCRUT_NEUTRAL)
    return internal(m, "a path application of a value that is not a path");
  /* The end point rule: a neutral path at i0 or i1 is its end. */
  if (ival_is_end(r, 0) && a != NULL)
    return a;
  if (ival_is_end(r, 1) && b != NULL)
    return b;
  ends = arena_alloc(m->arena, 2u * sizeof *ends);
  v = new_value(m, VAL_PAPP);
  if (ends == NULL || v == NULL)
    return oom(m);
  ends[0] = a;
  ends[1] = b;
  v->dom = p;
  v->arg = r;
  v->args = ends;
  v->argc = 2;
  return v;
}

static const Value *reduce_part(Machine *m, Op op, const Value *const *a, uint32_t n, Op ctor, uint32_t index) {
  const Value *out;
  if (blocked(m, a[0], op, 0, 0, a, n, &out))
    return out;
  if (!val_is(a[0], ctor))
    return internal(m, "a projection of a value of the wrong type");
  return a[0]->args[index];
}

static const Value *reduce_either(Machine *m, const Value *const *a, uint32_t n) {
  const Value *out;
  if (blocked(m, a[2], OP_EITHER, 0, 0, a, n, &out))
    return out;
  if (val_is(a[2], OP_INL))
    return apply_value(m, a[0], a[2]->args[0]);
  if (val_is(a[2], OP_INR))
    return apply_value(m, a[1], a[2]->args[0]);
  return internal(m, "an either of a value that is not a Sum");
}

static const Value *reduce_option(Machine *m, const Value *const *a, uint32_t n) {
  const Value *out;
  if (blocked(m, a[2], OP_OPTION_ELIM, 0, 0, a, n, &out))
    return out;
  if (val_is(a[2], OP_NONE))
    return a[0];
  if (val_is(a[2], OP_SOME))
    return apply_value(m, a[1], a[2]->args[0]);
  return internal(m, "an option of a value that is not an Option");
}

static const Value *reduce_pure(Machine *m, uint32_t inst, const Value *x) {
  switch ((Carrier)inst) {
  case CARRIER_OPTION:
    return make(m, VAL_OP, OP_SOME, 0, 1, x, NULL, NULL);
  case CARRIER_LIST:
    return make(m, VAL_OP, OP_CONS, 0, 2, x, constant(m, OP_NIL), NULL);
  case CARRIER_SUM:
    return make(m, VAL_OP, OP_INR, 0, 1, x, NULL, NULL);
  }
  return internal(m, "an unknown carrier");
}

static const Value *map_list(Machine *m, const Value *f, const Value *xs) {
  Spine s;
  ValBuf out;
  uint32_t i;
  if (!spine_of(m, xs, &s))
    return NULL;
  buf_init(&out);
  for (i = 0; i < s.count; i++)
    if (!buf_push(m, &out, apply_value(m, f, s.cells[i]->args[0])))
      return NULL;
  return build_list(m, out.items, out.count, list_tail(m, s.tail, OP_MAP, CARRIER_LIST, f));
}

static const Value *reduce_map(Machine *m, uint32_t inst, const Value *const *a, uint32_t n) {
  const Value *x = a[1];
  const Value *out;
  if (blocked(m, x, OP_MAP, inst, 0, a, n, &out))
    return out;
  switch ((Carrier)inst) {
  case CARRIER_OPTION:
    return val_is(x, OP_SOME) ? make(m, VAL_OP, OP_SOME, 0, 1, apply_value(m, a[0], x->args[0]), NULL, NULL) : x;
  case CARRIER_SUM:
    return val_is(x, OP_INR) ? make(m, VAL_OP, OP_INR, 0, 1, apply_value(m, a[0], x->args[0]), NULL, NULL) : x;
  case CARRIER_LIST:
    return map_list(m, a[0], x);
  }
  return internal(m, "an unknown carrier");
}

static int buf_append(Machine *m, ValBuf *buf, const Spine *s) {
  uint32_t i;
  for (i = 0; i < s->count; i++)
    if (!buf_push(m, buf, s->cells[i]->args[0]))
      return 0;
  return 1;
}

/* A list bind concatenates the lists F X. A neutral end in any of them
   leaves the whole bind stuck. */
static const Value *bind_list(Machine *m, const Value *const *a, uint32_t n) {
  Spine s;
  Spine t;
  ValBuf out;
  uint32_t i;
  if (!spine_of(m, a[0], &s))
    return NULL;
  if (scrut_of(s.tail) == SCRUT_NEUTRAL)
    return val_op(m, VAL_STUCK, OP_BIND, CARRIER_LIST, 0, a, n);
  buf_init(&out);
  for (i = 0; i < s.count; i++) {
    if (!spine_of(m, apply_value(m, a[1], s.cells[i]->args[0]), &t) || !buf_append(m, &out, &t))
      return NULL;
    if (scrut_of(t.tail) == SCRUT_NEUTRAL)
      return val_op(m, VAL_STUCK, OP_BIND, CARRIER_LIST, 0, a, n);
    if (scrut_of(t.tail) == SCRUT_TRAP)
      return build_list(m, out.items, out.count, t.tail);
  }
  return build_list(m, out.items, out.count, list_tail(m, s.tail, OP_BIND, CARRIER_LIST, a[1]));
}

static const Value *reduce_bind(Machine *m, uint32_t inst, const Value *const *a, uint32_t n) {
  const Value *x = a[0];
  const Value *out;
  if (blocked(m, x, OP_BIND, inst, 0, a, n, &out))
    return out;
  switch ((Carrier)inst) {
  case CARRIER_OPTION:
    return val_is(x, OP_SOME) ? apply_value(m, a[1], x->args[0]) : x;
  case CARRIER_SUM:
    return val_is(x, OP_INR) ? apply_value(m, a[1], x->args[0]) : x;
  case CARRIER_LIST:
    return bind_list(m, a, n);
  }
  return internal(m, "an unknown carrier");
}

/* fold step z n: step applied n times to z. */
static const Value *reduce_fold_nat(Machine *m, const Value *const *a, uint32_t n) {
  const Value *acc = a[1];
  const Value *out;
  uint64_t k;
  if (blocked(m, a[2], OP_FOLD_NAT, 0, 0, a, n, &out))
    return out;
  if (a[2]->kind != VAL_NAT)
    return internal(m, "a fold of a value that is not a Nat");
  for (k = 0; k < a[2]->nat && acc != NULL; k++)
    acc = spend(m, 1) ? apply_value(m, a[0], acc) : NULL;
  return acc;
}

/* fold f z xs: the right fold. The end of the list is folded first. */
static const Value *reduce_fold_list(Machine *m, const Value *const *a) {
  const Value *acc = NULL;
  Spine s;
  uint32_t i;
  if (!spine_of(m, a[2], &s))
    return NULL;
  switch (scrut_of(s.tail)) {
  case SCRUT_TRAP:
    acc = s.tail;
    break;
  case SCRUT_NEUTRAL:
    acc = make(m, VAL_STUCK, OP_FOLD_LIST, 0, 3, a[0], a[1], s.tail);
    break;
  case SCRUT_VALUE:
    acc = val_is(s.tail, OP_NIL) ? a[1] : internal(m, "a list that does not end in nil");
    break;
  }
  for (i = s.count; i > 0 && acc != NULL; i--)
    acc = apply_value(m, apply_value(m, a[0], s.cells[i - 1u]->args[0]), acc);
  return acc;
}

/* CASES holds NCASES cases, then the result type RES at CASES[NCASES]. */
static const Value *stuck_fold(Machine *m, uint32_t family, const Value *const *cases, uint32_t ncases, const Value *x) {
  const Value **args = arena_alloc(m->arena, ((size_t)ncases + 2u) * sizeof *args);
  uint32_t i;
  if (args == NULL)
    return oom(m);
  for (i = 0; i <= ncases; i++)
    args[i] = cases[i];
  args[ncases + 1u] = x;
  return val_op(m, VAL_STUCK, OP_FOLD_FAMILY, family, 0, args, ncases + 2u);
}

static const Value *fold_path(Machine *m, uint32_t family, const Value *const *cases, uint32_t ncases, const Value *x, const Value *p);
static const Value *fold_hcomp(Machine *m, uint32_t family, const Value *const *cases, uint32_t ncases, const Value *x);
static const Value *fold_path_field(Machine *m, uint32_t family, const Value *const *cases, uint32_t ncases, const Value *x, uint32_t field);
static const Value *fold_square(Machine *m, uint32_t family, const Value *const *cases, uint32_t ncases, const Value *x, const Value *s);
static const Value *kan_closure4(Machine *m, ValKind kind, ClosTag tag, uint32_t field, uint32_t argc, const Value *x, const Value *y, const Value *z, const Value *w);

/* fold case... x over a domain family: the case of the constructor of X,
   applied to its fields. A recursive field and a path field (S4 item 14)
   arrive folded. CASES holds
   NCASES cases, then the result type RES. A formal hcomp goes to
   fold_hcomp. */
static const Value *fold_family(Machine *m, uint32_t family, const Value *const *cases, uint32_t ncases, const Value *x) {
  const FamilyInfo *fam;
  const CtorInfo *ctor;
  const Value *acc;
  const Value *field;
  uint32_t i;
  if (x == NULL)
    return NULL;
  switch (scrut_of(x)) {
  case SCRUT_TRAP:
    return x;
  case SCRUT_NEUTRAL:
    return stuck_fold(m, family, cases, ncases, x);
  case SCRUT_VALUE:
    break;
  }
  if (val_is(x, OP_HCOMP) && x->argc == 4u)
    return fold_hcomp(m, family, cases, ncases, x);
  if (!val_is(x, OP_CTOR) || x->inst >= m->ctor_count || family >= m->family_count)
    return internal(m, "a fold of a value that is not in the family");
  fam = &m->families[family];
  ctor = &m->ctors[x->inst];
  if (x->inst < fam->first_ctor || x->inst - fam->first_ctor >= ncases || x->argc != fam->param_count + ctor->field_count + ctor->interval_count)
    return internal(m, "a fold of a constructor of another family");
  if (ctor->interval_count > 2u)
    return internal(m, "a fold of a constructor with more than 2 interval fields");
  if (!enter(m))
    return NULL;
  acc = cases[x->inst - fam->first_ctor];
  for (i = 0; i < ctor->field_count && acc != NULL; i++) {
    field = x->args[fam->param_count + i];
    acc = apply_value(m, acc, ctor->fields[i].recursive ? fold_family(m, family, cases, ncases, field) : ctor->fields[i].path ? fold_path_field(m, family, cases, ncases, x, i) : field);
  }
  if (ctor->interval_count == 1u && acc != NULL)
    acc = fold_path(m, family, cases, ncases, x, acc);
  if (ctor->interval_count == 2u && acc != NULL)
    acc = fold_square(m, family, cases, ncases, x, acc);
  m->depth--;
  return acc;
}

/* (case args') @ r for the path constructor value X at the interval R (S4
   R4 (a)). P is the case at the folded fields. The ends are the folds of
   X at i0 and i1, for a neutral P. */
static const Value *fold_path(Machine *m, uint32_t family, const Value *const *cases, uint32_t ncases, const Value *x, const Value *p) {
  const Value *ends[2];
  const Value **a;
  uint32_t i;
  int side;
  for (side = 0; side < 2; side++) {
    a = arena_alloc(m->arena, (size_t)x->argc * sizeof *a);
    if (a == NULL)
      return oom(m);
    for (i = 0; i + 1u < x->argc; i++)
      a[i] = x->args[i];
    a[x->argc - 1u] = val_iend(m, side);
    ends[side] = fold_family(m, family, cases, ncases, val_ctor(m, x->inst, a, x->argc));
  }
  return path_apply(m, p, x->args[x->argc - 1u], ends[0], ends[1]);
}

/* The constructor value X with its 2 interval fields set to R1 and R2. At
   an end it reduces to its boundary. */
static const Value *ctor_at2(Machine *m, const Value *x, const Value *r1, const Value *r2) {
  const Value **a = arena_alloc(m->arena, (size_t)x->argc * sizeof *a);
  uint32_t i;
  if (a == NULL || r1 == NULL || r2 == NULL)
    return a == NULL ? oom(m) : NULL;
  for (i = 0; i + 2u < x->argc; i++)
    a[i] = x->args[i];
  a[x->argc - 2u] = r1;
  a[x->argc - 1u] = r2;
  return val_ctor(m, x->inst, a, x->argc);
}

/* (case args') @ r1 @ r2 for the constructor value X with 2 interval
   fields (S4 item 15). S is the case at the folded fields, a square. For
   a neutral S, the outer ends are the folds of X at i0 and i1 in the
   first interval field, as paths in the second (CLOS_FOLD_FACE); the
   inner ends are the folds of X at r1 and i0, i1. */
static const Value *fold_square(Machine *m, uint32_t family, const Value *const *cases, uint32_t ncases, const Value *x, const Value *s) {
  const Value *stuck = stuck_fold(m, family, cases, ncases, x);
  const Value *r1 = x->args[x->argc - 2u];
  const Value *outer[2];
  const Value *inner[2];
  int side;
  for (side = 0; side < 2; side++) {
    outer[side] = kan_closure4(m, VAL_PLAM, CLOS_FOLD_FACE, family, 3u, x, val_iend(m, side), stuck, NULL);
    inner[side] = fold_family(m, family, cases, ncases, ctor_at2(m, x, r1, val_iend(m, side)));
    if (outer[side] == NULL || inner[side] == NULL)
      return NULL;
  }
  return path_apply(m, path_apply(m, s, r1, outer[0], outer[1]), x->args[x->argc - 1u], inner[0], inner[1]);
}

/* 1 when the step result R ends the list. *TAIL is then the end. */
static int unfold_end(Machine *m, const Value *g, uint64_t left, const Value *seed, const Value *r, const Value **tail) {
  const Value *p = val_is(r, OP_SOME) ? r->args[0] : r;
  if (val_is(r, OP_NONE)) {
    *tail = constant(m, OP_NIL);
    return 1;
  }
  switch (scrut_of(p)) {
  case SCRUT_TRAP:
    *tail = p;
    return 1;
  case SCRUT_NEUTRAL:
    *tail = make(m, VAL_STUCK, OP_UNFOLD, 0, 3, g, val_nat(m, left), seed);
    return 1;
  case SCRUT_VALUE:
    break;
  }
  if (val_is(p, OP_PAIR))
    return 0;
  *tail = internal(m, "an unfold step that is not an Option of a pair");
  return 1;
}

/* unfold g limit seed: at most LIMIT elements. */
static const Value *reduce_unfold(Machine *m, const Value *const *a, uint32_t n) {
  const Value *seed = a[2];
  const Value *out;
  const Value *r;
  const Value *tail = NULL;
  ValBuf items;
  uint64_t i;
  if (blocked(m, a[1], OP_UNFOLD, 0, 0, a, n, &out))
    return out;
  if (a[1]->kind != VAL_NAT)
    return internal(m, "an unfold limit that is not a Nat");
  buf_init(&items);
  for (i = 0; i < a[1]->nat; i++) {
    r = spend(m, 1) ? apply_value(m, a[0], seed) : NULL;
    if (r == NULL)
      return NULL;
    if (unfold_end(m, a[0], a[1]->nat - i, seed, r, &tail))
      return build_list(m, items.items, items.count, tail);
    if (!buf_push(m, &items, r->args[0]->args[0]))
      return NULL;
    seed = r->args[0]->args[1];
  }
  return build_list(m, items.items, items.count, constant(m, OP_NIL));
}

static const Value *filter_option(Machine *m, const Value *const *a, uint32_t n) {
  const Value *test;
  const Value *out;
  if (!val_is(a[1], OP_SOME))
    return a[1];
  test = apply_value(m, a[0], a[1]->args[0]);
  if (test == NULL)
    return NULL;
  if (blocked(m, test, OP_FILTER, CARRIER_OPTION, 0, a, n, &out))
    return out;
  return val_is(test, OP_FLAG_YES) ? a[1] : constant(m, OP_NONE);
}

static const Value *filter_list(Machine *m, const Value *const *a) {
  const Value *test;
  Spine s;
  ValBuf kept;
  uint32_t i;
  if (!spine_of(m, a[1], &s))
    return NULL;
  buf_init(&kept);
  for (i = 0; i < s.count; i++) {
    test = apply_value(m, a[0], s.cells[i]->args[0]);
    if (test == NULL)
      return NULL;
    switch (scrut_of(test)) {
    case SCRUT_TRAP:
      return build_list(m, kept.items, kept.count, test);
    case SCRUT_NEUTRAL:
      return build_list(m, kept.items, kept.count, make(m, VAL_STUCK, OP_FILTER, CARRIER_LIST, 2, a[0], s.cells[i], NULL));
    case SCRUT_VALUE:
      break;
    }
    if (val_is(test, OP_FLAG_YES) && !buf_push(m, &kept, s.cells[i]->args[0]))
      return NULL;
  }
  return build_list(m, kept.items, kept.count, list_tail(m, s.tail, OP_FILTER, CARRIER_LIST, a[0]));
}

static const Value *reduce_filter(Machine *m, uint32_t inst, const Value *const *a, uint32_t n) {
  const Value *out;
  if (blocked(m, a[1], OP_FILTER, inst, 0, a, n, &out))
    return out;
  switch ((Carrier)inst) {
  case CARRIER_OPTION:
    return filter_option(m, a, n);
  case CARRIER_LIST:
    return filter_list(m, a);
  case CARRIER_SUM:
    break;
  }
  return internal(m, "a filter with no instance");
}

/* 1 when both arguments are numbers. Else *OUT is the trap or the stuck
   operation. */
static int nat_args(Machine *m, Op op, const Value *const *a, uint32_t n, uint64_t *x, uint64_t *y, const Value **out) {
  *out = a[0]->kind == VAL_TRAP ? a[0] : a[1];
  if (a[0]->kind == VAL_TRAP || a[1]->kind == VAL_TRAP)
    return 0;
  if (blocked(m, a[0], op, 0, 0, a, n, out) || blocked(m, a[1], op, 0, 0, a, n, out))
    return 0;
  if (a[0]->kind != VAL_NAT || a[1]->kind != VAL_NAT) {
    *out = internal(m, "a Nat operation on a value that is not a Nat");
    return 0;
  }
  *x = a[0]->nat;
  *y = a[1]->nat;
  return 1;
}

static const Value *nat_add(Machine *m, const Value *const *a, uint32_t n) {
  const Value *out;
  uint64_t x = 0;
  uint64_t y = 0;
  if (!nat_args(m, OP_NAT_ADD, a, n, &x, &y, &out))
    return out;
  return x + y < x ? overflow(m) : val_nat(m, x + y);
}

static const Value *nat_sub(Machine *m, const Value *const *a, uint32_t n) {
  const Value *out;
  uint64_t x = 0;
  uint64_t y = 0;
  if (!nat_args(m, OP_NAT_SUB, a, n, &x, &y, &out))
    return out;
  return val_nat(m, x < y ? 0 : x - y);
}

/* natDiv and natMod (S2, R14): the floor and the rest; a zero divisor traps
   like an overflow does. */
static const Value *nat_divmod(Machine *m, Op op, const Value *const *a, uint32_t n) {
  const Value *out;
  uint64_t x = 0;
  uint64_t y = 0;
  if (!nat_args(m, op, a, n, &x, &y, &out))
    return out;
  if (y == 0)
    return overflow(m);
  return val_nat(m, op == OP_NAT_DIV ? x / y : x % y);
}

/* 1 when each argument is a value of the kind WANT. Else *OUT is the trap or
   the stuck operation. */
static int word_args(Machine *m, Op op, ValKind want, const Value *const *a, uint32_t n, const Value **out) {
  uint32_t i;
  for (i = 0; i < n; i++) {
    if (a[i]->kind == VAL_TRAP) {
      *out = a[i];
      return 0;
    }
  }
  for (i = 0; i < n; i++) {
    if (blocked(m, a[i], op, 0, 0, a, n, out))
      return 0;
  }
  for (i = 0; i < n; i++) {
    if (a[i]->kind != want) {
      *out = internal(m, "a word operation on a value of another kind");
      return 0;
    }
  }
  return 1;
}

static const Value *word_value(Machine *m, const U256 *x) {
  U256 *w = arena_alloc(m->arena, sizeof *w);
  if (w == NULL)
    return oom(m);
  *w = *x;
  return val_word(m, WORD_U256, w);
}

/* u256Add traps on an overflow; u256Sub stops at 0. */
static const Value *u256_arith(Machine *m, Op op, const Value *const *a, uint32_t n, int (*fn)(U256 *, const U256 *, const U256 *)) {
  const Value *out;
  U256 r;
  if (!word_args(m, op, VAL_WORD, a, n, &out))
    return out;
  return fn(&r, a[0]->word, a[1]->word) ? word_value(m, &r) : overflow(m);
}

/* u256Le, u256Eq and addrEq. */
static const Value *word_test(Machine *m, Op op, const Value *const *a, uint32_t n) {
  const Value *out;
  int order;
  if (!word_args(m, op, VAL_WORD, a, n, &out))
    return out;
  order = u256_cmp(a[0]->word, a[1]->word);
  return flag(m, op == OP_U256_LE ? order <= 0 : order == 0);
}

static const Value *to_u256(Machine *m, const Value *const *a, uint32_t n) {
  const Value *out;
  U256 r;
  if (!word_args(m, OP_TO_U256, VAL_NAT, a, n, &out))
    return out;
  u256_from_u64(&r, a[0]->nat);
  return word_value(m, &r);
}

/* The 32 big-endian bytes of X at OUT (limb 3 is the most significant). */
static void word_bytes(const U256 *x, unsigned char *out) {
  for (uint32_t i = 0; i < 32u; i++)
    out[i] = (unsigned char)(x->limb[3u - i / 8u] >> (56u - 8u * (i % 8u)));
}

/* hash x y (S2, R16): KECCAK256 of the 64 packed bytes, as IR_OP_KECCAK. */
static const Value *hash_words(Machine *m, const Value *const *a, uint32_t n) {
  const Value *out;
  unsigned char data[64], digest[32];
  U256 r;
  if (!word_args(m, OP_HASH, VAL_WORD, a, n, &out))
    return out;
  word_bytes(a[0]->word, data);
  word_bytes(a[1]->word, data + 32);
  keccak256(data, sizeof data, digest);
  for (uint32_t k = 0; k < 4u; k++) {
    r.limb[3u - k] = 0;
    for (uint32_t j = 0; j < 8u; j++)
      r.limb[3u - k] = (r.limb[3u - k] << 8) | digest[8u * k + j];
  }
  return word_value(m, &r);
}

/* addrToU256 a (S2, R20): the same word in the U256 sort. */
static const Value *addr_to_u256(Machine *m, const Value *const *a, uint32_t n) {
  const Value *out;
  if (!word_args(m, OP_ADDR_TO_U256, VAL_WORD, a, n, &out))
    return out;
  return word_value(m, a[0]->word);
}

/* The curve ops (S2, R17): a trap argument gives the trap; else the
   application is stuck, closed arguments included. The chain test is the
   differential gate (O5). */
static const Value *opaque(Machine *m, Op op, const Value *const *a, uint32_t n) {
  for (uint32_t i = 0; i < n; i++) {
    if (a[i]->kind == VAL_TRAP)
      return a[i];
  }
  return val_op(m, VAL_STUCK, op, 0, 0, a, n);
}

static const Value *nat_test(Machine *m, Op op, const Value *const *a, uint32_t n) {
  const Value *out;
  uint64_t x = 0;
  uint64_t y = 0;
  if (!nat_args(m, op, a, n, &x, &y, &out))
    return out;
  return flag(m, op == OP_NAT_EQ ? x == y : x <= y);
}

static const Value *reduce_flag_if(Machine *m, const Value *const *a, uint32_t n) {
  const Value *out;
  if (blocked(m, a[0], OP_FLAG_IF, 0, 0, a, n, &out))
    return out;
  if (val_is(a[0], OP_FLAG_YES))
    return a[1];
  if (val_is(a[0], OP_FLAG_NO))
    return a[2];
  return internal(m, "a flagIf of a value that is not a Flag");
}

static const Value *reduce_proj(Machine *m, uint32_t inst, uint32_t field, const Value *const *a, uint32_t n) {
  const Value *x = a[0];
  const Value *out;
  uint32_t at;
  if (blocked(m, x, OP_PROJ, inst, field, a, n, &out))
    return out;
  /* A formal hcomp has no fields: the projection is stuck (S4 R3 (a)). */
  if (val_is(x, OP_HCOMP))
    return val_op(m, VAL_STUCK, OP_PROJ, inst, field, a, n);
  if (!val_is(x, OP_CTOR) || x->inst != inst || inst >= m->ctor_count || m->ctors[inst].family >= m->family_count)
    return internal(m, "a projection of a value of the wrong family");
  at = m->families[m->ctors[inst].family].param_count + field;
  return at < x->argc ? x->args[at] : internal(m, "a projection of a missing field");
}

/* Slice K4a0. The zero value of the word type V: a key with no value gives it. */
static const Value *kmap_zero(Machine *m, const Value *v) {
  static const U256 zero = {{0, 0, 0, 0}};
  if (val_is(v, OP_FLAG))
    return flag(m, 0);
  if (val_is(v, OP_U256) || val_is(v, OP_ADDR))
    return val_word(m, val_is(v, OP_ADDR) ? WORD_ADDR : WORD_U256, &zero);
  return val_is(v, OP_NAT) ? val_nat(m, 0) : internal(m, "a map value type that is not a word type");
}

static int kmap_is_zero(const Value *v) {
  static const U256 zero = {{0, 0, 0, 0}};
  return (v->kind == VAL_NAT && v->nat == 0u) || (v->kind == VAL_WORD && u256_cmp(v->word, &zero) == 0) || val_is(v, OP_FLAG_NO);
}

/* -1, 0 or 1. A key is a Nat, or a word of the key sort. */
static int kmap_cmp(const Value *a, const Value *b) {
  if (a->kind == VAL_WORD && b->kind == VAL_WORD)
    return u256_cmp(a->word, b->word);
  return a->nat < b->nat ? -1 : a->nat > b->nat ? 1 : 0;
}

/* mapGet V m k: the value at K, or the zero value of V. */
static const Value *reduce_kmap_get(Machine *m, const Value *const *a, uint32_t n) {
  const Value *out;
  uint32_t i;
  if (blocked(m, a[1], OP_KMAP_GET, 0, 0, a, n, &out) || blocked(m, a[2], OP_KMAP_GET, 0, 0, a, n, &out))
    return out;
  if (!val_is(a[1], OP_KMAP_OF))
    return internal(m, "a mapGet of a value that is not a map");
  for (i = 0; i + 1u < a[1]->argc; i += 2u) {
    if (kmap_cmp(a[1]->args[i], a[2]) == 0)
      return a[1]->args[i + 1u];
  }
  return kmap_zero(m, a[0]);
}

/* mapSet V m k v: the map with V at K. The keys stay in order and a zero
   value removes the key, so the form is canonical and conversion is
   structural. */
static const Value *reduce_kmap_set(Machine *m, const Value *const *a, uint32_t n) {
  const Value *map = a[1];
  const Value **items;
  const Value *out;
  uint32_t count = 0;
  uint32_t i;
  int placed;
  if (blocked(m, map, OP_KMAP_SET, 0, 0, a, n, &out) || blocked(m, a[2], OP_KMAP_SET, 0, 0, a, n, &out) || blocked(m, a[3], OP_KMAP_SET, 0, 0, a, n, &out))
    return out;
  if (!val_is(map, OP_KMAP_OF))
    return internal(m, "a mapSet of a value that is not a map");
  items = arena_alloc(m->arena, ((size_t)map->argc + 2u) * sizeof *items);
  if (items == NULL)
    return oom(m);
  placed = kmap_is_zero(a[3]);
  for (i = 0; i + 1u < map->argc; i += 2u) {
    int order = kmap_cmp(map->args[i], a[2]);
    if (order > 0 && !placed) {
      items[count++] = a[2];
      items[count++] = a[3];
      placed = 1;
    }
    if (order != 0) {
      items[count++] = map->args[i];
      items[count++] = map->args[i + 1u];
    }
  }
  if (!placed) {
    items[count++] = a[2];
    items[count++] = a[3];
  }
  return val_op(m, VAL_OP, OP_KMAP_OF, 0, 0, items, count);
}

static uint32_t varied_arity(const Machine *m, Op op, uint32_t inst) {
  const CtorInfo *c = inst < m->ctor_count ? &m->ctors[inst] : NULL;
  const FamilyInfo *f = inst < m->family_count ? &m->families[inst] : NULL;
  if (op == OP_CTOR && c != NULL && c->family < m->family_count)
    return m->families[c->family].param_count + c->field_count + c->interval_count;
  if (op == OP_FAMILY && f != NULL)
    return f->param_count;
  if (op == OP_FOLD_FAMILY && f != NULL)
    return f->ctor_count + 2u;
  return UINT32_MAX;
}

static int arity_ok(const Machine *m, Op op, uint32_t inst, uint32_t n) {
  if ((size_t)op >= sizeof OP_ARITY)
    return 0;
  if (op == OP_KMAP_OF)
    return n % 2u == 0u;
  if (op == OP_SYSTEM)
    return n % 2u == 0u;
  if (OP_ARITY[op] != VARIES)
    return n == (uint32_t)OP_ARITY[op];
  return n == varied_arity(m, op, inst);
}

/* The Kan operations (S2, R11 (a)). A line L is a path value in U, and
   L(r) is L at the interval value r. A Kan rule gives a value at once; a
   rule that needs a function of an interval value gives a closure tag. */

#define KAN_FRESH_BASE 0x20000000u
#define KAN_FRESH_MASK 0x0fffffffu

static const Value *line_at(Machine *m, const Value *line, const Value *r) {
  return path_apply(m, line, r, NULL, NULL);
}

/* L at a fresh interval variable, never at i0: a neutral line can be a
   type former at i0. Eval has no level, so the Machine gives a level above
   each context level. A Kan rule reads only the head kind. */
static const Value *fresh_ivar(Machine *m) {
  uint32_t level = KAN_FRESH_BASE + (m->fresh & KAN_FRESH_MASK);
  m->fresh++;
  return val_ivar(m, level);
}

static const Value *line_head(Machine *m, const Value *line) {
  return line_at(m, line, fresh_ivar(m));
}

/* T when it is a KIND value (the former of a line at each point), else an
   internal error. */
static const Value *line_former(Machine *m, const Value *t, ValKind kind) {
  if (t == NULL || t->kind == kind)
    return t;
  return internal(m, "a line that changes its type former");
}

static const Value *kan_closure4(Machine *m, ValKind kind, ClosTag tag, uint32_t field, uint32_t argc, const Value *x, const Value *y, const Value *z, const Value *w) {
  const Value **args;
  Value *v;
  if (x == NULL || (argc > 1u && y == NULL) || (argc > 2u && z == NULL) || (argc > 3u && w == NULL))
    return NULL;
  args = arena_alloc(m->arena, 4u * sizeof *args);
  if (args == NULL)
    return oom(m);
  v = new_value(m, kind);
  if (v == NULL)
    return NULL;
  args[0] = x;
  args[1] = y;
  args[2] = z;
  args[3] = w;
  v->name = kind == VAL_PLAM ? "i" : "x";
  v->tag = tag;
  v->field = field;
  v->args = args;
  v->argc = argc;
  return v;
}

static const Value *kan_closure(Machine *m, ValKind kind, ClosTag tag, uint32_t field, uint32_t argc, const Value *x, const Value *y, const Value *z) {
  return kan_closure4(m, kind, tag, field, argc, x, y, z, NULL);
}

static const Value *transp(Machine *m, const Value *line, const Value *phi, const Value *base);
static const Value *hcomp(Machine *m, const Value *ty, const Value *phi, const Value *sys, const Value *base);
/* Structural rules that reach a universe retain the outer operation:
   composing in the universe needs Glue, and surface hcomp refuses it. */
static const Value *hcomp_stuck(Machine *m, const Value *ty, const Value *phi, const Value *sys, const Value *base);
static const Value *comp(Machine *m, const Value *line, const Value *phi, const Value *sys, const Value *base);
static const Value *hfill(Machine *m, const Value *ty, const Value *phi, const Value *sys, const Value *base, const Value *r);
static const Value *sys_map(Machine *m, const Value *sys, ClosTag tag, uint32_t field, uint32_t extra, const Value *x, const Value *y, const Value *z);
static const Value *hcomp_pathp_at(Machine *m, const Value *ty, const Value *phi, const Value *sys, const Value *base, const Value *j);
static const Value *transp_pathp_at(Machine *m, const Value *line, const Value *phi, const Value *base, const Value *r);

/* The part FIELD (0: witness, 1: payload) of the Sigma value U. */
static const Value *sigma_part(Machine *m, const Value *u, uint32_t field) {
  const Value *one[1];
  one[0] = u;
  return u == NULL ? NULL : reduce_part(m, field == 0u ? OP_WITNESS : OP_PAYLOAD, one, 1, OP_PACK, field);
}

static const Value *transp_stuck(Machine *m, const Value *line, const Value *phi, const Value *base) {
  return make(m, VAL_STUCK, OP_TRANSP, 0, 3, line, phi, base);
}

static const Value *not_typed(Machine *m) {
  return internal(m, "a transport of a value of the wrong type");
}

/* transp along the type argument FIELD of the data line L. */
static const Value *transp_part(Machine *m, const Value *line, uint32_t field, const Value *phi, const Value *x) {
  return transp(m, kan_closure(m, VAL_PLAM, CLOS_LINE_PART, field, 1, line, NULL, NULL), phi, x);
}

/* x'(r) = transp (fun j => A (r \/ ~j)) (phi \/ r) x: X in A(i1), moved
   back to A(r). */
static const Value *pi_back(Machine *m, const Value *line, const Value *phi, const Value *x, const Value *r) {
  return transp(m, kan_closure(m, VAL_PLAM, CLOS_PI_DOM, 0, 2, line, r, NULL), ival_join(m, phi, r), x);
}

/* a'(r) = transp (fun j => A (r /\ j)) (phi \/ ~r) u: U in A(i0), moved
   up to A(r). */
static const Value *sigma_up(Machine *m, const Value *line, const Value *phi, const Value *u, const Value *r) {
  return transp(m, kan_closure(m, VAL_PLAM, CLOS_SIGMA_DOM, 0, 2, line, r, NULL), ival_join(m, phi, ival_neg(m, r)), u);
}

/* The type of the point field FIELD of the constructor INST at the
   parameters of the family value T and the values EARLIER of the fields
   before it (S4 items 11 and 13). The field type is over the parameters
   and the earlier fields: the field before it is index 0. */
static const Value *field_type_at(Machine *m, uint32_t inst, uint32_t field, const Value *t, const Value *const *earlier) {
  const Env *env = NULL;
  uint32_t i;
  if (t == NULL)
    return NULL;
  if (!val_is(t, OP_FAMILY) || inst >= m->ctor_count || t->inst != m->ctors[inst].family || field >= m->ctors[inst].field_count)
    return internal(m, "a line that changes its type former");
  for (i = 0; i < t->argc + field; i++) {
    env = env_push(m, env, i < t->argc ? t->args[i] : earlier[i - t->argc]);
    if (env == NULL)
      return NULL;
  }
  return eval_core(m, env, m->ctors[inst].fields[field].type);
}

/* The line of the point field FIELD of the constructor value V under
   transp L phi: L itself for a recursive field. */
static const Value *field_line(Machine *m, const Value *line, const Value *phi, const Value *v, uint32_t field) {
  return m->ctors[v->inst].fields[field].recursive ? line : kan_closure(m, VAL_PLAM, CLOS_FAMILY_FIELD, field, 3, line, v, phi);
}

/* The type at R of the point field FIELD of the constructor value V under
   transp L phi (S4 item 13): the field type at the parameters of L(r) and
   at the fills at R of the earlier fields. The fill of field k is
   transp (fun j => line_k (r /\ j)) (phi \/ ~r) (field k of V), as
   transp_sigma moves the first part. A field type that uses no earlier
   field gets the fields of V, which it does not read. */
static const Value *family_field_at(Machine *m, const Value *line, const Value *phi, const Value *v, uint32_t field, const Value *r) {
  const Value *t = line_former(m, line_at(m, line, r), VAL_OP);
  const Value **fills;
  uint32_t p;
  uint32_t k;
  if (t == NULL)
    return NULL;
  if (!val_is(t, OP_FAMILY) || v->inst >= m->ctor_count || field >= m->ctors[v->inst].field_count || v->argc < t->argc + field)
    return internal(m, "a line that changes its type former");
  p = t->argc;
  if (!m->ctors[v->inst].fields[field].dependent)
    return field_type_at(m, v->inst, field, t, v->args + p);
  fills = arena_alloc(m->arena, ((size_t)field + 1u) * sizeof *fills);
  if (fills == NULL)
    return oom(m);
  for (k = 0; k < field; k++) {
    fills[k] = transp(m, kan_closure(m, VAL_PLAM, CLOS_LINE_MEET, 0, 2, field_line(m, line, phi, v, k), r, NULL), ival_join(m, phi, ival_neg(m, r)), v->args[p + k]);
    if (fills[k] == NULL)
      return NULL;
  }
  return field_type_at(m, v->inst, field, t, fills);
}

static const Value *kan_apply(Machine *m, const Value *k, const Value *r) {
  const Value *const *a = k->args;
  const Value *t;
  switch (k->tag) {
  case CLOS_LINE_PART:
    t = line_former(m, line_at(m, a[0], r), VAL_OP);
    if (t != NULL && k->field >= t->argc)
      return internal(m, "a line that changes its type former");
    return t == NULL ? NULL : t->args[k->field];
  case CLOS_PI_DOM:
    t = line_former(m, line_at(m, a[0], ival_join(m, a[1], ival_neg(m, r))), VAL_PI);
    return t == NULL ? NULL : t->dom;
  case CLOS_SIGMA_DOM:
    t = line_former(m, line_at(m, a[0], ival_meet(m, a[1], r)), VAL_SIGMA);
    return t == NULL ? NULL : t->dom;
  case CLOS_PI_COD:
    return closure_apply(m, line_former(m, line_at(m, a[0], r), VAL_PI), pi_back(m, a[0], a[1], a[2], r));
  case CLOS_SIGMA_COD:
    return closure_apply(m, line_former(m, line_at(m, a[0], r), VAL_SIGMA), sigma_up(m, a[0], a[1], a[2], r));
  case CLOS_TRANSP_PI:
    return transp(m, kan_closure(m, VAL_PLAM, CLOS_PI_COD, 0, 3, a[0], a[1], r), a[1], apply_value(m, a[2], pi_back(m, a[0], a[1], r, val_iend(m, 0))));
  case CLOS_CONST:
    return a[0];
  case CLOS_SYS_APP:
    return apply_value(m, closure_apply(m, a[0], r), a[1]);
  case CLOS_SYS_PAPP:
    return path_apply(m, closure_apply(m, a[0], r), a[1], a[2], a[3]);
  case CLOS_SYS_SIGMA:
    return sigma_part(m, closure_apply(m, a[0], r), k->field);
  case CLOS_SYS_ARG:
    t = closure_apply(m, a[0], r);
    if (t != NULL && (t->kind != VAL_OP || k->field >= t->argc))
      return internal(m, "a system branch that changes its constructor");
    return t == NULL ? NULL : t->args[k->field];
  case CLOS_SYS_MEET:
    return closure_apply(m, a[0], ival_meet(m, a[1], r));
  case CLOS_COMP_BRANCH:
    return transp(m, kan_closure(m, VAL_PLAM, CLOS_LINE_JOIN, 0, 2, a[1], r, NULL), r, closure_apply(m, a[0], r));
  case CLOS_LINE_JOIN:
    return line_at(m, a[0], ival_join(m, a[1], r));
  case CLOS_HCOMP_PI:
    t = closure_apply(m, a[0], r);
    if (t == NULL)
      return NULL;
    if (t->kind == VAL_UNIV)
      return apply_value(m, hcomp_stuck(m, a[0], a[1], a[2], a[3]), r);
    return hcomp(m, t, a[1], sys_map(m, a[2], CLOS_SYS_APP, 0, 1, r, NULL, NULL), apply_value(m, a[3], r));
  case CLOS_HCOMP_PATHP:
    return hcomp_pathp_at(m, a[0], a[1], a[2], a[3], r);
  case CLOS_SIGMA_FILL_COD:
    return closure_apply(m, a[0], hfill(m, a[0]->dom, a[1], a[2], a[3], r));
  case CLOS_TRANSP_PATHP:
    return transp_pathp_at(m, a[0], a[1], a[2], r);
  case CLOS_PATHP_LINE:
    return closure_apply(m, line_former(m, line_at(m, a[0], r), VAL_PATHP), a[1]);
  case CLOS_PATHP_END:
    t = line_former(m, line_at(m, a[0], r), VAL_PATHP);
    return t == NULL ? NULL : t->args[k->field];
  case CLOS_SYS_FOLD:
    if (a[1]->argc < 2u)
      return internal(m, "a fold branch without its cases");
    return fold_family(m, k->field, a[1]->args, a[1]->argc - 2u, closure_apply(m, a[0], r));
  case CLOS_FAMILY_FIELD:
    return family_field_at(m, a[0], a[2], a[1], k->field, r);
  case CLOS_SYS_TRANSP:
    return transp(m, a[1], a[2], closure_apply(m, a[0], r));
  case CLOS_LINE_MEET:
    return line_at(m, a[0], ival_meet(m, a[1], r));
  case CLOS_FOLD_PATH:
    if (a[3]->argc < 2u)
      return internal(m, "a folded path field without its cases");
    return fold_family(m, k->field, a[3]->args, a[3]->argc - 2u, path_apply(m, a[0], r, a[1], a[2]));
  case CLOS_FOLD_FACE:
    if (a[2]->argc < 2u)
      return internal(m, "a folded face without its cases");
    return fold_family(m, k->field, a[2]->args, a[2]->argc - 2u, ctor_at2(m, a[0], a[1], r));
  case CLOS_CORE:
    break;
  }
  return internal(m, "a closure with an unknown tag");
}

static const Value *transp_list(Machine *m, const Value *line, const Value *phi, const Value *base) {
  Spine sp;
  const Value **items;
  const Value *tail;
  uint32_t i;
  if (!spine_of(m, base, &sp))
    return NULL;
  items = arena_alloc(m->arena, ((size_t)sp.count + 1u) * sizeof *items);
  if (items == NULL)
    return oom(m);
  for (i = 0; i < sp.count; i++) {
    items[i] = transp_part(m, line, 0, phi, sp.cells[i]->args[0]);
    if (items[i] == NULL)
      return NULL;
  }
  tail = scrut_of(sp.tail) == SCRUT_NEUTRAL ? transp_stuck(m, line, phi, sp.tail) : sp.tail;
  return build_list(m, items, sp.count, tail);
}

/* A boundary face may use the constructor's interval fields, but not a
   family parameter that can change along transport. */
static int ctor_interval_face(const Core *t, uint32_t dimensions) {
  if (t->kind == CORE_VAR)
    return t->index < dimensions;
  if (t->kind == CORE_IZERO || t->kind == CORE_IONE)
    return 1;
  if (t->kind == CORE_INEG)
    return ctor_interval_face(t->a, dimensions);
  if (t->kind == CORE_IMEET || t->kind == CORE_IJOIN)
    return ctor_interval_face(t->a, dimensions) && ctor_interval_face(t->b, dimensions);
  return 0;
}

/* Rebuilding a path constructor commutes with its boundary when each
   branch is a recursive field: both sides transport that field along L.
   General boundary terms need a boundary correction. Until that rule is
   implemented, retain the transport rather than change its endpoints. */
static int field_transport_boundary(const CtorInfo *ctor) {
  const Core *sys = ctor->boundary;
  uint32_t k;
  if (sys == NULL)
    return 1;
  if (sys->kind != CORE_OP || sys->op != OP_SYSTEM)
    return 0;
  for (k = 0; k + 1u < sys->argc; k += 2u) {
    const Core *branch = sys->args[k + 1u];
    uint32_t index;
    if (!ctor_interval_face(sys->args[k], ctor->interval_count)
        || branch->kind != CORE_PLAM || branch->b->kind != CORE_VAR
        || branch->b->index <= ctor->interval_count)
      return 0;
    /* A branch is under one extra, unused path binder. */
    index = branch->b->index - ctor->interval_count - 1u;
    if (index >= ctor->field_count || !ctor->fields[ctor->field_count - 1u - index].recursive)
      return 0;
  }
  return 1;
}

/* S4 item 11 (R7 (a)): L(i) = F (p i), a family with parameters. A
   constructor goes into its point fields, each along its field type at the
   parameters of L (a recursive field along L itself); the interval fields
   do not change, and the parameters are those of L(i1). A formal hcomp
   goes into its tubes and its base (CCHM):
   transp L phi (hcomp F(p i0) psi u u0)
     = hcomp F(p i1) psi (fun j => [ k -> transp L phi (u j) ]) (transp L phi u0). */
static const Value *transp_family(Machine *m, const Value *line, const Value *phi, const Value *base) {
  const Value *end = line_former(m, line_at(m, line, val_iend(m, 1)), VAL_OP);
  const CtorInfo *ctor;
  const Value **args;
  const Value *sys;
  uint32_t q;
  uint32_t i;
  if (end == NULL)
    return NULL;
  if (val_is(base, OP_HCOMP) && base->argc == 4u) {
    sys = sys_map(m, base->args[2], CLOS_SYS_TRANSP, 0, 2, line, phi, NULL);
    return sys == NULL ? NULL : hcomp(m, end, base->args[1], sys, transp(m, line, phi, base->args[3]));
  }
  if (!val_is(base, OP_CTOR) || base->inst >= m->ctor_count || !val_is(end, OP_FAMILY))
    return not_typed(m);
  ctor = &m->ctors[base->inst];
  q = end->argc + ctor->field_count;
  if (end->inst != ctor->family || base->argc != q + ctor->interval_count)
    return not_typed(m);
  if (!field_transport_boundary(ctor))
    return transp_stuck(m, line, phi, base);
  args = arena_alloc(m->arena, ((size_t)base->argc + 1u) * sizeof *args);
  if (args == NULL)
    return oom(m);
  for (i = 0; i < end->argc; i++)
    args[i] = end->args[i];
  for (i = end->argc; i < q; i++) {
    args[i] = transp(m, field_line(m, line, phi, base, i - end->argc), phi, base->args[i]);
    if (args[i] == NULL)
      return NULL;
  }
  for (i = q; i < base->argc; i++)
    args[i] = base->args[i];
  return val_ctor(m, base->inst, args, base->argc);
}

/* Rules 3, 4 and 8 (R4 (a)): L(i) is the data type FORMER at a fresh i. A
   closed type gives the base; a constructor base goes into its arguments;
   a neutral base and a domain family are stuck. */
static const Value *transp_data(Machine *m, const Value *head, const Value *line, const Value *phi, const Value *base) {
  int neutral = scrut_of(base) == SCRUT_NEUTRAL;
  switch (head->op) {
  case OP_NAT: /* R27: a base former has no path structure */
  case OP_FLAG:
  case OP_UNIT:
  case OP_U256:
  case OP_ADDR:
  case OP_KMAP:
    return base;
  case OP_PROD:
    if (neutral)
      break;
    if (!val_is(base, OP_PAIR))
      return not_typed(m);
    return make(m, VAL_OP, OP_PAIR, 0, 2, transp_part(m, line, 0, phi, base->args[0]), transp_part(m, line, 1, phi, base->args[1]), NULL);
  case OP_SUM:
    if (neutral)
      break;
    if (!val_is(base, OP_INL) && !val_is(base, OP_INR))
      return not_typed(m);
    return make(m, VAL_OP, base->op, 0, 1, transp_part(m, line, val_is(base, OP_INL) ? 0u : 1u, phi, base->args[0]), NULL, NULL);
  case OP_OPTION:
    if (neutral)
      break;
    if (val_is(base, OP_NONE))
      return base;
    if (!val_is(base, OP_SOME))
      return not_typed(m);
    return make(m, VAL_OP, OP_SOME, 0, 1, transp_part(m, line, 0, phi, base->args[0]), NULL, NULL);
  case OP_LIST:
    if (neutral)
      break;
    return transp_list(m, line, phi, base);
  case OP_FAMILY:
    /* S4 item 6 (R7 (a)): a family with no parameters is the same type at
       each point of the line by its form (not by regularity), so the base
       stays, also a neutral base or a formal hcomp. */
    if (head->inst < m->family_count && m->families[head->inst].param_count == 0u)
      return base;
    if (neutral)
      break;
    return transp_family(m, line, phi, base);
  case OP_INTERVAL:
  case OP_UNIT_VAL:
  case OP_FLAG_YES:
  case OP_FLAG_NO:
  case OP_PAIR:
  case OP_INL:
  case OP_INR:
  case OP_NONE:
  case OP_SOME:
  case OP_NIL:
  case OP_CONS:
  case OP_PACK:
  case OP_CTOR:
  case OP_FIRST:
  case OP_SECOND:
  case OP_EITHER:
  case OP_OPTION_ELIM:
  case OP_PURE:
  case OP_MAP:
  case OP_BIND:
  case OP_FOLD_NAT:
  case OP_FOLD_LIST:
  case OP_FOLD_FAMILY:
  case OP_UNFOLD:
  case OP_FILTER:
  case OP_WITNESS:
  case OP_PAYLOAD:
  case OP_NAT_ADD:
  case OP_NAT_SUB:
  case OP_NAT_EQ:
  case OP_NAT_LE:
  case OP_FLAG_IF:
  case OP_U256_ADD:
  case OP_U256_SUB:
  case OP_U256_LE:
  case OP_U256_EQ:
  case OP_TO_U256:
  case OP_ADDR_EQ:
  case OP_NAT_DIV:
  case OP_NAT_MOD:
  case OP_HASH:
  case OP_ECRECOVER:
  case OP_EC_ADD:
  case OP_EC_MUL:
  case OP_EC_PAIRING:
  case OP_ADDR_TO_U256:
  case OP_PROJ:
  case OP_KMAP_OF:
  case OP_KMAP_GET:
  case OP_KMAP_SET:
  case OP_TRANSP:
  case OP_SYSTEM:
  case OP_HCOMP:
    return internal(m, "a transport along a line that is not a type");
  }
  return transp_stuck(m, line, phi, base);
}

/* Rule 6: L(i) = Sigma (x : A i) (B i x). The base goes through its
   projections, so a neutral base moves too. */
static const Value *transp_sigma(Machine *m, const Value *line, const Value *phi, const Value *base) {
  const Value *one[1];
  const Value *u;
  const Value *v;
  one[0] = base;
  u = reduce_part(m, OP_WITNESS, one, 1, OP_PACK, 0);
  v = reduce_part(m, OP_PAYLOAD, one, 1, OP_PACK, 1);
  return make(m, VAL_OP, OP_PACK, 0, 2, sigma_up(m, line, phi, u, val_iend(m, 1)), transp(m, kan_closure(m, VAL_PLAM, CLOS_SIGMA_COD, 0, 3, line, phi, u), phi, v), NULL);
}

/* transp L phi a, the rules of the S2 kickoff section 1.2 in order. */
static const Value *transp(Machine *m, const Value *line, const Value *phi, const Value *base) {
  const Value *face;
  const Value *head;
  if (line == NULL || phi == NULL || base == NULL || !spend(m, 1))
    return NULL;
  face = face_norm(m, phi);
  if (face == NULL)
    return NULL;
  /* Rule 1: phi is true. */
  if (ival_is_end(face, 1) || base->kind == VAL_TRAP)
    return base;
  head = line_head(m, line);
  if (head == NULL)
    return NULL;
  switch (head->kind) {
  case VAL_UNIV: /* rule 2 */
    return base;
  case VAL_OP: /* rules 3, 4, 8 */
    return transp_data(m, head, line, face, base);
  case VAL_PI: /* rule 5 */
    return kan_closure(m, VAL_LAM, CLOS_TRANSP_PI, 0, 3, line, face, base);
  case VAL_SIGMA: /* rule 6 */
    return transp_sigma(m, line, face, base);
  case VAL_TRAP:
    return head;
  case VAL_NAT:
  case VAL_WORD:
  case VAL_LAM:
  case VAL_PLAM:
  case VAL_IVAL:
    return internal(m, "a transport along a line that is not a type");
  case VAL_PATHP: /* rule 7 */
    return kan_closure(m, VAL_PLAM, CLOS_TRANSP_PATHP, 0, 3, line, face, base);
  case VAL_VAR:
  case VAL_APP:
  case VAL_STUCK:
  case VAL_PAPP:
    break;
  }
  /* Rule 9: a neutral line. */
  return transp_stuck(m, line, face, base);
}

/* hcomp, comp and hfill (S2b, S2 kickoff section 1.3). A system is a
   VAL_OP OP_SYSTEM with the arguments key0, branch0, key1, branch1, ...;
   each branch is a PLAM over the tube variable. */

/* Normalize face keys and discard unreachable branches before applying
   constructor projections or comparing generated systems. */
static const Value *system_value(Machine *m, const Value *const *args, uint32_t argc) {
  const Value **live;
  const Value *key;
  uint32_t k;
  uint32_t n = 0;
  live = arena_alloc(m->arena, ((size_t)argc + 1u) * sizeof *live);
  if (live == NULL)
    return oom(m);
  for (k = 0; k + 1u < argc; k += 2u) {
    key = face_norm(m, args[k]);
    if (key == NULL)
      return NULL;
    if (!ival_is_end(key, 0)) {
      live[n++] = key;
      live[n++] = args[k + 1u];
    }
  }
  return val_op(m, VAL_OP, OP_SYSTEM, 0, 0, live, n);
}

/* SYS with each branch b replaced by the closure TAG of b and the EXTRA
   values X, Y, Z. The keys do not change. */
static const Value *sys_map(Machine *m, const Value *sys, ClosTag tag, uint32_t field, uint32_t extra, const Value *x, const Value *y, const Value *z) {
  const Value **args;
  uint32_t k;
  if (sys == NULL)
    return NULL;
  args = arena_alloc(m->arena, ((size_t)sys->argc + 1u) * sizeof *args);
  if (args == NULL)
    return oom(m);
  for (k = 0; k + 1u < sys->argc; k += 2u) {
    args[k] = sys->args[k];
    args[k + 1u] = kan_closure4(m, VAL_PLAM, tag, field, extra + 1u, sys->args[k + 1u], x, y, z);
  }
  return system_value(m, args, sys->argc);
}

/* SYS with one more branch KEY -> BRANCH. */
static const Value *sys_add(Machine *m, const Value *sys, const Value *key, const Value *branch) {
  const Value **args;
  uint32_t k;
  if (sys == NULL)
    return NULL;
  args = arena_alloc(m->arena, ((size_t)sys->argc + 2u) * sizeof *args);
  if (args == NULL)
    return oom(m);
  for (k = 0; k < sys->argc; k++)
    args[k] = sys->args[k];
  args[sys->argc] = key;
  args[sys->argc + 1u] = branch;
  return system_value(m, args, sys->argc + 2u);
}

static const Value *constant_line(Machine *m, const Value *v) {
  return kan_closure(m, VAL_PLAM, CLOS_CONST, 0, 1, v, NULL, NULL);
}

static const Value *hcomp_stuck(Machine *m, const Value *ty, const Value *phi, const Value *sys, const Value *base) {
  const Value *args[4];
  args[0] = ty;
  args[1] = phi;
  args[2] = sys;
  args[3] = base;
  return val_op(m, VAL_STUCK, OP_HCOMP, 0, 0, args, 4);
}

/* 1 when TY is a domain family with a path constructor (a HIT). */
static int hit_type(const Machine *m, const Value *ty) {
  const FamilyInfo *f;
  uint32_t k;
  if (!val_is(ty, OP_FAMILY) || ty->inst >= m->family_count)
    return 0;
  f = &m->families[ty->inst];
  for (k = 0; k < f->ctor_count; k++)
    if (f->first_ctor + k < m->ctor_count && m->ctors[f->first_ctor + k].interval_count > 0u)
      return 1;
  return 0;
}

/* hcomp at a HIT with no true key (S4 R3 (a)): a canonical value, never
   VAL_STUCK (conv_loop rejects VAL_OP against VAL_STUCK). There is no
   regularity, so this holds also at i0 and with a neutral base. */
static const Value *hcomp_formal(Machine *m, const Value *ty, const Value *phi, const Value *sys, const Value *base) {
  const Value *args[4];
  args[0] = ty;
  args[1] = phi;
  args[2] = sys;
  args[3] = base;
  return val_op(m, VAL_OP, OP_HCOMP, 0, 0, args, 4);
}

/* fun (k : I) => fold (p @ k) for the path field FIELD of the constructor
   value X (S4 item 14). The ends of p are the fields ENDS of X. The
   closure reads the cases from a stuck fold of X. */
static const Value *fold_path_field(Machine *m, uint32_t family, const Value *const *cases, uint32_t ncases, const Value *x, uint32_t field) {
  const FieldInfo *f = &m->ctors[x->inst].fields[field];
  uint32_t p = m->families[family].param_count;
  return kan_closure4(m, VAL_PLAM, CLOS_FOLD_PATH, family, 4u, x->args[p + field], x->args[p + f->ends[0]], x->args[p + f->ends[1]], stuck_fold(m, family, cases, ncases, x));
}

/* fold (hcomp F phi u u0) = hcomp RES phi (fun i => [ k -> fold (u i) ])
   (fold u0). CASES holds NCASES cases, then RES. The branches read the
   cases from a stuck fold of X (CLOS_SYS_FOLD). */
static const Value *fold_hcomp(Machine *m, uint32_t family, const Value *const *cases, uint32_t ncases, const Value *x) {
  const Value *pack = stuck_fold(m, family, cases, ncases, x);
  const Value *sys = pack == NULL ? NULL : sys_map(m, x->args[2], CLOS_SYS_FOLD, family, 1, pack, NULL, NULL);
  const Value *base = sys == NULL ? NULL : fold_family(m, family, cases, ncases, x->args[3]);
  return base == NULL ? NULL : hcomp(m, cases[ncases], x->args[1], sys, base);
}

/* The index of the branch of SYS under the first true (i1) key, or
   sys->argc when no key is true. hcomp and Glue use this search. */
static uint32_t true_branch(Machine *m, const Value *sys) {
  uint32_t k;
  for (k = 0; k + 1u < sys->argc; k += 2u)
    if (ival_is_end(face_norm(m, sys->args[k]), 1))
      return k + 1u;
  return sys->argc;
}

/* phi is true, so the join of the keys is true and one key is i1: the
   result is its branch at i1. */
static const Value *hcomp_true(Machine *m, const Value *ty, const Value *phi, const Value *sys, const Value *base) {
  uint32_t k = true_branch(m, sys);
  if (k < sys->argc)
    return closure_apply(m, sys->args[k], val_iend(m, 1));
  return hit_type(m, ty) ? hcomp_formal(m, ty, phi, sys, base) : hcomp_stuck(m, ty, phi, sys, base);
}

/* The type of the argument FIELD of the constructor value V in the data
   type TY, or NULL. */
static const Value *data_field_type(const Value *ty, const Value *v, uint32_t field) {
  if (val_is(v, OP_PAIR) && val_is(ty, OP_PROD) && field < 2u)
    return ty->args[field];
  if ((val_is(v, OP_INL) && val_is(ty, OP_SUM)) || (val_is(v, OP_SOME) && val_is(ty, OP_OPTION)) || (val_is(v, OP_CONS) && val_is(ty, OP_LIST) && field == 0u))
    return ty->args[0];
  if (val_is(v, OP_INR) && val_is(ty, OP_SUM))
    return ty->args[1];
  return val_is(v, OP_CONS) && val_is(ty, OP_LIST) ? ty : NULL;
}

/* 1 when the branch B at a fresh i has the head of V: the same literal or
   the same constructor (the K10 head test). */
static int same_head_at(Machine *m, const Value *b, const Value *v) {
  const Value *x = closure_apply(m, b, fresh_ivar(m));
  if (x == NULL)
    return 0;
  if (v->kind == VAL_NAT)
    return x->kind == VAL_NAT && x->nat == v->nat;
  if (v->kind == VAL_WORD) /* R27: a word literal is a base value */
    return x->kind == VAL_WORD && x->nat == v->nat && u256_cmp(x->word, v->word) == 0;
  return x->kind == VAL_OP && x->op == v->op && x->inst == v->inst && x->argc == v->argc;
}

/* A data type (R4 (a)): each live branch at a fresh i has the head of u0.
   A literal or a nullary constructor gives u0; a constructor with
   arguments goes into them. A neutral u0, a domain family and a head
   mismatch are stuck. */
static const Value *hcomp_data(Machine *m, const Value *ty, const Value *phi, const Value *sys, const Value *base) {
  const Value **args;
  const Value *field;
  uint32_t k;
  if (hit_type(m, ty))
    return hcomp_formal(m, ty, phi, sys, base);
  if (val_is(ty, OP_FAMILY) || scrut_of(base) == SCRUT_NEUTRAL || (base->kind != VAL_NAT && base->kind != VAL_WORD && base->kind != VAL_OP))
    return hcomp_stuck(m, ty, phi, sys, base);
  for (k = 0; k + 1u < sys->argc; k += 2u)
    if (!ival_is_end(face_norm(m, sys->args[k]), 0) && !same_head_at(m, sys->args[k + 1u], base))
      return hcomp_stuck(m, ty, phi, sys, base);
  if (base->kind != VAL_OP || base->argc == 0u) /* R27: a literal is the base */
    return base;
  args = arena_alloc(m->arena, ((size_t)base->argc + 1u) * sizeof *args);
  if (args == NULL)
    return oom(m);
  for (k = 0; k < base->argc; k++) {
    field = data_field_type(ty, base, k);
    if (field == NULL || field->kind == VAL_UNIV)
      return hcomp_stuck(m, ty, phi, sys, base);
    args[k] = hcomp(m, field, phi, sys_map(m, sys, CLOS_SYS_ARG, k, 0, NULL, NULL, NULL), base->args[k]);
    if (args[k] == NULL)
      return NULL;
  }
  return val_op(m, VAL_OP, base->op, base->inst, base->field, args, base->argc);
}

/* Sigma: the first part is an hcomp in A; the second part is a comp along
   i |-> B (the hfill of the first part at i). */
static const Value *hcomp_sigma(Machine *m, const Value *ty, const Value *phi, const Value *sys, const Value *base) {
  const Value *w;
  const Value *cod;
  const Value *u = sigma_part(m, base, 0);
  const Value *v = sigma_part(m, base, 1);
  const Value *first = sys_map(m, sys, CLOS_SYS_SIGMA, 0, 0, NULL, NULL, NULL);
  const Value *second = sys_map(m, sys, CLOS_SYS_SIGMA, 1, 0, NULL, NULL, NULL);
  if (ty->dom->kind == VAL_UNIV)
    return hcomp_stuck(m, ty, phi, sys, base);
  w = hcomp(m, ty->dom, phi, first, u);
  cod = w == NULL ? NULL : closure_apply(m, ty, w);
  if (cod == NULL)
    return NULL;
  if (cod->kind == VAL_UNIV)
    return hcomp_stuck(m, ty, phi, sys, base);
  return make(m, VAL_OP, OP_PACK, 0, 2, w, comp(m, kan_closure4(m, VAL_PLAM, CLOS_SIGMA_FILL_COD, 0, 4, ty, phi, first, u), phi, second, v), NULL);
}

/* PathP at j: hcomp (A j) (phi \/ ~j \/ j) [sys @ j, ~j -> a, j -> b]
   (u0 @ j), with a and b the ends of the path type. */
static const Value *hcomp_pathp_at(Machine *m, const Value *ty, const Value *phi, const Value *sys, const Value *base, const Value *j) {
  const Value *a = ty->args[0];
  const Value *b = ty->args[1];
  const Value *at = closure_apply(m, ty, j);
  const Value *nj = ival_neg(m, j);
  const Value *s;
  if (at == NULL)
    return NULL;
  if (at->kind == VAL_UNIV)
    return path_apply(m, hcomp_stuck(m, ty, phi, sys, base), j, a, b);
  s = sys_add(m, sys_add(m, sys_map(m, sys, CLOS_SYS_PAPP, 0, 3, j, a, b), nj, constant_line(m, a)), j, constant_line(m, b));
  return hcomp(m, at, ival_join(m, phi, ival_join(m, nj, j)), s, path_apply(m, base, j, a, b));
}

/* Rule 7 of transp at r: comp (fun i => L(i) at r) (phi \/ ~r \/ r)
   [phi -> p0 @ r, ~r -> the i0 end of L(i), r -> the i1 end of L(i)]
   (p0 @ r). */
static const Value *transp_pathp_at(Machine *m, const Value *line, const Value *phi, const Value *base, const Value *r) {
  const Value *t = line_former(m, line_at(m, line, val_iend(m, 0)), VAL_PATHP);
  const Value *end = line_former(m, line_at(m, line, val_iend(m, 1)), VAL_PATHP);
  const Value *at = end == NULL ? NULL : closure_apply(m, end, r);
  const Value *nr = ival_neg(m, r);
  const Value *p = t == NULL ? NULL : path_apply(m, base, r, t->args[0], t->args[1]);
  const Value *empty = val_op(m, VAL_OP, OP_SYSTEM, 0, 0, NULL, 0);
  const Value *s;
  if (at == NULL)
    return NULL;
  if (at->kind == VAL_UNIV)
    return path_apply(m, transp_stuck(m, line, phi, base), r, end->args[0], end->args[1]);
  s = sys_add(m, sys_add(m, sys_add(m, empty, phi, constant_line(m, p)), nr, kan_closure(m, VAL_PLAM, CLOS_PATHP_END, 0, 1, line, NULL, NULL)), r, kan_closure(m, VAL_PLAM, CLOS_PATHP_END, 1, 1, line, NULL, NULL));
  return comp(m, kan_closure(m, VAL_PLAM, CLOS_PATHP_LINE, 0, 2, line, r, NULL), ival_join(m, phi, ival_join(m, nr, r)), s, p);
}

/* hfill A phi sys u0 r = hcomp A (phi \/ ~r) [j |-> sys (r /\ j),
   ~r -> u0] u0. */
static const Value *hfill(Machine *m, const Value *ty, const Value *phi, const Value *sys, const Value *base, const Value *r) {
  const Value *nr = ival_neg(m, r);
  return hcomp(m, ty, ival_join(m, phi, nr), sys_add(m, sys_map(m, sys, CLOS_SYS_MEET, 0, 1, r, NULL, NULL), nr, constant_line(m, base)), base);
}

/* comp L phi sys u0 = hcomp L(i1) phi [i |-> transp (fun j => L (i \/ j))
   i (sys i)] (transp L i0 u0). */
static const Value *comp(Machine *m, const Value *line, const Value *phi, const Value *sys, const Value *base) {
  return hcomp(m, line_at(m, line, val_iend(m, 1)), phi, sys_map(m, sys, CLOS_COMP_BRANCH, 0, 1, line, NULL, NULL), transp(m, line, val_iend(m, 0), base));
}

/* hcomp A phi sys u0, the rules of the S2 kickoff section 1.3 in order. */
static const Value *hcomp(Machine *m, const Value *ty, const Value *phi, const Value *sys, const Value *base) {
  const Value *face;
  if (ty == NULL || phi == NULL || sys == NULL || base == NULL || !spend(m, 1))
    return NULL;
  face = face_norm(m, phi);
  if (face == NULL)
    return NULL;
  if (base->kind == VAL_TRAP)
    return base;
  if (ival_is_end(face, 1))
    return hcomp_true(m, ty, face, sys, base);
  switch (ty->kind) {
  case VAL_PI:
    return kan_closure4(m, VAL_LAM, CLOS_HCOMP_PI, 0, 4, ty, face, sys, base);
  case VAL_SIGMA:
    return hcomp_sigma(m, ty, face, sys, base);
  case VAL_PATHP:
    return kan_closure4(m, VAL_PLAM, CLOS_HCOMP_PATHP, 0, 4, ty, face, sys, base);
  case VAL_OP:
    return hcomp_data(m, ty, face, sys, base);
  case VAL_TRAP:
    return ty;
  case VAL_NAT:
  case VAL_WORD:
  case VAL_LAM:
  case VAL_PLAM:
  case VAL_IVAL:
    return internal(m, "an hcomp in a value that is not a type");
  case VAL_UNIV:
  case VAL_VAR:
  case VAL_APP:
  case VAL_STUCK:
  case VAL_PAPP:
    break;
  }
  return hcomp_stuck(m, ty, face, sys, base);
}

/* A constructor value (S4 R2 (a)). The boundary of a path constructor is
   over the parameters, the point fields and the interval fields, in that
   order: at a true face the value is the branch, else it is the OP_CTOR
   value. `loop i0` is `base`. A point constructor has no boundary. */
static const Value *reduce_ctor(Machine *m, uint32_t inst, uint32_t field, const Value *const *a, uint32_t n) {
  const Env *env = NULL;
  const Value *sys;
  uint32_t i;
  uint32_t k;
  if (inst >= m->ctor_count || m->ctors[inst].boundary == NULL)
    return val_op(m, VAL_OP, OP_CTOR, inst, field, a, n);
  for (i = 0; i < n; i++) {
    env = env_push(m, env, a[i]);
    if (env == NULL)
      return NULL;
  }
  sys = eval_core(m, env, m->ctors[inst].boundary);
  if (sys == NULL)
    return NULL;
  k = true_branch(m, sys);
  return k < sys->argc ? closure_apply(m, sys->args[k], val_iend(m, 1)) : val_op(m, VAL_OP, OP_CTOR, inst, field, a, n);
}

const Value *val_ctor(Machine *m, uint32_t inst, const Value *const *a, uint32_t n) {
  return reduce_ctor(m, inst, 0, a, n);
}

static const Value *reduce_op(Machine *m, Op op, uint32_t inst, uint32_t field, const Value *const *a, uint32_t n) {
  if (!arity_ok(m, op, inst, n))
    return internal(m, "an operation with the wrong number of arguments");
  switch (op) {
  case OP_NAT:
  case OP_FLAG:
  case OP_U256:
  case OP_ADDR:
  case OP_UNIT:
  case OP_PROD:
  case OP_SUM:
  case OP_OPTION:
  case OP_LIST:
  case OP_INTERVAL:
  case OP_FAMILY:
  case OP_UNIT_VAL:
  case OP_FLAG_YES:
  case OP_FLAG_NO:
  case OP_PAIR:
  case OP_INL:
  case OP_INR:
  case OP_NONE:
  case OP_SOME:
  case OP_NIL:
  case OP_CONS:
  case OP_PACK:
  case OP_KMAP:
  case OP_KMAP_OF:
    return val_op(m, VAL_OP, op, inst, field, a, n);
  case OP_CTOR:
    return reduce_ctor(m, inst, field, a, n);
  case OP_SYSTEM:
    return system_value(m, a, n);
  case OP_KMAP_GET:
    return reduce_kmap_get(m, a, n);
  case OP_KMAP_SET:
    return reduce_kmap_set(m, a, n);
  case OP_FIRST:
    return reduce_part(m, op, a, n, OP_PAIR, 0);
  case OP_SECOND:
    return reduce_part(m, op, a, n, OP_PAIR, 1);
  case OP_WITNESS:
    return reduce_part(m, op, a, n, OP_PACK, 0);
  case OP_PAYLOAD:
    return reduce_part(m, op, a, n, OP_PACK, 1);
  case OP_EITHER:
    return reduce_either(m, a, n);
  case OP_OPTION_ELIM:
    return reduce_option(m, a, n);
  case OP_PURE:
    return reduce_pure(m, inst, a[0]);
  case OP_MAP:
    return reduce_map(m, inst, a, n);
  case OP_BIND:
    return reduce_bind(m, inst, a, n);
  case OP_FOLD_NAT:
    return reduce_fold_nat(m, a, n);
  case OP_FOLD_LIST:
    return reduce_fold_list(m, a);
  case OP_FOLD_FAMILY:
    return fold_family(m, inst, a, n - 2u, a[n - 1u]);
  case OP_UNFOLD:
    return reduce_unfold(m, a, n);
  case OP_FILTER:
    return reduce_filter(m, inst, a, n);
  case OP_NAT_ADD:
    return nat_add(m, a, n);
  case OP_NAT_SUB:
    return nat_sub(m, a, n);
  case OP_NAT_EQ:
  case OP_NAT_LE:
    return nat_test(m, op, a, n);
  case OP_U256_ADD:
    return u256_arith(m, op, a, n, u256_add);
  case OP_U256_SUB:
    return u256_arith(m, op, a, n, u256_sub);
  case OP_U256_LE:
  case OP_U256_EQ:
  case OP_ADDR_EQ:
    return word_test(m, op, a, n);
  case OP_TO_U256:
    return to_u256(m, a, n);
  case OP_NAT_DIV:
  case OP_NAT_MOD:
    return nat_divmod(m, op, a, n);
  case OP_HASH:
    return hash_words(m, a, n);
  case OP_ADDR_TO_U256:
    return addr_to_u256(m, a, n);
  case OP_ECRECOVER:
  case OP_EC_ADD:
  case OP_EC_MUL:
  case OP_EC_PAIRING:
    return opaque(m, op, a, n);
  case OP_FLAG_IF:
    return reduce_flag_if(m, a, n);
  case OP_PROJ:
    return reduce_proj(m, inst, field, a, n);
  case OP_TRANSP:
    return transp(m, a[0], a[1], a[2]);
  case OP_HCOMP:
    return hcomp(m, a[0], a[1], a[2], a[3]);
  }
  return internal(m, "an unknown operation");
}

static const Value *eval_op(Machine *m, const Env *env, const Core *c) {
  const Value **args = arena_alloc(m->arena, ((size_t)c->argc + 1u) * sizeof *args);
  uint32_t i;
  if (args == NULL)
    return oom(m);
  for (i = 0; i < c->argc; i++) {
    args[i] = eval_core(m, env, c->args[i]);
    if (args[i] == NULL)
      return NULL;
  }
  return reduce_op(m, c->op, c->inst, c->field, args, c->argc);
}

static const Value *eval_pathp(Machine *m, const Env *env, const Core *c) {
  const Value **ends = arena_alloc(m->arena, 2u * sizeof *ends);
  Value *v;
  if (ends == NULL)
    return oom(m);
  ends[0] = eval_core(m, env, c->args[0]);
  ends[1] = eval_core(m, env, c->args[1]);
  v = (Value *)val_closure(m, VAL_PLAM, c->name, NULL, env, c->b);
  if (ends[0] == NULL || ends[1] == NULL || v == NULL)
    return NULL;
  v->kind = VAL_PATHP;
  v->args = ends;
  v->argc = 2;
  return v;
}

static const Value *eval_inner(Machine *m, const Env *env, const Core *c) {
  switch (c->kind) {
  case CORE_VAR:
    return env_lookup(m, env, c->index);
  case CORE_GLOBAL:
    return def_value(m, c->index);
  case CORE_NAT:
    return val_nat(m, c->nat);
  case CORE_WORD:
    return val_word(m, c->nat, c->word);
  case CORE_TRAP:
    return val_trap(m);
  case CORE_UNIV:
    return val_univ(m, c->nat);
  case CORE_LAM:
    return val_closure(m, VAL_LAM, c->name, NULL, env, c->b);
  case CORE_PI:
    return val_closure(m, VAL_PI, c->name, eval_core(m, env, c->a), env, c->b);
  case CORE_SIGMA:
    return val_closure(m, VAL_SIGMA, c->name, eval_core(m, env, c->a), env, c->b);
  case CORE_APP:
    return apply_value(m, eval_core(m, env, c->a), eval_core(m, env, c->b));
  case CORE_OP:
    return eval_op(m, env, c);
  case CORE_PLAM:
    return val_closure(m, VAL_PLAM, c->name, NULL, env, c->b);
  case CORE_PATHP:
    return eval_pathp(m, env, c);
  case CORE_PAPP:
    return path_apply(m, eval_core(m, env, c->a), eval_core(m, env, c->b), eval_core(m, env, c->args[0]), eval_core(m, env, c->args[1]));
  case CORE_IZERO:
    return val_iend(m, 0);
  case CORE_IONE:
    return val_iend(m, 1);
  case CORE_INEG:
    return ival_neg(m, eval_core(m, env, c->a));
  case CORE_IMEET:
    return ival_meet(m, eval_core(m, env, c->a), eval_core(m, env, c->b));
  case CORE_IJOIN:
    return ival_join(m, eval_core(m, env, c->a), eval_core(m, env, c->b));
  }
  return internal(m, "an unknown core term");
}

const Value *eval_core(Machine *m, const Env *env, const Core *c) {
  const Value *v;
  if (c == NULL || !spend(m, 1) || !enter(m))
    return NULL;
  v = eval_inner(m, env, c);
  m->depth--;
  return v;
}

/* Eta: a function is equal to G when the two agree on a fresh variable. */
static int conv_fun(Machine *m, uint32_t level, const Value *a, const Value *b) {
  const Value *x = val_var(m, level);
  return conv_values(m, level + 1u, apply_value(m, a, x), apply_value(m, b, x));
}

static int conv_binder(Machine *m, uint32_t level, const Value *a, const Value *b) {
  const Value *x = val_var(m, level);
  return conv_values(m, level, a->dom, b->dom) && conv_values(m, level + 1u, closure_apply(m, a, x), closure_apply(m, b, x));
}

/* Path eta (R11): a path is equal to Q when the two agree at a fresh
   interval variable. A neutral Q there is a VAL_PAPP with no ends. */
static int conv_path(Machine *m, uint32_t level, const Value *a, const Value *b) {
  const Value *i = val_ivar(m, level);
  const Value *at_a = a->kind == VAL_PLAM ? closure_apply(m, a, i) : path_apply(m, a, i, NULL, NULL);
  const Value *at_b = b->kind == VAL_PLAM ? closure_apply(m, b, i) : path_apply(m, b, i, NULL, NULL);
  return conv_values(m, level + 1u, at_a, at_b);
}

/* Two path types: the lines at a fresh interval variable and both ends. */
static int conv_pathp(Machine *m, uint32_t level, const Value *a, const Value *b) {
  const Value *i = val_ivar(m, level);
  return conv_values(m, level, a->args[0], b->args[0]) && conv_values(m, level, a->args[1], b->args[1])
      && conv_values(m, level + 1u, closure_apply(m, a, i), closure_apply(m, b, i));
}

static int same_head(const Value *a, const Value *b) {
  return a->op == b->op && a->inst == b->inst && a->field == b->field && a->argc == b->argc;
}

/* Product and Sigma eta compare a pair with the projections of a neutral. */
static int conv_pair(Machine *m, uint32_t level, const Value *pair, const Value *neutral) {
  Op first = pair->op == OP_PAIR ? OP_FIRST : OP_WITNESS;
  Op second = pair->op == OP_PAIR ? OP_SECOND : OP_PAYLOAD;
  return conv_values(m, level, pair->args[0], make(m, VAL_STUCK, first, 0, 1, neutral, NULL, NULL))
      && conv_values(m, level, pair->args[1], make(m, VAL_STUCK, second, 0, 1, neutral, NULL, NULL));
}

/* The last argument of an operation is compared in the loop, so a long
   list does not nest. */
static int conv_loop(Machine *m, uint32_t level, const Value *a, const Value *b) {
  uint32_t i;
  while (a != NULL && b != NULL && spend(m, 1)) {
    if (a->kind == VAL_LAM || b->kind == VAL_LAM)
      return conv_fun(m, level, a, b);
    if (a->kind == VAL_PLAM || b->kind == VAL_PLAM)
      return conv_path(m, level, a, b);
    if ((val_is(a, OP_PAIR) || val_is(a, OP_PACK)) && scrut_of(b) == SCRUT_NEUTRAL)
      return conv_pair(m, level, a, b);
    if ((val_is(b, OP_PAIR) || val_is(b, OP_PACK)) && scrut_of(a) == SCRUT_NEUTRAL)
      return conv_pair(m, level, b, a);
    if (a->kind != b->kind)
      return 0;
    switch (a->kind) {
    case VAL_WORD:
      return a->nat == b->nat && u256_cmp(a->word, b->word) == 0;
    case VAL_NAT:
    case VAL_UNIV:
    case VAL_VAR:
      return a->nat == b->nat;
    case VAL_TRAP:
      return 1;
    case VAL_LAM:
    case VAL_PLAM:
      return 0;
    case VAL_PI:
    case VAL_SIGMA:
      return conv_binder(m, level, a, b);
    case VAL_PATHP:
      return conv_pathp(m, level, a, b);
    case VAL_IVAL:
      return ival_equal(a, b);
    case VAL_PAPP:
    case VAL_APP:
      if (!conv_values(m, level, a->dom, b->dom))
        return 0;
      a = a->arg;
      b = b->arg;
      break;
    case VAL_OP:
    case VAL_STUCK:
      if (!same_head(a, b))
        return 0;
      if (a->argc == 0)
        return 1;
      for (i = 0; i + 1u < a->argc; i++)
        if (!conv_values(m, level, a->args[i], b->args[i]))
          return 0;
      a = a->args[a->argc - 1u];
      b = b->args[b->argc - 1u];
      break;
    }
  }
  return 0;
}

int conv_values(Machine *m, uint32_t level, const Value *a, const Value *b) {
  int same;
  if (a == NULL || b == NULL || !enter(m))
    return 0;
  same = conv_loop(m, level, a, b);
  m->depth--;
  return same;
}

static Core *new_core(Machine *m, CoreKind kind) {
  Core *c = arena_alloc(m->arena, sizeof *c);
  if (c == NULL) {
    oom(m);
    return NULL;
  }
  c->kind = kind;
  return c;
}

static const Core *core_leaf(Machine *m, CoreKind kind, uint64_t nat) {
  Core *c = new_core(m, kind);
  if (c != NULL)
    c->nat = nat;
  return c;
}

static const Core *core_word(Machine *m, const Value *v) {
  Core *c = new_core(m, CORE_WORD);
  if (c != NULL) {
    c->nat = v->nat;
    c->word = v->word;
  }
  return c;
}

static const Core *quote_var(Machine *m, uint32_t level, const Value *v) {
  Core *c;
  if (v->nat >= level) {
    internal(m, "a variable above its level");
    return NULL;
  }
  c = new_core(m, CORE_VAR);
  if (c != NULL)
    c->index = level - 1u - (uint32_t)v->nat;
  return c;
}

static const Core *quote_app(Machine *m, uint32_t level, const Value *v) {
  const Core *fn = quote_value(m, level, v->dom);
  const Core *arg = quote_value(m, level, v->arg);
  Core *c;
  if (fn == NULL || arg == NULL)
    return NULL;
  c = new_core(m, CORE_APP);
  if (c == NULL)
    return NULL;
  c->a = fn;
  c->b = arg;
  return c;
}

static const Core *core_pair(Machine *m, CoreKind kind, const Core *a, const Core *b) {
  Core *c;
  if (a == NULL || (b == NULL && kind != CORE_INEG))
    return NULL;
  c = new_core(m, kind);
  if (c == NULL)
    return NULL;
  c->a = a;
  c->b = b;
  return c;
}

/* The two ends of a PATHP or a PAPP value. */
static const Core *const *quote_ends(Machine *m, uint32_t level, const Value *v) {
  const Core **ends = arena_alloc(m->arena, 2u * sizeof *ends);
  if (ends == NULL) {
    oom(m);
    return NULL;
  }
  if (v->argc != 2 || v->args[0] == NULL || v->args[1] == NULL) {
    internal(m, "a path with no ends");
    return NULL;
  }
  ends[0] = quote_value(m, level, v->args[0]);
  ends[1] = quote_value(m, level, v->args[1]);
  return ends[0] != NULL && ends[1] != NULL ? ends : NULL;
}

static const Core *quote_binder(Machine *m, uint32_t level, const Value *v, CoreKind kind) {
  int has_dom = kind == CORE_PI || kind == CORE_SIGMA;
  int interval = kind == CORE_PLAM || kind == CORE_PATHP;
  const Core *dom = has_dom ? quote_value(m, level, v->dom) : NULL;
  const Core *body = quote_value(m, level + 1u, closure_apply(m, v, interval ? val_ivar(m, level) : val_var(m, level)));
  const Core *const *ends = kind == CORE_PATHP ? quote_ends(m, level, v) : NULL;
  Core *c;
  if (body == NULL || (dom == NULL && has_dom) || (ends == NULL && kind == CORE_PATHP))
    return NULL;
  c = new_core(m, kind);
  if (c == NULL)
    return NULL;
  c->name = v->name;
  c->a = dom;
  c->b = body;
  c->args = ends;
  c->argc = ends != NULL ? 2u : 0u;
  return c;
}

static const Core *quote_lit(Machine *m, uint32_t level, uint32_t lit) {
  Core *c;
  if ((lit >> 1u) >= level) {
    internal(m, "an interval variable above its level");
    return NULL;
  }
  c = new_core(m, CORE_VAR);
  if (c == NULL)
    return NULL;
  c->index = level - 1u - (lit >> 1u);
  return (lit & 1u) != 0u ? core_pair(m, CORE_INEG, c, NULL) : c;
}

static const Core *quote_ival(Machine *m, uint32_t level, const Value *v) {
  const Core *out = core_leaf(m, CORE_IZERO, 0);
  const Core *t;
  const IConj *k;
  uint32_t i;
  uint32_t j;
  for (i = 0; i < v->conj_count; i++) {
    k = &v->conj[i];
    t = k->count == 0 ? core_leaf(m, CORE_IONE, 0) : quote_lit(m, level, k->lits[0]);
    for (j = 1; j < k->count; j++)
      t = core_pair(m, CORE_IMEET, t, quote_lit(m, level, k->lits[j]));
    out = i == 0 ? t : core_pair(m, CORE_IJOIN, out, t);
  }
  return out;
}

static const Core *quote_papp(Machine *m, uint32_t level, const Value *v) {
  const Core *path = quote_value(m, level, v->dom);
  const Core *r = quote_value(m, level, v->arg);
  const Core *const *ends = quote_ends(m, level, v);
  Core *c;
  if (path == NULL || r == NULL || ends == NULL)
    return NULL;
  c = new_core(m, CORE_PAPP);
  if (c == NULL)
    return NULL;
  c->a = path;
  c->b = r;
  c->args = ends;
  c->argc = 2;
  return c;
}

static const Core *quote_op(Machine *m, uint32_t level, const Value *v) {
  const Core **args = arena_alloc(m->arena, ((size_t)v->argc + 1u) * sizeof *args);
  Core *c;
  uint32_t i;
  if (args == NULL) {
    oom(m);
    return NULL;
  }
  for (i = 0; i < v->argc; i++) {
    args[i] = quote_value(m, level, v->args[i]);
    if (args[i] == NULL)
      return NULL;
  }
  c = new_core(m, CORE_OP);
  if (c == NULL)
    return NULL;
  c->op = v->op;
  c->inst = v->inst;
  c->field = v->field;
  c->args = args;
  c->argc = v->argc;
  return c;
}

static const Core *quote_inner(Machine *m, uint32_t level, const Value *v) {
  switch (v->kind) {
  case VAL_NAT:
    return core_leaf(m, CORE_NAT, v->nat);
  case VAL_WORD:
    return core_word(m, v);
  case VAL_TRAP:
    return core_leaf(m, CORE_TRAP, 0);
  case VAL_UNIV:
    return core_leaf(m, CORE_UNIV, v->nat);
  case VAL_VAR:
    return quote_var(m, level, v);
  case VAL_APP:
    return quote_app(m, level, v);
  case VAL_LAM:
    return quote_binder(m, level, v, CORE_LAM);
  case VAL_PI:
    return quote_binder(m, level, v, CORE_PI);
  case VAL_SIGMA:
    return quote_binder(m, level, v, CORE_SIGMA);
  case VAL_OP:
  case VAL_STUCK:
    return quote_op(m, level, v);
  case VAL_PLAM:
    return quote_binder(m, level, v, CORE_PLAM);
  case VAL_PATHP:
    return quote_binder(m, level, v, CORE_PATHP);
  case VAL_IVAL:
    return quote_ival(m, level, v);
  case VAL_PAPP:
    return quote_papp(m, level, v);
  }
  internal(m, "an unknown value");
  return NULL;
}

const Core *quote_value(Machine *m, uint32_t level, const Value *v) {
  const Core *c;
  if (v == NULL || !enter(m))
    return NULL;
  c = quote_inner(m, level, v);
  m->depth--;
  return c;
}

const char *op_name(const Machine *m, Op op, uint32_t inst, uint32_t field) {
  if (op == OP_FAMILY)
    return inst < m->family_count ? m->families[inst].name : "?";
  if (op == OP_CTOR)
    return inst < m->ctor_count ? m->ctors[inst].name : "?";
  if (op == OP_PROJ)
    return inst < m->ctor_count && field < m->ctors[inst].field_count ? m->ctors[inst].fields[field].name : "?";
  return (size_t)op < sizeof OP_NAMES / sizeof OP_NAMES[0] ? OP_NAMES[op] : "?";
}

/* The printer keeps len + 4 <= cap, so "..." always fits. */
typedef struct {
  Machine *m;
  char *buf;
  size_t cap;
  size_t len;
  int full;
  const char *const *names;
  uint32_t name_count;
  const char *extra[PRINT_NAMES_MAX];
  uint32_t extra_count;
  unsigned depth;
} Printer;

static void put(Printer *p, const char *s) {
  size_t n = strlen(s);
  if (p->full)
    return;
  if (p->len + n + 4u <= p->cap) {
    memcpy(p->buf + p->len, s, n + 1u);
    p->len += n;
    return;
  }
  memcpy(p->buf + p->len, "...", 4u);
  p->len += 3u;
  p->full = 1;
}

static const char *var_name(const Printer *p, uint64_t level) {
  if (level < p->name_count && p->names[level] != NULL)
    return p->names[level];
  if (level >= p->name_count && level - p->name_count < p->extra_count && level - p->name_count < PRINT_NAMES_MAX)
    return p->extra[level - p->name_count];
  return "_";
}

static void print_value(Printer *p, const Value *v, int atom);

static int is_chain(const Value *v) {
  return v != NULL && (v->kind == VAL_OP || v->kind == VAL_STUCK) && v->argc > 0 && v->op != OP_SYSTEM;
}

static int print_name_taken(const Printer *p, const char *name) {
  if (strcmp(name, "PathP") == 0 || strcmp(name, "i0") == 0 || strcmp(name, "i1") == 0)
    return 1; /* printed path types and interval ends are outside OP_NAMES */
  for (uint32_t i = 0; i < p->name_count; i++)
    if (p->names[i] != NULL && strcmp(p->names[i], name) == 0)
      return 1;
  for (uint32_t i = 0; i < p->extra_count && i < PRINT_NAMES_MAX; i++)
    if (strcmp(p->extra[i], name) == 0)
      return 1;
  for (size_t i = 0; i < sizeof OP_NAMES / sizeof OP_NAMES[0]; i++)
    if (strcmp(OP_NAMES[i], name) == 0)
      return 1;
  for (uint32_t i = 0; i < p->m->family_count; i++)
    if (strcmp(p->m->families[i].name, name) == 0)
      return 1;
  for (uint32_t i = 0; i < p->m->ctor_count; i++) {
    const CtorInfo *ctor = &p->m->ctors[i];
    if (strcmp(ctor->name, name) == 0)
      return 1;
    for (uint32_t j = 0; j < ctor->field_count; j++)
      if (strcmp(ctor->fields[j].name, name) == 0)
        return 1;
  }
  return 0;
}

static void print_binder(Printer *p, const Value *v) {
  uint32_t level = p->name_count + p->extra_count;
  const char *name = v->name != NULL ? v->name : "x";
  char fresh[32];
  uint32_t suffix = 0;
  while (print_name_taken(p, name) || name[0] == '%') {
    snprintf(fresh, sizeof fresh, "x%u", ++suffix);
    name = fresh;
  }
  int interval = v->kind == VAL_PLAM || v->kind == VAL_PATHP;
  const Value *body = closure_apply(p->m, v, interval ? val_ivar(p->m, level) : val_var(p->m, level));
  int binder_dom = v->dom != NULL && (v->dom->kind == VAL_PI || v->dom->kind == VAL_SIGMA || v->dom->kind == VAL_LAM || v->dom->kind == VAL_UNIV);
  switch (v->kind) {
  case VAL_LAM:
  case VAL_PLAM:
    put(p, "fun ");
    put(p, name);
    put(p, " => ");
    break;
  case VAL_PATHP:
    put(p, "PathP (fun ");
    put(p, name);
    put(p, " => ");
    break;
  case VAL_PI:
    if (v->name == NULL) {
      print_value(p, v->dom, binder_dom);
      put(p, " -> ");
      break;
    }
    put(p, "(");
    put(p, name);
    put(p, " : ");
    print_value(p, v->dom, 0);
    put(p, ") -> ");
    break;
  case VAL_SIGMA:
    put(p, "Sigma (");
    put(p, name);
    put(p, " : ");
    print_value(p, v->dom, 0);
    put(p, ") ");
    break;
  case VAL_NAT:
  case VAL_WORD:
  case VAL_TRAP:
  case VAL_UNIV:
  case VAL_OP:
  case VAL_VAR:
  case VAL_APP:
  case VAL_STUCK:
  case VAL_IVAL:
  case VAL_PAPP:
    break;
  }
  if (p->extra_count < PRINT_NAMES_MAX)
    p->extra[p->extra_count] = name;
  p->extra_count++;
  print_value(p, body, v->kind == VAL_SIGMA);
  p->extra_count--;
  if (v->kind == VAL_PATHP) {
    put(p, ") ");
    print_value(p, v->args[0], 1);
    put(p, " ");
    print_value(p, v->args[1], 1);
  }
}

/* A join of meets: a meet binds closer than a join, so only an atom
   position needs parentheses. */
static void print_ival(Printer *p, const Value *v, int atom) {
  int parens = atom && (v->conj_count > 1u || (v->conj_count == 1u && v->conj[0].count > 1u));
  uint32_t i;
  uint32_t j;
  uint32_t lit;
  put(p, v->conj_count == 0 ? "i0" : "");
  put(p, parens ? "(" : "");
  for (i = 0; i < v->conj_count; i++) {
    put(p, i == 0 ? "" : " \\/ ");
    put(p, v->conj[i].count == 0 ? "i1" : "");
    for (j = 0; j < v->conj[i].count; j++) {
      lit = v->conj[i].lits[j];
      put(p, j == 0 ? "" : " /\\ ");
      put(p, (lit & 1u) != 0 ? "~" : "");
      put(p, var_name(p, lit >> 1u));
    }
  }
  put(p, parens ? ")" : "");
}

/* A system prints as its tube, `fun i => [ k -> t, ... ]`: each branch is
   applied to ONE fresh interval name, so all bodies share that binder. A
   key is in the outer context. */
static void print_system(Printer *p, const Value *v, int atom) {
  uint32_t level = p->name_count + p->extra_count;
  const Value *first = v->argc >= 2u ? v->args[1] : NULL;
  const char *name = first != NULL && first->kind == VAL_PLAM && first->name != NULL ? first->name : "i";
  char fresh[32];
  uint32_t suffix = 0;
  uint32_t k;
  while (print_name_taken(p, name) || name[0] == '%') {
    snprintf(fresh, sizeof fresh, "_x%u", suffix++);
    name = fresh;
  }
  put(p, atom ? "(" : "");
  put(p, "fun ");
  put(p, name);
  put(p, " => [");
  if (p->extra_count < PRINT_NAMES_MAX)
    p->extra[p->extra_count] = name;
  p->extra_count++;
  for (k = 0; k + 1u < v->argc; k += 2u) {
    put(p, k == 0 ? " " : ", ");
    print_value(p, v->args[k], 0);
    put(p, " -> ");
    print_value(p, line_at(p->m, v->args[k + 1u], val_ivar(p->m, level)), 0);
  }
  p->extra_count--;
  put(p, " ]");
  put(p, atom ? ")" : "");
}

static void print_other(Printer *p, const Value *v, int atom) {
  char num[96];
  if (v == NULL) {
    put(p, "?");
    return;
  }
  if (v->kind == VAL_OP && v->op == OP_SYSTEM) {
    print_system(p, v, atom);
    return;
  }
  switch (v->kind) {
  case VAL_NAT:
    snprintf(num, sizeof num, "%llu", (unsigned long long)v->nat);
    put(p, num);
    return;
  case VAL_WORD:
    word_text(v->nat, v->word, num, sizeof num);
    put(p, num);
    return;
  case VAL_TRAP:
    put(p, "trap");
    return;
  case VAL_VAR:
    put(p, var_name(p, v->nat));
    return;
  case VAL_OP:
  case VAL_STUCK:
    put(p, op_name(p->m, v->op, v->inst, v->field));
    return;
  case VAL_IVAL:
    print_ival(p, v, atom);
    return;
  case VAL_UNIV:
  case VAL_APP:
  case VAL_LAM:
  case VAL_PI:
  case VAL_SIGMA:
  case VAL_PLAM:
  case VAL_PATHP:
  case VAL_PAPP:
    break;
  }
  if (atom)
    put(p, "(");
  switch (v->kind) {
  case VAL_UNIV:
    snprintf(num, sizeof num, "Type %llu", (unsigned long long)v->nat);
    put(p, num);
    break;
  case VAL_APP:
    print_value(p, v->dom, v->dom->kind != VAL_APP && v->dom->kind != VAL_VAR);
    put(p, " ");
    print_value(p, v->arg, 1);
    break;
  case VAL_LAM:
  case VAL_PI:
  case VAL_SIGMA:
  case VAL_PLAM:
  case VAL_PATHP:
    print_binder(p, v);
    break;
  case VAL_PAPP:
    print_value(p, v->dom, v->dom->kind != VAL_PAPP && v->dom->kind != VAL_APP && v->dom->kind != VAL_VAR);
    put(p, " @ ");
    print_value(p, v->arg, 1);
    break;
  case VAL_NAT:
  case VAL_WORD:
  case VAL_TRAP:
  case VAL_VAR:
  case VAL_OP:
  case VAL_STUCK:
  case VAL_IVAL:
    break;
  }
  if (atom)
    put(p, ")");
}

/* A map prints as mapOf [(K, V), (K, V)] (slice K4a0). */
static void print_kmap(Printer *p, const Value *v, int atom) {
  uint32_t i;
  if (atom)
    put(p, "(");
  put(p, "mapOf [");
  for (i = 0; i + 1u < v->argc && !p->full; i += 2u) {
    put(p, i == 0u ? "(" : ", (");
    print_value(p, v->args[i], 0);
    put(p, ", ");
    print_value(p, v->args[i + 1u], 0);
    put(p, ")");
  }
  put(p, "]");
  if (atom)
    put(p, ")");
}

/* An operation prints as its name and its arguments. The last argument is
   printed in the loop, so a long list does not nest. */
static void print_value(Printer *p, const Value *v, int atom) {
  uint32_t open = 0;
  uint32_t i;
  if (p->depth >= PRINT_DEPTH_MAX) {
    put(p, "...");
    p->full = 1;
    return;
  }
  p->depth++;
  while (is_chain(v) && !val_is(v, OP_KMAP_OF) && !p->full) {
    if (atom) {
      put(p, "(");
      open++;
    }
    put(p, op_name(p->m, v->op, v->inst, v->field));
    /* A fold over a family does not print its result type RES (the
       argument before the scrutinee): the source form has no RES. */
    for (i = 0; i + 1u < v->argc; i++) {
      if (v->op == OP_FOLD_FAMILY && i + 2u == v->argc)
        continue;
      put(p, " ");
      print_value(p, v->args[i], 1);
    }
    put(p, " ");
    v = v->args[v->argc - 1u];
    atom = 1;
  }
  if (val_is(v, OP_KMAP_OF))
    print_kmap(p, v, atom);
  else
    print_other(p, v, atom);
  for (i = 0; i < open; i++)
    put(p, ")");
  p->depth--;
}

int value_print(Machine *m, const char *const *names, uint32_t name_count, const Value *v, char *buf, size_t cap) {
  Printer p;
  if (cap == 0)
    return 0;
  buf[0] = '\0';
  if (cap < 4u)
    return 0;
  memset(&p, 0, sizeof p);
  p.m = m;
  p.buf = buf;
  p.cap = cap;
  p.names = names;
  p.name_count = name_count;
  print_value(&p, v, 0);
  return !p.full && !m->diag->set;
}
