# sealed-lang status

The build implements the kit milestones M0 to M3 (`SPEC.md` section 10).
The slices S0 to S6 built the language, the deposit contract and the
protocol layer. S7 wrote the docs and ran the final gate (`## S7`). S8
fixed the S7 review items (`## S8`). S9 built the open list after S8 (`## S9`). S10 built the open list after S9 (`## S10`).
The sections above `## S2` give the summary of S0 to S6; the slice records give the details.

## Implemented

- The kit host tcc-evm-contract (S0, beae7ec): `langc check`, `eval`,
  `build` and `run` (`probe/CAPABILITY.md` section 1).
- The type formers F1 to F14 are DONE, F7 in `check` and `eval` only. F15
  is DONE in `check` and `eval` (S9 step 9.3) and in EVM output for a one-constructor family state field (S10 R95); no Bag state field (O84, R80; `SPEC.md:83-97`).
- The base span (S2): the core types `Nat`, `Flag`, `U256` and `Addr`,
  and the operations `hash`, `natDiv`, `natMod`, `addrToU256`,
  `ecrecover`, `ecAdd`, `ecMul` and `ecPairing`. On the EVM, a curve
  operation is one STATICCALL to a precompile (`## S2`).
- Paths (S3): the interval, `PathP`, `Path`, `refl`, `sym`, `cong`,
  `funExt`, `transp` and `hcomp`. `transport`, `subst`, `J` and `trans`
  are guest definitions (`## S3`).
- HITs and QITs (S4): path constructors with a boundary system, `S1`,
  `Trunc1`, `Trunc0`, `Quot`, `isProp` and `isSet` (`## S4`).
- The deposit contract (S5, Algorithm 1 of the paper):
  `examples/deposit.lang` with 9 entries and 10 views, the share rule, and
  the Merkle tree library and the Groth16 verifier as guest definitions.
  The runtime code is 17069 bytes at the S10 close (`SPEC.md:244-250`).
- The protocol layer (S6, Algorithms 2 and 3): `Bag`, `Bid`, the four laws, the median, `processBid` and the property checks.
  Since S9 step 9.4, local `Bag` folds that reduce to word code can reach the EVM (`test/bag.lang`); `processBid` stays off chain (R76).
  Laws have no run-time value. `langc check` prints the axiom report (`SPEC.md:283-300`).

## Remaining work

`SPEC.md:316-332` gives the items open at S7 and their state at the S10 close:

1. `settle` (O7, O44): closed, not taken (9.1, R88); it refuses a caller with no deposit (8.4).
2. `vault` (O46): closed in S8 step 8.3; `pull` credits `self env` (R74).
3. The binding of each cm to its proof (O50, O58): PARTIAL; input 1 = cm (8.5); the circuit stays open (R87).
4. An entry that calls `processBid` (O57): closed as off chain only (R76).
5. The R35 refactor of `rule_hcomp` (O35): done in S8 step 8.2 (R77).
6. The EIP-170 budget (O48): 7507 bytes are free under 24576 at the S10 close (R96).
7. `od -v`: sealed-lang's 4 copies are fixed (R79); the 7 kit sites too, kit commit 0ead0b7 (R85).
8. F15: DONE in `check` and `eval` (`family Lot`, 9.3, R86) and in EVM output for the `Lot n` state field (10.2 and 10.3, R95); no Bag state field (O84, R80).

## Known limits

- `hcomp` spends the `m->fuel` step budget (R28). `unfold` has a `Nat`
  step limit (`SPEC.md:93`).
- `langc eval` takes no env and no state. `ecrecover` and the curve
  operations stay stuck in it. Only `test/precompile.sh` checks the curve
  words (`## S2`).
- `langc run` stops with EVAL_ENTRY at a signed call or a proof call
  (O47, `## S5`).
- On the EVM, a `Nat` is a uint64 word and a view returns one word of 32
  bytes. If a residual fold's seed or step can trap, its step must be
  strict in its accumulator, else REFUSE_LOWER (`SPEC.md:132`, :239-243).
- The `build/lowertool` arena is 32 MiB (`## S5`).
- The property rows check fixed inputs only. They do not prove Lemma 11
  or Theorems 7 to 10 (`## S6`).

## Internal boundaries

`src/front/` holds the lexer, the parser, the checker with the BUILTINS
table and the refusal codes (`src/front/check.c`), and the evaluator
(`src/front/eval.c`). `src/lower.c` lowers a checked program to the IR of
`src/ir.h`. `src/evm.c` and `src/asm.c` write the EVM bytecode.
`src/keccak.c` is the keccak256 unit. `src/main.c` is the `langc` command.

`test/gate.sh` checks the examples, the parse and check refusals, the
evals and the axiom report. The other scripts in `test/` check the runs,
the build output, the dispatch, the EVM chains, the precompiles, the
deposit contract, the mock token and the diff of `langc run` with `evm
run`. The C test tools check the domain rows, the path rows, the run
budgets, the build output, the lowering, the emitter, the hashing code and
the assembler. `slottool` is a helper of the chain scripts.

`SPEC.md:62-79` gives the forms that a program cannot use, with their
codes: for example a family (REFUSE_DATA) and an axiom (REFUSE_AXIOM).
Only `domain/domain.lang` declares the families, the HITs, the QITs and
the four laws. A path, a path constructor, a formal `hcomp` and a law have
no run-time form (`SPEC.md:252-262`).

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
`slashUsr`, `slash`, `withdraw`, `settle`; R43) and 10 views (O8). The
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

## S6

Slice S6 (the protocol layer) is complete at commit 2c42e04 and the docs
of this section. The gate `make -B check` is GREEN: examples 28, parse
refusals 15, check refusals 85, evals 84, runs 6, run refusals 10,
asmtool 8 of 8, builds 3, lower refusals 14, checks 31, dispatch 20, evm
steps 43, precompile 32 steps and 0 failures, deposit 25 assertions,
deposit EVM 41 calls and 0 failures, mock 13 rows, diff 16 rows, emit 24,
all regressions passed. Against S5: examples 27 -> 28, check refusals 83
-> 85, evals 64 -> 84, and 1 domain regression row (a domain axiom
checks). The runtime of `examples/deposit.lang` is 17119 bytes, as at S5
(no change to deposit.lang, R57 (a)).

S6 adds the law form and the protocol layer of the paper (Algorithms 2
and 3, Lemma 11) as helpers that never reach the EVM. `domain/domain.lang`
has the QIT `Bag A` (`bagNil`, `bagCons`, `bagSwap`, `bagTrunc`; R51),
`Bid` (value, cm), the order `bidLe` (value first; on a tie the larger cm
wins, R55), `bidMax`, the insert `insNat` with its seed `insNatSeed`, and
the four laws `bidMaxSwap`, `bidSet`, `insSwap` and `listNatSet` (R52,
R53). `check_axiom` in `src/front/check.c` does the law type test first
(after the Pi binders, a Path or a PathP, else REFUSE_AXIOM_TYPE), then
the origin test (an axiom in a program is REFUSE_AXIOM). A law is a
neutral in `langc eval` and REFUSE_LOWER on the EVM. `langc check` prints
the axiom report (SPEC section 8, the USER function `report_axioms`), and
test/gate.sh compares it for each example. `examples/protocol.lang` has
`bagMax` (the winner), `bidMaxSwapAt`, `sortBag`, `medianAt` (R54),
`insSwapAt`, `signComWord` (`hash cm (toU256 t)`, R55), `stampBy`,
`addrIn`, `piSlashed`, `tooLate`, `stampsOk`, `processBid` (codes 1 to 5
in the order of Algorithm 3, R56), the property helpers `inRange`,
`isCode`, `isAccept` and `ncOf`, and 20 eval rows. The check refusals
`axiom-type.lang` (REFUSE_AXIOM_TYPE) and `bag-swap-boundary.lang`
(PATH_END) are new. `axiom.lang` now has a law type (`Path Nat 0 1`),
thus only the origin test refuses it (it kills M10).

