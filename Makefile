# C64 Kanji ROM Cartridge Project Makefile
# 
# Usage:
#   make              - Build hello.p8
#   make TARGET=foo   - Build foo.p8  
#   make run          - Build and run hello.p8
#   make run-strings  - Run with string resources
#   make dict         - Update dictionary and create CRT
#   make clean        - Remove build artifacts
#   make help         - Show help

# Configuration
PROJECT_ROOT := $(shell pwd)
SRC_DIR := $(PROJECT_ROOT)/prog8/src
BUILD_DIR := $(PROJECT_ROOT)/prog8/build
DICCONV_DIR := $(PROJECT_ROOT)/dicconv
FONTCONV_DIR := $(PROJECT_ROOT)/fontconv
CREATECRT_DIR := $(PROJECT_ROOT)/createcrt
STRINGRES_DIR := $(PROJECT_ROOT)/stringresources
C_DIR := $(PROJECT_ROOT)/c
C_BUILD_DIR := $(C_DIR)/build

# Default values
TARGET ?= hello
DICT_FILE ?= skkdic.txt

# Emulator configuration
# For VICE (default):
EMU_COMMAND ?= x64sc
EMU_CARTRIDGE_OPT ?= -cartcrt
EMU_AUTOSTART_OPT ?= -autostart
EMU_MODEM_OPTS ?= -rsdev2 "127.0.0.1:25232" -rsuserbaud "2400" -rsdev2ip232 -rsuserdev "1" -userportdevice "2"
EMU_EXTRA_OPTS ?=

# Alternative emulator examples:
# For CCS64: EMU_COMMAND=ccs64 EMU_CARTRIDGE_OPT=-cart EMU_AUTOSTART_OPT=-autorun
# For Hoxs64: EMU_COMMAND=hoxs64 EMU_CARTRIDGE_OPT=-cartridge EMU_AUTOSTART_OPT=-autostart

# CRT files
CRT_DIR := $(PROJECT_ROOT)/crt
BASIC_CRT := $(CRT_DIR)/c64jpkanji.crt
STRINGS_CRT := $(CRT_DIR)/c64jpkanji_with_strings.crt

# Derived files
P8_FILE := $(SRC_DIR)/$(TARGET).p8
PRG_FILE := $(BUILD_DIR)/$(TARGET).prg
DICT_BASENAME := $(basename $(DICT_FILE))
BINARY_DICT := $(DICCONV_DIR)/$(DICT_BASENAME).bin

# Font files
FONT_GOTHIC_BIN := $(FONTCONV_DIR)/font_misaki_gothic.bin
FONT_MINCHO_BIN := $(FONTCONV_DIR)/font_misaki_mincho.bin
FONT_JISX0201_BIN := $(FONTCONV_DIR)/font_jisx0201.bin

# Default target
.PHONY: all
all: $(PRG_FILE)

