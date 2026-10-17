/* Domain declaration regressions use the same parser and checker as langc. */
#include <stdio.h>
#include <string.h>

#include "front/check.h"
#include "front/lexer.h"
#include "front/parser.h"

static int domain_report(const char *text, const char *want, const char *report) {
  Arena arena;
  Diag diag;
  TokenList tokens = {0};
  DeclList decls = {0};
  Machine machine;
  arena_init(&arena, (size_t)1 << 24);
  diag_init(&diag);
  int ok = lex_source(&arena, "test-domain", text, strlen(text), &tokens, &diag)
    && parse_source(&arena, "test-domain", ORIGIN_DOMAIN, &tokens, &decls, &diag)
    && check_program(&arena, &decls, &machine, &diag);
  int passed = want == NULL ? ok : !ok && diag.code != NULL && strcmp(diag.code, want) == 0;
  if (passed && want == NULL && machine.has_state) {
    const CtorInfo *ci = &machine.ctors[machine.families[machine.state_family].first_ctor];
    for (uint32_t i = 0; i < ci->field_count; i++)
      passed &= eval_core(&machine, NULL, ci->fields[i].type) != NULL;
  }
  if (passed && want == NULL && report != NULL) {
    char output[1024];
    FILE *out = tmpfile();
    if (out == NULL) {
      passed = 0;
    } else {
      report_axioms(&machine, out);
      rewind(out);
      size_t len = fread(output, 1, sizeof output - 1u, out);
      output[len] = '\0';
      passed = !ferror(out) && feof(out) && strcmp(output, report) == 0;
      if (!passed)
        fprintf(stderr, "FAIL axiom report: wanted %s, got %s\n", report, output);
      fclose(out);
    }
  }
  if (!passed) {
    fprintf(stderr, "FAIL domain: wanted %s, got %s: %s\n", want == NULL ? "ok" : want,
            ok ? "ok" : diag.code == NULL ? "no diagnostic" : diag.code, text);
  }
  arena_release(&arena);
  return passed;
}

static int domain(const char *text, const char *want) {
  return domain_report(text, want, NULL);
}

static int normal_roundtrip(const char *prefix, const char *type, const char *body) {
  char source[4096];
  char normal[2048];
  Arena arena;
  Diag diag;
  TokenList tokens = {0};
  DeclList decls = {0};
  Machine machine;
  snprintf(source, sizeof source, "%s\ndef value : %s := %s", prefix, type, body);
  arena_init(&arena, (size_t)1 << 24);
  diag_init(&diag);
  int ok = lex_source(&arena, "test-roundtrip", source, strlen(source), &tokens, &diag)
    && parse_source(&arena, "test-roundtrip", ORIGIN_DOMAIN, &tokens, &decls, &diag)
    && check_program(&arena, &decls, &machine, &diag);
  if (ok)
    ok = value_print(&machine, NULL, 0, def_value(&machine, machine.def_count - 1u), normal, sizeof normal);
  if (!ok) {
    fputs("FAIL normal roundtrip: ", stderr);
    diag_print(&diag, stderr);
  }
  arena_release(&arena);
  if (!ok)
    return 0;
  snprintf(source, sizeof source,
    "%s\ndef original : %s := %s\ndef value : %s := %s\n"
    "def same : Path (%s) original value := refl", prefix, type, body, type, normal, type);
  return domain(source, NULL);
}

