#!/bin/sh
# The assertions are sealed-lang definitions applied to the real deposit entries.
set -eu
tmp=${TMPDIR:-/tmp}/langc-deposit.$$
mkdir -p "$tmp"
trap 'rm -rf "$tmp"' EXIT HUP INT TERM
cat examples/deposit.lang test/deposit.lang >"$tmp/deposit.lang"
fail=0
rows=0
for name in testLastLeaf testFullTree testBeyondTree testMaxAuction testMaxAuctionSpan testZeroAuction testOutsideAuction \
  testFirstAmount testSecondIndex testFirstRoot testSecondRoot testCircuitRoot testFieldDeposit testSmallDeposit testDuplicateDeposit \
  testInactiveSlot testInactiveAgain testInactiveCaller testInactiveMissing testFirstSlotCount testOverlapCount \
  testLastSlotCount testAuctionLimit testAuctionCaller testAuctionReversed testAuctionLong testAuthNonce testSettleNoDeposit; do
  rows=$((rows + 1))
  result=$(build/langc eval "$tmp/deposit.lang" "$name" 2>&1) || true
  if [ "$result" != 1 ]; then
    echo "FAIL deposit $name: $result"
    fail=$((fail + 1))
  fi
done
echo "deposit: $rows assertions, $fail failures"

. test/evmchain.sh
prog=examples/deposit.lang
proposer=0x00000000000000000000000000000000000000aa
user=0x00000000000000000000000000000000000000bb
other=0x00000000000000000000000000000000000000cc
chain_rows=0
# This token returns true without storage writes, so evmchain's raw rows stay contract-only.
alloc=',"0x00000000000000000000000000000000000000ee":{"balance":"0x0","code":"0x60015f5260205ff3","storage":{}}'
chain() {
  _want=$1 _time=$2 _caller=$3 _name=$4
  shift 4
  chain_rows=$((chain_rows + 1))
  cp "$tmp/raw" "$tmp/before"
  _input=$(input "$prog" "$_name" "$@")
  if ! step "$_time" "$_caller" "$_input"; then
    echo "FAIL deposit EVM $_name: evm failed"
    fail=$((fail + 1))
    return
  fi
  case $result in
    "out "*) result="out $(hexnorm "${result#out }")" ;;
  esac
  if [ "$result" != "$_want" ]; then
    echo "FAIL deposit EVM $_name: want $_want, got $result"
    fail=$((fail + 1))
  fi
  if [ "$_want" = revert ]; then
    awk '{ v = tolower($2); sub(/^0+/, "", v); print $1, (v == "" ? "0" : v) }' "$tmp/before" | sort >"$tmp/before.sorted"
    if ! cmp -s "$tmp/before.sorted" "$tmp/got"; then
      echo "FAIL deposit EVM $_name: reverted call changed storage"
      diff -u "$tmp/before.sorted" "$tmp/got" || true
      fail=$((fail + 1))
    fi
  fi
}
# Set the nextLeaf slot to exercise the capacity boundary without billions of deposits.
next_leaf() {
  awk -v slot="$(pad b)" '$1 != slot' "$tmp/raw" >"$tmp/storage"
  printf '%s %s\n' "$(pad b)" "$(pad "$1")" >>"$tmp/storage"
  mv "$tmp/storage" "$tmp/raw"
}
if ! deploy "$prog"; then
  echo 'FAIL deposit EVM deployment'
  fail=$((fail + 1))
else
  code_chars=$(tr -d '\n' <"$tmp/code" | wc -c | tr -d ' ')
  echo "deposit EVM runtime: $((code_chars / 2)) bytes"
  chain revert 0 "$user" regAuction 9u 0 0
  chain ok 0 "$proposer" regAuction 9u 18446744073709551615 18446744073709551615
  chain ok 0 "$proposer" regAuction 10u 18446744073709551612 18446744073709551615
  chain revert 0 "$proposer" regAuction 11u 18446744073709551615 18446744073709551615
  chain 'out ffffffffffffffff' 0 "$user" aucEndOf 0u
  chain revert 0 "$proposer" regAuction 12u 3 1
  chain revert 0 "$proposer" regAuction 12u 1 5
  chain revert 10 "$user" registerDeposit 7u 99u
  chain ok 10 "$user" registerDeposit 7u 100u
  chain 'out 64' 10 "$user" depositOf "$user"
  chain revert 10 "$user" registerDeposit 9u 100u
  chain revert 20 "$user" makeInactive "$user"
  chain revert 20 "$proposer" makeInactive "$other"
  chain ok 20 "$proposer" makeInactive "$user"
  chain revert 30 "$proposer" makeInactive "$user"
  next_leaf ffffffff
  chain ok 30 "$other" registerDeposit 9u 100u
  chain revert 30 "$proposer" registerDeposit 11u 100u
  next_leaf 100000001
  chain revert 30 "$proposer" registerDeposit 11u 100u
fi
if ! deploy "$tmp/deposit.lang"; then
  echo 'FAIL deposit EVM signature deployment'
  fail=$((fail + 1))
else
  prog="$tmp/deposit.lang"
  for name in testSignature0 testSignature1 testSignatureReplay testSignatureLegacy; do
    chain 'out 1' 0 "$user" "$name"
  done
