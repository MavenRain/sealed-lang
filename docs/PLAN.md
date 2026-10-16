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
| Bid pool | `ℬ`, `max(ℬ)`, median | `Bag A`: the QIT of finite multisets (`bagNil`, `bagCons`, `swap`, `trunc`); `max` and `median` as folds with the commutation paths | S6 |
| Erasure | the EVM | a path, an interval term, a HIT path constructor and a QIT path case have no run-time form; the lowering keeps the point constructors and refuses a path at run time (`REFUSE_LOWER`) | S4 |

## 4. The base span

The span is the set of core types and core operations a program can use.
The design says the minimum that implements the contracts. This is the
proposal. S2 measures the kit's current span and removes what section 2
does not use (O12).

Core types: `Nat`, `Flag`, `U256`, `Addr`. No other base type. `G1`, `G2`,
`Proof` and `VKey` are products of words, not base types.

Core operations, by group:

- Kit, kept: `natAdd`, `natSub`, `natMul`, `natEq`, `natLe`, `flagIf`,
  `u256Add`, `u256Sub`, `u256Eq`, `u256Le`, `addrEq`, `mapEmpty`,
  `mapGet`, `mapSet`, the `Env` projections, the `Out` constructors `pay`
  and `pull`, and the events. The S1 survey lists the exact kit set.
- New, EVM precompiles: `hash`, `ecrecover`, `ecAdd`, `ecMul`,
  `ecPairing`. In `langc eval`, `hash` computes (the kit has keccak); the
  four curve operations are opaque: a closed application stays a neutral
  term. On the EVM each is one STATICCALL to its precompile. The chain test
  on `evm` is the differential gate for them (O5).
- New, for the tree: the index arithmetic of a depth-32 tree needs a
  halving and a parity test, as `natDiv2` and `natOdd`, or as `u256Shr`
  and `u256And`. The S1 survey picks the pair the kit is closest to.
- New, core laws: `fold` on a QIT into `U256` needs a path for each path
  constructor. The commutation laws of `u256Max` (for `max`) and the laws
  the median fold needs are core path constants, trusted by the kernel and
  never run (O10). The S6 kickoff lists each law.

Removed from the kit span: the sample domain (`Color`, `Item`, `Stack`,
`Lot`), `Eq` with `refl`, `symm`, `trans`, `transport` and `cong` (O3), and
each kit operation that no entry, view or helper of section 6 uses.

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
  checks where a check is a term (the paper's Theorems 8 to 10 are
  not terms of this language; `docs/VALIDATION.md` says so).
- S7: `SPEC.md`, `probe/CAPABILITY.md`, `docs/STATUS.md`,
  `docs/VALIDATION.md` and `README.md`. Final gate.
- Gate: `make check` in this directory. Mutants one at a time, `make -B`
  for each build.

## 8. Open items

- O1: the name sealed-lang.
- O2: the kit tcc-evm-contract, with the cubical kernel ported from
  coin-lang into this repository. Alternatives: start from coin-lang and
  port the EVM target; or a new lang-template kit.
- O3: the kit `Eq` goes in S3 and `Path` takes its place, as coin-lang did.
  Alternative: keep `Eq` as a name for `Path`.
- O4: `hash` is KECCAK256 of the packed 32-byte words. The paper's
  implementation uses Poseidon in the circuits. A circuit that proves a
  keccak Merkle path is costly; an on-chain Poseidon is a library, not a
  base operation. Default: keccak now, Poseidon as a later library slice if
  the circuits need it.
- O5: the four curve operations are opaque in `langc eval` and exact on
  the EVM. A C99 BN254 pairing under TinyCC is a later option. The
  Groth16 fixture for S5 is a known-good `(vk, inputs, proof)` triple; its
  source is a ruling.
- O6: the deposit asset is an ERC-20 token through the kit `pull` and
  `pay`. Native value needs CALLVALUE and a value call, which the kit does
  not have.
- O7: `settle(id, bid)` is an entry of the same contract: it records the
  winner and pulls the payment. The paper leaves settlement to the bid
  transaction. Alternative: out of scope.
- O8: a `Map` value is a word, so an auction record is one map per field
  and `getAucRecord` is one view per field. Alternative: extend `Map` to
  product values (kit work).
- O9: the registry is an incremental Merkle tree of depth 32 with a root
  per slot, as a guest library. Alternative: a list of leaves with a full
  recomputation per insert.
- O10: no Glue and no univalence (not in the request). The core laws for
  the QIT folds are path constants of the span, listed one by one in the
  S6 kickoff. Alternative: port coin-lang S3 and prove the laws on a
  finite model.
- O11: loops come from the HIT declaration form; the kernel test is the
  circle. No domain family has a loop constructor by default. Alternative:
  the slot clock as a cyclic quotient.
- O12: `Π` enters one NOP tuple per call. Alternative: array arguments in
  the ABI decoder (kit work).
- O13: `now` is the slot number. The S1 survey tells which EVM word the
  kit reads for `now`.
- O14: the proposer set is a `Map Addr Flag` fixed in `init`.
- O15: tests in the guest language and `langc run` scripts only.
