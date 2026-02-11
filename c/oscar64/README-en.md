# Oscar64 Japanese Display Sample

| [English](README-en.md) | [日本語](README.md) |
|---------------------------|------------------------|

Basic sample program for Japanese display using the Oscar64 compiler.

## Overview

This sample displays Japanese text in text mode on Commodore 64 when used with the MagicDesk cartridge (`c64jpkanji.crt`). It demonstrates basic usage of the shared library `oscar64_lib/`.

## Features

- Japanese display in text mode
- Shift-JIS string output
- Color settings (text, background, border)

## Notes

### Text Mode Character Limit

Text mode is initially configured to define (display) only 64 characters. Therefore, the message "Looping forever..." in this program will be truncated to "Looping foreve" as the character limit is exceeded.

To display all characters, you need to change the definition range using the `jtxt_set_range()` function:

```c
// Allocate 128 characters for definition (default is 64)
jtxt_set_range(128, 128);
```

## File Structure

```
oscar64/
├── Makefile           # Build configuration
└── src/
    └── main.c         # Main program
```

Library files are referenced from `../oscar64_lib/`.

## Building

```bash
# Build in this directory
make

# Or from root directory
cd ../..
make oscar-build
```

## Running

Requires MagicDesk cartridge. Run from root directory:

```bash
cd ../..
make oscar-hello
```

To run standalone, specify the cartridge separately:

```bash
x64sc -cartcrt ../../crt/c64jpkanji.crt -autostart hello.prg
```

## Requirements

- Oscar64 Compiler
- MagicDesk Cartridge (`c64jpkanji.crt`)
- VICE Emulator or real hardware

## Sample Code

```c
#include "jtxt.h"

int main(void) {
    // Initialize in text mode
    jtxt_init(JTXT_TEXT_MODE);

    // Clear screen
    jtxt_cls();

    // Display Japanese text
    jtxt_locate(5, 8);
    jtxt_puts("こんにちは！");

    while (1) {
        // Main loop
    }
    return 0;
}
```

## Related Projects

- `../oscar64_lib/` - Shared library (jtxt, IME, c64u network)
- `../oscar64_qe/` - QE text editor (with IME)
- `../oscar64_crt/` - EasyFlash version (standalone cartridge)
- `../oscar64_term/` - Terminal (Ultimate II+ network)

## License

MIT License
