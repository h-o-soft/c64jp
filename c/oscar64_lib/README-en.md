# Oscar64 Japanese Text Library (jtxt)

| [English](README-en.md) | [日本語](README.md) |
|---------------------------|------------------------|

Shared library for the Oscar64 compiler. Provides Japanese text display (jtxt), Kana-Kanji conversion (IME), and Ultimate II+ network communication (c64u) functionality.

## Overview

This library is shared by `c/oscar64/`, `c/oscar64_qe/`, `c/oscar64_crt/`, and `c/oscar64_term/`. It provides Japanese display and input functionality when used with the MagicDesk cartridge (`c64jpkanji.crt`).

## Features

### jtxt (Japanese Display)
- **Text Mode**: Fast Japanese display using PCG (Programmable Character Generator)
- **Bitmap Mode**: Japanese display on 320x200 pixel bitmap screen
- **Shift-JIS Support**: Handle text using standard character encoding
- **String Resources**: Load predefined strings from cartridge

### IME (Kana-Kanji Conversion)
- Romaji to Hiragana to Kanji single-clause conversion
- High-speed dictionary lookup using ROM cartridge data
- Verb conjugation support (okuriari conversion)
- Learning function (candidate selection frequency tracking)

### c64u (Ultimate II+ Network Communication)
- TCP/IP communication via Ultimate II+ cartridge network features
- Socket creation, connection, send/receive, and disconnection
- PETSCII/ASCII character code conversion

## File Structure

```
oscar64_lib/
├── include/
│   ├── jtxt.h           # Japanese display header
│   ├── ime.h            # Kana-Kanji conversion header
│   ├── c64u_network.h   # Ultimate II+ network communication header
│   └── c64_oscar.h      # Oscar64-specific definitions
└── src/
    ├── jtxt.c           # Core library
    ├── jtxt_bitmap.c    # Bitmap mode functions
    ├── jtxt_charset.c   # Character definition functions
    ├── jtxt_resource.c  # String resource functions
    ├── jtxt_text.c      # Text mode functions
    ├── ime.c            # Kana-Kanji conversion
    └── c64u_network.c   # Ultimate II+ network communication
```

## API Reference

### Initialization

| Function | Description |
|----------|-------------|
| `jtxt_init(mode)` | Initialize library (TEXT_MODE / BITMAP_MODE) |
| `jtxt_cleanup()` | Cleanup |
| `jtxt_set_mode(mode)` | Switch display mode |
| `jtxt_set_range(start, count)` | Set PCG range |

### Text Mode

| Function | Description |
|----------|-------------|
| `jtxt_cls()` | Clear screen |
| `jtxt_locate(x, y)` | Set cursor position |
| `jtxt_putc(c)` | Output single character (Shift-JIS) |
| `jtxt_puts(str)` | Output string |
| `jtxt_newline()` | New line |
| `jtxt_set_color(color)` | Set text color |
| `jtxt_set_bgcolor(bg, border)` | Set background and border colors |

### Bitmap Mode

| Function | Description |
|----------|-------------|
| `jtxt_bcls()` | Clear screen |
| `jtxt_blocate(x, y)` | Set cursor position |
| `jtxt_bputc(c)` | Output single character |
| `jtxt_bputs(str)` | Output string |
| `jtxt_bnewline()` | New line |
| `jtxt_bbackspace()` | Backspace |
| `jtxt_bcolor(fg, bg)` | Set foreground and background colors |
| `jtxt_bwindow(top, bottom)` | Set display window |
| `jtxt_bscroll_up()` | Scroll up |

### String Resources

| Function | Description |
|----------|-------------|
| `jtxt_putr(id)` | Output resource string in text mode |
| `jtxt_bputr(id)` | Output resource string in bitmap mode |

## Usage Example

```c
#include "jtxt.h"

int main(void) {
    // Initialize in text mode
    jtxt_init(JTXT_TEXT_MODE);

    // Clear screen
    jtxt_cls();

    // Display Japanese string
    jtxt_puts("こんにちは世界！");

    // Display at specific position
    jtxt_locate(5, 10);
    jtxt_set_color(2);  // Red
    jtxt_puts("Commodore 64で日本語");

    // Cleanup
    jtxt_cleanup();
    return 0;
}
```

## Requirements

- **Oscar64 Compiler**: https://github.com/drmortalwombat/oscar64
- **MagicDesk Cartridge**: `c64jpkanji.crt` (generate with `make crt` in root directory)

## Building

This library is not built directly. It is referenced by other projects (`oscar64/`, `oscar64_qe/`, `oscar64_term/`, etc.).

```bash
# When using from oscar64/
cd ../oscar64
make

# When using from oscar64_qe/
cd ../oscar64_qe
make

# When using from oscar64_term/
cd ../oscar64_term
make
```

## Memory Map

| Address | Usage |
|---------|-------|
| $0340-$03FF | String buffer (192 bytes) |
| $0400-$07FF | Text screen RAM |
| $3000-$37FF | PCG character RAM |
| $5C00-$5FFF | Bitmap screen RAM |
| $6000-$7FFF | Bitmap data |
| $8000-$9FFF | MagicDesk bank area |
| $DE00 | MagicDesk bank register |

## Related Projects

- `../oscar64/` - Basic sample (Hello World)
- `../oscar64_qe/` - QE text editor (with IME)
- `../oscar64_crt/` - EasyFlash version (standalone cartridge)
- `../oscar64_term/` - Terminal (Ultimate II+ network, Telnet, XMODEM)

## License

MIT License
