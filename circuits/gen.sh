#!/bin/sh
# sealed-lang S11: the one-shot generator for the eligibility proof
# (docs/PLAN.md O104 (a)). This is not a test. `make check` never runs
# it. Its outputs enter the repository only as guest constants.
#
# Step 11.1 runs the compile. The step 11.2 commands (the setup, the
# key, the proofs, the calldata) are the comment lines marked "11.2";
# step 11.2 uncomments them.
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

# 11.2 the setup (R3 (a): a local powers of tau, one contributor, a
# 11.2 beacon, a test setup; the ptau and zkey files stay out of the
# 11.2 repository)
# 11.2 snarkjs powersoftau new bn128 15 "$OUT/pot15_0000.ptau"
# 11.2 snarkjs powersoftau contribute "$OUT/pot15_0000.ptau" "$OUT/pot15_0001.ptau" --name="sealed-lang test setup" -e="sealed-lang"
# 11.2 snarkjs powersoftau beacon "$OUT/pot15_0001.ptau" "$OUT/pot15_beacon.ptau" 0102030405060708090a0b0c0d0e0f101112131415161718191a1b1c1d1e1f20 10 -n="sealed-lang beacon"
# 11.2 snarkjs powersoftau prepare phase2 "$OUT/pot15_beacon.ptau" "$OUT/pot15_final.ptau"
# 11.2 the key
# 11.2 snarkjs groth16 setup "$OUT/eligibility.r1cs" "$OUT/pot15_final.ptau" "$OUT/eligibility_0000.zkey"
# 11.2 snarkjs zkey contribute "$OUT/eligibility_0000.zkey" "$OUT/eligibility_0001.zkey" --name="sealed-lang" -e="sealed-lang"
# 11.2 snarkjs zkey export verificationkey "$OUT/eligibility_0001.zkey" "$OUT/verification_key.json"
# 11.2 the three proofs (hu with cm1, hu with cm2, hu' with cm3): for
# 11.2 each N in 1 2 3, with the witness file "$OUT/input-N.json"
# 11.2 snarkjs wtns calculate "$OUT/eligibility_js/eligibility.wasm" "$OUT/input-N.json" "$OUT/witness-N.wtns"
# 11.2 snarkjs groth16 prove "$OUT/eligibility_0001.zkey" "$OUT/witness-N.wtns" "$OUT/proof-N.json" "$OUT/public-N.json"
# 11.2 snarkjs groth16 verify "$OUT/verification_key.json" "$OUT/public-N.json" "$OUT/proof-N.json"
# 11.2 snarkjs zkey export soliditycalldata "$OUT/public-N.json" "$OUT/proof-N.json"
# 11.2 the guest constants: test/vkfmt (C) turns verification_key.json
# 11.2 and proof-N.json into the VerifyKey and VerifyProof constants of
# 11.2 examples/eligibility.lang in the EIP-197 order of proofOf.
