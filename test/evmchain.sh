# Chain helpers for test/evm.sh (slice K4c) and test/mock.sh (slice K4d),
# modeled on the anchor kit test/evmchain.sh. Source this file from the kit
# root. The helpers need evm (go-ethereum), bc, od and build/slottool. The
# caller sets tmp, a work directory.

# The receiver of each call, and the config of each prestate: each fork at
# block 0 and shanghaiTime 0 (PUSH0).
receiver=0000000000000000000000007265636569766572
config='{"chainId":1,"homesteadBlock":0,"eip150Block":0,"eip155Block":0,"eip158Block":0,"byzantiumBlock":0,"constantinopleBlock":0,"petersburgBlock":0,"istanbulBlock":0,"berlinBlock":0,"londonBlock":0,"mergeNetsplitBlock":0,"shanghaiTime":0,"terminalTotalDifficulty":0,"terminalTotalDifficultyPassed":true}'

# The Poseidon(2) library (S11 step 11.3, O101): the code of build/poseidonlib
# at the address of `poseidon2` (src/lower.c lower_curve). step() puts it in
# each prestate.
library=",\"0x00000000000000000000000000000000000000f5\":{\"balance\":\"0x0\",\"code\":\"0x$(build/poseidonlib)\",\"storage\":{}}"

# keccak HEX: keccak256 of the bytes HEX (CALLDATACOPY, SHA3, RETURN of 32 bytes).
keccak() { evm run --code 365f5f37365f205f5260205ff3 --input "$1" 2>&1 | cut -c3-; }
# sel TEXT: the selector of the signature TEXT.
sel() { keccak "$(printf '%s' "$1" | od -An -v -tx1 | tr -d ' \n')" | cut -c1-8; }
# pad H: H as 64 hex digits.
pad() { printf '%64s' "$1" | tr ' ' 0; }

# hexnorm H: the hex number H in lowercase with no leading zeros ("0" for zero).
hexnorm() {
  printf 'obase=16; ibase=16; %s\n' "$(printf '%s' "$1" | tr a-f A-F)" | bc | tr A-F a-f
}

# word TOKEN: the word of a value that `langc run` reads or prints, as hexnorm.
word() {
  case $1 in
    0x*u) _h=${1#0x}; hexnorm "${_h%u}" ;;
    0x*) hexnorm "${1#0x}" ;;
    flagYes) echo 1 ;;
    flagNo) echo 0 ;;
    *u) printf 'obase=16; %s\n' "${1%u}" | bc | tr A-F a-f ;;
    *) printf 'obase=16; %s\n' "$1" | bc | tr A-F a-f ;;
  esac
}

# types PROG NAME: the ABI types of the arguments of the entry or view NAME,
# from the `langc abi` line of NAME (C-K4-16): Nat uint64, Flag bool, U256
# uint256 and Addr address. Env and State are not arguments of the call.
types() {
  build/langc abi "$1" --text >"$tmp/types" || return 1
  awk -v n="$2" '$3 != "event" && index($2, n "(") == 1 {
    s = substr($2, length(n) + 2)
    print substr(s, 1, length(s) - 1)
  }' "$tmp/types"
}

# input PROG NAME ARGS...: the call data of the entry NAME of PROG with the
# arguments ARGS. The selector comes from the SHA3 op of evm, not from langc.
input() {
  _sig="$2($(types "$1" "$2"))"
  shift 2
  _words=''
  for _a in "$@"; do
    _words=$_words$(pad "$(word "$_a")")
  done
  printf '%s%s\n' "$(sel "$_sig")" "$_words"
}

# put SLOT VALUE: one expected storage row. A zero word is not stored.
put() {
  [ "$2" = 0 ] || echo "$1 $2" >>"$tmp/want.rows"
}