int main(void) {
  int ok = 1;
  /* The empty domain (sealed-lang S2, R21) lexes, parses and checks. */
  ok &= domain("", NULL);
  ok &= domain("family F := makeF [ i1 -> missing ]", "DOMAIN_FAMILY");
  ok &= domain("family F := makeF [ ]", "DOMAIN_FAMILY");
  ok &= domain("family T := pt (value : Nat) | arc (previous : T) (i : I) "
               "[ ~i -> pt (fold (fun n => n) 1 previous), i -> pt (fold (fun n => n) 1 previous) ]", "HIT_BOUNDARY");
  ok &= domain("family U := u\n"
               "family T := pt (value : Nat) | arc (i : I) "
               "[ ~i -> pt (fold 0 u), i -> pt (fold 0 u) ]", NULL);
  ok &= domain("family T (A : Type 0) := leafT (v : A) | nodeT (l : T Nat) (r : T A)", "DOMAIN_FAMILY");
  ok &= domain("family F := makeF (field : Nat) (field : Flag)", "REFUSE_NAME");
  ok &= domain("family F := makeF (makeF : Nat)", "REFUSE_NAME");
  ok &= domain("family F := makeF (firstField : Nat) (secondField : Flag)", NULL);
  ok &= domain("family F := a (field : Nat) | b (field : Nat)", "REFUSE_NAME");
  ok &= domain("def s : Sum Nat Nat := inl 7\ndef bad : Sum Flag Nat := bind s (fun x => inr x)", "TYPE_MISMATCH");
  ok &= domain("family Lot (n : Nat) := makeLot (amount : Nat)\n"
               "def s : Sum Nat Nat := inl 0\n"
               "def f : (n : Nat) -> Nat -> Lot n := fun n x => makeLot n x\n"
               "def bad : (n : Nat) -> Lot n := first (pair (either f f s) 0)", "TYPE_MISMATCH");
  ok &= domain("def bad : Type 1 := Nat", "TYPE_MISMATCH");
  ok &= domain("def bad : Type 2 := Type 1", "TYPE_UNIVERSE");
  ok &= domain("family U := makeU (decode : Type 0)\ndef bad : Type 0 := U", "TYPE_MISMATCH");
  ok &= domain("family U := makeU (decode : Type 0)\nfamily W := makeW (wrapped : U)\ndef bad : Type 0 := W", "TYPE_MISMATCH");
  ok &= domain("family Box (A : Type 1) := box (unbox : A)\ndef bad : Type 0 := Box (Type 0)", "TYPE_MISMATCH");
  ok &= domain("family U := makeU (decode : Type 0)\ndef good : Type 1 := U", NULL);
  ok &= domain("def bad : Type 0 := Path (Type 0) Nat Nat", "TYPE_MISMATCH");
  ok &= domain("def good : Type 1 := Path (Type 0) Nat Nat", NULL);
  ok &= domain("def ConstNat : Nat -> Type 0 := fun n => Nat\n"
               "state State := makeState (count : Nat) (other : ConstNat count)\n"
               "def init : State := makeState 3 7\n"
               "def value : Path Nat (other init) 7 := refl", NULL);
  ok &= domain("state State := makeState (count : Nat) (proof : Path Nat count count)", "REFUSE_STATE");
  ok &= domain("family U := makeU (decode : Type 0)\n"
               "def bad : Type 0 := Path U (makeU Nat) (makeU Nat)", "TYPE_MISMATCH");
  ok &= domain("family U := makeU (decode : Type 0)\n"
               "def good : Type 1 := Path U (makeU Nat) (makeU Nat)", NULL);
  ok &= domain("family U := makeU (decode : Type 0)\n"
               "family W := makeW (proof : Path U (makeU Nat) (makeU Nat))\n"
               "def bad : Type 0 := W", "TYPE_MISMATCH");
  ok &= domain("family U := makeU (decode : Type 0)\n"
               "family W := makeW (proof : Path U (makeU Nat) (makeU Nat))\n"
               "def good : Type 1 := W", NULL);
  ok &= domain("def eta : (p : Prod Nat Nat) -> Path (Prod Nat Nat) (pair (first p) (second p)) p := fun p => refl", NULL);
  ok &= domain("def eta : (p : Sigma (n : Nat) (Path Nat n n)) -> Path (Sigma (n : Nat) (Path Nat n n)) (pack (witness p) (payload p)) p := fun p => refl", NULL);
  /* A domain axiom (S6 R52 (a)): the law type unfolds a definition, and an
     application of the axiom is a neutral of its type. */
  ok &= domain("def isPropT : (P : Type 0) -> Type 0 := fun (P : Type 0) => (a : P) -> (b : P) -> Path P a b\n"
               "axiom natSetLaw : (x : Nat) -> (y : Nat) -> isPropT (Path Nat x y)\n"
               "def use : Path (Path Nat 0 0) refl refl := natSetLaw 0 0 refl refl", NULL);
  /* Cached aliases are definitions with proofs, not new trusted axioms. */
  ok &= domain_report("def value : Nat := 0", NULL, "");
  ok &= domain_report("axiom law : Path Nat 0 0\n"
                      "axiom otherLaw : Path Nat 0 0\n"
                      "def alias : Path Nat 0 0 := law\n"
                      "def use : Path (Path Nat 0 0) alias law := refl", NULL,
                      "axiom law\naxiom otherLaw\n");
  /* Printing must keep an axiom free when a binder has the same name.
     The generated name x1 is also an axiom, so it must stay free too. */
  ok &= normal_roundtrip("axiom law : Path Nat 0 0\n"
                         "axiom x1 : Path Nat 0 0\n"
                         "def keepLaw : Path Nat 0 0 := law",
    "(Path Nat 0 0) -> Path Nat 0 0", "fun (law : Path Nat 0 0) => keepLaw");
  ok &= normal_roundtrip("", "Nat -> (A : Type 0) -> A -> A",
    "fun (i0 : Nat) (A : Type 0) (a : A) => transp (fun (i : I) => A) (~i1) a");
  if (ok) puts("domain regressions: passed");
  return ok ? 0 : 1;
}