# Help
.PHONY: help
help:
	@echo "C64 Kanji ROM Cartridge Project"
	@echo ""
	@echo "Usage:"
	@echo "  make [TARGET=name]     - Build program (default: hello)"
	@echo "  make run [TARGET=name] - Build and run"
	@echo "  make run-strings       - Run with string resources"
	@echo "  make run-no-crt        - Run without cartridge"
	@echo "  make dict              - Update dictionary and create CRT"
	@echo "  make crt               - Create CRT file only"
	@echo "  make crt-skk-jisyo-m   - Create CRT with SKK-JISYO.M dictionary"
	@echo "  make fonts             - Create font files only"
	@echo "  make d64               - Create D64 disk image with all programs"
	@echo "  make build-all         - Build all release targets"
	@echo "  make release-files     - Create both CRT and D64 files"
	@echo "  make clean             - Remove build artifacts"
	@echo "  make clean-all         - Remove all generated files including fonts"
	@echo ""
	@echo "C version targets:"
	@echo "  make c-build           - Build C version with llvm-mos"
	@echo "  make c-hello           - Build and run C hello program"
	@echo "  make c-test            - Build and run C test program"
	@echo "  make c-ime-test        - Build and run C IME test program"
	@echo "  make c-clean           - Remove C build artifacts"
	@echo ""
	@echo "Oscar64 targets:"
	@echo "  make oscar-build       - Build hello with Oscar64"
	@echo "  make oscar-hello       - Build and run hello (Oscar64)"
	@echo "  make oscar-qe-build    - Build QE with Oscar64"
	@echo "  make oscar-qe-run      - Build and run QE (Oscar64)"
	@echo "  make oscar-crt-build   - Build EasyFlash CRT (Oscar64)"
	@echo "  make oscar-crt-run     - Build and run EasyFlash CRT (Oscar64)"
	@echo "  make oscar-clean       - Remove Oscar64 hello build artifacts"
	@echo "  make oscar-qe-clean    - Remove Oscar64 QE build artifacts"
	@echo "  make oscar-crt-clean   - Remove Oscar64 EasyFlash CRT build artifacts"
	@echo ""
	@echo "QE Text Editor targets (llvm-mos):"
	@echo "  make qe-build          - Build QE text editor"
	@echo "  make qe-run            - Build and run QE text editor"
	@echo "  make qe-test           - Build and run QE test program"
	@echo "  make qe-clean          - Remove QE build artifacts"
	@echo ""
	@echo "  make help              - Show this help"
	@echo ""
	@echo "Examples:"
	@echo "  make TARGET=ime_test run"
	@echo "  make TARGET=stateful_test run-strings"
	@echo "  make TARGET=modem_test run-modem"
	@echo "  make EMU_COMMAND=ccs64 EMU_CARTRIDGE_OPT=-cart run"
	@echo "  make EMU_EXTRA_OPTS='-warp -autostartprgmode 1' run"
	@echo ""
	@echo "Configurable variables:"
	@echo "  TARGET     - Program name to build (default: hello)"
	@echo "  DICT_FILE  - Dictionary file name (default: skk-bccwj.txt)"
	@echo ""
	@echo "Emulator configuration:"
	@echo "  EMU_COMMAND        - Emulator command (default: x64sc)"
	@echo "  EMU_CARTRIDGE_OPT  - Cartridge option (default: -cartcrt)"
	@echo "  EMU_AUTOSTART_OPT  - Autostart option (default: -autostart)"
	@echo "  EMU_MODEM_OPTS     - Modem emulation options"
	@echo "  EMU_EXTRA_OPTS     - Additional emulator options"

# Create directories
$(BUILD_DIR):
	@mkdir -p $(BUILD_DIR)

$(C_BUILD_DIR):
	@mkdir -p $(C_BUILD_DIR)

# Build program
$(PRG_FILE): $(P8_FILE) | $(BUILD_DIR)
	@echo "=== Prog8 Kanji Display Build ==="
	@if [ ! -f "$(P8_FILE)" ]; then \
		echo "Error: $(P8_FILE) not found"; \
		exit 1; \
	fi
	@echo "Compiling: $(TARGET).p8"
	@prog8c -asmlist -target c64 -out "$(BUILD_DIR)" "$(P8_FILE)"
	@echo "✓ $(TARGET).p8 compilation completed"

# Font files creation
$(FONT_GOTHIC_BIN) $(FONT_MINCHO_BIN) $(FONT_JISX0201_BIN):
	@echo "=== Creating Font Files ==="
	@echo "Building fonts using fontconv Makefile..."
	@cd $(FONTCONV_DIR) && $(MAKE)
	@echo "Font files creation completed"

# Dictionary binary conversion
$(BINARY_DICT): $(DICCONV_DIR)/$(DICT_FILE)
	@echo "=== Dictionary Binary Conversion ==="
	@echo "Converting: $(DICT_FILE) -> $(notdir $(BINARY_DICT))"
	@cd $(DICCONV_DIR) && python3 dicconv.py "$(DICT_FILE)" "$(notdir $(BINARY_DICT))"
	@echo "Dictionary binary conversion completed: $(notdir $(BINARY_DICT))"

