#include <string.h>

#include "front/contract.h"
#include "front/front.h"

static int load_one(Arena *arena, const char *source, Origin origin, const char *text, size_t len, DeclList *out, Diag *diag) {
  TokenList toks;
  if (!lex_source(arena, source, text, len, &toks, diag)) return 0;
  return parse_source(arena, source, origin, &toks, out, diag);
}

int front_load(Arena *arena, const char *source, const char *text, size_t len, DeclList *out, Diag *diag) {
  out->items = NULL;
  out->count = 0;
  out->cap = 0;
  return load_one(arena, "domain/domain.lang", ORIGIN_DOMAIN, (const char *)domain_source, domain_source_len, out, diag)
      && load_one(arena, CONTRACT_SOURCE_NAME, ORIGIN_DOMAIN, contract_source, contract_source_len, out, diag)
      && load_one(arena, source, ORIGIN_PROGRAM, text, len, out, diag);
}

int front_load_domain(Arena *arena, const char *source, const char *domain, const char *text, size_t len, DeclList *out, Diag *diag) {
  out->items = NULL;
  out->count = 0;
  out->cap = 0;
  return load_one(arena, "domain/domain.lang", ORIGIN_DOMAIN, (const char *)domain_source, domain_source_len, out, diag)
      && load_one(arena, CONTRACT_SOURCE_NAME, ORIGIN_DOMAIN, contract_source, contract_source_len, out, diag)
      && load_one(arena, source, ORIGIN_DOMAIN, domain, strlen(domain), out, diag)
      && load_one(arena, source, ORIGIN_PROGRAM, text, len, out, diag);
}
