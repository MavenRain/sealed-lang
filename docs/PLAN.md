# sealed-lang plan

## 1. Rulings

The USER gave these rulings in the request (`design/DESIGN.md`):

- Purpose: censorship-resistant sealed-bid auctions on EVM chains, as in
  Alpos, Heimbach, Nayak and Wadhwa, "Censorship-Resistant Sealed-Bid
  Auctions on Blockchains", arXiv:2606.14939 (June 2026). Section 2 gives
  the on-chain surface of that paper.
- Type constructors and higher-kinded types: products, coproducts, monads
  (map and bind), algebras and coalgebras (fold and unfold), quotients,
  paths, loops, HITs and QITs, as in cubical type theory (CTT).
- Base types and base operations: only the minimum span that implements the
  contracts of the paper (section 4).
- Host: TinyCC. Target: the EVM bytecode of contracts, for any EVM chain.

These are the builder's calls. The USER did not rule them. Section 8 lists
them as open items:

- Name: sealed-lang (O1). The compiler binary is `build/langc`.
- Host kit: `hosts/tcc-evm-contract` of lang-template (O2). It has the
  EVM contract target: `U256`, `Addr`, `Map`, `State`, `Env`, `Out`, named
  events, storage layout, dispatch, token calls and a chain test on `evm`.
  The cubical kernel (section 5) is ported from coin-lang into this
  repository. It does not go into lang-template.
- Tests: unit and integration tests are in the guest language and in the
  kit's `langc run` scripts. No new node, Python or shell harness. The kit
  gate scripts stay.

## 2. The paper's on-chain surface

The paper puts one contract on chain, the deposit contract (Algorithm 1).
Bids, timestamps and inclusion lists are off chain. Only the winning bid
transaction reaches the chain. The contract has this state:

| Field | Paper | Type in this language |
|---|---|---|
| `ctr` | next auction id, strictly increasing | `U256` |
| `aucsPerSlot` | auctions registered per slot, at most `nA` | `Map Nat Nat` |
| `auctions` | `AucID -> (item, tstart, tend, S)` | one `Map U256 _` per field (O8) |
| `deposits` | Merkle registry of `(A_u, C_u)` with amount `D` | `Map Addr U256` (amount), `Map Addr U256` (`C_u`), tree fields (O9) |
| `inactives` | users under an inactivity timer | `Map Addr Nat` (timer start) |
| `slashed` | set of slashed handles `h_u` | `Map U256 Flag` |
| `minDeposit`, `nA`, proposers | parameters | `U256`, `Nat`, `Map Addr Flag` (O14) |

It has these functions. Each becomes an entry or a view (section 6):

| Function | Checks | Effect |
|---|---|---|
| `regAuction(item, tstart, tend)` | slot count below `nA` for each slot in range | stores the record under a new id |
| `getAucRecord(id)` | none | view |
| `registerDeposit(A_u, C_u, D)` | caller is `A_u`, `D >= minDeposit` | records the deposit, inserts the leaf |
| `makeInactive(A_u)` | caller is a proposer, `A_u` has a deposit | starts the `nA` timer |
| `updateActivity(A_u, Π)` | caller is a proposer or `A_u`, `A_u` is inactive | moves `A_u` back to active |
| `slashUsr(h_u, (cm1, π1), (cm2, π2))` | both eligibility proofs verify | adds `h_u` to `slashed` |
| `slash(A_u, h_u, Π)` | signature of `A_u`, `h_u` in `Π` and slashed | removes the deposit |
| `withdraw(A_u, Π)` | message and signature of `A_u` valid | removes the deposit, pays `minDeposit` |

The primitives the contract needs: a hash (`C_u = H(ρ)`, `h_u = H(ρ, id)`,
`cm = H(tx, C_u)`), a Merkle tree over that hash with a root per slot, a
Groth16 verifier over BN254 (the eligibility proof `π^m` is verified on
chain by `slashUsr`), a signature check (the user signs `Π`), the caller,
the time, maps, and a token transfer. Section 4 turns this into the span.

The off-chain logic (Algorithm 2, the timestamper; Algorithm 3, the
inclusion-list proposer) is also written in this language, as helpers that
`langc eval` runs and that never reach the EVM. The bid pool `ℬ` of a slot
is a finite multiset, and `max` over it (the winner) and the median over
the timestamps (Lemma 11) are folds on a QIT (section 5). This is where the
quotients and the paths do work.

## 3. The design on the kit basis

