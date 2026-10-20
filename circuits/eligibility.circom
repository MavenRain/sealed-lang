pragma circom 2.0.0;

// sealed-lang S11 step 11.1: the eligibility proof pi^m.
//
// Paper: arXiv:2606.14939, section 5.3 ("Zero-Knowledge proof of
// membership") and Algorithm 2 (elgPrv, line 7). Plan rows:
// docs/PLAN.md O100 (the relation), O101 (R1 (a): Poseidon for every
// hash in the circuit) and O102 (R2: each public word enters the
// verifier as two 128-bit limbs, high then low).
//
// The relation, with H = Poseidon:
//   C  = H(rho)
//   hu = H(rho, aucId)
//   (addr, C) is a leaf of the Merkle tree with the root `root`:
//     leaf = H(addr, C), node = H(left, right) along the path
//   cm = H(tx, C)
// The statement is (root, hu, cm) as six limbs: rootHi, rootLo, huHi,
// huLo, cmHi, cmLo. The witnesses are rho, aucId, addr (A_u as one
// scalar, the addrToU256 form of O16), tx, pathElements and pathIndex.
// C is computed inside the circuit. There is no activity term (O100).
//
// The includes resolve from a circomlib 2.0.5 install outside the
// repository (`circom -l <circomlib>/circuits`, see gen.sh; O104).

include "poseidon.circom";
include "bitify.circom";

// A field value as two 128-bit limbs. Num2Bits_strict binds the 254
// bits to the canonical value below r (AliasCheck), so a public pair
// that encodes x + r has no witness (O102). The limbs satisfy
// hi < 2^126 and lo < 2^128.
template Limbs() {
    signal input in;
    signal output hi;
    signal output lo;

    component bits = Num2Bits_strict();
    bits.in <== in;

    var low = 0;
    var weight = 1;
    for (var i = 0; i < 128; i++) {
        low += bits.out[i] * weight;
        weight = weight * 2;
    }
    var high = 0;
    weight = 1;
    for (var i = 128; i < 254; i++) {
        high += bits.out[i] * weight;
        weight = weight * 2;
    }
    lo <== low;
    hi <== high;
}

// One Merkle level. bit = 0 puts the current node on the left, bit = 1
// on the right. The right child is linear in the left child, so each
// level costs one constraint for the switch plus the hash.
template Node() {
    signal input cur;
    signal input sib;
    signal input bit;
    signal output out;

    bit * (1 - bit) === 0;

    signal left;
    signal right;
    left <== cur + bit * (sib - cur);
    right <== cur + sib - left;

    component h = Poseidon(2);
    h.inputs[0] <== left;
    h.inputs[1] <== right;
    out <== h.out;
}

template Eligibility(depth) {
    signal input rho;
    signal input aucId;
    signal input addr;
    signal input tx;
    signal input pathElements[depth];
    signal input pathIndex[depth];

    signal output rootHi;
    signal output rootLo;
    signal output huHi;
    signal output huLo;
    signal output cmHi;
    signal output cmLo;

    // C = H(rho)
    component cu = Poseidon(1);
    cu.inputs[0] <== rho;

    // leaf = H(addr, C)
    component leaf = Poseidon(2);
    leaf.inputs[0] <== addr;
    leaf.inputs[1] <== cu.out;

    // the path from the leaf to the root
    component node[depth];
    for (var i = 0; i < depth; i++) {
        node[i] = Node();
        if (i == 0) {
            node[i].cur <== leaf.out;
        } else {
            node[i].cur <== node[i - 1].out;
        }
        node[i].sib <== pathElements[i];
        node[i].bit <== pathIndex[i];
    }

    // hu = H(rho, aucId)
    component hu = Poseidon(2);
    hu.inputs[0] <== rho;
    hu.inputs[1] <== aucId;

    // cm = H(tx, C)
    component cm = Poseidon(2);
    cm.inputs[0] <== tx;
    cm.inputs[1] <== cu.out;

    // the statement as limbs, in the order root, hu, cm
    component rootLimbs = Limbs();
    rootLimbs.in <== node[depth - 1].out;
    rootHi <== rootLimbs.hi;
    rootLo <== rootLimbs.lo;

    component huLimbs = Limbs();
    huLimbs.in <== hu.out;
    huHi <== huLimbs.hi;
    huLo <== huLimbs.lo;

    component cmLimbs = Limbs();
    cmLimbs.in <== cm.out;
    cmHi <== cmLimbs.hi;
    cmLo <== cmLimbs.lo;
}

// depth 32 = treeDepth (O42, O100). All inputs are private; the six
// outputs are the public signals, in declaration order.
component main = Eligibility(32);
