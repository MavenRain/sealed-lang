# Formers on the tcc-evm-contract host

The front end of this kit is a copy of the tcc-wasm front end. `langc check`
and `langc eval` accept the formers F1 to F15 as that front end does.
`test/gate.sh` runs the examples on them.

## EVM output (slice K4, done in the kit)

`langc build` lowers a contract to EVM bytecode (docs/host/README.md,
section `EVM output`). The facts a program author needs:

- The state is a list of word fields: Nat, Flag, U256 and Addr. A `List T`
  field holds words (T a word type). A `Map K V` field holds one word per
  key. An Option field, a Prod field, a nested List, a List over a type
  that is not a word give REFUSE_LOWER (K4a). Map keys must be Nat, U256
  or Addr; values must be Nat, Flag, U256 or Addr. Other Map types give
  REFUSE_MAP during checking. A Map nested in another state field gives
  REFUSE_STATE.
- An entry body lowers to first-order word code: the word values, Option,
  Prod, `flagIf` and `if`. `some (pair STATE OUT)` stores the changed
  fields, `none` reverts, a trap reverts with `Trap()` (K4b).
- A view returns one word (K4c). A residual `fold` over a List field runs
  its body once per element, from the last element to the head. Its seed
  and step must lower to words. If either can trap, the step must be
  provably strict in its accumulator, or the build gives REFUSE_LOWER.
  A residual fold at a word position that is not over a List field also
  gives REFUSE_LOWER (K4c). Folds that normalize to words before lowering,
  such as a sum over a closed `cons` chain, can build.
- `pay` and `pull` are token CALLs (K4d). `emit` has no EVM form: use a
  named event.
- The formers F1 to F15 are checked and evaluated (F12 and F13 as `Path`
  since S3). On the EVM only the first-order word residual of a body
  lowers. A residual of another form gives REFUSE_LOWER
  (`test/lower/expect.txt`, 14 rows).

## The S2 operations (slice S2)

| Op | `check` and `eval` | EVM | Test |
|---|---|---|---|
| `natDiv`, `natMod` | DONE. A divisor 0 traps. | DONE: DIV and MOD with the zero trap. | The `ratio` and `remainder` eval rows of `examples/entries.lang`; `leafStep` in `test/precompile.sh`. |
| `hash` | DONE: KECCAK256 of two words (`src/keccak.c`). | DONE: the IR op KECCAK. | The eval row `precompile hashZero`; the chain step `hashv`. |
| `addrToU256` | DONE. | DONE: the word itself. | The eval row `ownerWord`; the chain step `addrWord`. |
| `ecrecover` | OPAQUE: a closed application stays stuck. | DONE: one STATICCALL to precompile 1; the output word is stored 0 first. | The chain steps `recover` and `recoverSum`. |
| `ecAdd`, `ecMul` | OPAQUE. | DONE: one STATICCALL to 6 or 7; the result is a two-word block; `first` and `second` read it. | The chain steps `addX`, `addY`, `mulX`, `mulY`, `addBad`. |
| `ecPairing` | OPAQUE. | DONE: one STATICCALL to 8 over a closed list of pairs, 6 words per pair. An open list gives REFUSE_LOWER. | The chain steps `pairOk` and `pairNil`; `test/lower/pairing-open.lang`. |

Removed in S2 (R19): `natMul`, `u256Mul`, `u256Div` and `u256Min`
(`test/check/pruned-op.lang` expects NAME_UNKNOWN). Removed in S2 (R21):
the sample domain. `test/precompile.sh` is the only check of the curve
words: each constant was confirmed by a direct `evm run --input` to the
precompile before it entered the script.

## Paths (slice S3)

