# C64 Kanji ROM Cartridge - Oscar64 EasyFlash Build
#
# This Makefile builds an EasyFlash cartridge with embedded font data
# using Oscar64 compiler's automatic RAM expansion feature.

# Configuration
OSCAR64 = oscar64
TARGET = hello_easyflash
SOURCE = $(TARGET).c
OUTPUT_CRT = $(TARGET).crt

# Source files
JTXT_SOURCES = src/jtxt.c src/jtxt_bitmap.c src/jtxt_charset.c \
               src/jtxt_resource.c src/jtxt_text.c

# Oscar64 compiler options for EasyFlash
# -n: No startup code (we provide our own)
# -tf=crt: Target format is CRT (EasyFlash, 16KB banks)
# -i: Include directory
OSCAR_FLAGS = -n -tf=crt -i=include

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

# Clean build artifacts
.PHONY: clean
clean:
	@echo "Cleaning build artifacts..."
	@rm -f $(OUTPUT_CRT) $(TARGET).asm $(TARGET).lbl $(TARGET).map $(TARGET).int
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
