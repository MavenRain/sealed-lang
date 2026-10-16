# sealed-lang status

> Write here: one paragraph. Say which milestone the build implements, and
> which milestone it does not complete yet.

## Implemented

> Write here: one item for each feature that works. Name the former (F1 to
> F15), the core type or the core operation.

- <feature>.

## Remaining M0 work

> Write here: one numbered item for each missing feature. Say what each item
> needs first.

1. <work item>.

## Known limits

> Write here: one item for each limit of the current build, with its cause
> (a fuel budget, a host limit, a stack size).

- <limit>.

## Internal boundaries

> Write here: one paragraph for each module group. Say which module holds
> which job, which tests check which tables, and which types a program
> cannot use.

## S2

Slice S2 (the base span) is complete at commit bc8e168 plus the docs of
this section. The gate `make -B check` is GREEN: examples 19, parse
refusals 13, check refusals 59, evals 54, runs 5, run refusals 10, asmtool
8 of 8, builds 3, lower refusals 12, checks 29, dispatch 20, evm steps 43,
precompile 18 steps and 0 failures, mock 13 rows, diff 15 rows, emit 24,
all regressions passed.

The eight S2 operations are hash, natDiv, natMod, addrToU256, ecrecover,
ecAdd, ecMul and ecPairing. The four pruned operations are natMul,
u256Mul, u256Div and u256Min. The evaluator computes hash through
keccak.c and keeps the four curve operations stuck (opaque). The EVM
target executes each lowered curve statement as one STATICCALL to the
precompile (1 ecrecover, 6 ecAdd, 7 ecMul, 8 ecPairing); test/precompile.sh is the
only check of the curve words. `langc eval` takes no env and no state,
thus the eval rows use the closed defs hashZero, twoG and ownerWord and
the helper leaf.

Mutants M1 to M9 (kickoff section 2), one at a time on commit bc8e168.
Each kill is a test failure, not a compile error. Each file was restored
after its mutant and confirmed by shasum (evm.c 42e7b5b44e37, lower.c
6b94dd517b8e, eval.c 1927de36a9bf, check.c 32335d052fb3), then rebuilt
with `make -B`.

| Mutant | Edit | Killer | Verdict |
|---|---|---|---|
| M1 success flag not tested | evm.c IR_STMT_PRECOMPILE: the ISZERO and the jump to revert after STATICCALL replaced by POP | precompile.sh `addBad`: want revert, got out 0 | KILLED |
| M2 ecMul sent to address 6 | lower.c lower_curve: the ecMul address 7 set to 6 | precompile.sh `mulX` and `mulY`: want 2G, got revert (malformed point); the (0,0) rows give 1 and 2 | KILLED (4 failures) |
| M3 pairing stride 160 | lower.c lower_curve: field_count of a pairing = count minus one word per pair (a 320-byte input) | precompile.sh `pairOk`: want 1, got revert | KILLED |
| M4 hash as word add | lower.c BINARY_OPS: OP_HASH to IR_OP_WORD_ADD | precompile.sh `hashv`: want ad3228b6...5fb5, got 0 (gate.sh stays GREEN: the eval row does not lower) | KILLED |
| M5 natMod as DIV | lower.c BINARY_OPS: OP_NAT_MOD to IR_OP_DIV | precompile.sh `leafStep 7`: want 4, got 6; `leafStep 6`: want 3, got 6 (gate.sh stays GREEN) | KILLED |
| M6 eval natDiv by 0 gives 0 | eval.c nat_divmod: a zero divisor returns 0 instead of the trap | gate.sh evals `entries ratio 7 0` and `entries remainder 7 0`: want trap, got 0 | KILLED |
| M7 `second` of a curve value loads word 0 | lower.c lower_expr: the LOAD index of OP_SECOND set to 0 | precompile.sh `addY` and `mulY`: want the y of 2G, got the x | KILLED |
| M8 output not stored 0 first | evm.c IR_STMT_PRECOMPILE: the zero store of the output block removed | precompile.sh `recoverSum`: want the one address ceaccac6...699d, got twice the address (the stale bytes of the valid signature) | KILLED |
| M9 pruned op in BUILTINS | check.c BUILTINS: row `{"u256Mul", 2, RULE_U256_2, OP_U256_ADD, OP_U256, 0}` added | gate.sh check refusal `pruned-op.lang`: want exit 1 and NAME_UNKNOWN, got exit 0 | KILLED |