| Concept | Design text | Encoding (proposal) | Slice |
|---|---|---|---|
| Contract | "contracts as in the paper" | the kit forms: `state State`, `def init`, entries `Env -> State -> words -> Option (Prod State (List Out))`, views, events | S5 |
| Words | the minimum span | `Nat` (slots, counts), `U256` (ids, amounts, hashes, field elements), `Addr`, `Flag` | kit |
| Hash | `Hash(...)` | `hash : List U256 -> U256`, KECCAK256 of the packed words (O4) | S2 |
| Merkle registry | `deposits`, `root_s` | guest library: incremental tree of depth 32 over `hash`, one `Map Nat U256` of filled subtrees, a root per slot (O9) | S5 |
| Groth16 | `ZkVrf` | `ecAdd`, `ecMul`, `ecPairing` (precompiles 6, 7, 8); `G1 := Prod U256 U256`; `G2 := Prod (Prod U256 U256) (Prod U256 U256)`; the verifier is a guest fold (O5) | S2, S5 |
| Signature | "signature of `A_u`" | `ecrecover : U256 -> Nat -> U256 -> U256 -> Addr` (precompile 1), compared with `addrEq` | S2 |
| Caller, time | sender, slots | the kit `Env`: `caller : Addr`, `now : Nat` as the slot (O13) | kit |
| Deposit asset | `D`, `minDeposit` | the kit token calls `pull` and `pay` (O6) | kit |
| Products, coproducts | F1, F2 | the kit forms `Prod`, `Sum`, `Option` | kit |
| Monads | map, bind | F5 on `Option` (revert), `Sum E`, `List` | kit |
| Algebras, coalgebras | fold, unfold | F6 and F7 on `List`, `Nat` and the domain families; the status of F7 on this kit is an S1 survey item | kit |
| Paths | "paths" | the interval `I`, `PathP`, `Path`, path abstraction and application, `transp`, `hcomp`; the kit `Eq` goes (O3) | S3 |
| Loops | "loops" | a path constructor from a point to itself in a HIT declaration; the kernel test is the circle (O11) | S4 |
| HITs | "HITs" | `family` in `domain/domain.lang` with path constructors; `fold` takes one case per point constructor and one path case per path constructor, with the boundary checked | S4 |
| Quotients, QITs | "quotients, QITs" | set quotients `A / R` and set-truncated families with path constructors; `fold` into a set needs the respect proofs and the `isSet` proof | S4 |
| Bid pool | `ℬ`, `max(ℬ)`, median | `Bag A`: the QIT of finite multisets in `domain/domain.lang` (`bagNil`, `bagCons`, `bagSwap`, `bagTrunc`; R51); `bagMax` (the winner) and `sortBag` with `medianAt` (the median) as folds, with the four domain laws as the path cases (R52, R53) | S6 |
| Erasure | the EVM | a path, an interval term, a HIT path constructor and a QIT path case have no run-time form; the lowering keeps the point constructors and refuses a path at run time (`REFUSE_LOWER`) | S4 |

## 4. The base span

The span is the set of core types and core operations a program can use.
The design says the minimum that implements the contracts. This is the
proposal. S2 measures the kit's current span and removes what section 2
does not use (O12).

Core types: `Nat`, `Flag`, `U256`, `Addr`. No other base type. `G1`, `G2`,
`Proof` and `VKey` are products of words, not base types.

Core operations, by group. The exact S2 span is the BUILTINS table of
`src/front/check.c` at commit bc8e168: 61 rows, which are 11 type formers
(`Nat`, `Flag`, `U256`, `Addr`, `Unit`, `Prod`, `Sum`, `Option`, `List`,
`Eq`, `Map`), 28 constructors and eliminators of F1 to F15, and 22 base
operations (14 kept from the kit, 8 new in S2):

- Kit, kept: `natAdd`, `natSub`, `natEq`, `natLe`, `flagIf`, `u256Add`,
  `u256Sub`, `u256Eq`, `u256Le`, `toU256`, `addrEq`, `mapEmpty`,
  `mapGet`, `mapSet`, the `Env` projections, the `Out` constructors `pay`
  and `pull`, and the events. S2 removed `natMul`, `u256Mul`, `u256Div`
  and `u256Min` (R19): the first draft of this section kept `natMul`.
- New, EVM precompiles (S2, R16 to R18): `hash : U256 -> U256 -> U256`
  (KECCAK256 of the two packed words, one IR op), `ecrecover : U256 ->
  Nat -> U256 -> U256 -> Addr`, `ecAdd`, `ecMul`, `ecPairing`. In `langc
  eval`, `hash` computes (the kit has keccak); the four curve operations
  are opaque: a closed application stays a stuck term. On the EVM each
  lowered curve statement executes one STATICCALL to its precompile
  (1, 6, 7, 8). Call sharing and fold execution are described in
  `docs/host/README.md`, section `The base span (slice S2)`. The result of
  `ecAdd` and `ecMul` is a heap block of two words read by `first` and `second`. A
  failed STATICCALL reverts. An empty `ecrecover` return reads as address
  0. `test/precompile.sh` on `evm` is the differential gate for them (O5,
  18 steps).
- New, for the tree (S2, R14): `natDiv` and `natMod` (floor and
  remainder, a divisor 0 traps). Halving is `natDiv i 2` and parity is
  `natMod i 2`. R13 keeps `now` as TIMESTAMP: the slot is `natDiv (natSub
  now genesis) slotLength` in the guest, with `genesis` and `slotLength`
  fixed in `init`.
- New, the address word (S2, R20): `addrToU256 : Addr -> U256`, the one
  conversion from `Addr`. A leaf is `hash (addrToU256 a) c`.
- New, core laws: `fold` on a QIT into `U256` needs a path for each path
  constructor. The S6 laws are `axiom` declarations in the domain (R52):
  the commutation laws `bidMaxSwap` over `bidMax` (for `bagMax`) and
  `insSwap` over `insNat` (for the median fold `sortBag`), and the isSet
  laws `bidSet` and `listNatSet` (`domain/domain.lang:79-80,100-101`).
  The kernel trusts them and does not run them (O10). The S6 kickoff lists
  each law.

Removed from the kit span in S2: the sample domain (`Color`, `Item`,
`Stack`, `Lot`; R21) and the four pruned operations (R19). `Eq` with
`refl`, `symm`, `trans`, `transport` and `cong` went in S3 (O3, R3).

