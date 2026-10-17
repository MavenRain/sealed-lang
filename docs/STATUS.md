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

## S4

Slice S4 (HITs and QITs) is complete at commit 0c72c03 plus step 4.6 (the
lowering arms and the pathtool rows) and the docs of this section. The gate
`make -B check` is GREEN: examples 24, parse refusals 15, check refusals
83, evals 61, runs 5, run refusals 10, asmtool 8 of 8, builds 3, lower
refusals 14, checks 31, dispatch 20, evm steps 43, precompile 18 steps and
0 failures, mock 13 rows, diff 15 rows, emit 24, all regressions passed
(path regressions too). Against S3: examples 21 -> 24, check refusals 77
-> 83, evals 59 -> 61, lower refusals 13 -> 14, checks 30 -> 31, pathtool
roundtrips 19 -> 26. The EVM counts (evm 43, precompile 18, mock 13, diff
15, emit 24), runs 5 and run refusals 10 did not change at any S4 step: a
HIT value is erased (R31, R37).

A domain family can now have path constructors, as in coin-lang 8c79e9a,
e0d4f3c and 25c14e0 (each Glue hunk dropped, R36): a constructor with
interval fields `(i : I)` and a boundary system `[ ~i -> a, i -> b ]`.
domain/domain.lang declares the circle S1, the propositional truncation
Trunc1, the set truncation Trunc0 and the set quotient Quot after the S3
path defs, with the guest defs isProp and isSet (R32). The checker checks
each boundary (HIT_BOUNDARY). `fold` over a HIT needs one case for each
path constructor, at the type of its boundary: a missing case gives
TYPE_MISMATCH, a case with a wrong end gives PATH_END, and a fold of
Trunc0 or Quot needs the isSet case. The evaluator reduces a path
constructor at an end to its boundary and keeps it neutral at a neutral
interval. A composite of path constructor values is a formal hcomp (a
canonical value). `transp` along a family parameter line reduces
(`transp_family`). BUILTINS stays 66 rows: S4 adds no operation.
examples/circle.lang, examples/trunc.lang and examples/quot.lang are on
Nat. R36 dropped the coin-lang defs that use ua or Fin, and with them the
eval rows `circle loopAt0` and `circle loop2At0`, so evals is 61, not the
kickoff 63. The new eval rows are `trunc truncMapped => trunc1 Nat 0` and
`quot toTrunc3 => trunc0 Nat 2`. A path constructor residual or a formal
hcomp at a word position gives REFUSE_LOWER (`test/lower/loop-word.lang`,
R31 and R33). The helpers `system_branches`, `system_overlaps`,
`system_core` (check.c) and `true_branch` (eval.c) are ported (R35); the
kit `rule_hcomp` keeps its inline code beside them.

Mutants M1 to M9 (kickoff section 5, step 4.7), one at a time on the 4.6
tree 2e914236 (commit 0c72c03 plus the staged step 4.6). Each kill is a
test failure, not a compile error. Each file was restored after its mutant
from the staged blob and confirmed by shasum (eval.c 63a0060ae9ce, check.c
2445a0e2b696, lower.c 82ead0b01620), then rebuilt with `make -B`.

| Mutant | Edit | Killer | Verdict |
|---|---|---|---|
| M1 no fold path end check | check.c path_ends: the end check (not UIP) returns 1 | check refusal `fold-path-boundary.lang`: want PATH_END, got exit 0 | KILLED |
| M2 missing path case accepted | check.c rule_fold and fold_family: a missing case of interval count 1 accepted | check refusal `fold-missing-path.lang`: want TYPE_MISMATCH, got exit 0 | KILLED |
| M3 path constructor is its first boundary | eval.c reduce_ctor: the first boundary at every interval | check refusal `loop-not-refl.lang`: want PATH_UIP, got exit 0 | KILLED |
| M4 no end rule | eval.c reduce_ctor: the end rule removed | check refusal `loop-not-refl.lang`: PATH_END "the path at i0 is loop i0, but the type needs base"; pathtool 7 rows fail | KILLED |
| M5 fold of a formal hcomp is its base | eval.c fold_hcomp: returns the folded base | lower refusal `loop-word.lang`: want REFUSE_LOWER, got exit 0 (the pathtool S1 rows pass) | KILLED |
| M6 no family transport | eval.c transp_family: returns the value unchanged | pathtool "transport preserves a parameter-dependent HIT boundary", "transport of a formal hcomp at Trunc1 along a parameter line", "transport of squash0 along a parameter line stays stuck" (the eval row `trunc truncMapped` passes) | KILLED |
| M7 no isSet case | check.c: a missing squash0 (isSet) case accepted (M2 at interval count 2) | check refusal `trunc0-no-set.lang`: want TYPE_MISMATCH, got exit 0 | KILLED |
| M8 square faces swapped | eval.c fold_square: the outer faces swapped | none: the full gate passes | SURVIVED (equivalent) |
| M9 word hcomp lowers as its base | lower.c lower_expr: a word hcomp lowers as its base (USER ruling (a)) | lower refusals `loop-word.lang` and `path-word.lang`: want REFUSE_LOWER, got exit 0 | KILLED |