Deviations from the kickoff: `stampBy` keeps the zero address guard of
`signedBy` (an invalid signature recovers 0, R18). In `processBid`, the
auction proof check gets cm, the stamp test folds the sorted list
`sortBag stamps` (laws 3 and 4 cover it, no fifth law), and no test reads
`aucId`.

Limits of S6:

- The protocol layer is off chain only (R57 (a)). `bagMax` and
  `processBid` never reach the EVM. `settle` did not change: it records
  any caller and supplied price (O7, O44).
- `ecrecover` and the Groth16 verify are stuck in `langc eval` (O5). Thus
  `processBid` takes the proof check `zkOk` and the stamp check `stampOk`
  as parameters (R56 (a)), the eval rows give fixtures, and `stampBy` has
  no eval row.
- The four laws are trusted, not proved. `docs/VALIDATION.md` gives the
  reason for each. The rows `bidMaxSwapAt` and `insSwapAt` check one
  closed instance each; they do not prove a law.
- The property rows check fixed inputs only. They do not prove Lemma 11
  or Theorems 7 to 10.
- `processBid` has no gossip of Pi with the 4Δ_g wait (`updateHistory`)
  and no `U[u].remove(AucID)`: its output is the new pool or a code.
- The binding of each cm to its proof inputs stays out (R58 (a), O50).

Open (recorded, not built) at S6: a row that checks a domain axiom with a
type that is not a law (the M11 note below; S7 step 7.1 adds it, bbe668d).
The R35 refactor of `rule_hcomp` (S4). The
lang-template `od -v` unit (7 files). The EIP-170 budget (R48, 7457 bytes
free). `settle` (O7, O44) and `vault` (O46).

Mutants M1 to M11 (kickoff section 6, step 4.7) and the extra M4strict,
one at a time on the tree edffdf5b (commit 3578e5d). A mutant of
examples/protocol.lang ran on a scratch copy of the file. A mutant of
domain/domain.lang or src/front/check.c changed the repo file, then
`make -B build/langc` (the domain is compiled into langc). Each file was
restored and confirmed by its blob hash, then rebuilt with `make -B`. Each
run used the 28 examples checks, the 85 check refusals and the 84 eval
rows with the rules of test/gate.sh. Each kill is a test failure, not a
compile error. In the table, `accept` is the output of `procAccept`:
`inr (bagCons (Prod U256 U256) (pair 9u 12u) (bagCons (Prod U256 U256)
(pair 5u 1u) (bagNil (Prod U256 U256))))`.

| Mutant | Edit | Killer | Verdict |
|---|---|---|---|
| M1 `bidMax` keeps the first bid | domain.lang:71 `bidMax`: `flagIf (bidLe a b) a a` | examples protocol.lang: PATH_END `bidMaxSwapAt`, the path at i1 is pair 5u 1u, but the type needs pair 7u 2u; thus each of the 20 protocol eval rows exits 1. On a copy with no `bidMaxSwapAt`: eval `winnerOf3` want pair 7u 2u, got pair 5u 1u; `tieBid` want pair 7u 4u, got pair 7u 2u | KILLED |
| M2 the median at index f + 1 | protocol.lang:36 `medianAt`: `(natAdd f 1)` folds of `listTail` | eval `medianOdd` got some 9, `medianEven` got some 6, `medianMissing` got none, `procAccept` got inl 4, `procStamp` got inl 4, `lemma11F1` got 0, `lateCut` got 0 | KILLED |
| M3 the `bagMax` swap case with the wrong ends | protocol.lang:10 `bagMax`: `bidMaxSwap y x r` | examples protocol.lang: TYPE_MISMATCH `bagMax`, expected a term of type PathP from `bidMax x (bidMax y r)` to `bidMax y (bidMax x r)`; thus each of the 20 protocol eval rows exits 1 | KILLED |
| M4 `processBid` accepts a late bid | protocol.lang:83 `tooLate`: `flagIf flagYes flagNo flagYes` (no tau <= t_end test) | eval `procLate` want inl 4, got `accept`; `lateCut` got 0 | KILLED |
| M4strict a strict late test (extra) | protocol.lang:83 `tooLate`: `natLe (natAdd m 1) tEnd` (tau < t_end) | eval `lateCut` got 0 (the median 5 at t_end 5 becomes late) | KILLED |
| M5 `signComWord` over the wrong word | protocol.lang:54 `signComWord`: `hash (toU256 t) cm` | eval `signComA` and `signComB` exchange their outputs (each row got the want of the other row); `procAccept` got inl 5; `lateCut` got 0 | KILLED |
| M6 `processBid` skips the slashed test | protocol.lang:109 `processBid`: `flagIf flagNo (inl 2)` | eval `procSlashed` want inl 2, got `accept`; `slashedAbort` got 0 | KILLED |
| M7 `processBid` skips the depositor test | protocol.lang:110 `processBid`: `flagIf flagYes` | eval `procDepositor` want inl 3, got `accept` | KILLED |
| M8 the tie rule flipped | domain.lang:69 `bidLe`: `u256Le (second b) (second a)` | eval `tieBid` want pair 7u 4u, got pair 7u 2u | KILLED |
| M9 `insNat` sorts down | domain.lang:91 `insNat`: `natLe x y` | eval `medianEven` want some 4, got some 6; `medianMissing` want some 7, got some 4 | KILLED |
| M10 the axiom origin test dropped | check.c:2725 `check_axiom`: `if (0)` for `d->origin == ORIGIN_PROGRAM` | check axiom.lang: want exit 1 and REFUSE_AXIOM, got exit 0 | KILLED |
| M11 the law type test dropped | check.c:2723 `check_axiom`: `if (0)` for `!law_type(c, tv)` | check axiom-type.lang: want REFUSE_AXIOM_TYPE, got REFUSE_AXIOM | KILLED |

Score: 11 of 11 killed, and M4strict killed. No mutant is equivalent.
Notes on the predictions of the kickoff:

- M1 and M3: a check failure of protocol.lang stops `langc eval` for each
  row of the file. The examples gate is the first killer.
- M3: the kickoff expected PATH_END. The swap case is the application
  `bidMaxSwap y x r`, and the type of that application is a Path with the
  two ends exchanged. Thus the type test of the fold fails with
  TYPE_MISMATCH before a boundary test. PATH_END is the code for a path
  abstraction with a wrong end (test/check/bag-swap-boundary.lang).
- M2: `lemma11F2` survives. Index 3 of the sorted stamps 0, 0, 10, 11, 13
  is 11, and 11 is in [10, 13].