The S3 span is the BUILTINS table at commit 9394ffd: 66 rows. S3 removed
the 6 Eq rows (`Eq`, `refl`, `symm`, `trans`, `transport`, `cong`) and
added 11 path rows (`I`, `i0`, `i1`, `PathP`, `Path`, `refl`, `sym`,
`cong`, `funExt`, `transp`, `hcomp`): 61 - 6 + 11 = 66. `trans`,
`transport`, `subst`, `J` and `JRefl` are guest defs in
`domain/domain.lang` (R25), not rows. Path computations normalize before
lowering; unsupported residuals at word positions give REFUSE_LOWER
(`test/lower/path-word.lang`).

The S4 span is the BUILTINS table at the S4 close: 66 rows, as in S3. S4
adds no row. The HIT declaration form (a constructor with interval fields
and a boundary system) is part of `family` in `domain/domain.lang`, and
`fold` takes one case for each path constructor. The S4 families are S1,
Trunc1, Trunc0 and Quot, with the guest defs isProp and isSet (R32). A
path constructor residual or a formal hcomp at a word position gives
REFUSE_LOWER (`test/lower/loop-word.lang`, R31).

The S5 span is the BUILTINS table at the S5 close: 66 rows, as in S4. S5
adds no row. The tree library and the Groth16 verifier library are guest
defs in `domain/domain.lang` over `hash`, `ecAdd`, `ecMul` and
`ecPairing` (R40), and `src/lower.c` has the share rule (R41).

## 5. The cubical kernel

coin-lang (private MavenRain/coin-lang) built a CCHM kernel on the tcc-json
front end: S1 `97e66e0` (interval, `PathP`), S2a `a4bb138` (`transp`), S2b
`582ecef` (`hcomp`), S4a `8c79e9a` (path constructors, the circle, formal
`hcomp`), S4b `e0d4f3c` (propositional truncation), S4c `25c14e0` (set
truncation and set quotients). Its Glue and univalence slices (S3a to S3c)
are not in this design (O10).

The tcc-json front end and the tcc-wasm front end come from one base.
The tcc-evm-contract front end is the tcc-wasm front end plus `U256`,
`Addr`, `Map`, the contract forms and the events (1293 diff lines against
tcc-wasm). The coin-lang front end is the tcc-json front end plus the
kernel (4171 diff lines against tcc-json). The port is a three-way merge of
the kernel hunks onto this kit's front end, one coin-lang slice at a time,
with the coin-lang tests ported as guest tests. `src/front/check.c` and
`src/front/eval.c` take the hunks; `src/lower.c` gets the erasure rule.

Kernel rules that this design keeps from coin-lang:

- `I` is a sort outside the universe. Interval terms are in De Morgan
  normal form. A value carries a lazy interval substitution.
- A path definition is checked against its end points by conversion. A
  path that holds only by proof irrelevance is a refusal: there is no
  uniqueness of identity proofs, so the loops and the quotients are not
  trivial.
- A HIT declaration lives in `domain/domain.lang` only (rule R1). A path
  constructor gives its end points. A QIT is a HIT with `trunc : isSet`.
- `fold` on a HIT takes the point cases, then one path case per path
  constructor, checked at the boundary. `fold` on a QIT into a type `B`
  takes `isSet B`.

## 6. The deposit contract in sealed-lang

`examples/deposit.lang` is the target program of S5. A sketch:

```
state State := makeState
  (ctr : U256) (minDeposit : U256) (depth : Nat)
  (aucsPerSlot : Map Nat Nat)
  (aucItem : Map U256 U256) (aucStart : Map U256 Nat) (aucEnd : Map U256 Nat)
  (amount : Map Addr U256) (leaf : Map Addr U256)
  (nextLeaf : Nat) (filled : Map Nat U256) (rootAt : Map Nat U256)
  (inactiveSince : Map Addr Nat) (slashed : Map U256 Flag)
  (proposer : Map Addr Flag)
```

Entries: `regAuction`, `registerDeposit`, `makeInactive`, `updateActivity`,
`slashUsr`, `slash`, `withdraw` and `settle` (O7). Views: `getAucRecord`
(one view per field, O8) and `rootOf`. The argument `Π` is a list of NOP
tuples. An entry takes words only, so `Π` enters one tuple per call with a
pending counter in the state (O12).

Helpers, never on the EVM: `verify` (Groth16, a fold over the public
inputs with `ecMul` and `ecAdd`, then one `ecPairing` on four pairs),
`insertLeaf` and `rootOfLeaves` (the tree), `signCom` and `processBid`
(Algorithms 2 and 3) over `Bag`.

Test scripts in `test/run/`: register two deposits, one auction, one
inactivity round trip, one double-commit slash with a fixture proof (O5),
one withdraw, each with the state printed after the call. The chain test
deploys the program on `evm` and replays the same scripts.

## 7. Slices and gates

- S0 (this commit): `bin/new-lang.sh sealed-lang tcc-evm-contract`,
  `design/DESIGN.md` (the request without change) and this plan.
- S1: survey and kickoff. Kernel facts of this kit's front end (the `Eq`
  sites, the value representation, the entry lowering in `src/lower.c`,
  the realization of `now`, the exact core operation set, the F7 status),
  the precompile call path in `src/evm.c`, and the coin-lang hunks to
  port. Then the rulings R1 to Rn for the USER. No build before the
  rulings.
- S2: the base span. The five precompile operations in the checker, the
  evaluator (opaque) and the EVM writer; the tree index pair; the removal
  of the unused kit operations and of the sample domain. Gate: a chain
  test that calls each precompile once with a known vector (EIP-196 and
  EIP-197 vectors, one `ecrecover` vector).