Score: 8 of 9 killed, 1 equivalent. M8 is equivalent: the faces reach
`path_apply` only as the stored ends of a neutral path application, and
`fold_square` runs only at an interval that is not an end (at an end,
`reduce_ctor` gives the squash0 boundary first). NbE does not substitute
into a stuck value, so no program sees the swap, and no row was added. The
kickoff M9 (the R31 arm returns a 0 word) had no target: no well-typed
entry reaches the `value_name` arms, because an entry parameter has a word
type and an interval binder occurs only under a system or a path
abstraction. The kickoff killers of M4 and M5 (the eval rows `circle
loopAt0` and `circle loop2At0`) were dropped by R36.

## S5

Slice S5 (the deposit contract) is complete at commit 656df7d and the
docs of this section. The gate `make -B check` is GREEN: examples 27,
parse refusals 15, check refusals 83, evals 64, runs 6, run refusals 10,
asmtool 8 of 8, builds 3, lower refusals 14, checks 31, dispatch 20, evm
steps 43, precompile 32 steps and 0 failures, deposit 25 assertions,
deposit EVM 41 calls and 0 failures, mock 13 rows, diff 16 rows, emit 24,
all regressions passed (the lower regressions with the row
`insert-depth-32`). Against S4: examples 24 -> 27, evals 61 -> 64, runs 5
-> 6, precompile 18 -> 32, diff 15 -> 16, and the new gate lines
`deposit: 25 assertions` and `deposit EVM: 41 calls`. The runtime of
`examples/deposit.lang` is 17119 bytes (EIP-170 24576, R48).

`examples/deposit.lang` implements the S5 deposit interface of the paper
(Algorithm 1) on the kit contract form, with the fixture limits below.
It has 9 entries (`regAuction`,
`registerDeposit`, `makeInactive`, `addTuple`, `updateActivity`,
`slashUsr`, `slash`, `withdraw`, `settle`; R43) and 9 views (O8). The
tree library (`treeDepth` 32, `zeroHash`, `insertLeaf`, `rootOfLeaves`)
and the Groth16 verifier library (`verify`: a fold of `ecMul` and `ecAdd`
over the public inputs, then one `ecPairing` on four pairs) are guest
defs in `domain/domain.lang` below Quot (R40). `examples/merkle.lang`
checks that the root of 1, 2 and 3 leaves by `insertLeaf` equals
`rootOfLeaves` (3 eval rows). `examples/groth16.lang` has the views
`verifyOk` (1) and `verifyBadOrder` (0) on the R39 fixture. `verify` stays
stuck in `langc eval` (O5), thus `test/precompile.sh` checks it on `evm`:
precompile 18 -> 20, then 32 with the USER rows of
`test/groth16-invalid.lang` (the IC count, each input below r, a
noncanonical y in `g1Neg`). The share rule in `src/lower.c` (R41) lowers
a pure word value that both arms of a word `flagIf` use once into a
local. Thus the depth-32 insert is linear in the depth (lowertool row
`insert-depth-32`). S5 adds no BUILTINS row (66).

S5 does not establish the paper's eligibility or winner guarantees.
`slashUsr` checks unequal cm words but does not bind them to the proof
inputs: `test/deposit.sh` accepts the same synthetic proof with inputs
(2, 3) for cm1 = 5 and cm2 = 6 (O50). Replacing the key alone does not
add the missing binding. `settle` records any caller and supplied price
after the auction ends; selecting the winner from sealed bids is S6
(`processBid`, O7/O44). For token custody, `vault` must be the executing
deposit contract's address: `pull` credits it, while `pay` spends the
executing contract's balance (O46).

