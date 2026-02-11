# C64 Kanji ROM Cartridge - Oscar64 EasyFlash Build
#
# This Makefile builds an EasyFlash cartridge with embedded font data
# using Oscar64 compiler's automatic RAM expansion feature.

# Configuration
OSCAR64 = oscar64
TARGET = hello_easyflash
SOURCE = $(TARGET).c
OUTPUT_CRT = $(TARGET).crt

# Shared jtxt library
LIB_DIR = ../oscar64_lib

# Source files (shared with oscar64_lib)
JTXT_SOURCES = $(LIB_DIR)/src/jtxt.c $(LIB_DIR)/src/jtxt_bitmap.c \
               $(LIB_DIR)/src/jtxt_charset.c $(LIB_DIR)/src/jtxt_resource.c \
               $(LIB_DIR)/src/jtxt_text.c

# Oscar64 compiler options for EasyFlash
# -n: No startup code (we provide our own)
# -tf=crt: Target format is CRT (EasyFlash, 16KB banks)
# -dJTXT_EASYFLASH: Enable EasyFlash-specific code paths
# -i: Include directory for jtxt library headers
OSCAR_FLAGS = -n -tf=crt -dJTXT_EASYFLASH -i=$(LIB_DIR)/include

# Emulator configuration
EMU = x64sc
EMU_OPTS = -cartcrt

# Default target
.PHONY: all
all: $(OUTPUT_CRT)

# Build CRT file
$(OUTPUT_CRT): $(SOURCE) $(JTXT_SOURCES)
	@echo "=== Building Oscar64 EasyFlash Kanji ROM ==="
	@echo "Compiling: $(SOURCE) and jtxt library"
	@if ! which $(OSCAR64) >/dev/null 2>&1; then \
		echo "Error: oscar64 not found. Please install Oscar64 compiler."; \
		exit 1; \
	fi
	$(OSCAR64) $(OSCAR_FLAGS) -o=$(OUTPUT_CRT) $(SOURCE) $(JTXT_SOURCES)
	@echo "✓ EasyFlash CRT file created: $(OUTPUT_CRT)"
	@ls -lh $(OUTPUT_CRT)

# Run in emulator
.PHONY: run
run: $(OUTPUT_CRT)
	@echo "=== Running $(OUTPUT_CRT) ==="
	@if which $(EMU) >/dev/null 2>&1; then \
		$(EMU) $(EMU_OPTS) $(OUTPUT_CRT); \
	else \
		echo "Error: Emulator $(EMU) not found."; \
		exit 1; \
	fi

# Build benchmark
BENCH_SOURCE = bench_bitmap.c
BENCH_CRT = bench_bitmap.crt

.PHONY: bench
bench: $(BENCH_CRT)

$(BENCH_CRT): $(BENCH_SOURCE) $(JTXT_SOURCES)
	@echo "=== Building Bitmap Benchmark ==="
	$(OSCAR64) $(OSCAR_FLAGS) -o=$(BENCH_CRT) $(BENCH_SOURCE) $(JTXT_SOURCES)
	@echo "Benchmark CRT created: $(BENCH_CRT)"
	@ls -lh $(BENCH_CRT)

# Run benchmark in emulator
.PHONY: run-bench
run-bench: $(BENCH_CRT)
	@echo "=== Running Benchmark ==="
	$(EMU) $(EMU_OPTS) $(BENCH_CRT)

# UII+ test (no jtxt, minimal CRT)
UII_TEST_CRT = test_uii.crt

.PHONY: test-uii
test-uii: $(UII_TEST_CRT)

$(UII_TEST_CRT): test_uii.c
	@echo "=== Building UII+ CRT Test ==="
	$(OSCAR64) -n -tf=crt -o=$(UII_TEST_CRT) test_uii.c
	@echo "Test CRT created: $(UII_TEST_CRT)"
	@ls -lh $(UII_TEST_CRT)

.PHONY: run-uii
run-uii: $(UII_TEST_CRT)
	$(EMU) $(EMU_OPTS) $(UII_TEST_CRT)

# Clean build artifacts
.PHONY: clean
clean:
	@echo "Cleaning build artifacts..."
	@rm -f $(OUTPUT_CRT) $(TARGET).asm $(TARGET).lbl $(TARGET).map $(TARGET).int
	@rm -f $(BENCH_CRT) bench_bitmap.asm bench_bitmap.lbl bench_bitmap.map bench_bitmap.int
	@echo "Cleanup completed"

# Show help
.PHONY: help
help:
	@echo "C64 Kanji ROM Cartridge - Oscar64 EasyFlash Build"
	@echo ""
	@echo "Usage:"
	@echo "  make       - Build EasyFlash CRT file"
	@echo "  make run   - Build and run in emulator"
	@echo "  make clean - Remove build artifacts"
	@echo "  make help  - Show this help"
	@echo ""
	@echo "Output:"
	@echo "  $(OUTPUT_CRT) - EasyFlash cartridge with embedded fonts"
	@echo ""
	@echo "Requirements:"
	@echo "  - oscar64 compiler"
	@echo "  - Font files in ../../fontconv/"
	@echo "  - VICE emulator (optional, for 'make run')"
	@echo ""
	@echo "EasyFlash Features:"
	@echo "  - 29KB RAM region ($0900-$8000) with automatic expansion"
	@echo "  - 16KB ROM banks for font data"
	@echo "  - No manual RAM copying required"

# Debug info
.PHONY: debug
debug:
	@echo "=== Build Configuration ==="
	@echo "OSCAR64: $(OSCAR64)"
	@echo "SOURCE: $(SOURCE)"
	@echo "OUTPUT_CRT: $(OUTPUT_CRT)"
	@echo "OSCAR_FLAGS: $(OSCAR_FLAGS)"
	@echo "EMU: $(EMU)"
	@echo ""
	@echo "=== Font Files ==="
	@ls -lh ../../fontconv/*.bin 2>/dev/null || echo "Font files not found"
