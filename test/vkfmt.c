/* The guest constants of a snarkjs Groth16 key and its proofs (S11 step 11.2, O104).
 *
 * build/vkfmt KEY [PROOF PUBLIC]... reads the verification_key.json of snarkjs
 * 0.7.6 and, for each N from 1, the proof-N.json and public-N.json of one proof.
 * It prints `def eligibilityKey : VerifyKey`, `def proofN : VerifyProof` and
 * `def statementN : List U256` on stdout. Each value is a decimal string in the
 * JSON file and a decimal U256 literal in the output. G1 is (x, y). G2 is in the
 * EIP-197 order of proofOf, (x_im, x_re), (y_im, y_re) (SPEC.md :144): snarkjs
 * writes [x_re, x_im], thus the tool prints the two halves in the other order.
 * This is a generator, not a test: `make check` does not run it. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define MAX_FILE 65536
#define MAX_WORD 80
#define MAX_PUBLIC 16

typedef char Word[MAX_WORD];

static int read_file(const char *path, char *text) {
  FILE *in = fopen(path, "r");
  if (in == NULL) return 0;
  size_t n = fread(text, 1, MAX_FILE - 1, in);
  int ok = ferror(in) == 0 && feof(in) != 0;
  fclose(in);
  text[n] = '\0';
  return ok;
}

/* Read up to max quoted decimal strings from at, in order, until the end of the
 * text. Give the position after the last string read in *end. */
static size_t words(const char *at, Word *out, size_t max, const char **end) {
  size_t count = 0;
  const char *p = at;
  while (count < max && (p = strchr(p, '"')) != NULL) {
    const char *q = strchr(p + 1, '"');
    if (q == NULL) break;
    size_t len = (size_t)(q - (p + 1));
    int digits = len > 0 && len < MAX_WORD && strspn(p + 1, "0123456789") == len;
    if (digits) {
      memcpy(out[count], p + 1, len);
      out[count][len] = '\0';
      count++;
    }
    p = q + 1;
  }
  *end = p == NULL ? at + strlen(at) : p;
  return count;
}

/* The max decimal strings after the key "name" of text. */
static size_t field(const char *text, const char *name, Word *out, size_t max) {
  char quoted[64];
  snprintf(quoted, sizeof quoted, "\"%s\"", name);
  const char *at = strstr(text, quoted), *end;
  return at == NULL ? 0 : words(at + strlen(quoted), out, max, &end);
}

/* The file name of path, for the comment line of each constant. */
static const char *base(const char *path) {
  const char *slash = strrchr(path, '/');
  return slash == NULL ? path : slash + 1;
}

/* A G1 point [x, y, "1"] (affine) at out[0..2]. */
static int g1(const Word *w) { return strcmp(w[2], "1") == 0; }

/* A G2 point [[x_re, x_im], [y_re, y_im], ["1", "0"]] (affine) at out[0..5]. */
static int g2(const Word *w) { return strcmp(w[4], "1") == 0 && strcmp(w[5], "0") == 0; }

static void print_g1(const Word *w) { printf("(pair %su %su)", w[0], w[1]); }

static void print_g2(const Word *w, const char *indent) {
  printf("(pair\n%s  (pair %su\n%s        %su)\n%s  (pair %su\n%s        %su))",
         indent, w[1], indent, w[0], indent, w[3], indent, w[2]);
}

