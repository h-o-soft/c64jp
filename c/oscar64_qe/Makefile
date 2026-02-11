# C64 QE Text Editor - Oscar64 Build
#
# Japanese text editor for C64 with IME support
# Requires external MagicDesk cartridge for font data

# Configuration
OSCAR64 = oscar64
TARGET = qe
OUTPUT = $(TARGET).prg

# Shared library path
LIB_DIR = ../oscar64_lib

# Source files
SOURCES = src/qe.c src/screen.c $(LIB_DIR)/src/ime.c \
          $(LIB_DIR)/src/jtxt.c $(LIB_DIR)/src/jtxt_bitmap.c \
          $(LIB_DIR)/src/jtxt_charset.c $(LIB_DIR)/src/jtxt_resource.c \
          $(LIB_DIR)/src/jtxt_text.c

# Oscar64 compiler options
# -O2: Optimization level (O3 causes compiler crash)
# -dQE_ENABLE_IME: Enable Japanese IME
# -dENABLE_FILE_IO: Enable file I/O with custom KERNAL wrappers
OSCAR_FLAGS = -O2 -dQE_ENABLE_IME -dENABLE_FILE_IO -i=include -i=$(LIB_DIR)/include

# Emulator configuration
EMU = x64sc

# Default target
.PHONY: all
all: $(OUTPUT)

# Build PRG file
$(OUTPUT): $(SOURCES)
	@echo "=== Building QE Text Editor with Oscar64 ==="
	@if ! which $(OSCAR64) >/dev/null 2>&1; then \
		echo "Error: oscar64 not found. Please install Oscar64 compiler."; \
		exit 1; \
	fi
	$(OSCAR64) $(OSCAR_FLAGS) -o=$(OUTPUT) $(SOURCES)
	@echo "✓ $(OUTPUT) created"

# Run in emulator (requires MagicDesk cartridge from root Makefile)
.PHONY: run
run: $(OUTPUT)
	@echo "=== Running QE Text Editor ==="
	@echo "Note: Use 'make oscar-qe-run' from root Makefile for cartridge support"
	$(EMU) $(OUTPUT)

# Clean build artifacts
.PHONY: clean
clean:
	@echo "Cleaning build artifacts..."
	@rm -f $(OUTPUT) *.asm *.int *.lbl *.map
	@echo "Cleanup completed"

# Show help
.PHONY: help
help:
	@echo "C64 QE Text Editor - Oscar64 Build"
	@echo ""
	@echo "Usage:"
	@echo "  make       - Build $(OUTPUT)"
	@echo "  make run   - Build and run (without cartridge)"
	@echo "  make clean - Remove build artifacts"
	@echo "  make help  - Show this help"
	@echo ""
	@echo "Note: For proper font display, run from root Makefile:"
	@echo "  make oscar-qe-run"