# list SLOT ARG: the length n at SLOT, element j at keccak256(SLOT) + (n-1-j) (C-c9-1).
list() {
  _base=$(build/slottool "$1")
  _len=$(printf '%s' "$2" | tr -d '()' | awk '{ for (i = 1; i <= NF; i++) if ($i != "cons" && $i != "nil") c++ } END { print c + 0 }')
  _j=0
  for _t in $(printf '%s' "$2" | tr -d '()'); do
    case $_t in
      cons|nil) ;;
      *)
        _at=$(printf 'obase=16; ibase=16; %s + %X\n' "$(printf '%s' "$_base" | tr a-f A-F)" "$((_len - 1 - _j))" | bc | tr A-F a-f)
        put "$(pad "$_at")" "$(word "$_t")"
        _j=$((_j + 1)) ;;
    esac
  done
  put "$1" "$(word "$_j")"
}

# map SLOT ARG: the value at key k at keccak256(k . SLOT).
map() {
  _slot=$1
  set -- $(printf '%s' "$2" | tr -d '()[],')
  shift
  while [ "$#" -ge 2 ]; do
    put "$(build/slottool "$(pad "$(word "$1")")$_slot")" "$(word "$2")"
    shift 2
  done
}

# tokens TEXT: the top-level tokens of TEXT, one for each line; a parenthesized
# or bracketed group is one token.
tokens() {
  printf '%s\n' "$1" | awk '{
    s = $0; d = 0; a = ""
    for (i = 1; i <= length(s); i++) {
      c = substr(s, i, 1)
      if (c == "(" || c == "[") d++
      if (c == ")" || c == "]") d--
      if (c == " " && d == 0) { print a; a = "" } else a = a c
    }
    if (a != "") print a
  }'
}

# family SLOT ARG PROG: the storage rows of the constructor value ARG, `(CTOR
# INDEX... FIELD...)`, of a family state field at the decimal slot SLOT: the k
# constructor fields at k consecutive slots from SLOT, the index arguments
# dropped (the form of lower_family_field, S10 R95 (a)). The index count p and
# the width k come from the one-constructor family line `family F (i : T)... :=
# CTOR (f : T)...` of PROG or of the domain. Prints k.
family() {
  _inner=${2#\(}
  tokens "${_inner%\)}" >"$tmp/ctor"
  _ctor=$(head -n 1 "$tmp/ctor")
  tokens "$(awk -v c="$_ctor" '$1 == "family" && $0 ~ (":= " c "( |$)") { print; exit }' "$3" domain/domain.lang)" >"$tmp/family"
  _params=0 _width=0 _side=0
  while IFS= read -r _tok; do
    case $_tok in
      :=) _side=1 ;;
      "|") break ;;
      "("*) if [ "$_side" -eq 0 ]; then _params=$((_params + 1)); else _width=$((_width + 1)); fi ;;
    esac
  done <"$tmp/family"
  _m=0
  while [ "$_m" -lt "$_width" ]; do
    put "$(pad "$(printf '%x' "$(($1 + _m))")")" "$(word "$(awk -v n="$((_params + _m + 2))" 'NR == n' "$tmp/ctor")")"
    _m=$((_m + 1))
  done
  echo "$_width"
}

# want STATE PROG: tmp/want gets the sorted storage rows `SLOT VALUE` of the
# state text STATE (`makeState ARGS...`) of PROG. A word, List or Map field
# takes one slot; a family field takes k slots, one for each constructor field
# (S10 R95 (a)).
want() {
  : >"$tmp/want.rows"
  tokens "${1#makeState }" >"$tmp/args"
  _n=0
  while IFS= read -r _arg; do
    _s=$(pad "$(printf '%x' "$_n")")
    case $_arg in
      "(cons "*|nil) list "$_s" "$_arg"; _n=$((_n + 1)) ;;
      "(mapOf "*) map "$_s" "$_arg"; _n=$((_n + 1)) ;;
      "("*) _n=$((_n + $(family "$_n" "$_arg" "$2"))) ;;
      *) put "$_s" "$(word "$_arg")"; _n=$((_n + 1)) ;;
    esac
  done <"$tmp/args"
  sort "$tmp/want.rows" >"$tmp/want"
}