static int key(const char *path) {
  static char text[MAX_FILE];
  Word alpha[3], beta[6], gamma[6], delta[6], ic[3 * (MAX_PUBLIC + 1)];
  if (!read_file(path, text)) {
    fprintf(stderr, "vkfmt: cannot read %s\n", path);
    return 0;
  }
  const char *n_at = strstr(text, "\"nPublic\"");
  long n_public = n_at == NULL ? -1 : strtol(n_at + strlen("\"nPublic\"") + strspn(n_at + strlen("\"nPublic\""), ": "), NULL, 10);
  size_t ic_words = n_public >= 0 && n_public <= MAX_PUBLIC ? 3u * (size_t)(n_public + 1) : 0u;
  int ok = ic_words > 0 && strstr(text, "\"groth16\"") != NULL && strstr(text, "\"bn128\"") != NULL &&
           field(text, "vk_alpha_1", alpha, 3) == 3 && g1(alpha) &&
           field(text, "vk_beta_2", beta, 6) == 6 && g2(beta) &&
           field(text, "vk_gamma_2", gamma, 6) == 6 && g2(gamma) &&
           field(text, "vk_delta_2", delta, 6) == 6 && g2(delta) &&
           field(text, "IC", ic, ic_words) == ic_words;
  for (size_t i = 0; ok && i < ic_words; i += 3) ok = g1(ic + i);
  if (!ok) {
    fprintf(stderr, "vkfmt: %s is not a groth16 bn128 key with affine points\n", path);
    return 0;
  }
  printf("-- The key of %s (nPublic %ld, %zu IC points), from build/vkfmt.\n", base(path), n_public, ic_words / 3u);
  printf("def eligibilityKey : VerifyKey :=\n  pair (pair ");
  print_g1(alpha);
  printf("\n    ");
  print_g2(beta, "    ");
  printf(")\n  (pair (pair\n    ");
  print_g2(gamma, "    ");
  printf("\n    ");
  print_g2(delta, "    ");
  printf(")\n");
  for (size_t i = 0; i < ic_words; i += 3) {
    printf("    (cons ");
    print_g1(ic + i);
    printf("\n");
  }
  printf("    nil");
  for (size_t i = 0; i < ic_words; i += 3) printf(")");
  printf(")\n\n");
  return 1;
}

static int proof(const char *path, int n) {
  static char text[MAX_FILE];
  Word a[3], b[6], c[3];
  if (!read_file(path, text)) {
    fprintf(stderr, "vkfmt: cannot read %s\n", path);
    return 0;
  }
  int ok = field(text, "pi_a", a, 3) == 3 && g1(a) && field(text, "pi_b", b, 6) == 6 && g2(b) &&
           field(text, "pi_c", c, 3) == 3 && g1(c);
  if (!ok) {
    fprintf(stderr, "vkfmt: %s is not a groth16 proof with affine points\n", path);
    return 0;
  }
  printf("-- The proof of %s, from build/vkfmt.\n", base(path));
  printf("def proof%d : VerifyProof :=\n  proofOf\n", n);
  printf("    %su\n    %su\n", a[0], a[1]);
  printf("    %su\n    %su\n    %su\n    %su\n", b[1], b[0], b[3], b[2]);
  printf("    %su\n    %su\n\n", c[0], c[1]);
  return 1;
}

static int statement(const char *path, int n) {
  static char text[MAX_FILE];
  Word w[MAX_PUBLIC + 1];
  const char *end;
  if (!read_file(path, text)) {
    fprintf(stderr, "vkfmt: cannot read %s\n", path);
    return 0;
  }
  size_t count = words(text, w, MAX_PUBLIC + 1, &end);
  if (count == 0 || count > MAX_PUBLIC) {
    fprintf(stderr, "vkfmt: %s is not a list of 1 to %d public words\n", path, MAX_PUBLIC);
    return 0;
  }
  printf("-- The public words of %s, from build/vkfmt.\n", base(path));
  printf("def statement%d : List U256 :=\n", n);
  for (size_t i = 0; i < count; i++) printf("  %s(cons %su\n", i == 0 ? "" : "  ", w[i]);
  printf("    nil");
  for (size_t i = 0; i < count; i++) printf(")");
  printf("\n\n");
  return 1;
}

int main(int argc, char **argv) {
  if (argc < 2 || argc % 2 != 0) {
    fputs("usage: vkfmt KEY [PROOF PUBLIC]...\n", stderr);
    return 2;
  }
  int ok = key(argv[1]);
  for (int i = 2; ok && i + 1 < argc; i += 2) ok = proof(argv[i], i / 2) && statement(argv[i + 1], i / 2);
  return ok ? 0 : 1;
}
