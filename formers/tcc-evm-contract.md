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
- The formers F1 to F15 are checked and evaluated. On the EVM only the
  first-order word residual of a body lowers. A residual of another form
  gives REFUSE_LOWER (`test/lower/expect.txt`, 12 rows).

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
