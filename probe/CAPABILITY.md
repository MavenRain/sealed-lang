# Host capability probe: tcc-evm-contract

Date: 2026-10-07 (the kit K1 probe, `docs/host/CAPABILITY.md:3`), filled
for sealed-lang on 2026-10-18. Host tree:
`/Users/oobi/Documents/lang-template` (`hosts/tcc-evm-contract`) at
1f925d1, read only. 1f925d1 is the last lang-template commit that changes
the kit before the seed beae7ec (2026-10-15). At beae7ec, `src/evm.c`,
`src/lower.c` and `test/gate.sh` are the same blobs as the kit at 1f925d1.
Driver: `build/langc` (built by `make -B check`). Machine: macOS, AArch64
(`docs/host/CAPABILITY.md:3`). Sealed-lang ran no new probe (R68 (a)).
Each fact below comes from the kit K1 probe, the S1 kickoff section 0
(`lang-template-work/sealed-lang-S1-kickoff.md`), the slice records in
`docs/STATUS.md` or the refusal codes in `src/front/check.c`. No wall time
in this file is a sealed-lang measurement.

To run the probe again: no run script exists. `probe/` holds this file and
`probe/guard.py` only, and a new run script is out (O15). Each step must
run below `probe/guard.py`, which stops a command above 4096 MB resident
memory or above 120 s, the defaults (`probe/guard.py:92-93`). The probe
writes only below a new temporary directory. No probe process ran for
sealed-lang, so no peak memory is recorded.

## Questions

Each host must answer these questions. Each answer is one numbered section
below. An answer sets the status of a former in `formers/tcc-evm-contract.md`.

| Question | Section | Formers |
|---|---|---|
| Which commands check a file, print a normal form, build and run? | 1 | all |
| Does the host take several files, or must the driver join them? | 2 | all |
| Which recursion does the host accept, and how does it refuse the rest? | 3 | F6, F7 |
| Which `Nat` primitives exist? Does `Nat` have an eliminator? | 4 | F6, F7 |
| How does a result leave the host? | 5 | target |
| What do check and build cost (time and resident memory) at N = 100 and N = 1000? | 6 | all |
| Can a family take a parameter? Can a constructor of such a family appear in a term? | 7 | F4, F12, F15 |
| Can a program project a Sigma? Does the kernel accept Sigma eta? | 8 | F11 |
| How does a program use paths: `Path`, `transport`, `cong`, an index type? | 9 | F12, F13 |
| How does the host report an axiom? | 10 | R2 |

## 1. Commands

| Job | Command | Result |
| --- | --- | --- |
| Check | `build/langc check FILE` | The axiom report, then `ok` (section 10), or one refusal code (sections 3 and 10) |
| Print normal form | `build/langc eval FILE NAME [ARGS...]` | The result of the named definition. Exit 0, 1 after a diagnostic, or 2 after an EVAL_ARGS diagnostic (`src/front/check.h:25-27`) |
| Build | `build/langc build FILE`; `--runtime` writes the runtime code only (`docs/PLAN.md:495-496`) | One hex line. Exit 0, 1 after REFUSE_LOWER, EVM_SIZE or OOM, or 2 after IO_WRITE (`src/lower.h:11-13`) |
| Run | `evm run --code HEX` (`docs/host/CAPABILITY.md:13`); chains of `evm --verbosity 0 run --prestate FILE --gas GAS --input DATA --value VALUE --json --dump` (:16, :20) | `0x<output>`. Each call reads the state that the previous call wrote |

- `build/langc run FILE SCRIPT` runs a call script on the state from
  `init` and prints each call and the final state
  (`src/main.c:50-51`, `src/front/check.h:28-31`).
- One file for each command (section 2).
- `test/evm.sh` runs 6 chains of `evm run --prestate ... --dump` (43
  steps, `docs/host/CAPABILITY.md:26`). `test/diff.sh` runs the CALL
  failure table (10 rows) and 6 call scripts on the mock ERC-20: the 5 kit
  scripts and `deposit.script` (S5, O47, `test/diff.sh:192-198`). Thus
  the diff count is 16 (`docs/STATUS.md:341`), not the kit count 5 of
  `docs/host/CAPABILITY.md:26`.
- Not probed: `evm transition` (t8n), `evm statetest` and anvil 0.3.0
  (`docs/host/CAPABILITY.md:14-15`, :50-52). The gate uses none of them.

## 2. Several files

No. A program is one file, with no import form (`src/front/lexer.c:20-30`).
The driver joins three parts in this order: `domain/domain.lang`, the
contract prelude `src/front/contract.c`, then the program
(`src/front/front.c:16-18`). The domain is compiled into `build/langc`
(`docs/STATUS.md:402`).

## 3. Recursion and totality

A definition can use only earlier definitions (rule R3). `def rec` gives
REFUSE_REC with the line `def rec is refused (rule R3); use fold or
unfold` (`src/front/check.c:2750`). A reference to the definition itself
or to a later name gives REFUSE_REC with the line `NAME is this definition
or a later one; a definition can use only earlier ones (rule R3)` (:1817).
Recursion is `fold` over a family (F6) and `unfold` into `List` with a
`Nat` step limit (F7, `SPEC.md` section 3).

## 4. Nat

