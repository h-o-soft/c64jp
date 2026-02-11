# C64 Japanese Terminal - Oscar64 Build
#
# Ultimate II+ network terminal with Japanese display
# Step 2: Telnet terminal with TCP connection

# Configuration
OSCAR64 = oscar64
TARGET = jterm
OUTPUT = $(TARGET).prg
OUTPUT_CRT = $(TARGET).crt

# Shared library path
LIB_DIR = ../oscar64_lib

# Source files
SOURCES = src/term_main.c src/telnet.c src/xmodem.c \
          $(LIB_DIR)/src/c64u_network.c \
          $(LIB_DIR)/src/jtxt.c $(LIB_DIR)/src/jtxt_bitmap.c \
          $(LIB_DIR)/src/jtxt_charset.c $(LIB_DIR)/src/jtxt_resource.c \
          $(LIB_DIR)/src/jtxt_text.c \
          $(LIB_DIR)/src/ime.c

# Oscar64 compiler options
OSCAR_FLAGS = -O2 -i=include -i=$(LIB_DIR)/include
OSCAR_FLAGS_CRT = -n -tf=crt8 -cid=19 -O2 -dJTXT_MAGICDESK_CRT -i=include -i=$(LIB_DIR)/include

# Ultimate 64 REST API
U64_IP =
U64_API = http://$(U64_IP)/v1

# Emulator configuration
EMU = x64sc

# Default target
.PHONY: all
all: $(OUTPUT)

# Build PRG file
$(OUTPUT): $(SOURCES)
	@echo "=== Building Terminal with Oscar64 ==="
	@if ! which $(OSCAR64) >/dev/null 2>&1; then \
		echo "Error: oscar64 not found. Please install Oscar64 compiler."; \
		exit 1; \
	fi
	$(OSCAR64) $(OSCAR_FLAGS) -o=$(OUTPUT) $(SOURCES)
	@echo "$(OUTPUT) created"

# Build MagicDesk CRT file
.PHONY: crt
crt: $(OUTPUT_CRT)

$(OUTPUT_CRT): $(SOURCES)
	@echo "=== Building Terminal CRT with Oscar64 ==="
	@if ! which $(OSCAR64) >/dev/null 2>&1; then \
		echo "Error: oscar64 not found. Please install Oscar64 compiler."; \
		exit 1; \
	fi
	$(OSCAR64) $(OSCAR_FLAGS_CRT) -o=$(OUTPUT_CRT) $(SOURCES)
	@echo "$(OUTPUT_CRT) created"
	@ls -lh $(OUTPUT_CRT)

# Run in emulator (requires MagicDesk cartridge)
.PHONY: run
run: $(OUTPUT)
	@echo "=== Running Terminal ==="
	$(EMU) $(OUTPUT)

# Run CRT in emulator
.PHONY: run-crt
run-crt: $(OUTPUT_CRT)
	@echo "=== Running Terminal CRT ==="
	$(EMU) -cartcrt $(OUTPUT_CRT)

# Deploy and run on Ultimate 64 via REST API
.PHONY: deploy
deploy: $(OUTPUT)
	@echo "=== Deploying to Ultimate 64 ($(U64_IP)) ==="
	curl -s -X POST --data-binary @$(OUTPUT) $(U64_API)/runners:run_prg
	@echo ""
	@echo "Deployed $(OUTPUT) to Ultimate 64"

# Clean build artifacts
.PHONY: clean
clean:
	@echo "Cleaning build artifacts..."
	@rm -f $(OUTPUT) $(OUTPUT_CRT) *.asm *.int *.lbl *.map
	@echo "Cleanup completed"

# Show help
.PHONY: help
help:
	@echo "C64 Japanese Terminal - Oscar64 Build"
	@echo ""
	@echo "Usage:"
	@echo "  make        - Build $(OUTPUT)"
	@echo "  make crt    - Build $(OUTPUT_CRT)"
	@echo "  make deploy - Build and run on Ultimate 64 ($(U64_IP))"
	@echo "  make run    - Build and run in VICE emulator"
	@echo "  make clean  - Remove build artifacts"
	@echo "  make help   - Show this help"