- M5: the kickoff expected the code 5 row. `procStamp` survives: its stamp
  words fail the stamp test in the two forms. `procAccept` and `lateCut`
  fail because their stamp words are `hash cm (toU256 t)`.
- M9: the odd row, `lemma11F1` and `lemma11F2` survive. The middle of an
  odd count of stamps is the same in the two orders. M9 keeps law 3 true
  (the down order also commutes), thus the check row `insSwapAt` does not
  kill it. The even and missing rows kill it.
- M11: the origin test refuses each program axiom, thus axiom-type.lang
  is refused with the other code. The kill is the code only. At S6 no row
  checked a domain axiom with a type that is not a law (the domain rows of
  test/fronttool.c were accepted laws). S7 step 7.1 (bbe668d) adds the row
  `axiom notLaw : Nat`, and that row kills M11 (`## S7`).

## S7

Slice S7 (the docs and the final gate) is complete. It has five steps.
The rulings R61 to R71 are RULED 2026-10-18, defaults (a).

- 7.1 (bbe668d): the M11 row (R61 (a)). `test/fronttool.c` gets the row
  `domain("axiom notLaw : Nat", "REFUSE_AXIOM_TYPE")`. M11 changes
  `!law_type(c, tv)` to `0` in `check_axiom` (`src/front/check.c:2723`).
  With M11, the only FAIL line is `FAIL domain: wanted REFUSE_AXIOM_TYPE,
  got ok: axiom notLaw : Nat`. Thus the row kills M11. After the restore,
  fronttool prints `domain regressions: passed`.
- 7.2 (b0b1b37): the source fixes (R66 (a), R67 (a)). `docs/PLAN.md`
  section 8 gets O51 to O60, and :136 names `bidMax` and `insNat`.
  `formers/FORMERS.md` (12 matrix cells and the S6 row) and
  `formers/tcc-evm-contract.md` (:34, :89, :90 and the S6 section) agree
  with the source.
- 7.3 (cf0f324): `SPEC.md` sections 1 to 8 (R62 (a), R70 (a)).
- 7.4 (f05d6f4): `SPEC.md` sections 9 and 10 (R63 (a), R64 (a), R66 (a))
  and `probe/CAPABILITY.md` (R68 (a)).
- 7.5: the top sections of this file and this section (R65 (a)), the
  header of `docs/VALIDATION.md` (R69 (a)), `README.md` (R71 (a)), the M3
  row and the status line of `SPEC.md` section 10, and the final gate.

The USER commits of 7.3 and 7.4 add rows to the staged trees. cf0f324:
`SPEC.md` sections 1, 2, 3 and 7 (for example the REFUSE_AXIOM_TYPE row
and 10 views with `byUserOrProposer`). f05d6f4: `probe/CAPABILITY.md`
(the `eval` and `run` syntax, `natSub` saturates at zero, and the Sigma eta
regression `test/fronttool.c:128`).

The S7 final gate `make -B check` is GREEN: examples 28, parse refusals
15, check refusals 85, evals 84, runs 6, run refusals 10, asmtool 8 of 8,
builds 3, lower refusals 14, checks 31, dispatch 20, evm steps 43,
precompile 32 steps and 0 failures, deposit 25 assertions, deposit EVM 41
calls and 0 failures, runtime 17119 bytes, mock 13 rows, diff 16 rows,
emit 24, all regressions passed. These are the S6 counts: the 7.1 row
prints in the line `domain regressions: passed`, and 7.2 to 7.5 change
docs only.

Stale cites with no S7 owner (recorded, not fixed):

- `docs/host/README.md:189` (more than 16 event fields):
  `src/front/check.c:2413-2414` uses ENTRY_PARAMS_MAX 32
  (`src/front/check.h:8`).
- `docs/host/README.md:245` (`Nat` and `U256` mul and div): S2 removed
  them.
- `docs/host/CAPABILITY.md:26` and `test/diff.sh:11` (5 call scripts): the
  gate runs 6, and diff gives 16 rows (`probe/CAPABILITY.md` section 1).
- `formers/FORMERS.md:300` and the FORMERS cells that cite the kit paths
  `hosts/tcc-evm-contract/...`.
- The `## S5` record counts 9 views (O8). `SPEC.md:244-247` counts 10,
  with `byUserOrProposer` (the USER row of cf0f324).
- The S6 trusted base of `docs/VALIDATION.md` gives the M11 hole as open
  (USER to rule). That is the state at S6. The 7.1 row closes it.

Open at the S7 close: the list of `## Remaining work` above
(`SPEC.md:316-332`).

## S8

Slice S8 (the review fixes of S7) is closed (8.6). The rulings R72 to R81
are RULED 2026-10-18.

- 8.1 (80d58b1): docs (R72, R76, R79, R80, R81).
- 8.2 (5328a25): `rule_hcomp` uses the shared system helpers (R77 (a)).
- 8.3: Env `self` (R74 (a), O46, O74). `self : Addr` is the third field
  of Env (`src/front/contract.c:7`). The lowering gives ADDRESS (0x30)
  (`src/lower.c:625`, `src/ir.h:56-58`, `src/evm.c:249`). `def vault` is
  gone from `examples/deposit.lang`, and `pull` credits `self env`.
  `langc run` gives `self` = 0x0000000000000000000000007265636569766572
  in make_env (`src/front/check.c:3052-3066`). That value is the receiver
  of `test/evmchain.sh:8` and the old `vault` value. The call form stays
  `NOW CALLER NAME ARGS`. The new evm row (`test/self.lang`,
  `test/self.script`, `test/evm.sh:131-132`) gives that receiver, not
  the caller 0x..bb. `src/front/check.c` (make_env) and
  `src/front/contract.c` (Env) now differ from the tcc kits of
  lang-template.

The 8.3 gate `make -B check` is GREEN: evm steps 43 -> 44, emit 24, diff
16 rows and 0 failures (no content change), deposit EVM runtime 17119 ->
17103 bytes (R78 ceiling 18143, EIP-170 24576), every other count at the
S7 baseline.

M14 changed the repo file, then `make -B build`. No Makefile target runs
the evm rows alone, thus the run was `sh test/evm.sh`. After the restore,
`make -B` and `sh test/evm.sh` gave evm 44, and `git diff` against the
index was empty.

| Mutant | Edit | Killer | Verdict |
|---|---|---|---|
| M14 `self` lowered to CALLER | evm.c:249 `IR_EXPR_ENV`: `EVM_OP_CALLER` in place of `EVM_OP_ADDRESS` for IR_ENV_SELF | evm `test/self.script:1 here`: want out 7265636569766572, got out bb | KILLED |

Current references reconciled in the staged 8.3 review (line counts kept):

- `self env` replaces the open `vault` deployment rule: `docs/STATUS.md:37`,
  `SPEC.md:234-235`, `SPEC.md:416-417`, `docs/host/README.md:314`,
  `formers/tcc-evm-contract.md:92`.
- Current runtime 17103 (7473 bytes free): `SPEC.md:247`, :326-327, :420,
  `docs/host/README.md:316`, `probe/CAPABILITY.md:115-116` and the formers row.
- `self` in the operation table and entry body: `SPEC.md:183`, :230 and
  `docs/host/README.md:240`. O35 and O57 now show their S8 closures above.
