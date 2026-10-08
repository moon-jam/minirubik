CC ?= cc
CFLAGS ?= -O3 -std=c99 -Wall -Wextra -Wpedantic
FRAMA_C ?= frama-c
CLANG_FORMAT := $(shell command -v clang-format-20 2>/dev/null || \
	command -v clang-format 2>/dev/null)
C_SOURCES := $(wildcard *.c *.h)
SAMPLE_STATE := 21345671111111
SAMPLE_SOLUTION := B' R' D2 R' B R B' R D2 B R'
VECTORS := tests/solutions.txt
BUILD := build
TABLE_GENERATOR := $(BUILD)/generate-tables
TABLES := $(BUILD)/solver-tables.inc
VERIFY_SOLVER := $(BUILD)/solver-verify
CROSS_CC ?= riscv64-unknown-elf-gcc
RIPES_BIN ?= /Applications/Ripes-v2.2.6-106-g5b8a616-mac-universal2.app/Contents/MacOS/Ripes
RV32_ASM := $(BUILD)/solver-gcc-O2.s
RV32_ELF := $(BUILD)/solver-gcc-O2.elf
RV32_CHECK := $(BUILD)/rv32-check
HOST_TABLE_SOURCES := experiments/search/cube.c experiments/search/tables.c \
                      experiments/search/bounds.c
HOST_TABLE_HEADERS := experiments/search/cube.h experiments/search/tables.h \
                      experiments/search/bounds.h
# One per rejection path: short, long, cubie digit low, cubie digit high,
# orientation digit low, orientation digit high, non-digit, duplicate, parity.
INVALID_STATES := 1234567111111 123456711111111 02345671111111 82345671111111 \
	12345671111110 12345671111114 1234567111111a 11345671111111 12345671111112

.PHONY: all check verify check-rv32 check-rv32-quick prove clean indent

all: solver mini

$(BUILD):
	mkdir -p $@

$(TABLE_GENERATOR): experiments/search/export.c $(HOST_TABLE_SOURCES) \
                    $(HOST_TABLE_HEADERS) \
                    experiments/baseline/solver.c | $(BUILD)
	$(CC) $(CFLAGS) experiments/search/export.c \
		$(HOST_TABLE_SOURCES) -o $@

$(TABLES): $(TABLE_GENERATOR)
	@set -eu; trap 'rm -f "$@.tmp"' 0 1 2 15; \
		$(TABLE_GENERATOR) > "$@.tmp"; mv "$@.tmp" "$@"

solver: solver.c $(TABLES)
	$(CC) $(CFLAGS) -I$(BUILD) $< -o $@

$(VERIFY_SOLVER): tests/verify.c solver.c $(TABLES) $(HOST_TABLE_SOURCES) \
                  $(HOST_TABLE_HEADERS) experiments/baseline/solver.c
	$(CC) $(CFLAGS) -I$(BUILD) tests/verify.c \
		$(HOST_TABLE_SOURCES) -o $@

verify: $(VERIFY_SOLVER)
	./$(VERIFY_SOLVER) --verify-all

$(RV32_ASM): solver.c $(TABLES)
	$(CROSS_CC) -O2 -march=rv32i -mabi=ilp32 -ffreestanding \
		-DRV32I_REFERENCE -I$(BUILD) -S $< -o $@

$(RV32_ELF): runner.s $(RV32_ASM)
	$(CROSS_CC) -march=rv32i -mabi=ilp32 -nostdlib -nostartfiles \
		-Wl,--no-relax,-e,_start $^ -o $@

$(RV32_CHECK): tests/rv32-check.c $(HOST_TABLE_SOURCES) $(HOST_TABLE_HEADERS) \
               experiments/baseline/solver.c | $(BUILD)
	$(CC) $(CFLAGS) -Iexperiments/search $< $(HOST_TABLE_SOURCES) -o $@

# Host-side driver: patch only the ELF input, then execute each case in Ripes.
check-rv32: $(RV32_ELF) $(RV32_CHECK) $(VECTORS)
	./$(RV32_CHECK) full "$(RIPES_BIN)" $(RV32_ELF) $(BUILD)/rv32-full

