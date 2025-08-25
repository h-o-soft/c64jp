# String Resource Management Tool

| [English](README-en.md) | [日本語](README.md) |
|---------------------------|------------------------|

String resource conversion and management tool for C64 Kanji ROM cartridge.

## Overview

Tool for storing fixed strings (menus, messages, descriptions, etc.) in ROM cartridge for fast access from programs. Converts from text files or CSV files to indexed binary format.

## Usage

### Basic Usage

```bash
# Convert text file to binary
python3 convert_string_resources.py input.txt output.bin

# Convert CSV file (with IDs)
python3 convert_string_resources.py strings.csv strings.bin

# Specify start offset (position within CRT file)
python3 convert_string_resources.py input.txt output.bin 0x20000

# No 8KB alignment (default has 8KB alignment)
python3 convert_string_resources.py input.txt output.bin 0x20000 --no-align
```

### Using with Project Makefile

```bash
# Run from project root
make TARGET=stateful_test run-strings  # Run with string resources
```

## Input File Format

### Text File Format

One string per line:
```
Hello, World!
Display Japanese on Commodore 64
Kanji ROM Cartridge
Menu Item 1
Menu Item 2
```

### CSV File Format

ID number and string separated by comma:
```csv
0,Main Menu
1,Start Game
2,Options
3,High Scores
4,Exit
10,Settings Screen
11,Volume Control
12,Display Settings
```

## Output Format (Binary)

### Header Structure
```
+0x00: 'STR' + 0x00 (4 bytes) - Magic number
+0x04: Entry count (2 bytes, little-endian)
+0x06: Reserved area (2 bytes)
+0x08: Index table start
```

### Index Table

3 bytes per entry:
```
+0: Offset low byte (1 byte)
+1: Offset high byte (1 byte)  
+2: Bank number (1 byte)
```

Actual address calculation:
```
Real address = Bank number * 8192 + Offset
```

### String Data

After the index table, each string is stored in Shift-JIS encoding with null termination.

## Usage from C64 Programs

### Prog8 Usage Example

```prog8
; Initialize string resources
jtxt.load_string_resource(resource_bank)

; Get and display string by ID
ubyte[] buffer = [0] * 256
if jtxt.get_string_by_id(5, buffer, 256) {
    jtxt.bputs(buffer)
}

; Get string by index
if jtxt.get_string_by_index(0, buffer, 256) {
    jtxt.bputs(buffer)
}
```

### Assembly Usage Example

```assembly
; Bank switching
lda #string_bank
sta $de00

; Get string address from index table
ldy string_id
lda index_table,y
sta ptr_lo
iny
lda index_table,y
sta ptr_hi

; Read string
ldy #0
loop:
    lda (ptr),y
    beq done
    jsr putchar
    iny
    bne loop
done:
```

## Technical Specifications

### convert_string_resources.py

Main functions:

#### load_strings_from_file
```python
def load_strings_from_file(filename):
    """Load strings from text or CSV file"""
    # CSV format: ID,string
    # Text format: one string per line
    return strings_dict
```

#### create_binary_resource
```python
def create_binary_resource(strings, base_offset=0):
    """Create binary resource from strings dictionary"""
    # 1. Create header
    # 2. Create index table
    # 3. Add string data
    return binary_data
```

### Memory Efficiency

- Automatic detection and sharing of duplicate strings
- 8KB bank boundary alignment
- Compression options (planned for future implementation)

## File Structure

```
stringresources/
├── convert_string_resources.py  # Conversion script
├── README.md                    # This file
├── test_strings.txt             # Sample: text format
├── verified_strings.csv         # Sample: CSV format
├── string_resources.bin         # Output: binary resource
└── string_resources_test.bin    # Test binary
```

## Limitations

- Maximum entries: 65,535 (16-bit index)
- Maximum string length: Virtually unlimited (depends on memory size)
- Character encoding: Shift-JIS only
- Total size: Up to MagicDesk cartridge capacity (1MB)

## Troubleshooting

### Character Corruption

- Check input file character encoding (UTF-8 recommended)
- Verify no characters that cannot be converted to Shift-JIS

### Memory Shortage

- Remove unnecessary strings
- Split long strings
- Split into multiple resource files

### Access Errors

- Verify bank number is correct
- Check offset calculation is correct
- Confirm string resources are included in CRT file

## Best Practices

1. **ID Management**: Explicitly manage IDs with CSV files
2. **Grouping**: Place related strings at nearby IDs
3. **Version Control**: Manage string resource files with Git
4. **Multi-language Support**: Create separate files per language

## Related Documentation

- [Project README](../README.md)
- [CRT Tools Manual](../CRT_TOOLS_MANUAL.md)
- [String Resource Design](../STRING_RESOURCE.md)