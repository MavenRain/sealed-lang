pragma circom 2.0.0;

// sealed-lang S11 step 11.2: the fixture tree of the three test proofs.
//
// This circuit has no inputs, and no proof uses it. Its witness gives the values of circuits/input-1.json, input-2.json and
// input-3.json (gen.sh compiles it and exports the witness). The
// hashes are the hashes of eligibility.circom (Poseidon, O101 R1 (a)).
//
// The tree has depth 32 and two leaves:
//   index 0: u  = (addr 3, rho 1), leafU  = H(3, H(1))
//   index 1: u' = (addr 5, rho 7), leafU2 = H(5, H(7))
// The value of an empty leaf is 0. The hash of an empty subtree of
// height i is E[i]: E[0] = 0 and E[i + 1] = H(E[i], E[i]). On the path
// of each leaf, the sibling at level 0 is the other leaf and the
// sibling at level i (1 to 31) is E[i]. Step 11.3 uses the same empty
// tree on the chain (O101, O106 11.3).
//
// The include resolves from a circomlib 2.0.5 install outside the
// repository (`circom -l <circomlib>/circuits`, see gen.sh; O104).

include "poseidon.circom";

// The leaf of (addr, rho): H(addr, H(rho)), as in Eligibility.
template Leaf(addr, rho) {
    signal output out;

    component cu = Poseidon(1);
    cu.inputs[0] <== rho;

    component leaf = Poseidon(2);
    leaf.inputs[0] <== addr;
    leaf.inputs[1] <== cu.out;
    out <== leaf.out;
}

template Fixture(addrU, rhoU, addrU2, rhoU2, depth) {
    signal output leafU;
    signal output leafU2;
    signal output empty[depth];

    component u = Leaf(addrU, rhoU);
    leafU <== u.out;

    component u2 = Leaf(addrU2, rhoU2);
    leafU2 <== u2.out;

    // empty[i] = E[i]
    component h[depth - 1];
    empty[0] <== 0;
    for (var i = 0; i < depth - 1; i++) {
        h[i] = Poseidon(2);
        h[i].inputs[0] <== empty[i];
        h[i].inputs[1] <== empty[i];
        empty[i + 1] <== h[i].out;
    }
}

// The outputs are the public signals, in declaration order: leafU,
// leafU2, then empty[0] to empty[31].
component main = Fixture(3, 1, 5, 7, 32);
