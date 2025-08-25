# jtxt.p8 - Japanese Text Display Library

| [English](jtxt-en.md) | [日本語](jtxt.md) |
|---------------------------|----------------------|

A Japanese text display library for C64 using MagicDesk Kanji ROM cartridge.

## Overview

jtxt.p8 is a library for displaying Japanese characters (Kanji, Hiragana, Katakana) on the Commodore 64. It uses Misaki fonts (8x8 pixels) stored in a MagicDesk format ROM cartridge and achieves high-speed rendering through PCG (Programmable Character Generator).

## Key Features

- **Two Display Mode Support**
  - Text mode: High-speed character display using PCG
  - Bitmap mode: More flexible screen control

- **Shift-JIS Character Code Support**
  - Full-width characters (JIS X 0208)
  - Half-width Katakana (JIS X 0201)
  - ASCII characters

- **Font Data Access**
  - Large-capacity font access via MagicDesk bank switching
  - Misaki Gothic/Misaki Mincho font support

- **String Resource Functions**
  - Load string resources from CRT files
  - Efficient management of fixed strings

## Constant Definitions

### Memory Addresses
```prog8
const uword ROM_BASE = $8000        ; ROM start address
const uword BANK_REG = $DE00        ; MagicDesk bank switching register
const uword CHARSET_RAM = $3000     ; RAM character set (text mode)
const uword BITMAP_BASE = $6000     ; Bitmap memory start
const uword SCREEN_RAM = $0400      ; Text mode screen RAM
const uword BITMAP_SCREEN_RAM = $5C00  ; Bitmap mode screen RAM
```

### Display Modes
```prog8
const ubyte TEXT_MODE = 0           ; Text mode
const ubyte BITMAP_MODE = 1         ; Bitmap mode
```

### String Resources
```prog8
const ubyte STRING_RESOURCE_BANK = 36  ; String resource start bank
const uword STRING_BUFFER = $0340      ; String buffer (192 bytes)
```

## Main Functions

### Initialization & Settings

#### init(start_char, char_count, mode)
Initialize the library and set the character range and display mode to use.
- `start_char`: Starting character code (range 128-255)
- `char_count`: Number of usable characters (max 127)
- `mode`: Display mode (TEXT_MODE or BITMAP_MODE)

```prog8
jtxt.init(128, 64, jtxt.TEXT_MODE)  ; Initialize in text mode
```

#### set_mode(mode)
Switch display mode.
- `mode`: TEXT_MODE or BITMAP_MODE

#### set_range(start_char, char_count)
Change the character range to use.

### Text Mode Functions

#### cls()
Clear the screen.

#### locate(x, y)
Set cursor position.
- `x`: X coordinate (0-39)
- `y`: Y coordinate (0-24)

#### putc(char_code)
Output one character. Supports Shift-JIS 2-byte characters.
- `char_code`: Character code

#### puts(addr)
Output NULL-terminated string.
- `addr`: String address

```prog8
ubyte[] message = [$82, $B1, $82, $F1, $82, $C9, $82, $BF, $82, $CD, $00]  ; "こんにちは"
jtxt.puts(&message)
```

#### newline()
Insert a newline.

#### set_color(color)
Set text color (0-15).

#### set_bgcolor(color)
Set background color (0-15).

#### set_bordercolor(color)
Set border color (0-15).

### Bitmap Mode Functions

#### bcls()
Clear bitmap screen.

#### blocate(x, y)
Set cursor position in bitmap mode.
- `x`: X coordinate (character units, 0-39)
- `y`: Y coordinate (character units, 0-24)

#### bcolor(fg, bg)
Set foreground and background colors in bitmap mode.
- `fg`: Foreground color (0-15)
- `bg`: Background color (0-15)

#### bputc(char_code)
Output one character in bitmap mode.

#### bputs(addr)
Output NULL-terminated string in bitmap mode.

#### bwindow(top, bottom)
Limit drawing range in bitmap mode.
- `top`: Start row (0-24)
- `bottom`: End row (0-24)

#### bwindow_enable() / bwindow_disable()
Enable/disable row range control.

#### bnewline()
Insert newline in bitmap mode.

#### bscroll_up()
Scroll bitmap screen up by one row.

### String Resource Functions

#### load_string_resource(index) -> uword
Load string resource at specified index.
- `index`: Resource index
- Return value: String buffer address ($0340)

```prog8
uword message = jtxt.load_string_resource(0)
jtxt.puts(message)  ; Display string from resource 0
```

#### putr(index)
Directly output string resource (text mode).

#### bputr(index)  
Directly output string resource (bitmap mode).

### Utility Functions

#### is_firstsjis(code) -> bool
Determine if the specified code is the first byte of Shift-JIS.

#### bput_hex2(value)
Display 2-digit hexadecimal number (bitmap mode).

#### bput_dec2(value) / bput_dec3(value)
Display 2-digit/3-digit decimal numbers (bitmap mode).

## Usage Examples

### Basic Japanese Display
```prog8
%import jtxt

main {
    sub start() {
        ; Initialize
        jtxt.init(128, 64, jtxt.TEXT_MODE)
        
        ; Screen setup
        jtxt.cls()
        jtxt.set_bgcolor(0)  ; Black background
        jtxt.set_color(1)    ; White text
        
        ; Display Japanese message
        ubyte[] message = [
            $82, $B1, $82, $F1, $82, $C9, $82, $BF, $82, $CD,  ; "こんにちは"
            $00
        ]
        jtxt.locate(10, 10)
        jtxt.puts(&message)
    }
}
```

### Bitmap Mode Display
```prog8
%import jtxt

main {
    sub start() {
        ; Initialize in bitmap mode
        jtxt.init(64, 190, jtxt.BITMAP_MODE)
        
        ; Clear screen
        jtxt.bcls()
        
        ; Set colors
        jtxt.bcolor(1, 0)  ; White text, black background
        
        ; Display Japanese
        jtxt.blocate(5, 5)
        jtxt.bputs(iso:"HELLO ")
        jtxt.bputs(&japanese_text)
    }
}
```

## Memory Usage

- **$0340-$03FF**: String buffer (192 bytes)
- **$3000-$3800**: PCG character RAM (text mode, 2KB)
- **$5C00-$5FFF**: Bitmap screen RAM (1KB)
- **$6000-$7FFF**: Bitmap data (8KB)
- **$8000-$9FFF**: MagicDesk bank switching area (8KB)

## Important Notes

1. **Character Range Limitations**
   - Maximum 127 characters can be displayed simultaneously (PCG limitation)
   - Proper range settings are necessary when displaying many different characters

2. **Bank Switching Effects**
   - MagicDesk bank switching uses $DE00
   - Be careful of conflicts with other I/O devices

3. **Shift-JIS Encoding**
   - Japanese strings must be encoded in Shift-JIS
   - Use `iso:` prefix for ASCII strings

4. **Memory Layout**
   - $6000-$7FFF is occupied when using bitmap mode
   - $3000-$37FF is occupied when using text mode