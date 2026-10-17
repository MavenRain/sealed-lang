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
  testFirstAmount testSecondIndex testFirstRoot testSecondRoot testSmallDeposit testDuplicateDeposit \
  testInactiveSlot testInactiveAgain testInactiveCaller testInactiveMissing testFirstSlotCount testOverlapCount \
  testLastSlotCount testAuctionLimit testAuctionCaller testAuctionReversed testAuctionLong; do
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
echo "deposit EVM: $chain_rows calls, $fail total failures"
[ "$fail" -eq 0 ]
