# swiftlink.p8 - SwiftLink RS-232 Communication Library

| [English](swiftlink-en.md) | [日本語](swiftlink.md) |
|--------------------------------|---------------------------|

A SwiftLink-compatible RS-232 serial communication library for Commodore 64. Controls the 6551 ACIA chip.

## Overview

swiftlink.p8 is a library for RS-232 serial communication using SwiftLink cartridge or its compatible products. It implements a 256-byte ring buffer driven by NMI interrupts to achieve stable communication.

## Key Features

- **6551 ACIA Control**
  - Support for various baud rates (actually 2x constant names, up to 38400bps)
  - Communication parameter settings like 8N1, 7E1
  - Hardware flow control (RTS/CTS)

- **NMI Interrupt-Driven**
  - Automatic buffering of received data
  - Prevention of data loss

- **256-Byte Ring Buffer**
  - Efficient data management
  - Buffer overflow detection

## I/O Address Setting

Uses `$DF00` by default, but can be changed to avoid conflicts with REU.

```prog8
const uword SWIFTLINK_BASE = $DF00  ; Default
; Recommended to change to $DF80 when using with REU
```

### Register Addresses
- `$DF00`: Data Register (R/W)
- `$DF01`: Status/Reset Register (R/W)
- `$DF02`: Command Register (W)
- `$DF03`: Control Register (W)

## Basic Usage

### Initialization and Communication Start
```prog8
%import swiftlink

main {
    sub start() {
        ; Initialize at 2400bps, 8N1
        swiftlink.init_simple(swiftlink.BAUD_2400)
        
        ; Or custom settings
        swiftlink.init(
            swiftlink.BAUD_9600, 
            swiftlink.DATA_8 | swiftlink.STOP_1 | swiftlink.PARITY_NONE
        )
    }
}
```

### Data Transmission
```prog8
; Send one byte
swiftlink.send_byte('A')

; Send string
swiftlink.send_string(iso:"Hello, World!\r\n")

; Send Shift-JIS string (Japanese)
ubyte[] message = [$82, $B1, $82, $F1, $82, $C9, $82, $BF, $82, $CD, $00]
swiftlink.send_string(&message)
```

### Data Reception
```prog8
; Receive one byte (blocking)
ubyte data = swiftlink.receive_byte()

; Receive one byte (non-blocking)
if swiftlink.data_available() {
    ubyte data = swiftlink.get_byte()
}

; Receive string
ubyte[256] buffer
ubyte len = swiftlink.receive_string(&buffer, 255, 10)  ; Max 255 bytes, timeout 10
```

## Main Functions

### Initialization & Settings

#### init(baud_rate, config)
Initialize with specified baud rate and settings.
- `baud_rate`: One of BAUD_* constants
- `config`: Combination of data bits, stop bits, and parity

#### init_simple(baud_rate)
Simple initialization with 8N1 settings.
- `baud_rate`: One of BAUD_* constants

#### close()
End communication and restore NMI handler.

### Transmission Functions

#### send_byte(data) -> bool
Send one byte.
- `data`: Data to send
- Return value: true=success, false=timeout

#### send_string(str_ptr) -> bool
Send NULL-terminated string.
- `str_ptr`: String address
- Return value: true=success, false=error

### Reception Functions

#### data_available() -> bool
Check if received data is available.

#### get_byte() -> ubyte
Get one byte from buffer (non-blocking).

#### receive_byte() -> ubyte
Receive one byte (blocking).

#### receive_string(buffer, max_len, timeout) -> ubyte
Receive string.
- `buffer`: Reception buffer
- `max_len`: Maximum reception length
- `timeout`: Timeout value (0=infinite wait)
- Return value: Number of bytes actually received

### Flow Control

#### set_rts(enabled)
Control RTS signal.
- `enabled`: true=enabled, false=disabled

#### get_dcd() -> bool
Get carrier detect status.

#### get_dsr() -> bool
Get data set ready status.

### Error Handling

#### get_last_error() -> ubyte
Get last error code.

#### clear_error()
Clear error status.

## Baud Rate Constants

**Note**: SwiftLink-compatible products use 3.6864MHz crystal, so actual communication speed is about 2x the constant names.

```prog8
const ubyte BAUD_300 = %00000110    ; Actually about 600bps
const ubyte BAUD_600 = %00000111    ; Actually about 1200bps  
const ubyte BAUD_1200 = %00001000   ; Actually about 2400bps
const ubyte BAUD_2400 = %00001010   ; Actually about 4800bps
const ubyte BAUD_4800 = %00001100   ; Actually about 9600bps
const ubyte BAUD_9600 = %00001110   ; Actually about 19200bps
const ubyte BAUD_19200 = %00001111  ; Actually about 38400bps
```

## Error Codes

```prog8
const ubyte ERROR_NONE = 0
const ubyte ERROR_TIMEOUT = 1
const ubyte ERROR_PARITY = 2
const ubyte ERROR_FRAMING = 3
const ubyte ERROR_OVERRUN = 4
const ubyte ERROR_BUFFER_FULL = 5
```

## Usage Example (from modem_test.p8)

```prog8
%import swiftlink
%import jtxt

main {
    sub start() {
        ; Initialize
        swiftlink.init_simple(swiftlink.BAUD_2400)
        jtxt.init(64, 190, jtxt.BITMAP_MODE)
        
        ; Send Japanese message
        ubyte[] hello = [
            $82, $B1, $82, $F1, $82, $C9, $82, $BF, $82, $CD,  ; "こんにちは"
            $0D, $0A, $00  ; CRLF
        ]
        swiftlink.send_string(&hello)
        
        ; Echo server
        repeat {
            if swiftlink.data_available() {
                ubyte ch = swiftlink.get_byte()
                jtxt.bputc(ch)  ; Screen display
                swiftlink.send_byte(ch)  ; Echo back
            }
        }
    }
}
```

## Usage with VICE Emulator

When using RS-232 emulation with VICE:

```bash
make TARGET=modem_test run-modem
```

Or manually:
```bash
x64sc -rsdev2 "127.0.0.1:25232" -rsuserbaud "2400" \
      -rsdev2ip232 -rsuserdev "1" -userportdevice "2"
```

## Important Notes

1. **I/O Address Conflicts**
   - When using REU: Move SwiftLink to `$DF80`
   - MagicDesk: Uses `$DE00` (no conflict)

2. **NMI Interrupts**
   - Be careful of conflicts with other NMI-using code
   - Always restore with close()

3. **Buffer Size**
   - Reception buffer is fixed at 256 bytes
   - Check buffer frequently during high-speed communication

4. **Real Hardware Operation**
   - Requires actual SwiftLink cartridge
   - Can connect to PC via null modem cable