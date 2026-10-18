# sealed-lang specification (draft)

## 1. Purpose

sealed-lang is a language for censorship-resistant sealed-bid auctions on
EVM chains (`design/DESIGN.md:14`). The protocol is the paper of Alpos,
Heimbach, Nayak and Wadhwa, "Censorship-Resistant Sealed-Bid Auctions on
Blockchains", arXiv:2606.14939 (`docs/PLAN.md:7-10`). Its type formers are
F1 to F15 of `formers/FORMERS.md`. Its core data types and core operations
are only the types and operations of `design/DESIGN.md` (the design).
`design/DESIGN.md` names the domain and the paper only. `docs/PLAN.md`
section 2 (:31-72) and section 4 (:95-168) find the minimum types and
operations in the paper.

One program describes one contract: its state, its initial state, its
entries, its views and its events (section 2). `langc check` checks the
program with the cubical kernel and prints the axiom report (section 8).
`langc build` writes the EVM bytecode of the contract
(section 7). The compiler is `build/langc`. The host is TinyCC
(`docs/PLAN.md:16`, :21).

One contract goes on chain: the deposit contract of Algorithm 1
(`docs/PLAN.md:33`, `examples/deposit.lang`). Bids, timestamps and
inclusion lists stay off chain. Only the winning bid transaction goes to
the chain (:34-35). Algorithm 2 (the timestamper) and Algorithm 3 (the
inclusion-list proposer) are off-chain helpers that `langc eval` runs
(:66-71, `examples/protocol.lang`).

## 2. Programs

A program is one file. It has no import (the keywords are in
`src/front/lexer.c:20-30`). The kernel loads `domain/domain.lang`, then
the contract prelude `src/front/contract.c`, then the program
(`src/front/front.c:16-18`). The prelude names are core names
(`docs/host/README.md:66`). A program is a sequence of declarations:

```
def NAME : TYPE := TERM
state State := CONSTRUCTOR (FIELD : TYPE) ...
event NAME (FIELD : TYPE) ...
```

`langc build` requires the following state and initial-state definitions
(`docs/host/README.md:81-102`, `examples/deposit.lang:9-22`).
`langc check` and `langc eval` also accept programs containing only helpers,
such as `examples/circle.lang`.

- `state State := CONSTRUCTOR ...`: one state only, with the name `State`,
  no parameters and one constructor, whose name is chosen by the program
  (`makeState` in the deposit example). Its fields are first order. In
  state storage, a `Map` is a top-level field only; helpers may also use
  `Map` values.
- `def init : State := TERM`: the initial state. The name `init` is fixed.

An entry has the type `(env : Env) -> (s : State) -> words -> Option (Prod
State (List Out))`. `none` reverts. `some (pair s2 outs)` commits. A view
has the same parameters and returns one word type
(`docs/host/README.md:98-100`). `Env` and `Out` are the prelude families
(`src/front/contract.c:7-10`). An `event` is an `Out` constructor with
word fields (`docs/host/README.md:187-190`).

The compiler refuses these host forms in a program (`formers/FORMERS.md`
R1 to R5, :45-59):

| Form | Code | Source |
|---|---|---|
| a `family` (R1: only the domain declares a family) | REFUSE_DATA | `src/front/check.c:2780` |
| an `axiom` whose type is not a Path or PathP after its Pi binders | REFUSE_AXIOM_TYPE | `src/front/check.c:2757-2758` |
| an `axiom` with a law type (R2: only the domain declares a law) | REFUSE_AXIOM | `src/front/check.c:2760` |
| `def rec`, or a reference to a later name or to its own name (R3) | REFUSE_REC | `src/front/check.c:2784`, :1851 |
| a definition with a core name or a prelude name (R4) | REFUSE_NAME | `src/front/check.c:2211` |
| a state that is not the one `State` above, or a state field containing a nested `Map` | REFUSE_STATE | `src/front/check.c:2700-2718`, `docs/host/README.md:94`, :174 |
| a `Map` key outside `Nat`, `U256`, `Addr`, or a value outside `Nat`, `Flag`, `U256`, `Addr` | REFUSE_MAP | `src/front/check.c:1607-1609` |
| an event with more than 32 fields, a field that is not a word, or two fields with one name | REFUSE_EVENT | `src/front/check.c:2447-2459` |
| a write to a `history` field that rule D-9 does not accept | REFUSE_HISTORY_WRITE | `src/front/check.c:2619-2644` |

