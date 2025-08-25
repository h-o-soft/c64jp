# ==============================================================================
# C64 Kanji ROM Font Conversion Makefile
# ==============================================================================

# Misaki font related definitions
MISAKI_VERSION = 2021-05-05a
MISAKI_ZIP = misaki_png_$(MISAKI_VERSION).zip
MISAKI_URL = https://littlelimit.net/arc/misaki/$(MISAKI_ZIP)
MISAKI_PNG = misaki_gothic.png

# Output files
FONT_GOTHIC_BIN = font_misaki_gothic.bin
FONT_MINCHO_BIN = font_misaki_mincho.bin
FONT_JISX0201_BIN = font_jisx0201.bin

# Tools
PYTHON = python3

# Default target
.PHONY: all
all: $(FONT_GOTHIC_BIN) $(FONT_MINCHO_BIN) $(FONT_JISX0201_BIN)

# Create font binaries (from mkfont.py)
$(FONT_GOTHIC_BIN) $(FONT_MINCHO_BIN) $(FONT_JISX0201_BIN): $(MISAKI_PNG)
	@echo "Converting Misaki font to C64 format..."
	$(PYTHON) mkfont.py
	@echo "✓ Font binaries created"

# Download and extract Misaki font
$(MISAKI_PNG): $(MISAKI_ZIP)
	@echo "Extracting Misaki font..."
	unzip -oq $(MISAKI_ZIP)
	@test -f $(MISAKI_PNG) || (echo "Error: Expected PNG file not found after extraction" && exit 1)
	@echo "✓ Misaki font extracted: $(MISAKI_PNG)"

$(MISAKI_ZIP):
	@echo "Downloading Misaki font..."
	@echo "URL: $(MISAKI_URL)"
	curl -L -o $(MISAKI_ZIP) $(MISAKI_URL)
	@test -f $(MISAKI_ZIP) || (echo "Error: Failed to download Misaki font" && exit 1)
	@echo "✓ Misaki font downloaded: $(MISAKI_ZIP)"

# Execute font conversion script (after Misaki font verification)
.PHONY: convert-font
convert-font: $(FONT_GOTHIC_BIN) $(FONT_MINCHO_BIN) $(FONT_JISX0201_BIN)
	@echo "✓ Font conversion completed"

# Check Misaki font status
.PHONY: check-misaki
check-misaki:
	@echo "Checking Misaki font status..."
	@if [ -f $(MISAKI_PNG) ]; then \
		echo "✓ Misaki font is available: $(MISAKI_PNG)"; \
	else \
		echo "✗ Misaki font not found"; \
		echo "  Run 'make download-misaki' to download it"; \
	fi

# Download Misaki font only
.PHONY: download-misaki
download-misaki: $(MISAKI_PNG)
	@echo "Misaki font download completed"

# Cleanup
.PHONY: clean
clean:
	@echo "Cleaning up generated files..."
	rm -f $(FONT_GOTHIC_BIN) $(FONT_MINCHO_BIN) $(FONT_JISX0201_BIN)
	@echo "✓ Cleanup completed"

# Complete cleanup including Misaki font files
.PHONY: clean-all
clean-all: clean
	@echo "Cleaning up Misaki font files..."
	rm -f $(MISAKI_ZIP) $(MISAKI_PNG) misaki*.png misaki.txt readme.txt
	@echo "✓ Complete cleanup finished"

# Help
.PHONY: help
help:
	@echo "Available targets:"
	@echo "  all            - Build font binary files (default)"
	@echo "  convert-font   - Convert Misaki font to C64 format"
	@echo "  download-misaki - Download and extract Misaki font"
	@echo "  check-misaki   - Check if Misaki font is available"
	@echo "  clean          - Remove generated binary files"
	@echo "  clean-all      - Remove all files including Misaki font"
	@echo "  help           - Show this help message"
	@echo ""
	@echo "Output files: $(FONT_GOTHIC_BIN), $(FONT_MINCHO_BIN), $(FONT_JISX0201_BIN)"
	@echo "The Makefile will automatically download Misaki font if needed."