- S3: paths. The interval, `PathP`, `Path`, `refl`, `sym`, `cong`,
  function extensionality, `transp`, `hcomp`; the kit `Eq` goes; each kit
  example and test that used `Eq` moves to `Path`. Refusals: a wrong end
  point, `I` used as a type, a conversion that holds only by irrelevance.
- S4: HITs and QITs. The declaration form, boundary checks, the eliminator
  with path cases, set truncation, set quotients, the circle test, the
  erasure rule in the lowering. Refusals: a missing path case, a path case
  with a wrong boundary, a QIT fold without `isSet`, a path at run time.
- S5: the deposit contract. `examples/deposit.lang`, the tree library, the
  verifier library, the run scripts, the chain replay. Mutants: a wrong
  root, a wrong public input order, a missing caller check, a timer that
  does not expire.
- S6: the protocol layer. `Bag`, `max`, `median`, the core laws they need,
  `signCom`, `processBid`, the four properties of the paper as guest
  checks where a check is a term (the paper's Theorems 7 to 10 are
  not terms of this language; `docs/VALIDATION.md` says so). Note (S6):
  eval rows check parts of Theorems 7 and 9 at fixed inputs.
- S7: `SPEC.md`, `probe/CAPABILITY.md`, `docs/STATUS.md`,
  `docs/VALIDATION.md` and `README.md`. Final gate.
- S8: the open list after S7. `settle` takes a depositor only, `self` for
  the vault, the binding of each cm to its proof with a re-based fixture
  key, the `rule_hcomp` refactor, the stale cites. Mutants M12 to M14.
- S9: the open list after S8: a Nat-indexed family, the run-time Bag
  lowering (Bag to List erasure), the kit `od -v` lines and the docs.
  Mutants M15 and M16.
- Gate: `make check` in this directory. Mutants one at a time, `make -B`
  for each build.

## 8. Open items

- O1: the name sealed-lang.
- O2: the kit tcc-evm-contract, with the cubical kernel ported from
  coin-lang into this repository. Alternatives: start from coin-lang and
  port the EVM target; or a new lang-template kit.
- O3 CLOSED (S3, R3): the kit `Eq` went in S3 and `Path` took its place,
  as coin-lang did. The alternative (keep `Eq` as a name for `Path`) was
  not taken.
- O4: `hash` is KECCAK256 of the packed 32-byte words. The paper's
  implementation uses Poseidon in the circuits. A circuit that proves a
  keccak Merkle path is costly; an on-chain Poseidon is a library, not a
  base operation. Default: keccak now, Poseidon as a later library slice if
  the circuits need it. S2: `hash` is KECCAK256 of two packed words
  (R16); `src/keccak.c` computes it in `langc eval`.
- O5: the four curve operations are opaque in `langc eval` and exact on
  the EVM. A C99 BN254 pairing under TinyCC is a later option. The
  Groth16 fixture for S5 is a known-good `(vk, inputs, proof)` triple; its
  source is a ruling. S2 DONE: opaque in `langc eval` (a closed application
  stays a stuck term), exact on the EVM; `test/precompile.sh` checks 18
  steps with constants confirmed on `evm` 1.14.12. The fixture source
  stays a ruling for S5. Note (S5): the fixture is R39 (O39). `verify`
  stays stuck in `langc eval`; `test/precompile.sh` checks it on `evm`
  (32 steps), and `test/deposit.sh` checks the slash path with
  the fixture proof on the chain.
- O6: the deposit asset is an ERC-20 token through the kit `pull` and
  `pay`. Native value needs CALLVALUE and a value call, which the kit does
  not have. Note (S5): `examples/deposit.lang` has `def asset` (the mock
  token address of `test/diff.sh`) and `def vault` (the chain receiver
  address); `pull` moves a deposit to the vault. `pay` calls
  `transfer` from the executing contract's balance, so a deployment must
  set `vault` to that contract's address and set `asset` to its token
  (R46). An external vault would receive deposits but would not fund
  withdrawals.
- O7: `settle(id, bid)` is an entry of the same contract: it records the
  winner and pulls the payment. The paper leaves settlement to the bid
  transaction. Alternative: out of scope. Note (S5): `settle` is in, with
  the 8 entries of O7 and `addTuple` (R43). `settle` also requires `id <
  ctr`, and a second `settle` of the same auction reverts. It accepts
  a caller with a deposit (R73) and any price after the end. Selecting the
  winner from sealed bids on chain is closed, not taken (R88, O88).
- O8: a `Map` value is a word, so an auction record is one map per field
  and `getAucRecord` is one view per field. Alternative: extend `Map` to
  product values (kit work). Note (S5): the record views are `aucItemOf`,
  `aucStartOf`, `aucEndOf`, `winnerOf` and `priceOf`; the other views are
  `rootOf`, `depositOf`, `isSlashed` and `pendingOf`.
- O9: the registry is an incremental Merkle tree of depth 32 with a root
  per slot, as a guest library. Alternative: a list of leaves with a full
  recomputation per insert. Note (S5): the tree library is in
  `domain/domain.lang` (R40), the depth is the constant `treeDepth` 32
  (R42), and the share rule of `src/lower.c` keeps the depth-32 insert
  below EIP-170 (R41). `registerDeposit` reverts when the tree is full.
