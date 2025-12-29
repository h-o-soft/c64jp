# QE - Oscar64 Text Editor

| [English](README-en.md) | [日本語](README.md) |
|---------------------------|------------------------|

Japanese-capable text editor for Commodore 64, built with Oscar64 compiler.

## Overview

This editor is a port of [qe](https://github.com/davidgiven/cpm65/blob/master/apps/qe.c) from David Given's CP/M-65 project for the Oscar64 compiler. It provides the same functionality as the llvm-mos version (`c/src/qe/`).

### Features

- **Japanese Display**: Full-width character display using Shift-JIS encoding
- **Bitmap Mode**: High-quality Japanese display via jtxt library
- **Kana-Kanji Conversion**: Japanese input via IME with romaji input
- **Vi-like Keybindings**: Efficient text editing operations
- **File I/O**: File read/write using C64 KERNAL routines

## File Structure

```
oscar64_qe/
├── Makefile           # Build configuration
├── include/
│   ├── ime.h          # IME header
│   └── screen.h       # Screen control header
└── src/
    ├── qe.c           # Main editor
    ├── screen.c       # Screen control
    └── ime.c          # Kana-kanji conversion
```

The jtxt library is referenced from `../oscar64_lib/`.

## Building

```bash
# Build in this directory
make

# Or from root directory
cd ../..
make oscar-qe-build
```

## Running

Requires MagicDesk cartridge. Run from root directory:

```bash
cd ../..
make oscar-qe-run
```

## Basic Operations

### Modes

- **Normal Mode** (startup): Cursor movement, deletion, commands
- **Insert Mode**: Text input
- **Replace Mode**: Overwrite text

### Normal Mode Keys

| Key | Action |
|-----|--------|
| `i` | Start insert mode |
| `A` | Start insert mode at end of line |
| `o` / `O` | Insert new line below/above |
| `h` `j` `k` `l` | Cursor movement (left/down/up/right) |
| `^` `$` | Move to start/end of line |
| `x` | Delete character |
| `dd` | Delete line |
| `J` | Join next line |
| `R` | Replace mode |
| `:` | Command mode |

### Command Mode

| Command | Action |
|---------|--------|
| `:w` | Save file |
| `:q` | Quit |
| `:q!` | Force quit |
| `:e filename` | Open file |
| `:n` | New file |

### IME Operations (in Insert Mode)

| Key | Action |
|-----|--------|
| `Commodore + Space` | Enable IME |
| `Space` | Convert/next candidate |
| `Enter` | Confirm |
| `ESC` | Cancel |
| `Ctrl+K` | Toggle hiragana/katakana |

## Requirements

- Oscar64 Compiler
- MagicDesk Cartridge (`c64jpkanji.crt`)
- VICE Emulator or real hardware

## Limitations

- Maximum file size: ~11KB
- No Undo/Redo
- No Search/Replace

## Related Projects

- `../oscar64_lib/` - Shared jtxt library
- `../oscar64/` - Basic sample
- `../oscar64_crt/` - EasyFlash version
- `../../src/qe/` - llvm-mos version

## License

Modified from original qe editor (© 2019 David Given). 2-Clause BSD License.

See `c/src/qe/README.md` for details.