check-rv32-quick: $(RV32_ELF) $(RV32_CHECK)
	./$(RV32_CHECK) quick "$(RIPES_BIN)" $(RV32_ELF) $(BUILD)/rv32-quick

mini: mini.c
	$(CC) $(CFLAGS) $< -o $@

check: solver mini $(VECTORS) verify
	./$(VERIFY_SOLVER) --self-test
	@expected=$$(mktemp); actual=$$(mktemp); \
		trap 'rm -f "$$expected" "$$actual"' 0 1 2 15; \
		count=0; \
		while IFS='|' read -r state solution; do \
			case "$$state" in ""|\#*) continue ;; esac; \
			printf '%s\n' "$$solution" >"$$expected"; \
			./$(VERIFY_SOLVER) --check-solution "$$state" "$$solution" || exit 1; \
			for binary in ./mini; do \
				$$binary "$$state" >"$$actual"; \
				status=$$?; \
				test $$status -eq 0 || { \
					echo "$$binary $$state: exit status $$status"; exit 1; }; \
				cmp -s "$$actual" "$$expected" || { \
					echo "$$binary $$state: output mismatch"; \
					echo "  expected: $$solution"; \
					printf '  got:      '; cat "$$actual"; \
					echo "  ($$(wc -c <"$$expected") bytes expected, \
$$(wc -c <"$$actual") produced)"; exit 1; }; \
			done; \
			count=$$((count + 1)); \
		done <$(VECTORS); \
		echo "$$count vectors: solver lengths/replay checked; mini output matched"
	@for binary in ./solver ./mini; do \
		for bad in $(INVALID_STATES); do \
			$$binary "$$bad" >/dev/null 2>&1; \
			status=$$?; \
			test $$status -eq 2 || { \
				echo "$$binary $$bad: expected status 2, got $$status"; exit 1; }; \
		done; \
		$$binary >/dev/null 2>&1; \
		status=$$?; \
		test $$status -eq 2 || { \
			echo "$$binary with no argument: expected status 2, got $$status"; \
			exit 1; }; \
		$$binary $(SAMPLE_STATE) $(SAMPLE_STATE) >/dev/null 2>&1; \
		status=$$?; \
		test $$status -eq 2 || { \
			echo "$$binary with two arguments: expected status 2, got $$status"; \
			exit 1; }; \
		$$binary $(SAMPLE_STATE) >&- 2>/dev/null; \
		status=$$?; \
		test $$status -eq 1 || { \
			echo "$$binary with stdout closed: expected status 1, got $$status"; \
			exit 1; }; \
	done
	@./$(VERIFY_SOLVER) --self-test >&- 2>/dev/null; \
		status=$$?; \
		test $$status -eq 1 || { \
			echo "solver --self-test with stdout closed: expected 1, got $$status"; \
			exit 1; }
	@echo "invalid input rejected with status 2, unwritable stdout with status 1"

prove: solver.c $(TABLES)
	@log=$$(mktemp); trap 'rm -f "$$log"' 0 1 2 15; \
		$(FRAMA_C) -cpp-extra-args=-I$(BUILD) -wp \
		-wp-fct quarter_turn,encode_state,valid,parse_state \
		-wp-rte -rte-verbose 0 -wp-prover alt-ergo -wp-timeout 20 \
		-wp-cache none solver.c >"$$log" 2>&1; rc=$$?; \
		grep -Fvx -e '[wp] Warning: Skipped RTE guards: unaligned pointers (\aligned not supported)' \
		-e '[wp] Warning: Skipped RTE guards: invalid function pointer calls (\valid_function not supported)' "$$log"; \
		test $$rc -eq 0 && awk '$$1 == "[wp]" && $$2 == "Proved" && $$3 == "goals:" && $$4 > 0 && $$4 == $$6 { ok = 1 } END { exit !ok }' "$$log" && \
		! grep -Eq '(^|[[:space:]])(Timeout|Unknown|Failed):' "$$log"

indent:
ifeq ($(CLANG_FORMAT),)
	$(error clang-format 20 not found)
endif
	@$(CLANG_FORMAT) --version | grep -q 'version 20' || \
		{ echo "error: clang-format version 20 required"; exit 1; }
	$(CLANG_FORMAT) -i $(C_SOURCES)

clean:
	$(RM) solver mini
	$(RM) -r $(BUILD)