Thus a program cannot add a data type, an unproved fact or general
recursion. Recursion comes only from `fold` and `unfold` (F6, F7).
The compiler writes the target; a program cannot.

## 3. Type formers

The type formers are F1 to F15 of `formers/FORMERS.md`. This language uses
the tcc-evm-contract column of the realization matrix
(`formers/FORMERS.md:284-306`). `formers/tcc-evm-contract.md` gives the
host form of each former and each slice: K4 :7, S2 :36, S3 :53, S4 :71,
S5 :85, S6 :110. F1 to F6 and F8 to F11 are DONE (`formers/FORMERS.md:286-291`,
:293-296). The slice rows K4a0 and S2 to S6 are DONE (:301-306). The
table gives each former that has a deviation.

| ID | Status on tcc-evm-contract | Effect on this language | Open item |
|---|---|---|---|
| F7 | DONE in checking and evaluation (`formers/FORMERS.md:292`) | `unfold` runs into `List` only, with a `Nat` step limit. A residual `unfold` is not lowered to EVM code (`src/lower.c:775`, :404). | none |
| F12 | DONE as `Path` since S3 (:297) | Equality is the cubical `Path`: `refl`, `sym`, `trans`. There is no UIP. The EVM output erases a path. | O3 |
| F13 | DONE as `Path` (:298) | Transport is `transp`. `cong` is a BUILTINS row. Both erase. | O3 |
| F14 | DONE for `Type 0` and `Type 1` (:299) | There is no cumulativity. | none |
| F15 | PARTIAL (:300) | Only `domain/domain.lang` declares an indexed family. A program `family` gives REFUSE_DATA. The matrix cell gives the EVM output as PLANNED. | R1, O32 |

## 4. Structures

- **Monad**: `pure`, `map`, `bind` on `Option`, `Sum E` and `List`
  (`formers/FORMERS.md:290`). In an entry, `Option` is the revert monad:
  `none` reverts (`docs/PLAN.md:86`).
- **Algebra**: `fold` on `Nat`, `List` and each domain family
  (`formers/FORMERS.md:291`). `unfold` runs into `List`, with a `Nat`
  step limit (:292).
- **Filterable**: `filter` on `Option` and `List` (:293).

The host does not give these carriers. `domain/domain.lang` declares them:
the HITs `S1` (:33), `Trunc1` (:37) and `Trunc0` (:41), the quotient
`Quot` (:46-49) and the QIT `Bag` (:56-61: `bagNil`, `bagCons`, the path
constructor `bagSwap`, the truncation `bagTrunc`). A fold on a HIT or a
QIT has one case for each constructor, including each path constructor,
at the type determined by its boundary (`formers/tcc-evm-contract.md:76`).
For `S1`, the loop case is a path at the image of `base`; for `Trunc1`,
the `squash1` case is an `isProp` proof. The truncation case for `Trunc0`,
`Quot` or `Bag` is an `isSet` proof. A `Bag` fold also has a `bagSwap`
case (:114). `bagMax` and `sortBag` are `Bag` folds; `medianAt` uses
`sortBag` and folds over its resulting list.
The four laws are their path cases (`docs/PLAN.md:92`,
`examples/protocol.lang:8`, :25, :33).

## 5. Core types

The design names no type (`design/DESIGN.md:14`). `docs/PLAN.md` section
2 (:31-72) and section 4 (:102-103) give the minimum: the words `Nat`,
`Flag`, `U256` and `Addr`, and no other base type. `G1`, `G2`, the proof
and the key are products of words. The kit types and the domain types are:

