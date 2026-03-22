CMAKE ?= cmake
CTEST ?= ctest
BUILD_DIR ?= build
CONFIGURE_FLAGS ?= -DQDATA_BUILD_TESTS=ON -DQDATA_BUILD_EXAMPLES=ON
CTEST_FLAGS ?= --output-on-failure -V
QS_EXTENDED_TESTS ?= 1
ROWS ?=
REPS ?=

BENCH_ARGS :=
ifneq ($(strip $(ROWS)),)
BENCH_ARGS += $(ROWS)
endif
ifneq ($(strip $(REPS)),)
BENCH_ARGS += $(REPS)
endif

.PHONY: all configure example bench benchmark benchmark-build test test-extended clean distclean

all: configure
	$(CMAKE) --build $(BUILD_DIR)

configure:
	$(CMAKE) -S . -B $(BUILD_DIR) $(CONFIGURE_FLAGS)

example: configure
	$(CMAKE) --build $(BUILD_DIR) --target qdata_mtcars_roundtrip

benchmark-build: configure
	$(CMAKE) --build $(BUILD_DIR) --target qdata_benchmark

# make benchmark ROWS=1000 REPS=1
benchmark: benchmark-build
	./$(BUILD_DIR)/qdata_benchmark $(BENCH_ARGS)

test: configure
	$(CMAKE) --build $(BUILD_DIR)
	$(CTEST) --test-dir $(BUILD_DIR) $(CTEST_FLAGS)

test-extended: configure
	$(CMAKE) --build $(BUILD_DIR)
	QS_EXTENDED_TESTS=$(QS_EXTENDED_TESTS) $(CTEST) --test-dir $(BUILD_DIR) $(CTEST_FLAGS)

clean:
	@if [ -d "$(BUILD_DIR)" ]; then $(CMAKE) --build $(BUILD_DIR) --target clean; fi

distclean:
	$(CMAKE) -E rm -rf $(BUILD_DIR)
