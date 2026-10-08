CC ?= cc
CFLAGS ?= -O3 -std=c99 -Wall -Wextra -Wpedantic
EXPERIMENTS := $(abspath ..)
BUILD := build
BIN := $(abspath $(BUILD)/search-eval)
MODES := 0 1 2 3 4 5 6 7
SOURCES := compare.c cube.c tables.c bounds.c ida_star.c variants.c \
           $(wildcard variants/*.c)
HEADERS := cube.h tables.h bounds.h ida_star.h variants.h
PROFILE_SOURCES := profile.c $(filter-out compare.c,$(SOURCES))
PROFILE_TIME := $(abspath $(BUILD)/profile-time)
PROFILE_COUNT := $(abspath $(BUILD)/profile-count)

.PHONY: all list check compare profile-storage verify-all clean

all: $(BIN)

$(BIN): $(SOURCES) $(HEADERS) ../baseline/solver.c ../baseline/solutions.txt | $(BUILD)
	$(CC) $(CFLAGS) $(SOURCES) -o $@

$(PROFILE_TIME): $(PROFILE_SOURCES) $(HEADERS) ../baseline/solver.c | $(BUILD)
	$(CC) $(CFLAGS) $(PROFILE_SOURCES) -o $@

$(PROFILE_COUNT): $(PROFILE_SOURCES) $(HEADERS) ../baseline/solver.c | $(BUILD)
	$(CC) $(CFLAGS) -DPROFILE_OPERATIONS $(PROFILE_SOURCES) -o $@

# Instrumented operation counts and separate uninstrumented phase timings.
profile-storage: $(PROFILE_TIME) $(PROFILE_COUNT)
	@set -eu; for mode in 1 7; do \
		(cd "$(EXPERIMENTS)" && "$(PROFILE_COUNT)" "$$mode") \
		> "$(BUILD)/profile-count-$$mode.json" \
		2> "$(BUILD)/profile-count-$$mode.log"; \
	done; for repeat in 1 2 3; do \
		if [ "$$repeat" = 2 ]; then modes="7 1"; else modes="1 7"; fi; \
		for mode in $$modes; do \
			(cd "$(EXPERIMENTS)" && "$(PROFILE_TIME)" "$$mode") \
			> "$(BUILD)/profile-time-$$mode-run-$$repeat.json" \
			2> "$(BUILD)/profile-time-$$mode-run-$$repeat.log"; \
			echo "profile mode $$mode / run $$repeat: complete"; \
		done; \
	done

list: $(BIN)
	@"$(BIN)" --list

$(BUILD):
	mkdir -p $@

# Quick comparison: the same 64 hard inputs for all eight configurations.
check: $(BIN)
	@set -eu; for mode in $(MODES); do \
		(cd "$(EXPERIMENTS)" && "$(BIN)" "$$mode" 64) \
		> "$(BUILD)/check-$$mode.json" \
		2> "$(BUILD)/check-$$mode.log"; \
		echo "mode $$mode: passed"; \
	done

# Three fresh processes per mode, rotating order; every run tests all hard inputs.
compare: $(BIN)
	@set -eu; for repeat in 1 2 3; do \
		for offset in $(MODES); do \
			mode=$$(((offset + repeat - 1) % 8)); \
			(cd "$(EXPERIMENTS)" && "$(BIN)" "$$mode") \
			> "$(BUILD)/mode-$$mode-run-$$repeat.json" \
			2> "$(BUILD)/mode-$$mode-run-$$repeat.log"; \
			echo "mode $$mode / run $$repeat: complete"; \
		done; \
	done

# The selected two-bit candidate: shortest lengths and path replay for all states.
verify-all: $(BIN)
	@cd "$(EXPERIMENTS)" && "$(BIN)" 7 all \
		> "$(abspath $(BUILD))/all-states.json" \
		2> "$(abspath $(BUILD))/all-states.log"
	@cat "$(BUILD)/all-states.json"

clean:
	$(RM) -r $(BUILD)