| Type | Meaning (design section) | Definition |
|---|---|---|
| `Nat` | a count, an index, a time (PLAN 4, :102) | BUILTINS `src/front/check.c:100`. On the EVM a uint64 word. |
| `Flag` | a truth value (PLAN 4, :102) | `src/front/check.c:101`: `flagYes`, `flagNo`. The word 0 or 1. |
| `U256` | an amount, a hash, a field element (PLAN 2, :60-64) | `src/front/check.c:102`. One word. |
| `Addr` | an account: a caller, a token, a vault (PLAN 2, :60-64) | `src/front/check.c:103`. The low 160 bits of a word. |
| `Unit`, `Prod`, `Sum`, `Option`, `List` | products, coproducts and the monad carriers (`design/DESIGN.md:14`) | `src/front/check.c:104-108` |
| `Map K V` | a contract map (PLAN 2, :60-64) | `src/front/check.c:154`. Probe-forced: the value is a word, thus a record is one map for each field (O8, `docs/PLAN.md:310-311`). |
| `State` | the contract storage (PLAN 2, :33) | the `state` declaration of section 2 (`src/front/lexer.c:28`) |
| `Env` | the call: `now` and `caller` (PLAN 2, :60-64) | `family Env := makeEnv (now : Nat) (caller : Addr)` (`src/front/contract.c:7`). Probe-forced: `now` is TIMESTAMP (O13, `docs/PLAN.md:345-346`). |
| `Out` | the effects of an entry: a token transfer, an event (PLAN 2, :60-64) | `pay`, `pull`, `emit` (`src/front/contract.c:8-10`) |
| `Bid` | a bid (PLAN 2, :34-35) | `Prod U256 U256` (`domain/domain.lang:67`) |
| `Bag A` | the bid pool, a multiset (PLAN 2, :66-71) | a QIT (`domain/domain.lang:56-61`) |
| `S1`, `Trunc1`, `Trunc0`, `Quot` | loops, HITs and quotients (`design/DESIGN.md:14`) | `domain/domain.lang:33`, :37, :41, :46-49 |
| `G1Point`, `G2Point` | BN254 points for Groth16 (PLAN 2, :60-64) | `Prod U256 U256` and `Prod (Prod U256 U256) (Prod U256 U256)` (`domain/domain.lang:156-157`). Probe-forced: G2 is in EIP-197 order, (x_im, x_re), (y_im, y_re) (:153). |
| `VerifyKey`, `VerifyProof` | a Groth16 key and proof (PLAN 2, :60-64) | word products (`docs/PLAN.md:103`, `domain/domain.lang:158-159`) |

Probe-forced: an entry or a view takes one tuple of at most 32 words for
each call (O12, O23, `docs/PLAN.md:338-340`, :370; `ENTRY_PARAMS_MAX` in
`src/front/check.h:8`).

## 6. Core operations

The design names no operation (`design/DESIGN.md:14`). `docs/PLAN.md`
section 2 (:60-64: hash, a Merkle tree, Groth16 over BN254, a signature
check, caller, time, maps, a token transfer) and section 4 (:105-168) give
the minimum. BUILTINS is `src/front/check.c:99-166`, 66 rows
(`docs/PLAN.md:147-150`): 10 type rows, 23 constructor and eliminator rows
of F1 to F11, 11 path rows (S3), 14 kit operations and 8 S2 operations.
The table gives the operations and the path rows of BUILTINS, the prelude
names and the domain library. Each cite without a file is in the file of
the row above it.