The primitives are the `Nat` rows of `SPEC.md` section 6: `natAdd` and
`natSub` (`src/front/check.c:143-144`), the kit comparisons, and `natDiv`
and `natMod` (S2), which trap on 0. S2 pruned `natMul` (O19). A definition
recurses on a `Nat` only through `fold` (section 3). On the EVM a `Nat` is
a uint64 word (`docs/host/README.md:218-219`). `natSub` saturates at zero:
`natSub 4 9` is 0 in `langc eval` (`src/front/eval.c:1117`) and in the
EVM output (`src/lower.c:411`, `src/evm.c:187`). The existing emitter
regression `sub-saturates` checks 4 minus 9 gives 0
(`test/emittool.c:129`).

## 5. How a result leaves the host

As one EVM contract: the creation code and the runtime code (`SPEC.md`
section 7). A view returns one word, 32 bytes
(`formers/tcc-evm-contract.md:22-28`). A named event is one LOG1. The
tests read the state that `evm run --prestate ... --dump` writes
(`docs/host/CAPABILITY.md:16`, :20). The guest tests read the result of
`build/langc eval` and `build/langc run` (section 1).

## 6. Cost

Not probed at N = 100 or 1000. No file in the repository gives the cost of
a check, a build or a run at N bids.

| Step | N = 100 | N = 1000 |
| --- | --- | --- |
| Check | not probed | not probed |
| Build | not probed | not probed |
| Run | not probed | not probed |

The nearest data:

- The deposit runtime code is 17103 bytes, 7473 bytes under the EIP-170
  limit of 24576 bytes (O48, `docs/PLAN.md:497`, `docs/STATUS.md:527-529`).
- The lowertool arena is 32 MiB (`docs/STATUS.md:331`).
- The deposit EVM chain does 41 calls with 0 failures
  (`docs/STATUS.md:245`). The deposit call script has 23 calls
  (`docs/STATUS.md:283`).
- The gas of each kit chain step (`docs/host/CAPABILITY.md:31-48`,
  measured 2026-10-10 at LT 382e224): for example, a deposit with one pull
  CALL on the mock uses 73471 gas (:46). No gas is measured for
  `examples/deposit.lang`.
- The property rows of S6 check fixed inputs only (`docs/STATUS.md:386`).

## 7. Parametric families

Only `domain/domain.lang` declares a family. A family in a program gives
REFUSE_DATA (`src/front/check.c:2746`, rule R1). The domain families
`Trunc1`, `Trunc0` and `Quot` take a type parameter (O32,
`docs/PLAN.md:414-416`), and so does `Bag A` (`domain/domain.lang:51-61`,
O51). Their constructors appear in terms in `check` and `eval`. F15 is
PARTIAL (`formers/FORMERS.md:300`): EVM output of an indexed family is
planned. Probe file: `domain/domain.lang`.

## 8. Sigma projection and eta

`witness` and `payload` check: `examples/sigma.lang:4-5` (F11). The form
`(n : Nat) * T` is also a Sigma type (:6). The kit cell of
`formers/FORMERS.md:296` gives F11 DONE in `check` and `eval`, and DONE in
EVM output for the first-order word residual of a body. Another residual
gives REFUSE_LOWER. Sigma eta is checked by the existing regression in
`test/fronttool.c:128`: for a neutral `p : Sigma (n : Nat) (Path Nat n n)`,
`refl` checks at `Path (Sigma (n : Nat) (Path Nat n n))
(pack (witness p) (payload p)) p`. A Sigma pattern is not probed for
sealed-lang.

## 9. Equality

A program writes `Path`, `PathP`, `refl`, `sym`, `cong`, `funExt`,
`transp` and `hcomp` (the 11 path rows of `SPEC.md` section 6, S3).
`transport`, `J` and `trans` are guest definitions in
`domain/domain.lang:8-21` (O25). The kit `Eq` is out since S3 (O3,
`docs/PLAN.md:278`). The families with path constructors are `S1`,
`Trunc1`, `Trunc0` and `Quot` (S4, O31, O32) and `Bag` (S6, O51), all in
`domain/domain.lang`. A Path on a word type is never an equality test
(O24). `Trunc1`, `Trunc0` and `Quot` take their type as a parameter (O32),
so one family serves each index type.

## 10. Axiom detection

`build/langc check FILE` prints one line `axiom NAME` for each law of
`domain/domain.lang`, in the order of the domain file, then `ok`
(`report_axioms`, `src/front/check.c:2792`, called at `src/main.c:183`).
Each program loads the domain, so a program with no axiom prints the four
lines `axiom bidMaxSwap`, `axiom bidSet`, `axiom insSwap` and `axiom
listNatSet`, then `ok` (`SPEC.md` section 8). `test/gate.sh` compares this
report for each example. A program with one axiom gives REFUSE_AXIOM
(`src/front/check.c:2726`). A domain axiom whose type is not a Path or a
PathP after its Pi binders gives REFUSE_AXIOM_TYPE (:2724). The 7.1 row of
`test/fronttool.c` checks it (R61 (a)).

## Consequences for M0

1. A program is one file. The driver joins it after the domain and the
   contract prelude (section 2).
2. No `def rec`: recursion is `fold` and `unfold` (section 3, R3).
3. The tests chain `evm run --prestate ... --dump`, with no port and no
   daemon. The gate uses no anvil (section 1,
   `docs/host/CAPABILITY.md:20-22`).
4. `pay` and `pull` are one CALL each, with all gas and 0 wei. The return
   must be the word 1, or empty with EXTCODESIZE not zero, else the step
   does a REVERT with empty data (`docs/host/CAPABILITY.md:27-29`).
5. A law is a domain axiom with a Path or PathP type. The axiom report is
   part of the gate (section 10, R2).
6. An entry takes one tuple of at most 32 words (ENTRY_PARAMS_MAX,
   `src/front/check.h:8`, O12, O23).
