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