- O10: no Glue and no univalence (not in the request). The core laws for
  the QIT folds are path constants of the span, listed one by one in the
  S6 kickoff. Alternative: port coin-lang S3 and prove the laws on a
  finite model. Note (S6): the four laws are `axiom` declarations in
  `domain/domain.lang` (R52 (a), R53 (a)): `bidMaxSwap` and `bidSet` for
  `bagMax`, `insSwap` and `listNatSet` for `sortBag`. An axiom in a
  program is REFUSE_AXIOM. A law is a neutral in `langc eval` and
  REFUSE_LOWER on the EVM. `langc check` prints the axiom report (SPEC
  section 8). `docs/VALIDATION.md` gives the reason that each law is
  true. The coin-lang S3a port was not taken: `finAll` does not cover
  U256 or a variable list.
- O11: loops come from the HIT declaration form; the kernel test is the
  circle. No domain family has a loop constructor by default. Alternative:
  the slot clock as a cyclic quotient. Note (S4): the HIT declaration form
  is in (R32). The kernel test is `examples/circle.lang` and the check rows
  `fold-missing-path.lang`, `fold-path-boundary.lang` and
  `loop-not-refl.lang`. No contract family has a path constructor.
- O12: `Π` enters one NOP tuple per call. Alternative: array arguments in
  the ABI decoder (kit work). Note (S2): ENTRY_PARAMS_MAX is 32 (R23), so
  one call carries at most 32 words. Note (S5): a tuple is two words (id,
  h), and `addTuple` adds one tuple to the pending accumulator `piAcc` of
  the user (R45). The user signs `hash piAcc[au] piNonce[au]`; a use of
  the tuples resets the counter and increments the nonce, so a signature
  over an empty Π does not replay.
- O13 CLOSED (S2, R13): the kit reads TIMESTAMP for `now` and keeps it.
  The slot is `natDiv (natSub now genesis) slotLength` in the guest.
- O14: the proposer set is a `Map Addr Flag` fixed in `init`. Note (S5):
  `init` sets one proposer (0x..aa). `regAuction` and `makeInactive`
  revert for any other caller (R44).
- O15: tests in the guest language and `langc run` scripts only.

The S1 kickoff added R16 to R23. The USER did not rule (RULED 2026-10-15);
the defaults (a) were applied in S2:

- O16 (R16): `hash` is 2-ary over U256 through the kit IR op KECCAK. A
  list hash is not in the span.
- O17 (R17): the result of `ecAdd` and `ecMul` is a heap block. A stuck
  curve value lowers to IR_STMT_PRECOMPILE, `first` and `second` to
  IR_EXPR_LOAD. The `ecPairing` input is a closed list of pairs.
- O18 (R18): a STATICCALL that returns 0 reverts. An empty `ecrecover`
  return reads as address 0, because the output word is stored 0 first.
- O19 (R19): `natMul`, `u256Mul`, `u256Div` and `u256Min` are pruned
  (the section 4 delta).
- O20 (R20): `addrToU256 : Addr -> U256` is the one conversion from
  `Addr`.
- O21 (R21): the sample domain is out. S4 fills `domain.lang` with the
  circle. Note (S4): `domain/domain.lang` holds the S3 path defs, isProp,
  isSet, S1, Trunc1, Trunc0 and Quot (R32).
- O22 (R22): `formers/tcc-evm-contract.md` is rewritten.
- O23 (R23): ENTRY_PARAMS_MAX is 32 (`src/front/check.h`).

The S3 kickoff added R24 to R30. The USER did not rule (RULED 2026-10-15);
the defaults (a) were applied in S3. No ruling changes the EVM output of
an existing program:

- O24 (R24): `Path` on a word type is never a word equality test. A path
  is a proof: a closed path erases, and a path residual at a word
  position gives REFUSE_LOWER.
- O25 (R25): `transport`, `subst`, `J`, `JRefl` and `trans` are guest
  defs in `domain/domain.lang`, as in coin-lang, above the S4 circle. The
  `test/review.sh` free-names check runs on them.
- O26 (R26): the refusal fixture is `test/lower/path-word.lang` (a path
  residual over a stuck curve word). The fallback was not necessary.
- O27 (R27): Nat, Flag, U256, Addr, Unit and Map are base lines in the
  Kan rules: `transp` is the identity. For `hcomp` on a face other than
  `i1`, a literal base is returned only when every live branch evaluates
  to that literal at a fresh interval; otherwise it stays stuck. A
  neutral base stays stuck on such a face. On `i1`, `hcomp` selects a
  true system branch at `i1`.
- O28 (R28): `hcomp` spends the `m->fuel` step budget (one step per
  reduction). There is no second budget.
- O29 (R29): `funExt` elaborates at check time to a path abstraction, as
  in coin-lang. It has no eval rule.
- O30 (R30): `build/pathtool` (`test/pathtool.c`) is the port of
  coin-lang `test/path-regressions.c`. It does 19 roundtrips (18 from
  coin-lang and the kit row "hcomp over a literal word base") and prints
  `path regressions: passed`, as the other regression tools do (the
  kickoff text "paths: N roundtrips passed" was not used).

The S4 kickoff added R31 to R38. The USER did not rule (RULED 2026-10-16);
the defaults (a) were applied in S4. For the mutant M9 the USER ruled (a).
No ruling changes the EVM output of an existing program:

- O31 (R31): a point constructor value lowers as a family value lowers. A
  path constructor residual or a formal hcomp is not a word: `value_name`
  (`src/lower.c`) names "a path constructor" and "a formal hcomp", and
  the lowering gives REFUSE_LOWER. No well-typed entry reaches the two
  arms (an entry parameter has a word type). The fixture
  `test/lower/loop-word.lang` (a fold of a formal hcomp at S1 into U256)
  is refused at the stuck `hcomp U256` (R33).
