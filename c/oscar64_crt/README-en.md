# C64 Kanji ROM Cartridge - Oscar64 EasyFlash Build

| [English](README-en.md) | [日本語](README.md) |
|---------------------------|------------------------|

Sample project that generates an EasyFlash cartridge integrating Kanji ROM and program using the Oscar64 compiler.

## Features

- **Single Source Build**: Generate CRT file directly from `hello_easyflash.c`
- **Font Embedding**: Embed binary data using `#embed` directive
- **Auto RAM Expansion**: Copy main code to RAM using EasyFlash auto-start
- **Cross-Bank Execution**: Copy and execute ROM bank code in RAM
- **No Python Required**: Complete with Oscar64 compiler only

## Building

```bash
make
```

This generates `hello_easyflash.crt`.

## Running

```bash
make run
```

Starts the cartridge in VICE emulator.

Also available from root directory:

```bash
make oscar-crt-run
```

## Memory Map

### RAM Areas

| Address | Usage |
|---------|-------|
| $0900-$7FFF | Main program (auto-copied) |
| $3000-$37FF | Character RAM (text mode) |
| $5C00-$5FFF | Screen RAM (bitmap mode) |
| $6000-$7FFF | Bitmap data |
| $C000-$CFFF | Additional code area (copied from ROM) |

### EasyFlash ROM Banks (16KB each, $8000-$BFFF)

| Bank | Contents | Size |
|------|----------|------|
| 0 | Main program (copied to RAM at startup) | 16KB |
| 1 | JIS X 0201 + Gothic Part 1 | 2KB + 14KB |
| 2 | Gothic Part 2 | 16KB |
| 3 | Gothic Part 3 | 16KB |
| 4 | Gothic Part 4 | 16KB |
| 5 | Gothic Part 5 (remainder) | ~6KB |
| 6 | Additional code (copied to $C000 for execution) | 4KB |

## Project Structure

```
hello_easyflash.c    Main source file
├── Main Region      $0900-$8000 (auto-copied to RAM)
│   ├── ccopy()      Cross-bank copy function
│   ├── bankcall_*() Bank call wrappers
│   ├── jtxt library
│   └── main()
├── Bank 1-5         Font data
└── Bank 6           Additional code (relocated to $C000)
```

## Cross-Bank Code Execution

Bank 6 code specifies relocation address with 7th parameter of `#pragma region`:

```c
#pragma region(bank6, 0x8000, 0xc000, , 6, { code6 }, 0xc000)
```

This places Bank 6 code at $8000 but links it for execution at $C000.

### Usage

```c
// 1. Copy from ROM to RAM
ccopy(6, (char*)0xC000, (char*)0x8000, 0x1000);

// 2. Call functions directly
test_from_bank6_first();
test_from_bank6_second();
```

## Requirements

- **Oscar64 Compiler**: https://github.com/drmortalwombat/oscar64
- **Font Files**: Place in `../../fontconv/`
  - `font_jisx0201.bin` (2KB)
  - `font_misaki_gothic.bin` (~70KB)
- **VICE Emulator** (for running only)

## Build Command Details

```bash
oscar64 -n -tf=crt -i=include -o=hello_easyflash.crt hello_easyflash.c src/*.c
```

- `-n`: No startup code
- `-tf=crt`: EasyFlash format CRT output (16KB banks)
- `-i=include`: Include directory

## Troubleshooting

### Font files not found

```bash
cd ../../fontconv
make
```

### Oscar64 not installed

Download and install from https://github.com/drmortalwombat/oscar64

## Related Projects

- `../oscar64_lib/` - Shared library (jtxt, IME, c64u network)
- `../oscar64/` - PRG version (uses external MagicDesk cartridge)
- `../oscar64_qe/` - QE text editor (uses external cartridge)
- `../oscar64_term/` - Terminal (Ultimate II+ network)

## License

The sample code in this project is free to use and modify.