| Operation | Type | Meaning (design section) |
|---|---|---|
| `natAdd`, `natSub` (`src/front/check.c:143-144`) | `Nat -> Nat -> Nat` | counts, indexes, times (PLAN 4) |
| `natEq`, `natLe` (:145-146) | `Nat -> Nat -> Flag` | compare counts and times (PLAN 4) |
| `u256Add`, `u256Sub` (:147-148) | `U256 -> U256 -> U256` | amounts (PLAN 2, :60-64) |
| `u256Le`, `u256Eq` (:149-150) | `U256 -> U256 -> Flag` | compare amounts and bids (PLAN 2) |
| `toU256` (:151) | `Nat -> U256` | a count as a word (PLAN 4) |
| `addrEq` (:152) | `Addr -> Addr -> Flag` | the caller check (PLAN 2, :60-64) |
| `flagIf` (:153) | `Flag -> A -> A -> A` | the choice (PLAN 4) |
| `mapEmpty`, `mapGet`, `mapSet` (:155-157) | `Map K V`, `Map K V -> K -> V`, `Map K V -> K -> V -> Map K V` | maps (PLAN 2, :60-64). A missing key reads as zero (`docs/host/README.md:168-170`). |
| `natDiv`, `natMod` (`src/front/check.c:158-159`) | `Nat -> Nat -> Nat` | division of counts (PLAN 4). DIV and MOD, a trap on 0 (`formers/tcc-evm-contract.md:40`). |
| `hash` (`src/front/check.c:160`) | `U256 -> U256 -> U256` | the hash (PLAN 2, :60-64). KECCAK256 of two words (`formers/tcc-evm-contract.md:41`). |
| `ecrecover` (`src/front/check.c:161`) | `U256 -> Nat -> U256 -> U256 -> Addr` | the signature check (PLAN 2). One STATICCALL to precompile 1 (`formers/tcc-evm-contract.md:43`). |
| `ecAdd`, `ecMul` (`src/front/check.c:162-163`) | `G1Point -> G1Point -> G1Point`, `G1Point -> U256 -> G1Point` | Groth16 over BN254 (PLAN 2). STATICCALL 6 and 7 (`formers/tcc-evm-contract.md:44`). |
| `ecPairing` (`src/front/check.c:164`) | `List (Prod G1Point G2Point) -> Flag` | the pairing check (PLAN 2). STATICCALL 8, 6 words for each pair (`formers/tcc-evm-contract.md:45`). |
| `addrToU256` (`src/front/check.c:165`) | `Addr -> U256` | an address as a word (PLAN 4, `formers/tcc-evm-contract.md:42`) |
| `I`, `i0`, `i1` (`src/front/check.c:109-111`) | the interval and its two ends | paths (`design/DESIGN.md:14`, PLAN 3 `docs/PLAN.md:88`). Erased (`formers/tcc-evm-contract.md:57`). |
| `PathP`, `Path`, `refl` (`src/front/check.c:112-113`, :125) | `PathP A x y`, `Path A x y`, `refl` | paths, no UIP. Erased (`formers/tcc-evm-contract.md:58`). |
| `sym`, `cong`, `funExt` (`src/front/check.c:138-140`) | `sym p`, `cong f p`, `funExt h` | path operations. `sym` and `cong` erase (`formers/tcc-evm-contract.md:59`). `funExt` is check only (:60). |
| `transp`, `hcomp` (`src/front/check.c:141-142`) | `transp` with 3 arguments, `hcomp` with 4 | the Kan operations. `transp` is the identity on base lines (`formers/tcc-evm-contract.md:61`). `hcomp` spends fuel (:62). |
| `now`, `caller` (`src/front/contract.c:7`) | `Env -> Nat`, `Env -> Addr` | time and caller (PLAN 2, :60-64). TIMESTAMP and CALLER (`docs/host/README.md:240`). |
| `pay` (`src/front/contract.c:8`) | token, to, amount: `Addr`, `Addr`, `U256` | the token transfer (PLAN 2). `transfer`, one CALL (`docs/host/CAPABILITY.md:27`). |
| `pull` (`src/front/contract.c:9`) | token, from, to, amount: `Addr`, `Addr`, `Addr`, `U256` | the deposit (PLAN 2). `transferFrom`, one CALL (`docs/host/CAPABILITY.md:27`). |
| `emit` (`src/front/contract.c:10`), `event` | `emitTag : Nat`, `emitFields : List U256`. A named event is an `Out` constructor. | events (PLAN 2). A named event is one LOG1 (`docs/host/README.md:244`). `emit` has no EVM form (:78). |
| `transport`, `subst`, `J`, `JRefl`, `trans` (`domain/domain.lang:8-21`) | the Kan library | paths (PLAN 3, `docs/PLAN.md:88`) |
| `isProp`, `isSet` (`domain/domain.lang:26`, :28) | the truncation levels | HITs and QITs (PLAN 3, `docs/PLAN.md:90-91`) |
| `bidLe`, `bidMax` (`domain/domain.lang:68`, :70) | on `Bid` | the bid order and the maximum (PLAN 3, `docs/PLAN.md:92`) |
| `insNatSeed`, `insNat` (`domain/domain.lang:86-87`) | on `List Nat` | the sort for the median (PLAN 3, `docs/PLAN.md:92`) |
| `treeDepth` to `rootOfLeaves` (`domain/domain.lang:103-149`) | the Merkle tree, depth 32 | the Merkle registry (PLAN 3, `docs/PLAN.md:80`; O42) |
| `listHead` to `verify` (`domain/domain.lang:151-215`) | the Groth16 verifier | Groth16 (PLAN 3, `docs/PLAN.md:81`). Exact on precompiles 6, 7 and 8 (`formers/tcc-evm-contract.md:90`). |
| `bidMaxSwap` (`domain/domain.lang:79`) | `(x : Bid) -> (y : Bid) -> (r : Bid) -> Path Bid (bidMax x (bidMax y r)) (bidMax y (bidMax x r))` | the law for the `bagSwap` case of `bagMax` (PLAN 3, `docs/PLAN.md:92`) |
| `bidSet` (`domain/domain.lang:80`) | `isSet Bid` | the law for the isSet case of `bagMax` |
| `insSwap` (`domain/domain.lang:100`) | `(x : Nat) -> (y : Nat) -> (l : List Nat) -> Path (List Nat) (insNat x (insNat y l)) (insNat y (insNat x l))` | the law for the `bagSwap` case of `sortBag` |
| `listNatSet` (`domain/domain.lang:101`) | `isSet (List Nat)` | the law for the isSet case of `sortBag` |