- O32 (R32): Trunc1, Trunc0 and Quot are in `domain/domain.lang` below
  S1, in the coin-lang text, because only `domain.lang` declares a family
  (R1). `examples/trunc.lang` and `examples/quot.lang` hold the uses.
- O33 (R33): no new base line. `fold_path` applies the path case at the
  interval (an end gives the point case); `fold_hcomp` maps to `hcomp` on
  the target type, and on a word type that is the R27 base line.
- O34 (R34): `build/pathtool` has the 8c79e9a, e0d4f3c and 25c14e0
  roundtrips in the kit `roundtrip(label, prelude, source)` shape, with
  the preludes S1, TRUNC0, TRUNC1 and PARAM_HIT: 19 -> 26 rows (the Glue
  row dropped). The 25c14e0 refusal "transport of squash0 is not the field
  rebuild" is the check row `squash0-not-rebuild.lang` (PATH_UIP). The
  tool prints `path regressions: passed`.
- O35 (R35): `system_branches`, `system_overlaps`, `system_core`
  (check.c, no Glue arm) and `true_branch` (eval.c) are ported. The kit
  `rule_hcomp` keeps its inline code, and the helpers are beside it. No
  fallback helper was necessary.
- O36 (R36): each Glue hunk and each Glue arm is dropped. The Fin uses
  moved to Nat (`trunc1 Nat 1` in place of `trunc1 (Fin 3) 1`). The
  coin-lang defs that need ua or Fin are dropped: code2, loopT, loopAt0,
  code3, loop2T and loop2At0 (circle), rot3, truncUa, truncAt, truncSq
  and truncHcomp (trunc), and the eval rows `circle loopAt0`, `circle
  loop2At0` and `trunc truncAt`. Kit-only: `toNat` (circle) and
  `truncMapped` (trunc) with its eval row. Thus evals is 61, not the
  kickoff 63.
- O37 (R37): a path at run time is a path in an entry, and the EVM is the
  run time of an entry: the R31 fixture is that refusal. `langc run` has
  no new row (run refusals 10).
- O38 (R38): the patch method was GNU patch `gpatch -p1 -N -F 1` per file,
  with a placement check of each insert-only hunk; a rejected or misplaced
  hunk went in by exact one-occurrence replacements. From step 4.5 the
  auto-mode classifier denied `gpatch` on the repository: the USER ran the
  4.5 patch script, and steps 4.6 and 4.8 used the Edit tool.

The S5 kickoff added R39 to R50. The USER did not rule (RULED 2026-10-16);
the defaults (a) were applied in S5. Two USER rulings came later: `langc
run` cannot step a successful signed or proof call (s7, O47), and `od -v`
at the 4 kit `od` sites (s8). No ruling changes the EVM output of an
existing program:

- O39 (R39): the synthetic fixture is candidate 1 of the kickoff (section 3), with
  n = 2 distinct public inputs, so the input order is observable. The key,
  the inputs and the proof are constants in `examples/groth16.lang`;
  `slashKey` in `examples/deposit.lang` is a copy, re-based in S8 (O75).
- O40 (R40): the tree library and the verifier library are guest defs in
  `domain/domain.lang` below Quot. `examples/merkle.lang`,
  `examples/groth16.lang` and `examples/deposit.lang` use them.
- O41 (R41): the share rule in `src/lower.c`. A residual word value that
  cannot trap (a `hash`, a word `flagIf` over such values, a load, a
  local) and that both arms of a word `flagIf` use is lowered once into a
  local before the IF (`share_arms`, the R17 memo rule for pure words).
  The insert is then linear in the depth. The lowertool row
  `insert-depth-32` checks a depth-32 insert under EIP-170. No expected
  output of an existing program changed.
- O42 (R42): `treeDepth` is the closed constant 32. The sketch field
  `depth` of section 6 is dropped.
- O43 (R43): 9 entries: the 8 of O7 and `addTuple` (the Π path of R45).
- O44 (R44): `regAuction` and `makeInactive` need a proposer. `slashUsr`
  and `slash` take any caller, with proof and signature checks subject
  to the fixture limits of O50. `settle` takes a caller with a deposit
  (R73) as payer and winner at the given price, with no proof, signature or
  sealed-bid winner check: closed, not taken (R88, O88). `addTuple` and
  `updateActivity` need the user or a proposer.
- O45 (R45): a Π tuple is two words (id : U256, h : U256). `addTuple au
  id h` sets `piAcc[au] := hash piAcc[au] (hash id h)`, `piCount[au] +
  1` and `piHas[piKey au nonce h] := flagYes`. The user signs `hash
  piAcc[au] piNonce[au]` with ecrecover (v, r, w; w is the s word, because
  s names the state). The signed message was `piAcc[au]` in step 4.5; the
  USER commit 86eb3d6 added the nonce, so a signature over an empty Π
  does not replay.
- O46 (R46): `def asset` is the mock token address of `test/diff.sh`.
  Closed in S8 (R74 (a), step 8.3): `def vault` is gone, and `pull`
  credits `self env` (ADDRESS), the executing deposit contract's
  address, whose balance `pay` spends. A deployment sets `asset` only.
- O47 (R47): one row `script examples/deposit.lang test/run/deposit.script`
  in `test/diff.sh` part 2 (diff 15 -> 16); the mock serves `pull` and
  `pay`. `test/evm.sh` has no row. USER ruling (s7): `langc run` stops
  with EVAL_ENTRY at a successful signed or proof call, because
  `ecrecover` and the curve operations stay stuck in `langc eval` (O5).
  Thus `test/run/deposit.script` holds the calls that eval can step (23
  calls, runs 5 -> 6), and the signed and proof calls are chain rows of
  `test/deposit.sh` (41 calls).