- Kept as records: `docs/PLAN.md:305-311` (O6, S5), `docs/STATUS.md:278`
  (S5) and :396 (S6).

8.4: `settle` refuses a caller with no deposit (R73 (a), O73). The check
is the first guard (`examples/deposit.lang:261-262`). `slash` and
`withdraw` call `clearUser`, which sets `amount` to 0u (:226), thus the
check reads `amount` only. No deployment of `test/deposit.sh` had an
auction with an end slot that can pass, thus the USER ruled a setup row
(`regAuction 9u 1 1`) and a reject row (`test/deposit.sh:135-137`). The
eval row `testSettleNoDeposit` is `test/deposit.lang:71-73`.

The 8.4 gate `make -B check` is GREEN: deposit 25 -> 26 assertions,
deposit EVM 41 -> 43 calls and 0 failures, runtime 17103 -> 17138 bytes
(R78 ceiling 18143, 1005 bytes under; EIP-170 24576, 7438 bytes free),
diff 16 rows and 0 failures (no content change), every other count at
the 8.3 baseline. At the 8.4 close, runtime 17103 and counts 25 and 41 at
`SPEC.md:247`, :326-327, :420, `docs/host/README.md:316`,
`probe/CAPABILITY.md:115-116` and `formers/tcc-evm-contract.md:92`
were stale; 8.6 reconciles them to the final 8.5 totals.

M12 changed the repo file, then `make -B build` and `sh test/deposit.sh`.
After the restore, `make -B` and `sh test/deposit.sh` gave 26 assertions
and 43 calls, and `git diff` against the index was empty.

| Mutant | Edit | Killer | Verdict |
|---|---|---|---|
| M12 no deposit check in `settle` | deposit.lang:261 `settle`: `flagIf flagNo` (no `amount` check) | deposit `testSettleNoDeposit` 0; deposit EVM `chain revert 20 "$other" settle`: want revert, got ok | KILLED |

8.5: `eligible` takes cm and requires input 1 = cm, and `slashUsr` passes
cm1 and cm2 (R75 (a), O75; `examples/deposit.lang:138-141`, :209-210).
The slash key is re-based (:118-120, :131): IC0' = IC0 + 3 IC2 = 46 G1
and IC2' = (0, 0), the point at infinity. `verify` accepts that key, thus
R75 stays (a). The fixture proof is valid for the inputs (2, x) for each
x. In `test/deposit.sh` the slashUsr ok row gives (2, 5) and (2, 6)
(:141), and a reject row gives input 1 = 3 for cm1 = 5 (:139-140). The
run script row (`test/run/deposit.script:29`, cm1 = cm2) and
`test/run/deposit.out` did not change.

The 8.5 gate `make -B check` is GREEN: deposit EVM 43 -> 44 calls and 0
failures, runtime 17138 -> 17069 bytes (R78 ceiling 18143, 1074 bytes
under; EIP-170 24576, 7507 bytes free), diff 16 rows and 0 failures (no
content change), every other count at the 8.4 baseline. The lines listed
in the 8.4 record above are reconciled in 8.6 to 17069, 26 and 44.

M13 changed the repo file, then `make -B build` and `sh test/deposit.sh`.
The mutant runtime was 17004. After the restore, `make -B` and `sh
test/deposit.sh` gave 26 assertions and 44 calls, and `git diff` against
the index was empty.

| Mutant | Edit | Killer | Verdict |
|---|---|---|---|
| M13 no cm binding in `eligible` | deposit.lang:141 `eligible`: no `u256Eq i1 cm` compare | deposit EVM `chain revert 20 "$other" slashUsr` (input 1 = 3, cm1 = 5): want revert, got ok, and storage changed | KILLED |

8.6 (the S8 close, docs only): each item of `SPEC.md:316-332` at its S8
state, the item rows O7, O35, O44, O50, O57 and O58 of `SPEC.md` section
9, the `SPEC.md` section 10 status line, the current runtime and count
lines listed in 8.4 and 8.5, and the `docs/VALIDATION.md` header for the S8
final gate. The commits of 8.3 to 8.5 are e08a5ef, 2d00376 and b68af34.
These lines now give 17069 bytes, 26 assertions or 44 calls:
`SPEC.md:247`, :326-327, :420, `docs/host/README.md:316`, :318,
`probe/CAPABILITY.md:115-119`, `formers/tcc-evm-contract.md:92`, and
`docs/STATUS.md:27` and :41. Line counts stay, except at the end of
`SPEC.md` (section 10, +6 lines) and here: no cite moved. Kept as records: `docs/PLAN.md:500` (O47), :503 (the O48
history), :546 (S6), :589 (O78) and the gate lines of `## S5` to `## S7`.
The open list after S8 is the `SPEC.md` section 10 status line.

The S8 final gate `make -B check` is GREEN, every count at the 8.5
baseline: examples 28, parse refusals 15, check refusals 85, evals 84,
runs 6, run refusals 10, asmtool 8, builds 3, lower refusals 14, checks
31, dispatch 20, evm steps 44, precompile 32 steps and 0 failures,
deposit 26 assertions, deposit EVM 44 calls and 0 failures, runtime 17069
bytes (R78 ceiling 18143, 1074 bytes under; EIP-170 24576, 7507 bytes
free), mock 13 rows, diff 16 rows and 0 failures, emit 24, and every
regressions line passed. The S8 mutants M12 to M14 are KILLED (above).

## S9

Slice S9 (the open list after S8) has the rulings R82 to R90, RULED
2026-10-18 with defaults (a) (`docs/PLAN.md` O82 to O90).

9.1 (docs only; R83, R88, R90 and the records of R87 (a)):

- `docs/PLAN.md`: the S9 row after the S8 row of section 7 (N = 3 lines,
  :269-271). The O7 and O44 rows are closed, not taken (R88; :314-315,
  :477-478, line counts kept). The rows O82 to O90 follow O81 (:603-643).
- The cite move (+3, records included): `SPEC.md` 88 (15 named cites, 72
  bare cites in section 9 and at :148, the range end at :447),
  `probe/CAPABILITY.md` 4, `docs/STATUS.md` 5 (:550, :612-613),
  `docs/host/README.md` 1. A scratchpad script compared the cited text at
  2aa2cf9 with the work tree: 120 cite pairs, 0 differences.
- `SPEC.md`: the range at :304 ends at :643; :316 and :318 (O7 and O44
  closed, R88); :349 and :412-413 (the O7 and O44 rows); :424-425 and
  :437-438 (O50 and O58 stay PARTIAL, R87 (a): circom and snarkjs, no
  gnark; a prover out of the repo, its output kept as constants); :464
  (`src/lower.c`, R84). :466 stays. Line counts kept.
- `docs/VALIDATION.md:98-103`: the S5 facts in the past tense and the S8
  facts (R75 in 8.5, R73 in 8.4). Line count kept.

The 9.1 gate `make -B check` is GREEN, every count at the S8 baseline:
examples 28, parse refusals 15, check refusals 85, evals 84, runs 6, run
refusals 10, asmtool 8, builds 3, lower refusals 14, checks 31, dispatch
20, evm steps 44, precompile 32 steps and 0 failures, deposit 26
assertions, deposit EVM 44 calls and 0 failures, runtime 17069 bytes
(R89, 7507 bytes free), mock 13 rows, diff 16 rows and 0 failures, emit
24, and every regressions line passed. No mutant (docs only).

