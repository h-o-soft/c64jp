# C64 Japanese Terminal

| [English](README-en.md) | [日本語](README.md) |
|---------------------------|------------------------|

Japanese-capable network terminal for Commodore 64, built with the Oscar64 compiler.

## Overview

A terminal application that connects to Telnet servers via Ultimate II+ cartridge network functionality, with Japanese text display support. Also available as a standalone MagicDesk CRT cartridge.

### Features

- **Telnet Connection**: TCP/IP network connection via Ultimate II+
- **Japanese Display**: Japanese display in bitmap mode via jtxt library
- **Kana-Kanji Conversion**: Japanese input via IME with romaji input
- **XMODEM File Transfer**: Both download (receive) and upload (send) supported
- **VT100 Escape Sequences**: Basic cursor movement and screen control
- **MagicDesk CRT Version**: Standalone cartridge operation using overlay banks

## File Structure

```
oscar64_term/
├── Makefile           # Build configuration
├── include/
│   ├── telnet.h       # Telnet protocol header
│   └── xmodem.h       # XMODEM protocol header
└── src/
    ├── term_main.c    # Main (connection UI, terminal session)
    ├── telnet.c       # Telnet protocol IAC handling
    └── xmodem.c       # XMODEM file transfer & KERNAL I/O
```

Shared libraries (jtxt, IME, c64u network) are referenced from `../oscar64_lib/`.

## Building

```bash
# Build PRG version
make

# Build MagicDesk CRT version
make crt

# Deploy to Ultimate 64 (PRG version)
make deploy

# Run in VICE emulator
make run
```

## Usage

### Connecting
1. A hostname input screen appears on startup
2. Enter the destination hostname and port number, then press Return
3. After successful connection, the terminal session begins

### Terminal Operations

| Key | Action |
|-----|--------|
| `Commodore + Space` | Enable/disable IME (Kana-Kanji conversion) |
| `F1` | XMODEM download (receive file) |
| `F3` | XMODEM upload (send file) |
| `F7` | Disconnect |
| `RUN/STOP` | Abort XMODEM transfer |

### XMODEM File Transfer

Both download and upload follow this procedure:
1. Select device number (+/- to change, Return to confirm)
2. Enter filename
3. Select file type (P: Program / S: Sequential / U: User)
4. Confirm with Y/N on the confirmation screen
5. Transfer begins (progress shown with dots)

Both XMODEM-CRC (CRC-16) and checksum modes are supported.

## MagicDesk CRT Version

The CRT version uses overlay banks to coexist IME and XMODEM within limited memory:

- **Bank 1**: IME overlay (normal operation)
- **Bank 37**: XMODEM overlay (during file transfer)

The IME overlay is automatically reloaded after XMODEM operations.

## Requirements

- **Oscar64 Compiler**: https://github.com/drmortalwombat/oscar64
- **Ultimate II+ Cartridge**: For network connectivity (real hardware or Ultimate 64)
- **MagicDesk Cartridge**: `c64jpkanji.crt` (generate with `make crt` in root directory)
- **VICE Emulator** (optional, network features require real hardware)

## Related Projects

- `../oscar64_lib/` - Shared library (jtxt, IME, c64u network)
- `../oscar64/` - Basic sample
- `../oscar64_qe/` - QE text editor
- `../oscar64_crt/` - EasyFlash version

## License

MIT License