The deposit tests are in four places. `test/deposit.lang` has 25 eval
assertions. `test/run/deposit.script` and `test/run/deposit.out` hold 23
calls that `langc eval` can step: two auctions and a refused third in the
same slot, a deposit below minDeposit, two deposits, `rootOf`, `addTuple`,
`makeInactive` by a user and by the proposer, `updateActivity` by another
user, an early `withdraw`, `slashUsr` with cm1 = cm2, and `settle` early,
once and twice, with the views. `test/diff.sh` replays the script on `evm`
with the mock token (diff 16). `test/deposit.sh` has 41 chain calls: the
signed calls with test key 1 and the slash path with the fixture
proof. USER ruling (s7): `langc run` stops with EVAL_ENTRY at a
successful signed or proof call, because `ecrecover` and the curve
operations are stuck in `langc eval` (O47). Deviations from the kickoff
(step 4.5): `slash` and `withdraw` also clear the timer of the user
(`clearUser`), `withdraw` also requires a nonzero amount, and `settle`
also requires `id < ctr`. The USER commit 86eb3d6 binds the signature to
the nonce: `signedBy` checks `hash piAcc[au] piNonce[au]` (O45).

Step 4.6 fixed the kit helper `sel` in `test/evmchain.sh`. `od -An -tx1`
without `-v` writes `*` in place of a repeated 16-byte row, so the hex of
a signature with 6 or more uint256 arguments had an odd length, and the
call reverted. USER ruling (s8): `-v` at the 4 kit `od` sites
(`test/evmchain.sh` and `test/dispatch.sh` `sel`, `test/mock.sh` and
`test/diff.sh` Transfer topic). The hex of each other signature did not
change. lang-template has the same line in 7 files; that fix is a
separate unit.

Mutants M1 to M9 (kickoff section 6, step 4.7) and the extra M9b, one at
a time on the tree 867ff888 (commit 656df7d). Each kill is a test
failure, not a compile error. Each file was restored by `git checkout`
and confirmed by its blob hash, then rebuilt with `make -B`.

| Mutant | Edit | Killer | Verdict |
|---|---|---|---|
| M1 wrong root | domain.lang:69 `treeLevel`: the left child is `hash z cur` | gate `merkle sameOne`, `sameTwo`, `sameThree`: want 1, got 0; run deposit.out call 11 `rootOf` and the final state differ | KILLED |
| M2 wrong public input order | domain.lang:159 `verify`: the 2 inputs zipped in the other order | precompile `verifyOk`: want out 1, got out 0 (also `verifyBadOrder`, `verifyScalar` x2, `verifyPoint`); deposit EVM `slashUsr`: want ok, got revert | KILLED |
| M3 missing caller check | deposit.lang:105 `makeInactive`: `flagIf flagYes` (no proposer check) | run deposit.out call 14 `makeInactive` revert became ok; deposit `testInactiveCaller` 0; deposit EVM `chain revert 20 "$user" makeInactive` got ok | KILLED |
| M4 timer that does not expire | deposit.lang:250 `withdraw`: compares with the timer, not the current slot | deposit EVM `chain ok 60 "$signer" withdraw`: want ok, got revert | KILLED |
| M5 no minimum deposit | deposit.lang:87 `registerDeposit`: `flagIf flagYes` (no `d >= minDeposit`) | run deposit.out call 6 `registerDeposit` revert became ok (pull 99u) | KILLED |
| M6 equal commits slash | deposit.lang:207 `slashUsr`: `flagIf flagNo` (cm1 = cm2 passes) | run exit 1: EVAL_ENTRY `slashUsr` at deposit.script:29 (call 18 reaches `verify`, which is stuck in eval) | KILLED |
| M7 A in place of -A | domain.lang:155 `verify`: pairs A, not -A | precompile `verifyOk`: want out 1, got out 0 (also `verifyScalar` x2, `verifyNoInputs`) | KILLED |
| M8 nextLeaf kept | deposit.lang:91 `registerDeposit`: keeps `nextLeaf s` | run deposit.out call 11 `rootOf` and the final state differ | KILLED |
| M9 share rule off | lower.c:678 `share_arms` returns `1 \|\|` (R41) | lowertool `FAIL insert-depth-32`, diag `OOM: out of memory` | KILLED |
| M9b auction count off by one (extra) | deposit.lang:39 `aucRoom`: count <= nA | run deposit.out call 5 `regAuction` revert became ok | KILLED |

Score: 9 of 9 killed, and M9b killed. diff.sh cannot kill a mutant in
shared library code or in the program source (M1, M8): `test/diff.sh`
compares `langc run` with the EVM run of the same source, and the mutant
changes both runs in the same way. The run script (`deposit.out`, the
expected output) and the eval rows kill them. For M9 the kickoff expected
EVM_SIZE: the 32 MiB arena of `build/lowertool` fills first, and the row
fails with OOM.
