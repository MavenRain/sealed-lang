/* Path regressions. Each row checks a definition, prints its type and its
 * value, checks the printed text again and compares the two results with
 * conv_values. This is the port of coin-lang test/path-regressions.c
 * (582ecef) onto this kit's harness shape (test/fronttool.c): the source is
 * parsed as a domain, so a row that needs a family or a domain definition
 * carries it in a prelude that both programs share. */
#include <stdio.h>
#include <string.h>

#include "front/check.h"
#include "front/lexer.h"
#include "front/parser.h"

static const char LOT[] = "family Lot (n : Nat) := makeLot (amount : Nat)\n";
static const char TRANSPORT[] =
  "def transport : (X : Type 0) -> (Y : Type 0) -> Path (Type 0) X Y -> X -> Y :=\n"
  "  fun (X : Type 0) => fun (Y : Type 0) => fun (p : Path (Type 0) X Y) => fun (a : X) => transp (fun (i : I) => p @ i) i0 a\n";
static const char S1[] = "family S1 := base | loop (i : I) [ ~i -> base, i -> base ]\n";
static const char TRUNC0[] =
  "family Trunc0 (A : Type 0) := trunc0 (trunc0Of : A) | squash0 (squash0From : Trunc0 A) (squash0To : Trunc0 A) (squash0Left : Path (Trunc0 A) squash0From squash0To) (squash0Right : Path (Trunc0 A) squash0From squash0To) (i : I) (j : I) [ ~i -> squash0Left @ j, i -> squash0Right @ j, ~j -> squash0From, j -> squash0To ]\n";
static const char TRUNC1[] =
  "family Trunc1 (A : Type 0) := trunc1 (truncOf : A) | squash1 (squashFrom : Trunc1 A) (squashTo : Trunc1 A) (i : I) [ ~i -> squashFrom, i -> squashTo ]\n";
static const char PARAM_HIT[] =
  "family Trunc1 (A : Type 0) := trunc1 (truncOf : A) | squash1 (squashFrom : Trunc1 A) (squashTo : Trunc1 A) (i : I) [ ~i -> squashFrom, i -> squashTo ]\n"
  "family ParamHit (a : Trunc1 Nat) := paramPoint (paramValue : Trunc1 Nat) |\n"
  "  paramLoop (i : I) [ ~i -> paramPoint a a, i -> paramPoint a a ]\n";

static int checked(Arena *arena, Machine *m, Diag *diag, const char *source) {
  TokenList tokens = {0};
  DeclList decls = {0};
  diag_init(diag);
  return lex_source(arena, "path-regression", source, strlen(source), &tokens, diag)
      && parse_source(arena, "path-regression", ORIGIN_DOMAIN, &tokens, &decls, diag)
      && check_program(arena, &decls, m, diag);
}

static int roundtrip(const char *label, const char *prelude, const char *source) {
  Arena arena;
  Machine m, replay;
  Diag diag;
  char type[8192], value[8192], program[32768];
  int ok;
  uint32_t index;
  arena_init(&arena, (size_t)128 << 20);
  snprintf(program, sizeof program, "%s%s", prelude, source);
  ok = checked(&arena, &m, &diag, program);
  index = ok ? m.def_count - 1u : 0u;
  if (ok)
    ok = value_print(&m, NULL, 0, m.defs[index].type, type, sizeof type)
      && value_print(&m, NULL, 0, def_value(&m, index), value, sizeof value);
  if (ok) {
    snprintf(program, sizeof program, "%sdef replay : %s := %s\n", prelude, type, value);
    ok = checked(&arena, &replay, &diag, program);
    if (!ok) fprintf(stderr, "%s\n", program);
  }
  if (ok)
    ok = conv_values(&m, 0, m.defs[index].type, replay.defs[replay.def_count - 1u].type)
      && conv_values(&m, 0, def_value(&m, index), def_value(&replay, replay.def_count - 1u));
  if (!ok) {
    fprintf(stderr, "FAIL %s\n", label);
    diag_print(&diag, stderr);
  }
  arena_release(&arena);
  return ok;
}