fi
# S5 4.6: the signed and proof calls that langc run cannot step (ecrecover and verify are stuck).
# The signer is test key 1. The tuple (0, 2) gives the handle 2, and the signatures of
# testSignature0 (nonce 0) and testSignature1 (nonce 1) sign hash(piAcc, nonce) of that Pi.
signer=0x7e5f4552091a69125d5dfcb7b8c2659029395bdf
r0=0x394512031ecf8c355f8ebf02dbfcf4881348761e76ebd7ef8b7bfb370eec1c8eu
s0=0x544f61e1bc9827bdfb3430f2c8c7e3d58945c9cb7ef032b37748e9e8dc5420cfu
r1=0x96e840dd75825d0ab1d6b53a507de4aa1e1768ff3babca6cae6e63ec654652edu
s1=0x0a8e747b34fddf6101867498e56acce617fba425356b102330aecae58fdde119u
# The fixture proof (A = 90 G1, B = G2, C = 17 G1), after cm. Its 2 inputs (hu, cm) come after it.
proof='0x057a7f67bed912cdb1c7f282ada6406e09017f13b7a37385580e7bc079d63978u 0x0ea55197090bd4ed6176a05d1be5a6a1c21aacb5e2ea40a0573d1696221bd87eu 0x198e9393920d483a7260bfb731fb5d25f1aa493335a9e71297e485b7aef312c2u 0x1800deef121f1e76426a00665e5c4479674322d4f75edadd46debd5cd992f6edu 0x090689d0585ff075ec9e99ad690c3395bc4b313370b38ef355acdadcd122975bu 0x12c85ea5db8c6deb4aab71808dcb408fe3d1e7690c43d37b4ce6cc0166fa7daau 0x1c6a451060210f3baad93fe1631753751da9857edae0468e8e4bee7dd33cfb2cu 0x2331a64aa86c50d2d1e0237893ef7744a77228881ce73fcc2ad555a37d4ab405u'
prog=examples/deposit.lang
if ! deploy "$prog"; then
  echo 'FAIL deposit EVM lifecycle deployment'
  fail=$((fail + 1))
else
  # One inactivity round trip, then a withdraw after nA slots.
  chain ok 10 "$signer" registerDeposit 7u 100u
  chain ok 10 "$proposer" addTuple "$signer" 0u 2u
  chain ok 20 "$proposer" makeInactive "$signer"
  chain ok 30 "$signer" updateActivity "$signer" 28 "$r0" "$s0"
  chain revert 30 "$signer" updateActivity "$signer" 28 "$r0" "$s0"
  chain 'out 0' 30 "$signer" pendingOf "$signer"
  chain ok 30 "$proposer" addTuple "$signer" 0u 2u
  chain ok 40 "$proposer" makeInactive "$signer"
  chain revert 50 "$signer" withdraw "$signer" 27 "$r1" "$s1"
  chain ok 60 "$signer" withdraw "$signer" 27 "$r1" "$s1"
  chain 'out 0' 60 "$signer" depositOf "$signer"
fi
if ! deploy "$prog"; then
  echo 'FAIL deposit EVM slash deployment'
  fail=$((fail + 1))
else
  # One double-commit slash of the handle 2 with the fixture proof, then the slash of the signer.
  chain ok 10 "$signer" registerDeposit 7u 100u
  chain ok 10 "$proposer" addTuple "$signer" 0u 2u
  # Auction 0 ends in slot 1. $other has no deposit, thus settle in slot 2 reverts (R73 (a)).
  chain ok 20 "$proposer" regAuction 9u 1 1
  chain revert 20 "$other" settle 0u 50u
  chain revert 20 "$other" slash "$signer" 2u 28 "$r0" "$s0"
  # The key accepts the proof for (2, x) for each x. Input 1 = 3, not cm1 = 5: revert (R75 (a)).
  chain revert 20 "$other" slashUsr 2u 5u $proof 2u 3u 6u $proof 2u 6u
  chain ok 20 "$other" slashUsr 2u 5u $proof 2u 5u 6u $proof 2u 6u
  chain 'out 1' 20 "$other" isSlashed 2u
  chain ok 20 "$other" slash "$signer" 2u 28 "$r0" "$s0"
  chain 'out 0' 20 "$other" depositOf "$signer"
  chain revert 20 "$other" slash "$signer" 2u 28 "$r0" "$s0"
fi
if ! deploy "$prog"; then
  echo 'FAIL deposit EVM root deployment'
  fail=$((fail + 1))
else
  # S11 step 11.3: the leaves poseidon2 (u, H(1)) and poseidon2 (u', H(7)) for u = 0x..03 and
  # u' = 0x..05 give the root of the circuit fixture in slot 1. A commitment r reverts.
  chain ok 10 0x0000000000000000000000000000000000000003 registerDeposit 18586133768512220936620570745912940619677854269274689475585506675881198879027u 100u
  chain ok 10 0x0000000000000000000000000000000000000005 registerDeposit 7061949393491957813657776856458368574501817871421526214197139795307327923534u 100u
  chain 'out 1efb9d39eb45e39697c03d8c7236557eba62ac405c584d59b5b96d1c18938d3a' 10 "$user" rootOf 1
  chain revert 10 "$user" registerDeposit 0x30644e72e131a029b85045b68181585d2833e84879b9709143e1f593f0000001u 100u
fi
echo "deposit EVM: $chain_rows calls, $fail total failures"
[ "$fail" -eq 0 ]
