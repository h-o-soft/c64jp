# C64 Japanese Text Library - Oscar64 PRG Build
#
# Simple hello world test program for Oscar64 compiler
# Requires external MagicDesk cartridge for font data

# Configuration
OSCAR64 = oscar64
TARGET = hello
OUTPUT = $(TARGET).prg

# Shared library path
LIB_DIR = ../oscar64_lib

# Source files
SOURCES = src/main.c \
          $(LIB_DIR)/src/jtxt.c $(LIB_DIR)/src/jtxt_bitmap.c \
          $(LIB_DIR)/src/jtxt_charset.c $(LIB_DIR)/src/jtxt_resource.c \
          $(LIB_DIR)/src/jtxt_text.c

# Oscar64 compiler options
OSCAR_FLAGS = -i=$(LIB_DIR)/include

# Emulator configuration
EMU = x64sc

# Default target
.PHONY: all
all: $(OUTPUT)

# Build PRG file
$(OUTPUT): $(SOURCES)
	@echo "=== Building $(OUTPUT) with Oscar64 ==="
	@if ! which $(OSCAR64) >/dev/null 2>&1; then \
		echo "Error: oscar64 not found. Please install Oscar64 compiler."; \
		exit 1; \
	fi
	$(OSCAR64) $(OSCAR_FLAGS) -o=$(OUTPUT) $(SOURCES)
	@echo "✓ $(OUTPUT) created"

# Run in emulator (requires MagicDesk cartridge from root Makefile)
.PHONY: run
run: $(OUTPUT)
	@echo "=== Running $(OUTPUT) ==="
	@echo "Note: Use 'make oscar-hello' from root Makefile for cartridge support"
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
	@echo "C64 Japanese Text - Oscar64 PRG Build"
	@echo ""
	@echo "Usage:"
	@echo "  make       - Build $(OUTPUT)"
	@echo "  make run   - Build and run (without cartridge)"
	@echo "  make clean - Remove build artifacts"
	@echo "  make help  - Show this help"
	@echo ""
	@echo "Note: For proper font display, run from root Makefile:"
	@echo "  make oscar-hello"