| Op | `check` and `eval` | EVM | Test |
|---|---|---|---|
| `I`, `i0`, `i1`, `~`, `/\`, `\/` | DONE: the interval and its De Morgan algebra. `I` is not a type: only a path abstraction or a PathP line binds an interval variable. | Erased. | `examples/paths.lang`; INTERVAL_TYPE rows `interval-type.lang`, `interval-pi.lang`, `interval-path.lang`. |
| `PathP`, `Path`, `refl`, `@` | DONE: a path abstraction must meet its ends. No rule makes two paths with the same ends equal (no UIP). | Erased. A path residual at a word position gives REFUSE_LOWER. | `examples/paths.lang`; PATH_END rows `path-end.lang`, `bad-refl.lang`; PATH_UIP rows `path-uip.lang`, `path-k.lang`, `path-ev.lang`; `test/lower/path-word.lang`. |
| `sym`, `cong` | DONE. | Erased. | The eval rows `equality back`, `equality backEnd`; the PATH_UIP row `path-sym.lang`; pathtool "dependent symmetry", "dependent congruence". |
| `funExt` | DONE in `check` only: it elaborates to a path abstraction (R29). | Erased. | `examples/paths.lang` `pointwise`; pathtool "dependent function extensionality". |
| `transp` | DONE: the identity on a base line (Nat, Flag, U256, Addr, Unit, Map; R27); Pi, Sigma, data and universe lines reduce. | Normalizes before lowering; a remaining Kan operation at a word position gives REFUSE_LOWER. | The eval rows `kan evPi`, `kan evPair`; TRANSP_LINE row `transp-line.lang`; pathtool "stuck transport fill". |
| `hcomp` | DONE: a system of faces. At the R27 base types on a face other than `i1`, a literal base is returned only when every live branch evaluates to that literal at a fresh interval; otherwise it stays stuck, as does a neutral base. On `i1`, a true system branch is selected at `i1`. Each step spends `m->fuel` (R28). | Normalizes before lowering; a remaining Kan operation at a word position gives REFUSE_LOWER. | The eval row `kan evTrans`; rows `system-cover.lang`, `system-overlap.lang`, `hcomp-cap.lang`, `hcomp-universe.lang`; pathtool "hcomp over a literal word base". |

The kit `Eq` is out in S3 (O3): the six Eq rows of BUILTINS. `transport`,
`subst`, `J`, `JRefl` and `trans` are guest defs in `domain/domain.lang`
(R25). `build/pathtool` (`test/pathtool.c`) does 19 quote and parse
roundtrips. The existing EVM regressions still pass with the S2 counts
(evm 43, diff 15, emit 24). Path-specific EVM coverage includes the
refusal of a remaining hcomp at a word position (`test/lower/path-word.lang`).

## HITs and QITs (slice S4)

| Form | `check` and `eval` | EVM | Test |
|---|---|---|---|
| Path constructor | DONE: a `family` constructor with interval fields `(i : I)` and a boundary system; each boundary is checked (HIT_BOUNDARY). A constructor of dimension 2 (squash0, quotSquash) has path fields. At an end it reduces to its boundary; at a neutral interval it stays neutral. No rule makes `loop` equal to `refl`. | A point constructor value lowers as a family value. A path constructor residual is not a word: REFUSE_LOWER (R31). | `examples/circle.lang`; PATH_UIP row `loop-not-refl.lang`; fronttool `domain` rows (HIT_BOUNDARY); DOMAIN_FAMILY row `state-boundary.lang`; pathtool "path constructor at a neutral interval". |
| `fold` over a HIT | DONE: one case for each path constructor, at the type of its boundary. A fold of Trunc0 or Quot needs the isSet case. `fold_path` applies the path case at the interval; `fold_hcomp` maps to `hcomp` on the target type (R33); `fold_square` folds the path fields of a square. | At an end the fold gives the point case. A stuck `hcomp` on a word type gives REFUSE_LOWER. | TYPE_MISMATCH rows `fold-missing-path.lang`, `trunc0-no-set.lang`; PATH_END row `fold-path-boundary.lang`; the eval rows `trunc truncMapped`, `quot toTrunc3`; `test/lower/loop-word.lang`; pathtool "fold boundary maps a point constructor path field", "fold square boundary preserves captured intervals in path fields". |
| `hcomp` at a HIT | DONE: a composite of path constructor values is a formal hcomp (a canonical value). | Erased. REFUSE_LOWER at a word position. | `examples/circle.lang` `loop2`; pathtool "formal hcomp at a HIT". |
| `transp` at a HIT | DONE: the identity at a family with no parameters. Along a parameter line `transp_family` reduces and keeps a boundary that depends on the parameter; squash0 stays stuck. | Erased. | `examples/circle.lang` `transpBase`; PATH_UIP row `squash0-not-rebuild.lang`; pathtool "transport of a formal hcomp at Trunc1 along a parameter line", "transport preserves a parameter-dependent HIT boundary", "transport of squash0 along a parameter line stays stuck". |

S4 adds no BUILTINS row (66). The families S1, Trunc1, Trunc0 and Quot
are in `domain/domain.lang` (R32). `build/pathtool` does 26 quote and
parse roundtrips. The EVM regressions did not change at any S4 step (evm
43, diff 15, emit 24, runs 5, run refusals 10): a HIT value is erased.

## The deposit contract (slice S5)

| Form | `check` and `eval` | EVM | Test |
|---|---|---|---|
| Tree library | DONE: guest defs in `domain/domain.lang:103-149` (R40). `treeDepth` is 32 (R42); `zeroHash l` is the root of an empty subtree; `insertLeaf idx cur filled` gives the new root and the new filled subtrees; `rootOfLeaves` gives the root of a closed list of leaves. | An unrolled depth-32 insert, linear in the depth by the share rule. | The eval rows `merkle sameOne`, `sameTwo`, `sameThree`; lowertool row `insert-depth-32`; `test/run/deposit.out` `rootOf`. |
| Groth16 verifier | DONE: `verify vk inputs proof` in `domain/domain.lang:151-215` (R40). It gives flagNo unless the key has one IC point per input plus IC0 and each input is below the scalar order r; then one `ecPairing` on (-A, B), (alpha, beta), (vk_x, gamma), (C, delta). Stuck in `langc eval` (O5). | Exact on the precompiles 6, 7 and 8. A point that is not on the curve reverts (R18). | `test/precompile.sh` steps `verifyOk` (1), `verifyBadOrder` (0) and the 12 `test/groth16-invalid.lang` steps (precompile 32). |
| Share rule | n/a (lowering only). | `share_arms` in `src/lower.c` (R41): a pure word value that both arms of a word `flagIf` use is lowered once into a local before the IF. No expected output of an existing program changed. | lowertool row `insert-depth-32`; the build, emit and evm regressions. |
| Deposit entries | DONE: `examples/deposit.lang`, 9 entries (R43) and 9 views (O8). The Π path is `addTuple` and a signature over `hash piAcc[au] piNonce[au]` (R45). | 17119 runtime bytes (R48). `pull` and `pay` move the deposit through `asset` and `vault` (R46). | `test/deposit.lang` (25 assertions); `test/run/deposit.script` (23 calls, R47); `test/diff.sh` row 16; `test/deposit.sh` (41 chain calls: the signed and proof calls). |

S5 adds no BUILTINS row (66). `langc run` stops with EVAL_ENTRY at a
successful signed or proof call (USER ruling, O47): the chain rows of
`test/deposit.sh` test those calls. The run counts changed at step 4.6
only: runs 5 -> 6, diff 15 -> 16. Mutants M1 to M9 and M9b of S5 are in
`docs/STATUS.md`: 9 of 9 killed, and M9b killed.

The deposit example uses the synthetic R39 key. `slashUsr` accepts the
same proof with inputs (2, 3) for unequal cm words 5 and 6: neither cm
is bound to the proof inputs, and replacing the key alone does not add
that binding (O50). `settle` accepts any caller and supplied price after
the auction ends. S6 adds off-chain bid validation and pool insertion
(`processBid`) and winner selection (`bagMax`), without connecting them
to `settle` (O7/O44). `vault` must equal the executing deposit contract's
address, since `pull` credits `vault` and `pay` spends the executing
contract's balance (O46).

## The protocol layer (slice S6)

| Form | `check` and `eval` | EVM | Test |
|---|---|---|---|
| `Bag` QIT | DONE: `family Bag (A : Type 0)` in `domain/domain.lang:51-61` (R51): `bagNil`, `bagCons`, the path constructor `bagSwap` and the truncation `bagTrunc`. A fold of `Bag` has a case for `bagSwap` and an isSet case for `bagTrunc`. | Not lowered: `examples/protocol.lang` has no entry (R57). | PATH_END row `bag-swap-boundary.lang`; the protocol eval rows. |
| Laws | DONE: four `axiom` declarations, in the domain only (R52, R53): `bidMaxSwap` and `bidSet` (`domain/domain.lang:79-80`), `insSwap` and `listNatSet` (`domain/domain.lang:100-101`). After the Pi binders the type must be a `Path` or a `PathP`. An axiom is a neutral in `langc eval`; `langc check` prints the axiom report. The kernel trusts the laws (O10). | A law has no run-time value: REFUSE_LOWER (`src/lower.c:391`). No lower row. | REFUSE_AXIOM row `axiom.lang`; REFUSE_AXIOM_TYPE row `axiom-type.lang`; fronttool `domain` rows; the axiom report in `test/gate.sh:6-17`. |
| Bid pool and median | DONE: `bagMax` folds a Bag with `bidMaxSwap` and `bidSet`; on equal value the larger cm wins (R55). `sortBag` folds a Bag with `insSwap` and `listNatSet`. `medianAt f b` is the stamp at index f of the sorted stamps, or `none` when the bag has at most f stamps (R54). `examples/protocol.lang:4-49`. | Off chain only (R57). | The eval rows `protocol winnerOf3`, `tieBid`, `emptyPool`, `medianOdd`, `medianEven`, `medianMissing`, `medianTooFew`. |
| `signComWord` and `processBid` | DONE: `signComWord cm t` is `hash cm (toU256 t)` (R55). `processBid` takes the verifiers as parameters and gives `Sum Nat (Bag Bid)`, with the codes 1 to 5 in the paper order (R56). `stampBy` uses `ecrecover`: stuck in `langc eval` (O5). `examples/protocol.lang:50-147`. | Off chain only (R57): no entry calls `processBid`. | The eval rows `protocol signComA`, `signComB`, `procAccept`, `procProof`, `procSlashed`, `procDepositor`, `procLate`, `procStamp`. |
| Property checks | DONE: `lemma11F1`, `lemma11F2`, `lateCut`, `slashedAbort` and `ncCount` give 1 (`examples/protocol.lang:149-206`, R59). Simultaneous Release and No Free Bid Withdrawal are checked in part; Hiding and APE are not terms. | n/a (off chain). | The eval rows `test/eval/expect.txt:80-84`; the property table in `docs/VALIDATION.md`. |

S6 adds `examples/protocol.lang` (examples 27 -> 28), 20 eval rows (evals
64 -> 84) and 2 check rows (check refusals 83 -> 85). The EVM counts at
the S6 close are the S5 counts (evm 43, diff 16, emit 24, runs 6, run
refusals 10, runtime 17119): `examples/deposit.lang` did not change.
Mutants M1 to M11 of S6 are in `docs/STATUS.md`: 11 of 11 killed, and
M4strict killed. The S7 row `test/fronttool.c:141-143` (REFUSE_AXIOM_TYPE)
also kills M11.

`processBid` validates a bid and adds it to the pool off chain;
`bagMax` selects the winner from that pool. `settle` did not change:
it accepts any caller and supplied price (O44, R57). The binding of each
cm to its proof's public inputs (O50) is not in S6 (R58).