- O48 (R48): the runtime of `examples/deposit.lang` (`langc build
  --runtime`) was measured at each step: 11725 bytes at 4.4, 17020 at
  4.5, 17119 at the S5 close, 17103 at 8.3, 17138 at 8.4, 17069 at 8.5
  (EIP-170 24576, 7507 free; S8 ceiling 18143, R78). No limit stopped a
  step. `rootAt` comes before `filled`: the other order is EVM_SIZE (4.4).
- O49 (R49): `regAuction` covers at most 4 slots (`tend - tstart <= 3`):
  `aucRoom` and `aucCounts` check and increment 4 offsets, counted back
  from tend (`natSub tend k`, a USER row).
- O50 (R50): the n public inputs of a proof are n words of the call after
  the proof. `eligible` checks that input 0 equals hu and, since S8 step
  8.5 (O75), that input 1 equals its cm. The slash key is re-based
  (IC0' = IC0 + 3 IC2, IC2' = 0), thus in `test/deposit.sh` the fixture
  proof is accepted with inputs (2, 5) for cm1 = 5 and (2, 6) for cm2 = 6,
  and input 1 = 3 for cm1 = 5 reverts. The key does not bind input 1, thus
  this does not establish two distinct proven commitments or real
  eligibility. The eligibility circuit of the paper is open (PARTIAL); a
  new `slashKey` alone does not add it. `slashUsr` takes 23 words: hu, then
  for each of the two commits a cm word, a proof (8 words) and its 2
  inputs.

For the S6 kickoff rulings R51 to R60 (2026-10-17), the USER said "Take
defaults for R51 to R60". The defaults (a) were applied in S6. Each row
names the S6 step that built it:

- O51 (R51): `Bag` is a QIT in `domain/domain.lang:51-61`: `bagNil`,
  `bagCons`, the path constructor `bagSwap` and the truncation `bagTrunc`
  (family-prefixed names, R4). Built in step 4.1.
- O52 (R52): a law is an `axiom` declaration, admitted in
  `domain/domain.lang` only (the origin test, as R1). After the Pi binders
  its type is a `Path` or a `PathP`, else REFUSE_AXIOM_TYPE. An axiom in a
  program is REFUSE_AXIOM. Built in step 4.1.
- O53 (R53): the four laws: `bidMaxSwap` and `bidSet`
  (`domain/domain.lang:79-80`, step 4.2), `insSwap` and `listNatSet`
  (`domain/domain.lang:100-101`, step 4.3). The kernel trusts them (O10).
- O54 (R54): `medianAt f b` is the stamp at index f of the sorted stamps
  (`sortBag`), or `none` (infinity) when the bag has at most f stamps. An
  even count uses the same rule. Built in step 4.3.
- O55 (R55): `signComWord cm t` is `hash cm (toU256 t)`, as in the paper
  pseudocode (step 4.4). On equal value the larger cm wins (`bidLe`, step
  4.2).
- O56 (R56): `processBid` takes the verifiers as parameters (the eval rows
  pass fixtures) and gives `Sum Nat (Bag Bid)`, with the codes 1 to 5 in
  the paper order. Built in step 4.5.
- O57 CLOSED (S8, R76): off chain, as in the paper (O76). `processBid`
  is off chain only (R57): no entry calls it, and `settle` did not change
  (O44). `examples/deposit.lang` did not change in S6 (17119 bytes, O48).
- O58 (R58): open. The binding of each cm to its proof's public inputs
  (O50) stays out of S6. `eligible` and `slashUsr` did not change.
- O59 (R59): the property checks of the S6 kickoff table (section 2):
  Simultaneous Release and No Free Bid Withdrawal are checked in part;
  Hiding and APE are not terms. The checks `lemma11F1`, `lemma11F2`,
  `lateCut`, `slashedAbort` and `ncCount` are in `examples/protocol.lang`
  (step 4.6). The table is in `docs/VALIDATION.md` (step 4.8).
- O60 (R60): the new rows go in `examples/protocol.lang` (examples 27 ->
  28), `test/eval/expect.txt` (evals 64 -> 84), `test/check/expect.txt`
  (check refusals 83 -> 85) and the domain regressions of
  `test/fronttool.c`. No new harness. Built in steps 4.2 to 4.6.

For the S8 kickoff rulings R72 to R81 (2026-10-18), the USER said "I rule
all defaults for R72 to R81". The defaults (a) apply in S8. Each row
names the S8 step that builds it:

- O61 to O71: the S7 docs rulings, RULED 2026-10-18, defaults (a), built
  in 7.1 to 7.5.
- O72 (R72): the S8 row of section 7 and the lines O61 to O81 of this
  section. RULED 2026-10-18. Built in step 8.1.
- O73 (R73): `settle` refuses a caller that holds no deposit (`mapGet
  (amount s) (caller env)` is 0u). `test/deposit.sh` adds a setup row and
  a reject row (USER). M12 drops the check, and the row kills it. Winner
  selection stays open. RULED 2026-10-18. Built in step 8.4.
- O74 (R74): `self : Addr` in Env, lowered to ADDRESS (0x30), replaces
  `def vault` (O46), and `pull` credits `self env`. One new evm row.
  Mutant M14 lowers `self` to CALLER, and the row kills it. RULED
  2026-10-18. Built in step 8.3.