# slots DUMP: tmp/raw gets the storage rows `SLOT VALUE` of the evm dump DUMP
# as evm writes them, tmp/got the same rows with VALUE as hexnorm, sorted.
slots() {
  awk -F'"' 'NF >= 5 && length($2) == 66 && substr($2, 1, 2) == "0x" { print substr($2, 3), $4 }' "$1" >"$tmp/raw"
  awk '{ v = tolower($2); sub(/^0+/, "", v); print $1, (v == "" ? "0" : v) }' "$tmp/raw" | sort >"$tmp/got"
}

# genesis TIME ALLOC: tmp/pre.json gets a prestate at TIMESTAMP TIME with the
# alloc entries ALLOC.
genesis() {
  printf '{"config":%s,"timestamp":"0x%x","gasLimit":"0x1c9c380","difficulty":"0x0","alloc":{%s}}\n' \
    "$config" "$1" "$2" >"$tmp/pre.json"
}

# deploy PROG: run the creation code of PROG. tmp/code gets the code of the
# new account, tmp/raw and tmp/got its storage.
deploy() {
  genesis 0 ''
  _hex=$(build/langc build "$1") || return 1
  evm run --prestate "$tmp/pre.json" --create --code "$_hex" --dump >"$tmp/dump" 2>&1 || return 1
  awk '$1 == "\"code\":" { v = $2; gsub(/[",]/, "", v); print tolower(substr(v, 3)); exit }' "$tmp/dump" >"$tmp/code"
  [ -s "$tmp/code" ] || return 1
  slots "$tmp/dump"
}

# step NOW CALLER INPUT: call the receiver (the code in tmp/code, the storage
# in tmp/raw) from CALLER with INPUT at TIMESTAMP NOW. result gets ok, revert,
# trap (the Trap() selector), error, `revert HEX` or `out HEX`. tmp/raw and
# tmp/got get the storage after the call. A set alloc (`,"0xADDR":{...}`)
# adds accounts to the prestate (test/diff.sh: the mock token); then tmp/raw
# holds the storage of all accounts. The prestate also holds the library.
step() {
  _st=$(awk '{ printf "%s\"0x%s\":\"0x%s\"", (NR > 1 ? "," : ""), $1, $2 }' "$tmp/raw")
  genesis "$1" "\"0x$receiver\":{\"balance\":\"0x0\",\"code\":\"0x$(cat "$tmp/code")\",\"storage\":{$_st}}$library${alloc:-}"
  evm run --prestate "$tmp/pre.json" --receiver "0x$receiver" --sender "$2" --input "$3" \
    --dump >"$tmp/dump" 2>&1 || return 1
  result=$(awk '
    /^0x[0-9a-f]*$/ { o = $0 }
    / error: |^error: / { e = 1; if ($0 !~ /execution reverted/) x = 1 }
    END {
      if (x) print "error"
      else if (e && o == "0xae96083a") print "trap"
      else if (e && (o == "" || o == "0x")) print "revert"
      else if (e) print "revert " substr(o, 3)
      else if (o == "" || o == "0x") print "ok"
      else print "out " substr(o, 3)
    }' "$tmp/dump")
  slots "$tmp/dump"
}

# logs CALLER INPUT: the logs of the step on tmp/pre.json (the LOGS block of
# `evm run --debug`), one line for each LOG: the topics and the data.
logs() {
  evm run --debug --prestate "$tmp/pre.json" --receiver "0x$receiver" --sender "$1" --input "$2" \
    >"$tmp/debug" 2>&1 </dev/null || return 1
  awk '
    /^#### LOGS ####/ { p = 1; next }
    /^####/ { p = 0 }
    !p { next }
    /^LOG[0-4]:/ { if (n) print t " " (d == "" ? "-" : d); n = 1; t = ""; d = ""; next }
    length($2) == 64 { t = t (t == "" ? "" : ",") $2; next }
    { for (i = 2; i <= NF && $i ~ /^[0-9a-f][0-9a-f]$/; i++) d = d $i }
    END { if (n) print t " " (d == "" ? "-" : d) }' "$tmp/debug" >"$tmp/gotlogs"
}
