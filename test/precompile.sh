#!/bin/sh
# The precompile gate (slice S2, kickoff 1.5). Deploy examples/precompile.lang,
# then do one `evm run --prestate` step for each view and compare the result
# word with a constant. Each constant was confirmed at build (2026-10-15, evm
# 1.14.12) with a direct `evm run --input HEX --receiver 0x..0N` to the
# precompile address N (1 ecrecover, 6 ecAdd, 7 ecMul, 8 ecPairing) before it
# entered this file. `langc run` cannot give these words: the evaluator keeps a
# curve op stuck (R17), thus this gate compares the chain result only. It needs
# evm (go-ethereum), bc, od and build/slottool. Run it from the kit root.
# Then deploy examples/groth16.lang (S5 4.3) and the verifier boundary
# regressions, comparing valid and invalid inputs and proof coordinates.
set -u
fail=0
steps=0
tmp=${TMPDIR:-/tmp}/langc-precompile.$$
mkdir -p "$tmp"
. test/evmchain.sh

prog=examples/precompile.lang
caller=0x00000000000000000000000000000000000000aa

# call WANT NAME ARGS...: one step of the view or entry NAME with ARGS at
# TIMESTAMP 1. The result must equal WANT: `out HEX` (HEX as hexnorm), `ok` or
# `revert`. An entry changes the storage of the next step.
call() {
  _want=$1 _name=$2
  shift 2
  steps=$((steps + 1))
  _input=$(input "$prog" "$_name" "$@")
  if ! step 1 "$caller" "$_input"; then
    echo "FAIL precompile $_name: evm: $(head -n 1 "$tmp/dump")"
    fail=$((fail + 1))
    return
  fi
  case $result in
    "out "*) _hex=${result#out }
      [ ${#_hex} -eq 64 ] && result="out $(hexnorm "$_hex")" ;;
  esac
  if [ "$result" != "$_want" ]; then
    echo "FAIL precompile $_name: want $_want, got $result"
    fail=$((fail + 1))
  fi
}

if ! deploy "$prog"; then
  echo "FAIL precompile deploy: $(head -n 1 "$tmp/dump")"
  fail=$((fail + 1))
else
  # 2G on BN254 (EIP-196): ecAdd (1,2) (1,2) and ecMul (1,2) 2.
  twoGx=30644e72e131a029b85045b68181585d97816a916871ca8d3c208c16d87cfd3
  twoGy=15ed738c0e0a7c92e7845f96b2ae9c0a68a6a449e3538fc7ff3ebf7a5a18a2c4
  call "out $twoGx" addX 1u 2u 1u 2u
  call "out $twoGy" addY 1u 2u 1u 2u
  call "out $twoGx" mulX 1u 2u 2u
  call "out $twoGy" mulY 1u 2u 2u
  # ecMul (1,2) 0 is the point at infinity, (0, 0).
  call "out 0" mulX 1u 2u 0u
  call "out 0" mulY 1u 2u 0u
  # (1, 1) is not on the curve: the precompile fails and the call reverts (R18).
  call revert addBad
  # e(G1, G2) e(-G1, G2) = 1 (EIP-197), and the empty product is 1.
  call "out 1" pairOk
  call "out 1" pairNil
  # The go-ethereum ecrecover vector (core/vm/testdata/precompiles/ecRecover.json, CallEcrecover0).
  h=0x38d18acb67d25c8bb9942764b62f18e17054f66a817bd4295423adf9ed98873eu
  sgood=0x789d1dd423d25f0772d2748d60f7e4b81bb14d086eba8e8e8efb6dcff8a4ae02u
  addr=ceaccac640adf55b2028469bd36ba501f28b699d
  call "out $addr" recover "$h" 27 "$h" "$sgood"
  # s = 0 gives an empty output: the address is 0 (R18).
  call "out 0" recover "$h" 27 "$h" 0u
  # The same ecrecover statement twice (a fold over the pushed words, the end of
  # the list first): the valid s, then s = 0. The sum is the one address (M8).
  call ok sign "$sgood"
  call ok sign 0u
  call "out $addr" recoverSum
  # keccak256 of 64 zero bytes; `langc eval examples/precompile.lang hashZero` gives the same word.
  call "out ad3228b676f7d3cd4284a5443f17f1962b36e491b30a40b2405849e597ba5fb5" hashv 0u 0u
  # leafStep i = natDiv i 2 + natMod i 2 (the parent index plus the side bit).
  call "out 4" leafStep 7
  call "out 3" leafStep 6
  call "out aa" addrWord 0x00000000000000000000000000000000000000aa
fi

# The Groth16 fixture (S5 4.3): verify accepts the inputs (2, 3) and rejects
# the same proof with the inputs in the other order.
prog=examples/groth16.lang
if ! deploy "$prog"; then
  echo "FAIL precompile deploy groth16: $(head -n 1 "$tmp/dump")"
  fail=$((fail + 1))
else
  call "out 1" verifyOk
  call "out 0" verifyBadOrder
fi

# Public inputs use the scalar field, while proof coordinates use the base
# field. Test both boundaries, exact IC arity and the valid infinity point.
prog=test/groth16-invalid.lang
if ! deploy "$prog"; then
  echo "FAIL precompile deploy groth16 boundaries: $(head -n 1 "$tmp/dump")"
  fail=$((fail + 1))
else
  call "out 1" verifyScalar 2u 90u
  call "out 0" verifyScalar 0x30644e72e131a029b85045b68181585d2833e84879b9709143e1f593f0000003u 90u
  call "out 0" verifyScalar 0x30644e72e131a029b85045b68181585d2833e84879b9709143e1f593f0000001u 68u
  call "out 1" verifyScalar 0x30644e72e131a029b85045b68181585d2833e84879b9709143e1f593f0000000u 57u
  call "out 0" verifyExtraInput
  call "out 0" verifyMissingInput
  call "out 0" verifyMissingAll
  call "out 0" verifyEmptyIC
  call "out 1" verifyNoInputs
  call "out 1" verifyPoint 0u
  call revert verifyPoint 0x30644e72e131a029b85045b68181585d97816a916871ca8d3c208c16d87cfd47u
  call revert verifyPoint 0xffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffu
fi

# The eligibility proof (S11 step 11.2): the key and proof 1 of circuits/gen.sh accept the six
# words of statement1 (root, hu, cm1 as limbs). The same proof with cmLo xor 1 gives flagNo:
# verify reverts only on a point that is not on the curve (R18). These rows are here and not in
# test/evm.sh because `langc run` keeps verify stuck (R17).
prog=examples/eligibility.lang
if ! deploy "$prog"; then
  echo "FAIL precompile deploy eligibility: $(head -n 1 "$tmp/dump")"
  fail=$((fail + 1))
else
  root='0x1efb9d39eb45e39697c03d8c7236557eu 0xba62ac405c584d59b5b96d1c18938d3au'
  hu='0x115cc0f5e7d690413df64c6b9662e9cfu 0x2a3617f2743245519e19607a4417189au'
  call "out 1" check 1u $root $hu 0x1fc91c27750f83fb2454de806a61ab22u 0x25fce4a75194fc0539184c4476f5701cu
  call "out 0" check 1u $root $hu 0x1fc91c27750f83fb2454de806a61ab22u 0x25fce4a75194fc0539184c4476f5701du
fi

rm -rf "$tmp"
echo "precompile: $steps steps checked"
echo "precompile gate: $fail failures"
[ "$fail" -eq 0 ]