Score: 9 of 9 killed.

## S3

Slice S3 (interval paths) is complete at commit 9394ffd plus the docs of
this section. The gate `make -B check` is GREEN: examples 21, parse
refusals 15, check refusals 77, evals 59, runs 5, run refusals 10, asmtool
8 of 8, builds 3, lower refusals 13, checks 30, dispatch 20, evm steps 43,
precompile 18 steps and 0 failures, mock 13 rows, diff 15 rows, emit 24,
all regressions passed (with the new line `path regressions: passed`).
Against S2: examples 19 -> 21, parse refusals 13 -> 15, check refusals
59 -> 77, evals 54 -> 59, lower refusals 12 -> 13, checks 29 -> 30. The
EVM counts (evm 43, precompile 18, mock 13, diff 15, emit 24) did not
change. These counts record the existing execution regressions.

The kit `Eq` is out (the six Eq rows of BUILTINS). `Path` takes its place,
as in coin-lang. The S3 rows of BUILTINS are I, i0, i1, PathP, Path, refl,
sym, cong, funExt, transp and hcomp: 61 - 6 + 11 = 66 rows. transport,
subst, J, JRefl and trans are guest defs in domain/domain.lang (R25). The
Kan rules treat Nat, Flag, U256, Addr, Unit and Map as base lines (R27):
transp is the identity. For hcomp on a face other than i1, a literal base
is returned only when every live branch evaluates to that literal at a
fresh interval; otherwise it stays stuck. A neutral base stays stuck on
such a face. On i1, hcomp selects a true system branch at i1. hcomp spends
the `m->fuel` step budget (R28).
funExt elaborates at check time to a path abstraction and has no eval
rule (R29). A path residual at a word position gives REFUSE_LOWER
(test/lower/path-word.lang, R24 and R26). build/pathtool (test/pathtool.c,
the port of coin-lang test/path-regressions.c) does 19 quote and parse
roundtrips and prints `path regressions: passed` (R30).

Mutants M1 to M9 (kickoff section 4.7), one at a time on commit 9394ffd.
Each kill is a test failure, not a compile error. Each file was restored
after its mutant with `git checkout` and confirmed by shasum (eval.c
dedc24fba7ab, check.c 1fd58e689199, lower.c 469645306144), then rebuilt
with `make -B`.

| Mutant | Edit | Killer | Verdict |
|---|---|---|---|
| M1 path ends swapped | eval.c path_apply: the i0 and i1 ends swapped | gate.sh bad-refl string: the message becomes TYPE_MISMATCH at the domain `transport`; also 21 of 21 examples, pathtool "dependent symmetry" | KILLED |
| M2 PathP end b at i0 | check.c rule_pathp: the end b checked at L[i0] | pathtool "a branch on a disjunctive face" (PATH_UIP). The named killer examples/paths.lang survives alone and gate.sh stays GREEN | KILLED |
| M3 no PATH_UIP guard | check.c path_ends: the PATH_UIP guard removed | check refusal `path-uip.lang`: want PATH_UIP, got PATH_END | KILLED |
| M4 sym body not reversed | check.c rule_sym: the body set to `q @ i` | pathtool "dependent symmetry"; also examples kan.lang and paths.lang, check row `path-sym.lang` | KILLED |
| M5 no De Morgan in ival_neg | eval.c ival_neg: the De Morgan step removed | pathtool "stuck transport fill" (also "dependent symmetry" and "dependent Sigma composition"); the examples fail at the domain `trans` | KILLED |
| M6 transp rule 1 removed | eval.c transp: rule 1 removed | eval row `kan evPi => 5`: PATH_END at the domain `JRefl` | KILLED |
| M7 no TRANSP_LINE check | check.c rule_transp: the TRANSP_LINE check removed | check refusal `transp-line.lang`: want TRANSP_LINE, got ok | KILLED |
| M8 no system overlap check | check.c rule_hcomp: the hcomp_overlap call removed | check refusal `system-overlap.lang`: want SYSTEM_OVERLAP, got HCOMP_CAP | KILLED |
| M9 path word lowers to 0 | lower.c lower_expr: the word-position refusal returns a 0 word | lower refusal `path-word.lang`: want REFUSE_LOWER, got exit 0 and bytecode; build.sh 2 failures (also `nat-fold.lang`) | KILLED |

Score: 9 of 9 killed.