A law is an `axiom` of the domain (section 8). `docs/VALIDATION.md:140-150`
gives the reason that each law is true.

## 7. Target and instance encoding

The target is one EVM contract for each program: the creation code and
the runtime code (`docs/host/README.md:208-280`). There is no JSON
instance. The creation code writes each nonzero word of `init` with
SSTORE (:212). The runtime code has a dispatcher, a REVERT block, a
`Trap()` block and one block for each entry and view (:213).

The storage:

| Type | Encoding |
|---|---|
| `Nat`, `Flag`, `U256`, `Addr` | one word: a uint64, 0 or 1, a uint256, the low 160 bits (`docs/host/README.md:218-219`) |
| state field i | slot i (`docs/host/README.md:218`) |
| `List T` field, T a word type | the length at slot s. Element j is at keccak256(s) + (n - 1 - j) (`docs/host/README.md:220`). |
| `Map K V` field | the value of key k is at keccak256(k . s). A missing key reads as zero (`docs/host/README.md:222`, :168-170). |
| an `Option`, `Prod`, nested `List` or non-word `List` field | REFUSE_LOWER. Another `Map` type gives REFUSE_MAP. A nested `Map` gives REFUSE_STATE (`formers/tcc-evm-contract.md:14-18`). |

The entries and the guards:

- An entry is nonpayable. Its selector is the first four bytes of the
  keccak256 of `name(t1,...)`,
  with Nat `uint64`, Flag `bool`, U256 `uint256` and Addr `address`
  (`docs/host/README.md:226-229`). An entry takes one tuple of at most 32
  words (section 5, probe-forced).
- An entry body is first-order word code. `some (pair STATE OUT)` stores
  the state. `none` reverts with empty data. A trap reverts with `Trap()`,
  selector `0xae96083a` (`formers/tcc-evm-contract.md:19-21`,
  `docs/host/README.md:240-244`).
- `now` is TIMESTAMP and `caller` is CALLER (`docs/host/README.md:240`).
  A named event is one LOG1, before the stores (:244).