# Create CRT directory
$(CRT_DIR):
	@mkdir -p $(CRT_DIR)

# Create basic CRT
$(BASIC_CRT): $(BINARY_DICT) $(FONT_GOTHIC_BIN) $(FONT_JISX0201_BIN) | $(CRT_DIR)
	@echo "=== Creating Basic CRT ==="
	@echo "Creating MagicDesk format CRT..."
	@cd $(CREATECRT_DIR) && python3 create_crt.py \
		--dictionary-file "../dicconv/$(notdir $(BINARY_DICT))" \
		--output "../crt/$(notdir $(BASIC_CRT))"
	@echo "Basic CRT creation completed: $(BASIC_CRT)"

# Create CRT with string resources
$(STRINGS_CRT): $(BINARY_DICT) $(FONT_GOTHIC_BIN) $(FONT_JISX0201_BIN) $(STRINGRES_DIR)/test_strings.txt | $(CRT_DIR)
	@echo "=== Creating CRT with String Resources ==="
	@echo "Creating MagicDesk version with string resources..."
	@cd $(CREATECRT_DIR) && python3 create_crt.py \
		--dictionary-file "../dicconv/$(notdir $(BINARY_DICT))" \
		--string-resource-file "../stringresources/test_strings.txt" \
		--output "../crt/$(notdir $(STRINGS_CRT))"
	@echo "CRT with string resources creation completed: $(STRINGS_CRT)"

# Update dictionary and CRT
.PHONY: dict
dict: $(BASIC_CRT) $(STRINGS_CRT)
	@echo "=== Created Files ==="
	@echo "Binary dictionary: $(BINARY_DICT)"
	@ls -lh "$(BINARY_DICT)"
	@echo ""
	@echo "CRT files:"
	@for crt in "$(BASIC_CRT)" "$(STRINGS_CRT)"; do \
		if [ -f "$$crt" ]; then \
			echo "  $$crt"; \
			ls -lh "$$crt" | awk '{print "    " $$5 " " $$9}'; \
		fi; \
	done
	@echo ""
	@echo "Dictionary update and CRT creation completed!"

# Create CRT files only
.PHONY: crt
crt: $(BASIC_CRT)

.PHONY: crt-strings
crt-strings: $(STRINGS_CRT)

# Create CRT with SKK-JISYO.M (larger dictionary, not included in repo due to license)
.PHONY: crt-skk-jisyo-m
crt-skk-jisyo-m:
	@if [ ! -f "$(DICCONV_DIR)/SKK-JISYO.M" ]; then \
		echo "Error: $(DICCONV_DIR)/SKK-JISYO.M not found"; \
		echo "Please download SKK-JISYO.M and place it in $(DICCONV_DIR)/"; \
		exit 1; \
	fi
	@$(MAKE) DICT_FILE=SKK-JISYO.M $(BASIC_CRT)

# Create font files only
.PHONY: fonts
fonts: $(FONT_GOTHIC_BIN) $(FONT_MINCHO_BIN) $(FONT_JISX0201_BIN)
	@echo "Font files creation completed"