- O75 (R75): `eligible` takes cm and requires input 1 = cm, and
  `slashUsr` passes cm1 and cm2 (O50, O58). The slash key becomes the
  re-based fixture key (IC0' = IC0 + 3 IC2, IC2' = 0). One reject row;
  mutant M13 drops the input 1 compare. O50 stays PARTIAL: the circuit of
  the paper binds input 1, the key does not. If `verify` refuses the key,
  the step goes to (b). RULED 2026-10-18. Built in step 8.5.
- O76 (R76): O57 is CLOSED (S8, R76). The paper keeps Algorithms 2 and 3
  off chain (`docs/PLAN.md:66-69`), and an entry needs a run-time Bag
  (O37), concrete verifiers, a real zk key and the registry. Docs only.
  RULED 2026-10-18. Built in step 8.1.
- O77 (R77): `rule_hcomp` calls `system_branches`, `system_overlaps` and
  `system_core` in place of its inline code (O35). Every gate count stays
  at the baseline. If a count moves, check.c is restored and O35 closes
  without the refactor. RULED 2026-10-18. Step 8.2 builds it.
- O78 (R78): S8 adds at most 1024 bytes to the runtime of 17119 bytes
  (ceiling 18143, O48). Each step that changes the runtime records the
  size in O48 and in `docs/STATUS.md` `## S8`. A step over the ceiling
  stops. No size pin. RULED 2026-10-18. Steps 8.3 to 8.5 build it.
- O79 (R79): the `od -v` unit is closed in sealed-lang: its 4 copies
  (`test/evmchain.sh`, `test/dispatch.sh`, `test/mock.sh` and
  `test/diff.sh`) have `-v`. The 7 kit sites with `od -An -tx1` and no
  `-v` stay a lang-template item: `hosts/tcc-evm-contract/test/`
  `evmchain.sh:14`, `dispatch.sh:17`, `mock.sh:22` and `diff.sh:29`;
  `hosts/tcc-evm-anchor/test/evmchain.sh:49`;
  `hosts/tcc-js/test/gate.sh:162`; `hosts/tcc-webgpu/test/gate.sh:201`.
  S8 does not edit the kit. RULED 2026-10-18. Built in step 8.1.
- O80 (R80): F15 stays PARTIAL (`formers/FORMERS.md:300`): no family in
  `domain/domain.lang` takes a Nat index. Docs only. RULED 2026-10-18.
  Built in step 8.1.
- O81 (R81): the 6 stale cites of `docs/STATUS.md` `## S7` are fixed.
  Docs only. RULED 2026-10-18. Built in step 8.1.
- O82 (R82): the S9 scope. S9 takes a Nat-indexed family (O86), the
  run-time Bag lowering (O84), the 7 kit `od -v` lines (O85) and the docs
  (O83, O88, O90). The eligibility circuit stays open (O87). RULED
  2026-10-18, defaults (a). To build in steps 9.1 to 9.5.
- O83 (R83): the S9 row of section 7 and the rows O82 to O90. The S9 row
  moves each cite at or above the old line 269 by 3 lines. Docs only.
  RULED 2026-10-18, defaults (a). Built in step 9.1.
- O84 (R84): the run-time Bag lowering (O37), in `src/lower.c`, not in the
  kit. `Bag A` erases to `List A`: bagNil to nil, bagCons to cons, and a
  Bag fold lowers as a List fold without the path cases. A Bag stays
  local to an entry (no Bag state field, K4c). One build row and one evm
  row. Mutant M16. RULED 2026-10-18, defaults (a). To build in step 9.4.
- O85 (R85): the 7 kit `od -v` lines of O79, as one lang-template step and
  one kit commit. sealed-lang does not change. No mutant. RULED
  2026-10-18, defaults (a). To build in step 9.2k.
- O86 (R86): a Nat-indexed family (F15, O80): the kit shape `family Lot
  (size : Nat)` in `domain/domain.lang`, one example, one eval row, one
  check refusal row (an index mismatch) and one `test/fronttool.c` row.
  Mutant M15. RULED 2026-10-18, defaults (a). To build in step 9.3.
- O87 (R87): the eligibility circuit of the paper and a prover stay open.
  O50 and O58 stay PARTIAL. The circuit statement needs the paper text.
  The tools on this machine are circom and snarkjs (no gnark). A prover
  would be a one-shot generator out of the repo, with its output kept in
  the repo as constants. Docs only. RULED 2026-10-18, defaults (a). The
  records are built in step 9.1.
- O88 (R88): the rest of O7 and O44 (winner selection from the sealed bids
  on chain) is closed, not taken. The paper keeps the bids and the winner
  selection off chain (`docs/PLAN.md:33-35`, :66-68) and leaves settlement
  to the bid transaction (O7). `processBid` stays off chain (R76, O57).
  `settle` keeps its S8 form. Docs only. RULED 2026-10-18, defaults (a).
  Built in step 9.1.
- O89 (R89): S9 does not change the deposit runtime: 17069 bytes, 7507
  bytes under 24576 (O48). A step that changes it stops and asks the
  USER. Each step records the size in `docs/STATUS.md` `## S9`. No pin.
  RULED 2026-10-18, defaults (a). Applies to each step, 9.1 to 9.5.
- O90 (R90): the docs lines that the S9 survey found.
  `docs/VALIDATION.md:98-103` keeps the S5 facts as S5 facts and adds the
  S8 facts (R73, R75). The `SPEC.md` section 10 status line names
  `src/lower.c` as the site of the run-time Bag lowering (R84), not the
  kit. The visibility line stays: the USER did not confirm it. Docs only.
  RULED 2026-10-18, defaults (a). Built in step 9.1.
