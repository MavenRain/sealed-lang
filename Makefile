# Builds the tcc-evm-contract kit with TinyCC. See README.md.
TCC ?= tcc
CC ?= cc
CFLAGS = -std=c99 -Wall -Werror -Isrc
CLANG_FLAGS = -std=c99 -Wall -Wextra -Wswitch-enum -Werror -fsyntax-only -Isrc
FRONT = src/front/base.c src/front/u256.c src/front/poseidon.c src/front/lexer.c src/front/parser.c src/front/front.c src/front/contract.c src/front/eval.c src/front/check.c src/keccak.c
TARGET = src/evm.c src/asm.c
SRC = src/main.c $(FRONT) $(TARGET) src/lower.c
TOOL = test/asmtool.c $(TARGET) src/keccak.c
FRONT_TOOL = test/fronttool.c $(filter-out src/front/front.c,$(FRONT))
PATH_TOOL = test/pathtool.c $(filter-out src/front/front.c,$(FRONT))
RUN_TOOL = test/runtool.c $(FRONT)
SLOT_TOOL = test/slottool.c src/keccak.c
BUILD_TOOL = test/buildtool.c $(TARGET) src/keccak.c
LOWER_TOOL = test/lowertool.c $(FRONT) $(TARGET)
EVM_TOOL = test/evmtool.c src/asm.c src/keccak.c
EMIT_TOOL = test/emittool.c $(filter-out src/evm.c,$(TARGET)) src/keccak.c
HEADERS = $(wildcard src/*.h src/front/*.h)

.PHONY: build check check-clang clean

build: build/langc build/asmtool build/fronttool build/pathtool build/runtool build/slottool build/buildtool build/lowertool build/evmtool build/emittool build/poseidonlib

build/domain.c: domain/domain.lang gen/embed.c
	mkdir -p build
	$(TCC) -run gen/embed.c domain/domain.lang build/domain.c

build/langc: $(SRC) $(HEADERS) build/domain.c
	$(TCC) $(CFLAGS) -o build/langc $(SRC) build/domain.c

build/asmtool: $(TOOL) $(HEADERS)
	mkdir -p build
	$(TCC) $(CFLAGS) -o build/asmtool $(TOOL)

build/fronttool: $(FRONT_TOOL) $(HEADERS)
	mkdir -p build
	$(TCC) $(CFLAGS) -o build/fronttool $(FRONT_TOOL)

build/pathtool: $(PATH_TOOL) $(HEADERS)
	mkdir -p build
	$(TCC) $(CFLAGS) -o build/pathtool $(PATH_TOOL)

build/runtool: $(RUN_TOOL) $(HEADERS) build/domain.c
	$(TCC) $(CFLAGS) -o build/runtool $(RUN_TOOL) build/domain.c

build/slottool: $(SLOT_TOOL) $(HEADERS)
	mkdir -p build
	$(TCC) $(CFLAGS) -o build/slottool $(SLOT_TOOL)

build/buildtool: $(BUILD_TOOL) $(HEADERS)
	mkdir -p build
	$(TCC) $(CFLAGS) -o build/buildtool $(BUILD_TOOL)

build/lowertool: $(LOWER_TOOL) src/lower.c $(HEADERS) build/domain.c
	$(TCC) $(CFLAGS) -o build/lowertool $(LOWER_TOOL) build/domain.c

build/evmtool: $(EVM_TOOL) src/evm.c $(HEADERS)
	mkdir -p build
	$(TCC) $(CFLAGS) -o build/evmtool $(EVM_TOOL)

build/emittool: $(EMIT_TOOL) src/evm.c $(HEADERS)
	mkdir -p build
	$(TCC) $(CFLAGS) -o build/emittool $(EMIT_TOOL)

# The Poseidon(2) library of the deposit tree (S11 step 11.3, O101): the tool
# prints the runtime code, and test/evmchain.sh puts it in each prestate.
build/poseidonlib: test/poseidonlib.c src/front/poseidon_const.h
	mkdir -p build
	$(TCC) $(CFLAGS) -o build/poseidonlib test/poseidonlib.c

# The generator of the eligibility constants (S11 step 11.2, O104). It is not in
# `build` or `check`: `make build/vkfmt`, then circuits/gen.sh gives its inputs.
build/vkfmt: test/vkfmt.c
	mkdir -p build
	$(TCC) $(CFLAGS) -o build/vkfmt test/vkfmt.c

# The generator of src/front/poseidon_const.h (S11 step 11.3, O101). It is not in
# `build` or `check`: `make build/poseidonfmt`, then run it on the circomlib 2.0.5
# poseidon_constants.circom of the O104 scratch install.
build/poseidonfmt: test/poseidonfmt.c
	mkdir -p build
	$(TCC) $(CFLAGS) -o build/poseidonfmt test/poseidonfmt.c

check-clang: build/domain.c
	$(CC) $(CLANG_FLAGS) $(SRC) build/domain.c
	$(CC) $(CLANG_FLAGS) $(TOOL)
	$(CC) $(CLANG_FLAGS) $(FRONT_TOOL)
	$(CC) $(CLANG_FLAGS) $(PATH_TOOL)
	$(CC) $(CLANG_FLAGS) $(RUN_TOOL) build/domain.c
	$(CC) $(CLANG_FLAGS) $(SLOT_TOOL)
	$(CC) $(CLANG_FLAGS) $(BUILD_TOOL)
	$(CC) $(CLANG_FLAGS) $(LOWER_TOOL) build/domain.c
	$(CC) $(CLANG_FLAGS) $(EMIT_TOOL)
	$(CC) $(CLANG_FLAGS) $(EVM_TOOL)
	$(CC) $(CLANG_FLAGS) test/poseidonlib.c

check: build/langc build/asmtool build/fronttool build/pathtool build/runtool build/slottool build/buildtool build/lowertool build/evmtool build/emittool build/poseidonlib check-clang
	sh test/gate.sh
	sh test/run.sh
	sh test/asm.sh
	sh test/build.sh
	sh test/dispatch.sh
	sh test/evm.sh
	sh test/precompile.sh
	sh test/deposit.sh
	sh test/mock.sh
	sh test/diff.sh
	sh test/review.sh
	build/fronttool
	build/pathtool
	build/runtool
	build/buildtool
	build/lowertool
	build/emittool
	build/evmtool

clean:
	rm -rf build
