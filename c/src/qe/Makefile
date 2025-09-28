# QE Text Editor for C64 - llvm-mos build
# Based on qe by David Given

# Compiler settings
CC = mos-c64-clang
AS = mos-c64-clang

COMMON_CFLAGS = -Oz -Wall -flto -std=c99 -I../../include \
    -flto=full -ffunction-sections -fdata-sections \
    -fshort-enums -fmerge-all-constants -fno-stack-protector -fverbose-asm
CFLAGS ?=

ENABLE_IME := 1

ifeq ($(QE_ENABLE_IME),1)
ENABLE_IME := 1
endif

ifneq ($(findstring -DQE_ENABLE_IME,$(CFLAGS)),)
ENABLE_IME := 1
endif
# Enable file I/O using KERNAL routines
CFLAGS += -DENABLE_FILE_IO
LDFLAGS = -Wl,--gc-sections -Wl,--Map=$(BUILD_DIR)/$(TARGET).map -Wl,--print-memory-usage,--icf=safe -Wl,-s

# Directories
SRC_DIR = .
LIB_DIR = lib
BUILD_DIR = ../../build
TARGET = qe

# Source files
C_SOURCES = qe.c $(LIB_DIR)/screen.c
TEST_SOURCES = fio_test.c
JTXT_SOURCES = ../../src/jtxt.c ../../src/jtxt_charset.c ../../src/jtxt_bitmap.c ../../src/jtxt_resource.c

ifeq ($(ENABLE_IME),1)
IME_SOURCES = ../../src/ime.c
endif

# Object files
C_OBJECTS = $(C_SOURCES:.c=.o)
JTXT_OBJECTS = $(JTXT_SOURCES:.c=.o)
IME_OBJECTS = $(IME_SOURCES:.c=.o)
OBJECTS = $(C_OBJECTS) $(JTXT_OBJECTS) $(IME_OBJECTS)

# Main targets
.PHONY: all clean test install qe-run

all: $(BUILD_DIR)/$(TARGET).prg

# Build main editor

ifeq ($(ENABLE_IME),1)
COMMON_CFLAGS += -DQE_ENABLE_IME
endif

$(BUILD_DIR)/$(TARGET).prg: $(OBJECTS) | $(BUILD_DIR)
	@echo "Linking QE editor..."
	$(CC) $(COMMON_CFLAGS) $(CFLAGS) $(LDFLAGS) -o $@ $(OBJECTS)
	@echo "✓ QE editor built: $@"

# Build test program

$(BUILD_DIR)/qe_fio_test.prg: fio_test.o $(LIB_DIR)/screen.o $(ASM_OBJECTS) | $(BUILD_DIR)
	@echo "Linking QE file I/O test..."
	$(CC) $(COMMON_CFLAGS) $(CFLAGS) $(LDFLAGS) -o $@ $^
	@echo "✓ QE test built: $@"

# Compile C sources

%.o: %.c
	@echo "Compiling $<..."
	$(CC) $(COMMON_CFLAGS) $(CFLAGS) -c $< -o $@

# Compile assembly sources
%.o: %.s
	@echo "Assembling $<..."
	$(AS) -c $< -o $@

# Create build directory
$(BUILD_DIR):
	@mkdir -p $(BUILD_DIR)

# Test target
test: $(BUILD_DIR)/qe_fio_test.prg
	@echo "QE test program built"

# Install target (copy to main project)
install: $(BUILD_DIR)/$(TARGET).prg
	@echo "QE editor installed to build directory"

# Run target
run: $(BUILD_DIR)/$(TARGET).prg
	@echo "=== Running QE Editor ==="
	@if [ -f "../../../crt/c64jpkanji.crt" ]; then \
		echo "Starting with Japanese kanji cartridge..."; \
		x64sc -cartcrt "../../../crt/c64jpkanji.crt" -autostart "$(BUILD_DIR)/$(TARGET).prg"; \
	else \
		echo "Starting without cartridge..."; \
		x64sc -autostart "$(BUILD_DIR)/$(TARGET).prg"; \
	fi

# Run test
run-test: $(BUILD_DIR)/qe_fio_test.prg
	@echo "=== Running QE Test ==="
	@x64sc -autostart "$(BUILD_DIR)/qe_fio_test.prg"

# QE run alias
qe-run: run

# Clean
clean:
	@echo "Cleaning QE build files..."
	@rm -f $(OBJECTS)
	@rm -f $(LIB_DIR)/*.o
	@rm -f $(BUILD_DIR)/$(TARGET).prg
	@rm -f $(BUILD_DIR)/qe_fio_test.prg
	@echo "QE cleanup completed"

# Debug info
debug:
	@echo "=== QE Build Configuration ==="
	@echo "CC: $(CC)"
	@echo "COMMON_CFLAGS: $(COMMON_CFLAGS)"
	@echo "CFLAGS: $(CFLAGS)"
	@echo "LDFLAGS: $(LDFLAGS)"
	@echo "C_SOURCES: $(C_SOURCES)"
	@echo "ASM_SOURCES: $(ASM_SOURCES)"
	@echo "OBJECTS: $(OBJECTS)"
	@echo "TARGET: $(BUILD_DIR)/$(TARGET).prg"

# Dependencies
qe.o: qe.c $(LIB_DIR)/screen.h
$(LIB_DIR)/screen.o: $(LIB_DIR)/screen.c $(LIB_DIR)/screen.h
fio_test.o: fio_test.c $(LIB_DIR)/screen.h
