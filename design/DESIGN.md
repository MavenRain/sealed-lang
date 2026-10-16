# sealed-lang domain design

> Paste the domain design below the line, verbatim. Do not edit it.

This file is the only source of the core types and the core operations of
sealed-lang. `SPEC.md` sections 5 and 6 cite its sections. Do not add a core
type or a core operation that this file does not give.

When the design changes, paste the new version. Then record the change as an
open item in `SPEC.md` section 9.

---

I want to write a new programming language for the purpose of constructing censorship-resistant sealed-bid auctions on EVM chains.  Type constructors and higher-kinded types should include: products, coproducts, monads (map/bind), algebras/coalgebras (fold/unfold), quotients, paths, loops, HITs and QITs (all like in Cubical TT).  Only admit the minimum span of base types and operations in order to implement contracts as in https://arxiv.org/pdf/2606.14939.  The host language shall by TinyCC, and the compilation target should be the EMV bytecode of contracts targeting any EVM-based chain.