9.3 (R86 (a), a Nat-indexed family):

- `domain/domain.lang:217-220`: the kit family `family Lot (size : Nat)
  := makeLot (lotPrice : Nat)` and its comment, at the end of the file,
  thus no cite of the file moves.
- Probe 1: the checker compares the index. `langc check` of `def bad :
  Lot 3 := makeLot 4 250` gives `TYPE_MISMATCH` ("expected a term of type
  Lot 3, found a term of type Lot 4"). The compare site is the argument
  loop of `conv_loop` (`src/front/eval.c:2470-2481`).
- Probe 2: no. `check` refuses a state field of the type `Lot 3`
  (`REFUSE_STATE`), and `src/lower.c:195-196` takes only the env, state
  and out families of an entry. F15 stays PARTIAL for EVM output. Builds
  stay 3.
- `examples/lot.lang` (from the kit `examples/inventory.lang:9-10`), the
  eval row `lot lotCost => 250` and the check refusal row `lot-index.lang
  TYPE_MISMATCH`. The kit fronttool row (kit `test/fronttool.c:36-39`) is
  at `test/fronttool.c:100-103` since beae7ec: no edit.
- Docs: `formers/FORMERS.md:300` (F15 at its 9.3 state), `docs/PLAN.md`
  O80 and O86 (line counts kept). `SPEC.md` and `docs/VALIDATION.md` do
  not change.
- Mutant M15 at `src/front/eval.c:2474`: `if (a->argc == 0 || val_is(a,
  OP_FAMILY)) return 1;` (two family types with the same head are equal,
  thus the checker ignores the index). The check refusal row
  `lot-index.lang` kills it: "want exit 1 and TYPE_MISMATCH, got exit 0".
  Restored: `git diff --stat src/` is empty, as before the mutant.

The 9.3 gate `make -B check` is GREEN: examples 29, check refusals 86,
evals 85, and each other count at the 9.1 baseline: parse refusals 15,
runs 6, run refusals 10, asmtool 8, builds 3, lower refusals 14, checks
31, dispatch 20, evm steps 44, precompile 32 steps and 0 failures,
deposit 26 assertions, deposit EVM 44 calls and 0 failures, runtime 17069
bytes (R89, no change), mock 13 rows, diff 16 rows and 0 failures, emit
24, and every regressions line passed.

9.4 (R84 (a), the run-time Bag lowering):

- Probe (session s1): a closed Bag sum builds at 9.3 (the evaluator
  reduces the Bag fold and the sum to a constant). A Bag with the entry
  argument as an element is refused (`K4c does not lower fold at a word
  position`): the Bag fold did reduce, and the stuck value is the List
  fold of the sum over the flagIf tree that `insNat` gives. A neutral Bag
  needs a run-time List value, out of the K4c scope.
- Finding: the erasure `Bag A` to `List A` (bagNil to nil, bagCons to
  cons, the path cases dropped) is the evaluator's reduction of a Bag
  fold over its constructors (`src/front/eval.c:915-928`), before
  `src/lower.c` runs. The gap is a List fold over a local List value.
- `src/lower.c:533-591`: `lower_local_fold` after `lower_fold`. nil gives
  Z; cons X REST gives F X (fold F Z REST) over two locals, the step is F
  at the levels of the locals; flagIf C A B gives an IF that sets one
  local; when the element or the fold of the rest can trap, F must trap
  on a trapped element and on a trapped accumulator (the `lower_fold`
  rule); any other List value is refused. One dispatch line (:837) after
  the field fold. The insert moves three cites: `SPEC.md:93`
  (`src/lower.c:835`), `formers/FORMERS.md:300` (:1005, :1036).
- `test/bag.lang` and `test/bag.script`: the entry `sumBag 5` sorts the
  Bag of 5, 2 and 3 and writes 10 to `total`. The build row `bag exit`
  in `test/build.sh` (checks 31 to 32) and the chain row in `test/evm.sh`
  (evm steps 44 to 45). Examples stay 29, builds 3, lower refusals 14 (no
  row of `test/lower/expect.txt` lowers).
- Docs: `formers/FORMERS.md:305` (the S6 cell: EVM output since 9.4),
  `docs/PLAN.md` O37 and O84. `SPEC.md` (466 lines) and
  `docs/VALIDATION.md` (166 lines) keep their counts.
- Mutant M16 at the last line of `lower_local_fold`: `return
  lower_expr(l, b, step);` becomes `return ir_local(l, acc);` (the cons
  case drops the head: the sum is 0). The evm row kills it: `FAIL evm
  test/bag.script:1 sumBag: want ok, got ok` (a storage diff), `evm
  gate: 1 failures`. Restored: `git diff --stat src/` is as before the
  mutant (61 insertions in `src/lower.c`).

The 9.4 gate `make -B check` is GREEN: checks 32, evm steps 45, and each
other count at the 9.3 baseline: examples 29, parse refusals 15, check
refusals 86, evals 85, runs 6, run refusals 10, asmtool 8, builds 3,
lower refusals 14, dispatch 20, precompile 32 steps and 0 failures,
deposit 26 assertions, deposit EVM 44 calls and 0 failures, runtime 17069
bytes (R89, no change), mock 13 rows, diff 16 rows and 0 failures, emit
24, and every regressions line passed.

9.5 (the S9 close, docs only; the USER committed 9.4 as 24c8090):

- `SPEC.md`: section 9 at the S9 state (:97 the F15 row; :305 the status
  line; :316 the list header; :321-322 O50 and O58, the circuit stays
  open, R87 (a); :326-327 and :420 O48 at 17069 bytes, R89; :328-330 the
  `od -v` unit, closed by the kit commit 0ead0b7, R85; :331-332 F15 DONE
  in `check` and `eval`, PARTIAL for EVM output) and section 10 (:456-466
  the status line: S9 closed, the open list after S9). 466 lines kept.
- `docs/VALIDATION.md` :3-4 and :8-13: the S9 final gate header,
  `build/langc` at 24c8090 (the 9.4 commit; 9.5 is docs only), the 9.4
  counts, M15 and M16. 166 lines kept.
- `docs/PLAN.md` O79 (:593, :597) and O85 (:622): the kit commit 0ead0b7
  ("Preserve repeated bytes in test hex encoding", R85, step 9.2k). Line
  counts kept.
- This file: :5-7, :13-14, :27-30, :34-43 (the summary at the S9 state);
  `docs/host/README.md:316` and `formers/tcc-evm-contract.md:92` (17069 bytes at the S9 close);
  `formers/tcc-evm-contract.md:114,116` (the local Bag folds of step 9.4).
  No cite moved. No code change. The kit does not change.
- Open after S9 (recorded, not built): the eligibility circuit of the
  paper and a prover (O50, O58 PARTIAL, R87 (a)); the EIP-170 budget
  (O48: 17069 bytes, 7507 free, R89); F15 EVM output (no Bag or family
  state field, R80). The paper text is not local.

The S9 final gate `make -B check` is GREEN at the 9.4 baseline: examples
29, parse refusals 15, check refusals 86, evals 85, runs 6, run refusals
10, asmtool 8, builds 3, lower refusals 14, checks 32, dispatch 20, evm
steps 45, precompile 32 steps and 0 failures, deposit 26 assertions,
deposit EVM 44 calls and 0 failures, runtime 17069 bytes (R89), mock 13
rows, diff 16 rows and 0 failures, emit 24, and every regressions line
passed. S9 is closed.

## S10

10.1 (the docs: R92, the R93 (a) records, R97 (a); R91 to R98 RULED
2026-10-19, defaults (a)):

- `docs/PLAN.md`: the S10 row of section 7 at :272-274 (3 lines, 648 to
  651) and the rows O91 to O98 at :652-708 (708 lines). The S10 row
  moves each cite of a `docs/PLAN.md` line at or above the old line 272
  by 3 lines: 22 explicit `docs/PLAN.md:NNN` cites on 21 lines:
  `SPEC.md` 14, this file :550 and :612, `docs/host/README.md:245`,
  and `probe/CAPABILITY.md:47`, :116, :132, :155; 72 bare `SPEC.md`
  section 9 cites (:302-443), 4 continuation cites (`SPEC.md:148`,
  this file :612-613) and the two ranges (`SPEC.md:304`, section 8
  is :278-701; `SPEC.md:447`, section 7 is :232-276). The `## S8` records
  here (:550, :612-613) move: a PLAN cite is live (the 9.1 precedent).
- `SPEC.md`: :322 adds R93 (a) to the O50, O58 row; :466 says the repo
  is public since 2026-10-18 (R97 (a): the USER confirmed it with the
  ruling). 466 lines kept.
- O93 holds the S10 survey facts of the circuit (the open relation, root
  input, Merkle depth, hash, scalar encoding and setup provenance;
  circom and snarkjs on the machine, no gnark, no BN254 arithmetic in
  the repo). Deviation from the R93 (a) text: the facts are in O93, not
  appended to O87, so that the cites to O88 to O90 do not move a second
  time.
- No code change. The deposit runtime is 17069 bytes (R96). The kit does
  not change. `docs/VALIDATION.md` does not change.

The 10.1 gate `make -B check` is GREEN at the S9 baseline: examples 29,
parse refusals 15, check refusals 86, evals 85, runs 6, run refusals 10,
asmtool 8, builds 3, lower refusals 14, checks 32, dispatch 20, evm steps
45, precompile 32 steps and 0 failures, deposit 26 assertions, deposit
EVM 44 calls and 0 failures, runtime 17069 bytes (R96), mock 13 rows,
diff 16 rows and 0 failures, emit 24, and every regressions line passed.

The staged review adds `probe/*.md` to O98's audit scope after finding
four PLAN citations there that also need the 3-line move.

10.2 (R95 (a) part 1: the checker case and the lowering form of the
`Lot n` state field; 2026-10-19, sessions s3 and s3b):

- `git diff --stat` against 58c13ca: 6 files, 125 insertions, 10
  deletions: `src/front/check.c` 58, `src/lower.c` 45, `test/build.sh`
  21, `test/check/expect.txt` 1, `test/check/state-bag-field.lang` 2
  (new), `test/lot-state.lang` 8 (new).
- The checker case, `src/front/check.c` (3202 to 3254 lines): the new
  helpers `closed_index` (:2657-2671) and `state_family` (:2673-2707)
  before `check_state`. A state field may have the type of a family with
  one constructor, no interval variable, closed indices (a literal or a
  canonical value with closed arguments) and word fields (Nat, Flag,
  U256, Addr). A recursive, dependent or path field refuses the family.
  The REFUSE_STATE site moves from :2681 to :2733 and its text ends "a
  Map K V, or a family with one constructor, closed indices and word
  fields (R95)".
- The lowering form, `src/lower.c` (1228 to 1259 lines): `Form` gains
  FORM_FAMILY (:29) and `form_of` (:35) returns it for a family type;
  the new `field_width` (:153-157) gives k slots to a family field with
  k constructor fields; the new `lower_family_field` (:159-172) stores
  the k constructor fields of the `init` value at k consecutive slots
  and drops the index arguments; `lower_state` (:175-188) walks a
  running slot. The K4a refusal (:1168-1171, old :1137-1140) admits
  families through `form_of`; its text adds "and families of words
  (R95)". The projection load and the constructor store are step 10.3.
- Rows: `test/check/state-bag-field.lang` (`held : Bag Nat`, a recursive
  family with three constructors) with the row `state-bag-field.lang
  REFUSE_STATE` in `test/check/expect.txt` (86 to 87 rows);
  `test/lot-state.lang` (`lots : Lot 3` between `owner` and `count`,
  `init := makeState 0x..aa (makeLot 3 250) 7`); one `expect` in
  `test/build.sh` ("lot-state build": exit 0 and the storage rows slot 0
  = 0x..aa, slot 1 = 250, slot 2 = 7, no slot holds the index 3). Counts:
  checks 32 to 33, builds stay 3.
- Deviation from the O95 text: `test/lot-state.lang` has no entry yet.
  `langc build` lowers every entry, and the load and the store of a
  family field are step 10.3, so an entry that touches `lots` would turn
  the build row into a refusal. Step 10.3 adds the two entries.
- The O95 cites in `docs/PLAN.md` :686-688 move in place (`check.c:2681`
  to :2733; `lower.c:27` to :29, :33 to :35, :151-157 to :175-188,
  :1137-1140 to :1168-1171). The other cites of `check.c` and `lower.c`
  lines (`SPEC.md`, `formers/`, this file, `docs/host/README.md`,
  `probe/CAPABILITY.md`) wait for the 10.4 audit (R98); the shift is +52
  for a `check.c` line at or above the old 2657, and for `lower.c` +31
  at or above the old 1120, +24 at or above the old 150, +2 at or above
  the old 27.
- Mutant M17 (the checker accepts any family: `state_family` returns
  `val_is(t, OP_FAMILY)` first): `langc check
  test/check/state-bag-field.lang` exits 0, so the row
  `state-bag-field.lang REFUSE_STATE` FAILS. KILLED by
  `state-bag-field.lang`. The line is restored; the restored row exits 1
  with REFUSE_STATE. The deposit runtime is 17069 bytes (R96). `SPEC.md`
  and `docs/VALIDATION.md` do not change.

The 10.2 gate `make -B check` is GREEN: examples 29, parse refusals 15,
check refusals 87, evals 85, runs 6, run refusals 10, asmtool 8, builds
3, lower refusals 14, checks 33, dispatch 20, evm steps 45, precompile
32 steps and 0 failures, deposit 26 assertions, deposit EVM 44 calls and
0 failures, runtime 17069 bytes (R96), mock 13 rows, diff 16 rows and 0
failures, emit 24, and every regressions line passed.

10.2 staged review (2026-10-19):

- HIGH: the initializer advanced by each family field's width, while entry
  reads and writes still used field numbers as storage slots. For the
  regression with `guard`, `held : Env` and `counter`, the counter is at
  slot 4. The old `bump` read and overwrote slot 2 (the stored caller)
  instead, leaving the counter at 7. Lists and Maps after a family also
  used the wrong length slot and hash seed.
- `src/lower.c:453` adds `slot_of`, used by word reads and writes, List
  folds, List writes and Map reads and writes. Family projections and
  constructor writes remain the work of step 10.3.
- Three C regressions in `test/lowertool.c` cover a stored Env, an empty
  family and an indexed two-field family. They check deployed storage and
  runtime addresses for views, word updates, List folds, List appends,
  List clears and Map access. All three fail against the original staged
  lowering and pass with the fix. No CI checks were weakened.
- The reviewed `make -B check` passes in an isolated copy of the complete
  worktree, including the new C regressions. Deposit runtime remains
  17069 bytes (R96). The O95 lowering refusal citation moves by 8 lines.

10.3 (R95 (a) part 2: the projection load, the constructor store and the
`Lot n` entries; 2026-10-19, sessions s4 and s4b):

- The USER rows of 10.2 (the 10.2 staged review above) are the base:
  `slot_of` (`src/lower.c:453`, the slot helper that the s4 prompt named
  as its 2.1) routed through every slot site, three `test/lowertool.c`
  layout regressions, the O95 cite of `docs/PLAN.md` moved by 8.
- `git diff --stat` against 167f580: 11 files, 111 insertions, 24
  deletions: `src/lower.c` 32, `test/evmchain.sh` 55, `test/lot-state.lang`
  13, `test/lot-state.script` 6 (new), `test/evm.sh` 5, `test/diff.sh` 4,
  `SPEC.md` 8, `docs/host/README.md` 4, this file 4, `docs/PLAN.md` 2,
  `formers/FORMERS.md` 2.
- The projection load, `src/lower.c` (1267 to 1299 lines): `family_at`
  (:460-463) after `slot_of`; the OP_PROJ arm of `lower_expr` (:863-866)
  lowers `lotPrice (lots s)`, a projection of the one constructor over a
  read of a FORM_FAMILY state field, to an SLOAD of `slot_of(l, f) +
  v->field`.
- The constructor store: `is_family_ctor` (:956-961) and
  `lower_family_write` (:963-977) before `lower_state_write`; the
  FORM_FAMILY arm (:996-997) of `lower_state_write` stores the k
  constructor fields of the value at `slot_of(i) + j` and drops the index
  arguments (the form of `lower_family_field`); a value that is not the
  constructor falls to the "does not lower a write of the field" refusal.
  The K4a refusal block moved from :1176-1179 to :1208-1211.
- The entries, `test/lot-state.lang` (8 to 19 lines): the view `lotCost`
  (`lotPrice (lots s)`) and the entry `restock price` (`makeLot 3 price`,
  owner and count kept). The script `test/lot-state.script` (new, 3
  steps: lotCost 250, restock 300, lotCost 300) and the evm row `chain
  test/lot-state.lang test/lot-state.script` in `test/evm.sh` (:131-133;
  139 to 142 lines). The script is at `test/` with the same-state, self
  and bag scripts, not at `test/run/` as O95 says.
- The want fix (s4b), `test/evmchain.sh` (181 to 216 lines): the `want`
  helper put field i of the state text at slot i and gave each argument
  to `bc` as one word, so the family value `(makeLot 3 300)` was a
  "Parse error: bad expression" and the three lot-state steps FAILED on
  the storage rows. Now `tokens` (:93-104) is the top-level tokenizer,
  `family` (:112-131) puts the k constructor fields of a `(CTOR INDEX...
  FIELD...)` value at k consecutive slots and drops the index arguments,
  with the index count p and the width k read from the one-constructor
  `family` line of the program or the domain, and `want STATE PROG`
  (:137-151) advances the slot by k for a family field and by one
  otherwise. The three callers pass the program: `test/evm.sh:66`,
  `test/diff.sh:68` and `:179`. The other 45 chain steps and the 16 diff
  rows do not change.
- The F15 docs lines: `SPEC.md` :97 (R95 in the refs), :332 and
  :464-465; `formers/FORMERS.md:300` (the sealed-lang cell: check.c:2733,
  lower.c:155-189, :863-866, :957-977); this file :14 and :43;
  `docs/host/README.md:89` (the field rule gains the family clause; the
  Map clause is still missing there: step 10.4) and :259;
  `docs/PLAN.md:688` (the O95 cite :1176-1179 to :1208-1211).
- Cites: the 10.3 insertions in `src/lower.c` shift a cite by +5 at or
  above the old 459, by +4 at or above the old 859 and by +32 at or above
  the old 1176, on top of the 10.2 shifts and the +8 of `slot_of`. The
  `want` helper of `test/evmchain.sh` moved from :93 to :137; the cites
  `evmchain.sh:8` and `evmchain.sh:14` did not move. The whole-repo audit
  is step 10.4 (R98).
- Mutant M18 (the constructor store at `slot_of(l, field) + j + 1`,
  `src/lower.c:972`): the evm row `test/lot-state.script:2` (restock)
  FAILS on the storage rows (slot 1 keeps 250, slot 2 gets 300) and step
  3 reads 250. KILLED by `test/lot-state.script:2`. The line is restored;
  the restored chain gate is 48 steps and 0 failures, and the worktree
  equals the index. The deposit runtime is 17069 bytes (R96).
  `docs/VALIDATION.md` does not change.

The 10.3 gate `make -B check` is GREEN: examples 29, parse refusals 15,
check refusals 87, evals 85, runs 6, run refusals 10, asmtool 8, builds
3, lower refusals 14, checks 33, dispatch 20, evm steps 48, precompile
32 steps and 0 failures, deposit 26 assertions, deposit EVM 44 calls and
0 failures, runtime 17069 bytes (R96), mock 13 rows, diff 16 rows and 0
failures, emit 24, and every regressions line passed.

10.4 (R98 (a): the whole-repo cite audit and the README:89 Map clause;
2026-10-19, sessions s5 and s5b):

- The method: the repo has no cite tool. A scratchpad sh extractor took
  every `FILE:line` and `FILE:line-line` cite in `SPEC.md`, `README.md`,
  `docs/*.md`, `docs/host/README.md`, `formers/*.md`, `probe/*.md`,
  `src/*.c` and `test/*.c` and compared the cited line now with the
  cited line at 05c0582 (the S9 close, the last commit before any S10
  code change). This explicit-cite pass missed a bare continuation cite,
  fixed in the staged review below. The S10 drift is (`src/front/check.c` +52 at or above the old 2657;
  `src/lower.c` by content). The C test tools and `src/*.c` cite no
  `check.c` or `lower.c` line (`check.c:3105` cites `test/evmchain.sh:8`,
  fresh). The foreign anchor `check.c:908` of `formers/FORMERS.md:299`
  is skipped (O98).
- Original pass: 22 moved cites on 20 lines, `file:line old -> new`:
  `SPEC.md:67` check.c:2746 -> 2798 (REFUSE_DATA);
  `SPEC.md:68` check.c:2723-2724 -> 2775-2776 (REFUSE_AXIOM_TYPE);
  `SPEC.md:69` check.c:2726 -> 2778 (REFUSE_AXIOM);
  `SPEC.md:70` check.c:2750 -> 2802 (REFUSE_REC; :1817 stays);
  `SPEC.md:72` check.c:2666-2684 -> 2718-2736 (the REFUSE_STATE block);
  `SPEC.md:93` lower.c:835 -> 883 (the field fold line of `lower_expr`)
  and :404 -> :435 (the K4c refusal);
  `SPEC.md:243` lower.c:502-520 -> 546-564 (`lower_fold` to the trapped
  accumulator refusal);
  `SPEC.md:247` lower.c:208-223 -> 239-254 (the entry role lines);
  `SPEC.md:262` lower.c:391 -> 422 (VAL_AXIOM) and :404 -> :435;
  `docs/PLAN.md:618` lower.c:533-591 -> 577-635 (`lower_local_fold`, the
  comment to the closing brace);
  `probe/CAPABILITY.md:75` check.c:2750 -> 2802;
  `probe/CAPABILITY.md:89` lower.c:411 -> 442 (the op table);
  `probe/CAPABILITY.md:130` check.c:2746 -> 2798;
  `probe/CAPABILITY.md:165` check.c:2792 -> 2844 (`report_axioms`);
  `probe/CAPABILITY.md:170` check.c:2726 -> 2778;
  `formers/FORMERS.md:300` lower.c:155-189 -> 155-188 (the block ends at
  the brace);
  `formers/FORMERS.md:305` lower.c:533-591 -> 577-635 and lower.c:391
  -> 422;
  `formers/tcc-evm-contract.md:114` lower.c:533-591 -> 577-635;
  `formers/tcc-evm-contract.md:115` lower.c:391 -> 422.
- Fresh, not moved (verified): `SPEC.md:71` (2177), :73 (1573-1575),
  :74 (2413-2425), :75 (2585-2610), the BUILTINS rows :132-182
  (check.c:99-166), `probe/CAPABILITY.md:84` (143-144); the 10.2 and
  10.3 cites `docs/PLAN.md:686-688` (check.c:2733, lower.c:29, :35,
  :175-188, :1208-1211) and `formers/FORMERS.md:300` (check.c:2733,
  lower.c:863-866, :957-977); the shell cites evmchain.sh:8
  (check.c:3105, `docs/host/README.md:125`), evmchain.sh:14 and
  diff.sh:29 (`docs/PLAN.md:597`) and diff.sh:192-198
  (`probe/CAPABILITY.md:57`); the 60 bare section 9 `docs/PLAN.md` cites
  of `SPEC.md` (each points at its O row; 10.1 moved them). The other
  cite targets (host README 328, FORMERS 306, tcc-evm-contract 131,
  CAPABILITY 188, `docs/host/CAPABILITY.md` 52, domain.lang 220, check.h
  33, contract.c 12) kept their line counts since 05c0582 with in-place
  S10 edits only; this file grows at its end only; `SPEC.md` kept 466.
- `docs/host/README.md:89`: the field rule gains the Map clause between
  the List clause and the family clause: "or a `Map K V` with word types
  `K` and `V` at the top level (one slot: the hash seed; the value of
  key k is at keccak256(k . slot), see `test/evmchain.sh` `map` and
  `src/lower.c` `lower_field`)". 328 lines kept.
- Files (6, 20 insertions, 20 deletions against 419d5a6): `SPEC.md` 18,
  `docs/PLAN.md` 2, `docs/host/README.md` 2, `formers/FORMERS.md` 4,
  `formers/tcc-evm-contract.md` 4, `probe/CAPABILITY.md` 10. Every line
  count kept (SPEC 466, PLAN 708, host README 328, CAPABILITY 188,
  FORMERS 306, tcc-evm-contract 131). No code change. The deposit
  runtime is 17069 bytes (R96).

The 10.4 gate `make -B check` is GREEN: examples 29, parse refusals 15,
check refusals 87, evals 85, runs 6, run refusals 10, asmtool 8, builds
3, lower refusals 14, checks 33, dispatch 20, evm steps 48, precompile
32 steps and 0 failures, deposit 26 assertions, deposit EVM 44 calls and
0 failures, runtime 17069 bytes (R96), mock 13 rows, diff 16 rows and 0
failures, emit 24, and every regressions line passed.

10.5 (the S10 close, docs only; the USER committed 10.3 as 419d5a6):

- `SPEC.md`: section 9 at the S10 state (:331-332 the F15 row: DONE in
  `check`, `eval` and EVM output for a one-constructor family of words
  as a state field; no Bag state field, O84, R80) and section 10
  (:456-466 the status line: S10 closed, the 5 steps with their
  commits, the open list after S10). 466 lines kept.
- `docs/PLAN.md` O91 (:656), O95 (:691) and O98 (:708): "To build" is
  now "Built" (steps 10.1 to 10.5 with 10.5 = the close; steps 10.2 and
  10.3; step 10.4). O96 (:693-696) stays: it applies to each step. 708
  lines kept.
- `docs/VALIDATION.md` :3-4 and :8-13: the S10 final gate header,
  `build/langc` at 419d5a6 (10.4 and 10.5 are docs only), the 10.3
  counts (87 check refusals, 33 build checks, 48 evm steps), M17 and
  M18. 166 lines kept.
- This file: :6 (S10 in the history line), :14 and :43 (the F15 lines),
  :27 and :41 (17069 bytes and 7507 free at the S10 close, R96) and :34
  (the state at the S10 close). The summary keeps its line count.
  No cite moved. No code change. The kit does not change.
- Open after S10 (recorded, not built): the eligibility circuit of the
  paper and a prover (O50, O58 PARTIAL; BLOCKED on the paper text and
  the USER rulings of the hash, the encoding and the setup, R93 (a),
  O93); the EIP-170 budget (O48: 17069 bytes, 7507 free, R96); no Bag
  state field (O84, R80). Winner selection on chain is closed, not taken
  (R88). The paper text is not local.

The S10 final gate `make -B check` is GREEN at the 10.3 baseline: examples
29, parse refusals 15, check refusals 87, evals 85, runs 6, run refusals
10, asmtool 8, builds 3, lower refusals 14, checks 33, dispatch 20, evm
steps 48, precompile 32 steps and 0 failures, deposit 26 assertions,
deposit EVM 44 calls and 0 failures, runtime 17069 bytes (R96), mock 13
rows, diff 16 rows and 0 failures, emit 24, and every regressions line
passed. S10 is closed.

10.4/10.5 staged review (2026-10-19, docs only):

- `docs/host/README.md:89`: spell out the Map key and value types.
  `Map Flag Nat` gives REFUSE_MAP; `Map Nat Flag` checks. A Flag is a
  word value but is not an allowed Map key (`src/front/check.c:1573-1575`).
- `probe/CAPABILITY.md:134-135`: replace the stale "planned" EVM claim
  with the implemented one-constructor family state fields (S10 R95).
- `probe/CAPABILITY.md:171`: move the bare REFUSE_AXIOM_TYPE cite from
  :2724 to :2776. The original pass above moved 22 cites on 20 lines;
  this continuation makes the final audit 23 cites on 21 lines.
- `SPEC.md:316`, :322 and :327: the open-list summary now names the
  S10 close, with the unchanged runtime governed by R96.

Validation: `make -q build/langc` exits 0. The Map checks above exit 1
(REFUSE_MAP) and 0 respectively. The recorded 10.4 gate exits 0 with
48 EVM steps and a 17069-byte deposit runtime; code, tests and the
Makefile are unchanged from 419d5a6. The full gate was not rerun for
these documentation corrections.
