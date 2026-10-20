#!/bin/sh
# sealed-lang S11: the one-shot generator for the eligibility proof
# (docs/PLAN.md O104 (a)). This is not a test. `make check` never runs
# it. Its outputs enter the repository only as guest constants.
#
# Step 11.1 added the compile. Step 11.2 added the fixture tree, the
# setup, the key, the proofs and the calldata.
#
# CIRCOMLIB is the `circuits` directory of a circomlib 2.0.5 install
# outside the repository (for example
# `npm --prefix <dir> install circomlib@2.0.5` gives
# <dir>/node_modules/circomlib/circuits). circomlib is GPL-3.0 and does
# not enter the repository. OUT defaults to build/circuits (ignored by
# git).
set -eu

HERE=$(dirname "$0")
OUT=${OUT:-"$HERE/../build/circuits"}
CIRCOMLIB=${CIRCOMLIB:?set CIRCOMLIB to the circomlib circuits directory}

mkdir -p "$OUT"

# 11.1 the compile
circom "$HERE/eligibility.circom" --r1cs --wasm --sym -l "$CIRCOMLIB" -o "$OUT"
snarkjs r1cs info "$OUT/eligibility.r1cs"

# 11.2 the fixture tree (fixture.circom): its witness holds leafU,
# leafU2 and the empty-subtree hashes E[0] to E[31] (witness entries 1
# to 34). circuits/input-1.json, input-2.json and input-3.json hold
# these values as decimal strings.
circom "$HERE/fixture.circom" --wasm --sym -l "$CIRCOMLIB" -o "$OUT"
echo '{}' >"$OUT/fixture-input.json"
snarkjs wtns calculate "$OUT/fixture_js/fixture.wasm" "$OUT/fixture-input.json" "$OUT/fixture.wtns"
snarkjs wtns export json "$OUT/fixture.wtns" "$OUT/fixture.json"

# 11.2 the setup (R3 (a): a local powers of tau, one contributor, a
# beacon, a test setup; the ptau and zkey files stay out of the
# repository)
snarkjs powersoftau new bn128 15 "$OUT/pot15_0000.ptau"
snarkjs powersoftau contribute "$OUT/pot15_0000.ptau" "$OUT/pot15_0001.ptau" --name="sealed-lang test setup" -e="sealed-lang"
snarkjs powersoftau beacon "$OUT/pot15_0001.ptau" "$OUT/pot15_beacon.ptau" 0102030405060708090a0b0c0d0e0f101112131415161718191a1b1c1d1e1f20 10 -n="sealed-lang beacon"
snarkjs powersoftau prepare phase2 "$OUT/pot15_beacon.ptau" "$OUT/pot15_final.ptau"

# 11.2 the key
snarkjs groth16 setup "$OUT/eligibility.r1cs" "$OUT/pot15_final.ptau" "$OUT/eligibility_0000.zkey"
snarkjs zkey contribute "$OUT/eligibility_0000.zkey" "$OUT/eligibility_0001.zkey" --name="sealed-lang" -e="sealed-lang"
snarkjs zkey export verificationkey "$OUT/eligibility_0001.zkey" "$OUT/verification_key.json"

# 11.2 the three proofs: input 1 is (hu, cm1), input 2 is (hu, cm2) and
# input 3 is (hu', cm3), all with one root.
for N in 1 2 3; do
  snarkjs wtns calculate "$OUT/eligibility_js/eligibility.wasm" "$HERE/input-$N.json" "$OUT/witness-$N.wtns"
  snarkjs groth16 prove "$OUT/eligibility_0001.zkey" "$OUT/witness-$N.wtns" "$OUT/proof-$N.json" "$OUT/public-$N.json"
  snarkjs groth16 verify "$OUT/verification_key.json" "$OUT/public-$N.json" "$OUT/proof-$N.json"
  snarkjs zkey export soliditycalldata "$OUT/public-$N.json" "$OUT/proof-$N.json"
  echo
done

# 11.2 the guest constants: build/vkfmt (test/vkfmt.c) turns
# verification_key.json and proof-N.json into the VerifyKey and
# VerifyProof constants of examples/eligibility.lang in the EIP-197
# order of proofOf.