int main(void) {
  int failed = 0;
  failed += !roundtrip("PathP binder capture", "",
    "def original : Sigma (PathP : Nat) (Path Nat PathP PathP) := pack 0 refl\n");
  failed += !roundtrip("anonymous funExt argument", "",
    "def original : (h : (_ : Nat) -> Path Nat 0 0) -> Path (Nat -> Nat) (fun n => 0) (fun n => 0) := fun h => funExt h\n");
  failed += !roundtrip("application of a path of functions", "",
    "def original : (p : Path (Nat -> Nat) (fun n => n) (fun n => n)) -> Path Nat 0 0 := fun p => fun (i : I) => (p @ i) 0\n");
  failed += !roundtrip("dependent congruence", LOT,
    "def original : (p : Path Nat 0 0) -> PathP (fun (i : I) => Lot (p @ i)) (makeLot 0 7) (makeLot 0 7) := fun p => cong (fun (n : Nat) => makeLot n 7) p\n");
  failed += !roundtrip("dependent symmetry", "",
    "def original : (A : Path (Type 0) Nat Nat) -> (p : PathP (fun (j : I) => A @ j) 0 1) -> PathP (fun (i : I) => A @ ~i) 1 0 := fun A p => sym p\n");
  failed += !roundtrip("dependent function extensionality", "",
    "def original : (A : Path (Type 0) Nat Nat) -> (h : (n : Nat) -> PathP (fun (j : I) => A @ j) n n) -> PathP (fun (i : I) => Nat -> A @ i) (fun n => n) (fun n => n) := fun A h => funExt h\n");
  failed += !roundtrip("stuck transport fill", TRANSPORT,
    "def original : PathP (fun (i : I) => (p : Path (Type 0) Nat Nat) -> p @ i) (fun (p : Path (Type 0) Nat Nat) => 0) (fun (p : Path (Type 0) Nat Nat) => transport Nat Nat p 0) := fun (i : I) => fun (p : Path (Type 0) Nat Nat) => transp (fun (j : I) => p @ (i /\\ j)) (~i) 0\n");
  failed += !roundtrip("stuck hcomp with two keys", "",
    "def original : (n : Nat) -> (p : Path Nat n n) -> Path Nat n n := fun n p => fun (j : I) => hcomp Nat (~j \\/ j) (fun (i : I) => [ ~j -> n, j -> p @ i ]) (p @ j)\n");
  failed += !roundtrip("stuck transport in a Path line", "",
    "def original : (A : Type 0) -> (x : A) -> (y : A) -> (r : Path A x y) -> (p : Path A x x) -> Path A y y := fun A x y r p => transp (fun (i : I) => Path A (r @ i) (r @ i)) i0 p\n");
  failed += !roundtrip("dead constructor branch", "",
    "def original : (x : Prod Nat Nat) -> (n : Nat) -> (p : Path Nat n n) -> Path (Prod Nat Nat) (pair n 0) (pair n 0) := fun x n p => fun (j : I) => hcomp (Prod Nat Nat) (~j \\/ j) (fun (i : I) => [ ~j -> pair n 0, j -> pair (p @ i) 0, i0 -> x ]) (pair n 0)\n");
  failed += !roundtrip("dependent Sigma composition", "",
    "def original : (n : Nat) -> (p : Path Nat n n) -> Path (Sigma (x : Nat) (Path Nat x x)) (pack n refl) (pack n refl) := fun n p => fun (j : I) => hcomp (Sigma (x : Nat) (Path Nat x x)) (~j \\/ j) (fun (i : I) => [ ~j -> pack n refl, j -> pack (p @ i) refl ]) (pack n refl)\n");
  failed += !roundtrip("universe function composition", "",
    "def original : (p : Path (Nat -> Type 0) (fun n => Nat) (fun n => Nat)) -> Path (Nat -> Type 0) (fun n => Nat) (fun n => Nat) := fun p => fun (j : I) => hcomp (Nat -> Type 0) (~j \\/ j) (fun (i : I) => [ ~j -> (fun n => Nat), j -> p @ i ]) (fun n => Nat)\n");
  failed += !roundtrip("universe Sigma composition", "",
    "def original : (p : Path (Sigma (x : Nat) (Type 0)) (pack 0 Nat) (pack 0 Nat)) -> Path (Sigma (x : Nat) (Type 0)) (pack 0 Nat) (pack 0 Nat) := fun p => fun (j : I) => hcomp (Sigma (x : Nat) (Type 0)) (~j \\/ j) (fun (i : I) => [ ~j -> pack 0 Nat, j -> p @ i ]) (pack 0 Nat)\n");
  failed += !roundtrip("universe product composition", "",
    "def original : (p : Path (Prod Nat (Type 0)) (pair 0 Nat) (pair 0 Nat)) -> Path (Prod Nat (Type 0)) (pair 0 Nat) (pair 0 Nat) := fun p => fun (j : I) => hcomp (Prod Nat (Type 0)) (~j \\/ j) (fun (i : I) => [ ~j -> pair 0 Nat, j -> pair 0 Nat ]) (pair 0 Nat)\n");
  failed += !roundtrip("universe path composition", "",
    "def original : (p : Path (Path (Type 0) Nat Nat) refl refl) -> Path (Path (Type 0) Nat Nat) refl refl := fun p => fun (j : I) => hcomp (Path (Type 0) Nat Nat) (~j \\/ j) (fun (i : I) => [ ~j -> refl, j -> p @ i ]) refl\n");
  failed += !roundtrip("transport of a universe path", "",
    "def original : (p : Path (Type 0) Nat Nat) -> Path (Type 0) Nat Nat := fun p => transp (fun (_ : I) => Path (Type 0) Nat Nat) i0 p\n");
  failed += !roundtrip("face action on a local type", "",
    "def original : (P : Path (Type 0) Nat Nat) -> PathP (fun (j : I) => (n : P @ j) -> P @ j) (fun (n : Nat) => n) (fun (n : Nat) => n) := fun P => fun (j : I) => fun (n : P @ j) => hcomp (P @ j) (~j \\/ j) (fun (i : I) => [ ~j -> n, j -> n ]) n\n");
  failed += !roundtrip("a branch on a disjunctive face", "",
    "def original : (p : Path Nat 0 1) -> PathP (fun (j : I) => Path Nat (p @ j) (p @ j)) refl refl := fun p => fun (j : I) => hcomp (Path Nat (p @ j) (p @ j)) (~j \\/ j) (fun (i : I) => [ ~j \\/ j -> refl ]) refl\n");
  /* Kit-only row (sealed-lang S3, R27): a word base with no live branch is the base. */
  failed += !roundtrip("hcomp over a literal word base", "",
    "def original : U256 := hcomp U256 i0 (fun (i : I) => [ ]) 7u\n");
  failed += !roundtrip("formal hcomp at a HIT", S1,
    "def original : Path S1 base base := fun (j : I) => hcomp S1 (~j \\/ j) (fun (i : I) => [ ~j -> base, j -> loop i ]) (loop j)\n");
  failed += !roundtrip("path constructor at a neutral interval", S1,
    "def original : PathP (fun (i : I) => Path S1 base (loop i)) refl (fun (j : I) => loop j) := fun (i : I) => fun (j : I) => loop (i /\\ j)\n");
  failed += !roundtrip("transport of a formal hcomp at Trunc1 along a parameter line", TRUNC1,
    "def original : (A : Type 0) -> (B : Type 0) -> (p : Path (Type 0) A B) -> (a : A) -> (b : A) ->\n"
    "  Path (Trunc1 B) (transp (fun (i : I) => Trunc1 (p @ i)) i0 (trunc1 A a)) (transp (fun (i : I) => Trunc1 (p @ i)) i0 (trunc1 A a)) :=\n"
    "  fun (A : Type 0) => fun (B : Type 0) => fun (p : Path (Type 0) A B) => fun (a : A) => fun (b : A) => fun (j : I) =>\n"
    "  transp (fun (i : I) => Trunc1 (p @ i)) i0 (hcomp (Trunc1 A) (~j \\/ j)\n"
    "    (fun (i : I) => [ ~j -> trunc1 A a, j -> squash1 A (trunc1 A b) (trunc1 A a) i ]) (squash1 A (trunc1 A a) (trunc1 A b) j))\n");
  failed += !roundtrip("transport preserves a parameter-dependent HIT boundary", PARAM_HIT,
    "def endpoint : ParamHit (trunc1 Nat 1) :=\n"
    "  transp (fun (i : I) => ParamHit (squash1 Nat (trunc1 Nat 0) (trunc1 Nat 1) i)) i0\n"
    "    (paramPoint (trunc1 Nat 0) (trunc1 Nat 0))\n"
    "def original : Path (ParamHit (trunc1 Nat 1)) endpoint endpoint :=\n"
    "  fun (j : I) => transp (fun (i : I) => ParamHit (squash1 Nat (trunc1 Nat 0) (trunc1 Nat 1) i)) i0\n"
    "    (paramLoop (trunc1 Nat 0) j)\n");
  /* The transport of a dimension-2 constructor along a parameter line stays
     stuck. Its boundary branches are path applications, not recursive
     fields, so a field rebuild needs a boundary correction. */
  failed += !roundtrip("transport of squash0 along a parameter line stays stuck", TRUNC0,
    "def original : (A : Type 0) -> (B : Type 0) -> (p : Path (Type 0) A B) -> (x : Trunc0 A) -> (y : Trunc0 A) ->\n"
    "  (u : Path (Trunc0 A) x y) -> (v : Path (Trunc0 A) x y) ->\n"
    "  Path (Path (Trunc0 B) (transp (fun (k : I) => Trunc0 (p @ k)) i0 x) (transp (fun (k : I) => Trunc0 (p @ k)) i0 y))\n"
    "    (fun (j : I) => transp (fun (k : I) => Trunc0 (p @ k)) i0 (u @ j)) (fun (j : I) => transp (fun (k : I) => Trunc0 (p @ k)) i0 (v @ j)) :=\n"
    "  fun (A : Type 0) => fun (B : Type 0) => fun (p : Path (Type 0) A B) => fun (x : Trunc0 A) => fun (y : Trunc0 A) =>\n"
    "  fun (u : Path (Trunc0 A) x y) => fun (v : Path (Trunc0 A) x y) =>\n"
    "  fun (i : I) => fun (j : I) => transp (fun (k : I) => Trunc0 (p @ k)) i0 (squash0 A x y u v i j)\n");
  /* Boundary images must fold path fields just as evaluation does. */
  failed += !roundtrip("fold boundary maps a point constructor path field",
    "family HH := hhBase | hhNode (hhX : HH) (hhY : HH) (hhP : Path HH hhX hhY) | hhLoop (i : I) [ ~i -> hhNode hhBase hhBase refl, i -> hhNode hhBase hhBase refl ]\n",
    "def original : Path Nat 0 0 := fun (t : I) => fold 0 (fun (x : Nat) (y : Nat) (p : Path Nat x y) => p @ t) refl (hhLoop t)\n");
  failed += !roundtrip("fold square boundary preserves captured intervals in path fields",
    "family HH := hhBase | hhNode (hhX : HH) (hhY : HH) (hhP : Path HH hhX hhY)\n"
    "  | hhSquare (hhU : HH) (hhV : Path HH hhU hhU) (i : I) (j : I) [ ~i -> hhNode hhU hhU (fun (k : I) => hhV @ (j /\\ ~j /\\ k /\\ ~k)),\n"
    "      i -> hhNode hhU hhU (fun (k : I) => hhV @ (j /\\ ~j /\\ k /\\ ~k)), ~j -> hhNode hhU hhU refl, j -> hhNode hhU hhU refl ]\n",
    "def original : HH -> HH := fun (z : HH) => fold hhBase (fun (x : HH) (y : HH) (p : Path HH x y) => hhNode x y p) (fun (x : HH) (p : Path HH x x) => fun (a : I) => fun (b : I) => hhSquare x p a b) z\n");
  if (failed == 0)
    puts("path regressions: passed");
  else
    printf("FAIL path regressions: %d failed\n", failed);
  return failed != 0;
}