# List built files
.PHONY: list
list:
	@echo "=== Built Files ==="
	@ls -la "$(BUILD_DIR)"/*.prg 2>/dev/null || echo "No PRG files found"



# Execution targets
.PHONY: run
run: $(PRG_FILE) $(BASIC_CRT)
	@echo "=== Auto-run: $(TARGET).prg ==="
	@echo "Starting with basic MagicDesk Kanji ROM..."
	@if which $(EMU_COMMAND) >/dev/null 2>&1; then \
		"$(EMU_COMMAND)" $(EMU_CARTRIDGE_OPT) "$(BASIC_CRT)" $(EMU_AUTOSTART_OPT) "$(PRG_FILE)" $(EMU_EXTRA_OPTS); \
	else \
		echo "Error: Emulator command not found: $(EMU_COMMAND)"; \
		exit 1; \
	fi

.PHONY: run-strings
run-strings: $(PRG_FILE) $(STRINGS_CRT)
	@echo "=== Auto-run: $(TARGET).prg ==="
	@echo "Starting with MagicDesk Kanji ROM with string resources..."
	@if which $(EMU_COMMAND) >/dev/null 2>&1; then \
		"$(EMU_COMMAND)" $(EMU_CARTRIDGE_OPT) "$(STRINGS_CRT)" $(EMU_AUTOSTART_OPT) "$(PRG_FILE)" $(EMU_EXTRA_OPTS); \
	else \
		echo "Error: Emulator command not found: $(EMU_COMMAND)"; \
		exit 1; \
	fi

.PHONY: run-no-crt
run-no-crt: $(PRG_FILE)
	@echo "=== Auto-run: $(TARGET).prg ==="
	@echo "Starting without cartridge..."
	@if which $(EMU_COMMAND) >/dev/null 2>&1; then \
		"$(EMU_COMMAND)" $(EMU_AUTOSTART_OPT) "$(PRG_FILE)" $(EMU_EXTRA_OPTS); \
	else \
		echo "Error: Emulator command not found: $(EMU_COMMAND)"; \
		exit 1; \
	fi

.PHONY: run-modem
run-modem: $(PRG_FILE) $(BASIC_CRT)
	@echo "=== Auto-run: $(TARGET).prg ==="
	@echo "Starting with modem emulation settings..."
	@if which $(EMU_COMMAND) >/dev/null 2>&1; then \
		"$(EMU_COMMAND)" $(EMU_CARTRIDGE_OPT) "$(BASIC_CRT)" \
			$(EMU_MODEM_OPTS) \
			$(EMU_AUTOSTART_OPT) "$(PRG_FILE)" $(EMU_EXTRA_OPTS); \
	else \
		echo "Error: Emulator command not found: $(EMU_COMMAND)"; \
		exit 1; \
	fi

.PHONY: run-modem-no-crt
run-modem-no-crt: $(PRG_FILE)
	@echo "=== Auto-run: $(TARGET).prg ==="
	@echo "Starting with modem emulation settings (no cartridge)..."
	@if which $(EMU_COMMAND) >/dev/null 2>&1; then \
		"$(EMU_COMMAND)" $(EMU_MODEM_OPTS) \
			$(EMU_AUTOSTART_OPT) "$(PRG_FILE)" $(EMU_EXTRA_OPTS); \
	else \
		echo "Error: Emulator command not found: $(EMU_COMMAND)"; \
		exit 1; \
	fi

# Cleanup
.PHONY: clean
clean:
	@echo "Removing build artifacts..."
	@rm -f $(BUILD_DIR)/*.prg
	@rm -f $(BUILD_DIR)/*.d64
	@rm -f $(DICCONV_DIR)/skkdic*.bin
	@rm -f $(CRT_DIR)/*.crt
	@echo "Cleanup completed"

.PHONY: clean-all
clean-all: clean c-clean qe-clean oscar-clean oscar-qe-clean oscar-crt-clean
	@echo "Removing all generated files..."
	@rm -rf $(BUILD_DIR)
	@cd $(FONTCONV_DIR) && $(MAKE) clean
	@echo "Complete cleanup finished"

# Debug information display
.PHONY: debug
debug:
	@echo "=== Debug Information ==="
	@echo "PROJECT_ROOT: $(PROJECT_ROOT)"
	@echo "TARGET: $(TARGET)"
	@echo "P8_FILE: $(P8_FILE)"
	@echo "PRG_FILE: $(PRG_FILE)"
	@echo "DICT_FILE: $(DICT_FILE)"
	@echo "BINARY_DICT: $(BINARY_DICT)"
	@echo "BASIC_CRT: $(BASIC_CRT)"
	@echo "STRINGS_CRT: $(STRINGS_CRT)"
	@echo "FONT_GOTHIC_BIN: $(FONT_GOTHIC_BIN)"
	@echo "FONT_MINCHO_BIN: $(FONT_MINCHO_BIN)"
	@echo "FONT_JISX0201_BIN: $(FONT_JISX0201_BIN)"
	@echo "EMU_COMMAND: $(EMU_COMMAND)"
	@echo "EMU_CARTRIDGE_OPT: $(EMU_CARTRIDGE_OPT)"
	@echo "EMU_AUTOSTART_OPT: $(EMU_AUTOSTART_OPT)"
	@echo "EMU_MODEM_OPTS: $(EMU_MODEM_OPTS)"
	@echo "EMU_EXTRA_OPTS: $(EMU_EXTRA_OPTS)"

# C version targets
.PHONY: c-build
c-build: $(C_BUILD_DIR) $(BASIC_CRT)
	@echo "=== Building C version with llvm-mos ==="
	@if ! which mos-c64-clang >/dev/null 2>&1; then \
		echo "Error: llvm-mos not found. Please install llvm-mos SDK."; \
		echo "Visit: https://github.com/llvm-mos/llvm-mos-sdk"; \
		exit 1; \
	fi
	@cd $(C_BUILD_DIR) && cmake ../
	@cd $(C_BUILD_DIR) && make
	@echo "C version build completed"

.PHONY: c-hello
c-hello: c-build
	@echo "=== Running hello_c.prg ==="
	@if which $(EMU_COMMAND) >/dev/null 2>&1; then \
		"$(EMU_COMMAND)" $(EMU_CARTRIDGE_OPT) "$(BASIC_CRT)" $(EMU_AUTOSTART_OPT) "$(C_BUILD_DIR)/hello_c.prg" $(EMU_EXTRA_OPTS); \
	else \
		echo "Error: Emulator command not found: $(EMU_COMMAND)"; \
		exit 1; \
	fi

.PHONY: c-test
c-test: c-build
	@echo "=== Running test_jtxt.prg ==="
	@if which $(EMU_COMMAND) >/dev/null 2>&1; then \
		"$(EMU_COMMAND)" $(EMU_CARTRIDGE_OPT) "$(BASIC_CRT)" $(EMU_AUTOSTART_OPT) "$(C_BUILD_DIR)/test_jtxt.prg" $(EMU_EXTRA_OPTS); \
	else \
		echo "Error: Emulator command not found: $(EMU_COMMAND)"; \
		exit 1; \
	fi

.PHONY: c-ime-test
c-ime-test: c-build
	@echo "=== Running ime_test_c.prg ==="
	@if which $(EMU_COMMAND) >/dev/null 2>&1; then \
		"$(EMU_COMMAND)" $(EMU_CARTRIDGE_OPT) "$(BASIC_CRT)" $(EMU_AUTOSTART_OPT) "$(C_BUILD_DIR)/ime_test_c.prg" $(EMU_EXTRA_OPTS); \
	else \
		echo "Error: Emulator command not found: $(EMU_COMMAND)"; \
		exit 1; \
	fi

.PHONY: c-clean
c-clean:
	@echo "Removing C build artifacts..."
	@rm -rf $(C_BUILD_DIR)
	@echo "C cleanup completed"

# Oscar64 targets
OSCAR_DIR := $(C_DIR)/oscar64

.PHONY: oscar-build
oscar-build:
	@echo "=== Building Oscar64 version ==="
	@cd $(OSCAR_DIR) && $(MAKE)

.PHONY: oscar-hello
oscar-hello: oscar-build $(BASIC_CRT)
	@echo "=== Running hello.prg (Oscar64) ==="
	@if which $(EMU_COMMAND) >/dev/null 2>&1; then \
		"$(EMU_COMMAND)" $(EMU_CARTRIDGE_OPT) "$(BASIC_CRT)" $(EMU_AUTOSTART_OPT) "$(OSCAR_DIR)/hello.prg" $(EMU_EXTRA_OPTS); \
	else \
		echo "Error: Emulator command not found: $(EMU_COMMAND)"; \
		exit 1; \
	fi

.PHONY: oscar-clean
oscar-clean:
	@echo "Removing Oscar64 build artifacts..."
	@cd $(OSCAR_DIR) && $(MAKE) clean

# Oscar64 QE targets
OSCAR_QE_DIR := $(C_DIR)/oscar64_qe

.PHONY: oscar-qe-build
oscar-qe-build:
	@echo "=== Building QE with Oscar64 ==="
	@cd $(OSCAR_QE_DIR) && $(MAKE)

.PHONY: oscar-qe-run
oscar-qe-run: oscar-qe-build $(BASIC_CRT)
	@echo "=== Running qe.prg (Oscar64) ==="
	@if which $(EMU_COMMAND) >/dev/null 2>&1; then \
		"$(EMU_COMMAND)" $(EMU_CARTRIDGE_OPT) "$(BASIC_CRT)" $(EMU_AUTOSTART_OPT) "$(OSCAR_QE_DIR)/qe.prg" $(EMU_EXTRA_OPTS); \
	else \
		echo "Error: Emulator command not found: $(EMU_COMMAND)"; \
		exit 1; \
	fi

.PHONY: oscar-qe-clean
oscar-qe-clean:
	@echo "Removing Oscar64 QE build artifacts..."
	@cd $(OSCAR_QE_DIR) && $(MAKE) clean

# Oscar64 EasyFlash CRT targets
OSCAR_CRT_DIR := $(C_DIR)/oscar64_crt

.PHONY: oscar-crt-build
oscar-crt-build:
	@echo "=== Building Oscar64 EasyFlash CRT ==="
	@if ! which oscar64 >/dev/null 2>&1; then \
		echo "Error: oscar64 not found. Please install Oscar64 compiler."; \
		exit 1; \
	fi
	@cd $(OSCAR_CRT_DIR) && $(MAKE)
	@echo "Oscar64 EasyFlash CRT build completed"

.PHONY: oscar-crt-run
oscar-crt-run: oscar-crt-build
	@echo "=== Running EasyFlash CRT (Oscar64) ==="
	@cd $(OSCAR_CRT_DIR) && $(MAKE) run

.PHONY: oscar-crt-clean
oscar-crt-clean:
	@echo "Removing Oscar64 EasyFlash CRT build artifacts..."
	@cd $(OSCAR_CRT_DIR) && $(MAKE) clean
	@echo "Oscar64 EasyFlash CRT cleanup completed"

# QE Text Editor targets
.PHONY: qe-build
qe-build: $(C_BUILD_DIR) $(BASIC_CRT)
	@echo "=== Building QE Text Editor ==="
	@cd $(C_DIR)/src/qe && $(MAKE) all
	@echo "QE text editor build completed"

.PHONY: qe-run
qe-run: qe-build
	@echo "=== Running QE Text Editor ==="
	@if which $(EMU_COMMAND) >/dev/null 2>&1; then \
		"$(EMU_COMMAND)" $(EMU_CARTRIDGE_OPT) "$(BASIC_CRT)" $(EMU_AUTOSTART_OPT) "$(C_BUILD_DIR)/qe.prg" $(EMU_EXTRA_OPTS); \
	else \
		echo "Error: Emulator command not found: $(EMU_COMMAND)"; \
		exit 1; \
	fi

.PHONY: qe-test
qe-test: $(C_BUILD_DIR) $(BASIC_CRT)
	@echo "=== Building and running QE test ==="
	@cd $(C_DIR)/src/qe && $(MAKE) test
	@if which $(EMU_COMMAND) >/dev/null 2>&1; then \
		"$(EMU_COMMAND)" $(EMU_CARTRIDGE_OPT) "$(BASIC_CRT)" $(EMU_AUTOSTART_OPT) "$(C_BUILD_DIR)/qe_fio_test.prg" $(EMU_EXTRA_OPTS); \
	else \
		echo "Error: Emulator command not found: $(EMU_COMMAND)"; \
		exit 1; \
	fi

.PHONY: qe-clean
qe-clean:
	@echo "Cleaning QE build artifacts..."
	@cd $(C_DIR)/src/qe && $(MAKE) clean
	@echo "QE cleanup completed"

# Commonly used combination aliases
.PHONY: ime
ime:
	@$(MAKE) TARGET=ime_test run

.PHONY: ime-strings
ime-strings:
	@$(MAKE) TARGET=ime_test run-strings

.PHONY: stateful
stateful:
	@$(MAKE) TARGET=stateful_test run-strings

.PHONY: modem
modem:
	@$(MAKE) TARGET=modem_test run-modem

# D64 disk image creation
# Release targets for D64 creation
RELEASE_TARGETS := hello hello_bitmap hello_resource ime_test modem_test
C_RELEASE_TARGETS := qe
D64_IMAGE := $(BUILD_DIR)/c64jp_programs.d64

.PHONY: build-all
build-all:
	@echo "=== Building All Release Targets ==="
	@for target in $(RELEASE_TARGETS); do \
		echo "Building $$target.p8..."; \
		$(MAKE) TARGET=$$target $(BUILD_DIR)/$$target.prg || exit 1; \
	done
	@echo "Building C language programs..."
	@for target in $(C_RELEASE_TARGETS); do \
		echo "Building C/$$target..."; \
		$(MAKE) -C $(C_DIR)/src/$$target || exit 1; \
	done
	@echo "All targets built successfully"

.PHONY: d64
d64: build-all $(BASIC_CRT)
	@echo "=== Creating D64 Disk Image ==="
	@if ! which c1541 >/dev/null 2>&1; then \
		echo "Error: c1541 not found. Please install VICE emulator."; \
		exit 1; \
	fi
	@echo "Creating disk image: $(D64_IMAGE)"
	@c1541 -format "c64jp,01" d64 "$(D64_IMAGE)"
	@echo "Adding programs to disk image..."
	@for target in $(RELEASE_TARGETS); do \
		if [ -f "$(BUILD_DIR)/$$target.prg" ]; then \
			case $$target in \
				hello_bitmap) d64name="hello bitmap" ;; \
				hello_resource) d64name="hello resource" ;; \
				ime_test) d64name="ime test" ;; \
				modem_test) d64name="modem test" ;; \
				*) d64name="$$target" ;; \
			esac; \
			echo "  Adding $$target.prg as \"$$d64name\""; \
			c1541 "$(D64_IMAGE)" -write "$(BUILD_DIR)/$$target.prg" "$$d64name"; \
		else \
			echo "  Warning: $$target.prg not found"; \
		fi; \
	done
	@echo "Adding C language programs..."
	@for target in $(C_RELEASE_TARGETS); do \
		if [ -f "$(C_BUILD_DIR)/$$target.prg" ]; then \
			echo "  Adding C/$$target.prg as \"$$target\""; \
			c1541 "$(D64_IMAGE)" -write "$(C_BUILD_DIR)/$$target.prg" "$$target"; \
		else \
			echo "  Warning: C/$$target.prg not found"; \
		fi; \
	done
	@echo "D64 disk image created: $(D64_IMAGE)"
	@echo ""
	@echo "Contents of $(D64_IMAGE):"
	@c1541 "$(D64_IMAGE)" -list

.PHONY: release-files
release-files: d64
	@echo "=== Release Files Ready ==="
	@echo "CRT File: $(BASIC_CRT)"
	@echo "D64 File: $(D64_IMAGE)"
	@ls -la "$(BASIC_CRT)" "$(D64_IMAGE)"
