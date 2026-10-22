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
  testLastSlotCount testAuctionLimit testAuctionCaller testAuctionReversed testAuctionLong testAuthNonce testSettleNoDeposit testRootLimbs; do
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
# S11 step 11.4: examples/eligibility.lang proof1 to proof3 (8 words, EIP-197 order) and their words.
p1='0x16e49ee77dae6a981f0572b426934e40384acaa6ab1688aa62ce1ed14414d3a0u 0x2bfbdd9642447b2521ae007ebe021ac496d06e415d623160ef0e3f4c2db7ee77u 0x229af34dec69e41e6103ef5e2a34556457821db8186bf1f89b583d11456ac58cu 0x1abb99a589ccbf58bdbab9b668665774a5de6d39de930aa485f86956a333cadfu 0x2e9a0d05465f0ef1b2e3045b737d7edf970abc9695080ad1561d79896540052u 0x2c0777cad0550b390ee9132f90661afa4ff966c8d0ba612c5fc68eb710275cf2u 0x2d0ee63195d43d7ed63e0002d1b0223050a23e3d9f37410876bb216e1bb8ccc1u 0x298cecefe1dd66e85bb74e3464a7b0c78130c1f1ce84feb5fb1dc8c1ffa849adu'
p2='0x19e19741ac7e80a78990bb2a31e373998d71a6df4aa60f0473d3939df2c79844u 0x1d1ed30ad9d3fcfbd5abfeb4431cc9dffc505ae2fecc5a320df01487b554785eu 0x1ae38d6acd0065905144c4f0832cff6623489847335b26c387b070aedd032ab0u 0x99abe641a8fa8b585e5c6dee1d5599749655d678f1264aee9fe31fb543205a3u 0x3012fa20cdf958bf7688a66bcdaa51b9b26d51ac2e8d447f8d1d8afe48b1840u 0x3ff6551e1b60ec7379b49c09e7e46d74aea1e6070740930b4deb039f4735df8u 0x272de51ab93702bece8ceb38d5ef9e31562df2cb33f197a0aa1ebb5711cc1d7u 0x2405f1554752ae6cb334b676fe04c1daefafd6c2be61d2d56bd950a96960a2d9u'
p3='0x1f8ae0dd102c1142a2f8228a32a06f2345ee855b1401cd4582f4cad47091ea17u 0x2894bc046663bdc5fc32d960bd988deb73679180a8c32bb350110c1bc3c70996u 0xefb01a919372c5a52a39d01fcbf8ce8162145b994fb838d76a05df7120c237u 0xd9fa05c95b55dd3834056d85305dad472b4bbd7ec0c9795c1cc721b3b8656aau 0xde929c948bcd7665d613d0e8a675e7fc8ff1ef1f15730b5020c73693e25b587u 0x22bac44befb548a10d870e58630c29b3c12a769c6a48ecc6249badc20567d22au 0x2677a6a69c52f24d57bb8271ac2f713406d9b4459b438721a909d10ece1ab0cfu 0x248613dcfb53bc362e3962b8fc89a7930ba106cd40a47c346b22aa7efb110c77u'
hu=0x115cc0f5e7d690413df64c6b9662e9cf2a3617f2743245519e19607a4417189au
cm1=0x1fc91c27750f83fb2454de806a61ab2225fce4a75194fc0539184c4476f5701cu
cm2=0x80dd4db0f1125728d2c783646ee0cb16e4361ea742e13b1645f8a3987cd7588u
cm3=0xa8bb43812004ec0e6755b2ccc5795b3ea342ba8d746c858742d3b1eae2be805u
# hu + r and cm1 + r: not canonical, thus other limbs and another statement.
hur=0x41c10f68c908306af646922217e4422c526a003aedebb5e2e1fb560e3417189bu
cm1r=0x502d6a9a56412424dca52436ebe3037f4e30ccefcb4e6c967cfa41d866f5701du
# The signer signs hash(piAcc, 0) of the tuple (0, hu) (piAcc 0x6d481c82...235ba20a).
rh=0x690d7734e8ee445dedffa7e4691267ce5e929430e4b678678ad97e00f7c4aa19u
sh=0x3a56d6019138a6f772a608a31d72781862e13d50f4b23518b5f755ec96ab27ecu
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
  # S11 step 11.4: u (0x..03) and u' (0x..05) deposit in slot 1 (the fixture root), the signer
  # in slot 2. Then the double-commit slash of hu (O102), then the slash of the signer.
  chain ok 10 0x0000000000000000000000000000000000000003 registerDeposit 18586133768512220936620570745912940619677854269274689475585506675881198879027u 100u
  chain ok 10 0x0000000000000000000000000000000000000005 registerDeposit 7061949393491957813657776856458368574501817871421526214197139795307327923534u 100u
  chain ok 20 "$signer" registerDeposit 7u 100u
  chain ok 20 "$proposer" addTuple "$signer" 0u "$hu"
  # Auction 0 ends in slot 1. $other has no deposit, thus settle in slot 2 reverts (R73 (a)).
  chain ok 20 "$proposer" regAuction 9u 1 1
  chain revert 20 "$other" settle 0u 50u
  chain revert 20 "$other" slash "$signer" "$hu" 27 "$rh" "$sh"
  # slashUsr hu, then (slot, cm, proof) two times. One cm twice, proof 3 (hu') under hu, and the
  # words hu + r and cm1 + r revert (the contract makes the limbs, O102).
  chain revert 20 "$other" slashUsr "$hu" 1 "$cm1" $p1 1 "$cm1" $p1
  chain revert 20 "$other" slashUsr "$hu" 1 "$cm1" $p1 1 "$cm3" $p3
  chain revert 20 "$other" slashUsr "$hur" 1 "$cm1" $p1 1 "$cm2" $p2
  chain revert 20 "$other" slashUsr "$hu" 1 "$cm1r" $p1 1 "$cm2" $p2
  chain ok 20 "$other" slashUsr "$hu" 1 "$cm1" $p1 1 "$cm2" $p2
  chain 'out 1' 20 "$other" isSlashed "$hu"
  chain ok 20 "$other" slash "$signer" "$hu" 27 "$rh" "$sh"
  chain 'out 0' 20 "$other" depositOf "$signer"
  chain revert 20 "$other" slash "$signer" "$hu" 27 "$rh" "$sh"
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
