# sealed-lang

sealed-lang is a language for the sealed-bid auctions of arXiv:2606.14939
on EVM chains. It comes from lang-template with the tcc-evm-contract host:
a TinyCC (tcc) host that compiles a program to EVM bytecode.

- `SPEC.md`: the specification.
- `design/DESIGN.md`: the domain design. It is the only source of the core
  types and the core operations.
- `formers/FORMERS.md`: the type formers. `formers/tcc-evm-contract.md` gives their
  status on the tcc-evm-contract host.
- `probe/CAPABILITY.md`: the host probe. `docs/host/CAPABILITY.md`: the
  kit probe of the host (slice K1) and the gas of each chain step.
- `docs/STATUS.md` and `docs/VALIDATION.md`: the status and the gate results.
- `docs/host/README.md`: how to build and test the host kit, and how to
  write the domain in `domain/`.
- `examples/`: the example programs. `examples/deposit.lang` is the
  deposit contract (S5). `examples/protocol.lang` is the protocol layer
  that never reaches the EVM (S6): the bid pool, the winner, the median of
  the timestamps and the bid check `processBid`. `langc check` prints the
  four laws of `domain/domain.lang` (the axiom report, `SPEC.md` section
  8).
- `circuits/`: the eligibility circuit of the paper (`eligibility.circom`,
  S11) and `gen.sh`, which compiles it with circom and makes the key and
  the proofs with snarkjs. circomlib stays out of the repository, and
  `make check` does not run `gen.sh`. `examples/eligibility.lang` checks
  a proof on the EVM.

Run `make check` in this directory. It builds `langc` and the test tools
and runs the gate (`docs/VALIDATION.md`). It needs tcc, clang and evm
(go-ethereum) on the PATH.

License: MIT or Apache-2.0, at your option (`LICENSE-MIT`,
`LICENSE-APACHE`).