- `pay` and `pull` CALL the token address supplied to the constructor
  (`src/front/contract.c:8-9`). In the deposit example that token is
  `asset`, and `vault` must be the address of the deposit contract
  (`formers/tcc-evm-contract.md:29`, :92, :106-108).
  A CALL forwards all gas and 0 wei. The return must be the
  word 1, or empty with EXTCODESIZE not zero, else the entry reverts
  (`docs/host/CAPABILITY.md:28-29`).
- A view returns one word, 32 bytes (`formers/tcc-evm-contract.md:22-28`,
  `docs/host/README.md:249`). A residual fold over a `List` field runs
  from the last element to the head. Its seed and step must lower to
  words. If either can trap, the step must be provably strict in its
  accumulator, else REFUSE_LOWER (`src/lower.c:502-520`).
- The deposit contract (`examples/deposit.lang`) has 9 entries (O43,
  `docs/PLAN.md:466`) and 10 views: the nine accessors (O8) and
  `byUserOrProposer`, whose type also gives it the view role
  (`examples/deposit.lang:144`, `src/lower.c:208-223`). Its runtime code is 17119 bytes
  (R48, `formers/tcc-evm-contract.md:92`), under the EIP-170 limit of
  24576 bytes (`docs/host/README.md:255`). The sketch of PLAN section 6 is
  old: O42 removes its `depth` field (`docs/PLAN.md:464-465`).

The erasure of proofs:

- Paths, the interval, HIT path constructors and QIT path cases have no
  run-time form (`docs/PLAN.md:93`).
- A Path on a word type is never an equality test. A closed path erases.
  A residual path gives REFUSE_LOWER (O24, `docs/PLAN.md:376-378`).
- A point constructor lowers as a family value. A path constructor or a
  formal hcomp gives REFUSE_LOWER (O31, `docs/PLAN.md:404-407`).
- A path at run time in an entry gives REFUSE_LOWER (O37,
  `docs/PLAN.md:435-437`).
- A law has no run-time value: REFUSE_LOWER (`src/lower.c:391`, :404).

## 8. Host and target

The host is TinyCC. The target check runs geth `evm`. The pinned
versions are in `docs/host/CAPABILITY.md:7-8` (the kit K1 probe,
2026-10-07, macOS AArch64, :3): tcc 0.9.28rc 2026-09-04 mob@0fb54300 and
geth `evm` 1.14.12-stable. These probe facts set the design:

- `evm run --code HEX` runs the creation code and the runtime code (:13).
  The tests use chains of `evm run --prestate ... --dump`, with no port
  and no daemon (:20).
- `pay` is `transfer` and `pull` is `transferFrom`, one CALL each, with
  the contract as the sender (:27).
- Each CALL forwards all gas and 0 wei. The return must be the word 1, or
  empty with EXTCODESIZE not zero, else a REVERT with empty data (:28-29).

The axiom report (S6, R52 (a)). `langc check FILE` prints one line
`axiom NAME` for each law of `domain/domain.lang`, in the order of the
domain file, then `ok`. At S6 the report is:

```
axiom bidMaxSwap
axiom bidSet
axiom insSwap
axiom listNatSet
ok
```

`test/gate.sh` compares this report for each example. A law is an
`axiom` in `domain/domain.lang` only. Its type after the Pi binders must
be a Path or a PathP (`isSet A` counts), else REFUSE_AXIOM_TYPE. An axiom
in a program is REFUSE_AXIOM. A law is a neutral in `langc eval`, and the
lowering refuses it (REFUSE_LOWER). `docs/VALIDATION.md` gives the reason
that each of the four laws is true.

## 9. Open items

> Write here: one item for each open question. Number the items O1, O2 and
> so on. Mark each decision as "RULED YYYY-MM-DD (USER): <the ruling>". Mark
> each fact that a probe found as "Probe-forced".

- O1. <question>.

## 10. Milestones

> Write here: the content of each milestone, and a dated status line under
> the current milestone.

| Milestone | Content |
|---|---|
| M0 | Host probe (`probe/CAPABILITY.md`); the core types construct and check; refusal list; examples and tests |
| M1 | Core operations |
| M2 | Queries and reads, or the contract |
| M3 | Hardening: speed, limits, a checked certificate for the output |
