# ime.p8 - Japanese Kana-Kanji Conversion Library

| [English](ime-en.md) | [日本語](ime.md) |
|---------------------------|----------------------|

A Japanese input system (IME) library for C64. Enables conversion from romanized input to Japanese.

## Overview

ime.p8 is an IME library that enables Japanese input on the Commodore 64. It performs conversion from romanized input to Hiragana, Katakana, and Kanji, and achieves single-clause conversion using dictionary data stored in a MagicDesk ROM cartridge.

## Key Features

- **Romanized → Kana Conversion**
  - Supports standard romanized input rules
  - Supports yōon (きゃ, しゃ etc.), hatsuon (ん), sokuon (っ)
  - Supports small letters (ぁぃぅぇぉ, ゃゅょ)

- **Kana-Kanji Conversion**
  - Single-clause conversion method
  - Selection from multiple candidates
  - Verb conjugation support (okuriari conversion)

- **Input Modes**
  - Hiragana mode
  - Katakana mode  
  - Full-width alphanumeric mode
  - Direct input (IME OFF)

## Basic Usage

### Initialization
```prog8
ime.init()  ; Initialize IME library
```

### Processing in Main Loop (Non-blocking)
```prog8
ubyte event = ime.process()

when event {
    ime.IME_EVENT_NONE -> {
        ; Ongoing (do nothing)
    }
    ime.IME_EVENT_CONFIRMED -> {
        ; String was confirmed
        uword text = ime.get_confirmed_text()
        jtxt.bputs(text)
    }
    ime.IME_EVENT_CANCELLED -> {
        ; Input was cancelled
    }
    ime.IME_EVENT_MODE_CHANGED -> {
        ; Input mode was changed
    }
    ime.IME_EVENT_KEY_PASSTHROUGH -> {
        ; Process keys that passed through IME
        ubyte key = ime.get_passthrough_key()
        ; Handle on application side
    }
    ime.IME_EVENT_DEACTIVATED -> {
        ; IME was deactivated
    }
}
```

## Key Operations

### IME Control
- **Commodore + Space**: Toggle IME ON/OFF
- **F1**: Hiragana mode
- **F3**: Katakana mode
- **F5**: Full-width alphanumeric mode
- **ESC**: Cancel input
- **Return**: Confirm

### Conversion Operations
- **Space**: Start conversion / Next candidate
- **Shift + Space**: Previous candidate
- **Return**: Confirm candidate
- **ESC**: Cancel conversion

## Main Functions

### Initialization & State Management

#### init()
Initialize IME library. Checks for dictionary data existence and resets internal state.

#### process() -> ubyte
Main IME processing function (non-blocking). Handles key input acquisition, romanization conversion, kana-kanji conversion, display updates, etc.
- Return value: One of IME_EVENT_* constants

#### is_ime_active() -> bool
Returns whether IME is active.

#### toggle_ime_mode()
Toggle IME ON/OFF.

### Retrieval Functions

#### get_confirmed_text() -> uword
Get address of confirmed string. Call after IME_EVENT_CONFIRMED event.

#### get_passthrough_key() -> ubyte
Get key code that passed through IME. Call after IME_EVENT_KEY_PASSTHROUGH event.

### Mode Settings

#### set_hiragana_mode()
Switch to Hiragana mode (equivalent to F1 key).

#### set_katakana_mode()
Switch to Katakana mode (equivalent to F3 key).

#### set_alphanumeric_mode()
Switch to full-width alphanumeric mode (equivalent to F5 key).

### Display Related

#### show_ime_status()
Update IME status display (shows [あ], [カ], [英] etc. in upper right of screen).

#### update_ime_display()
Update display of string being input.

#### draw_candidates_window()
Display conversion candidate window.

## IME Event Constants

```prog8
const ubyte IME_EVENT_NONE = 0          ; Ongoing
const ubyte IME_EVENT_CONFIRMED = 1     ; String confirmed
const ubyte IME_EVENT_CANCELLED = 2     ; Cancelled
const ubyte IME_EVENT_MODE_CHANGED = 3  ; Mode changed
const ubyte IME_EVENT_DEACTIVATED = 4   ; IME deactivated
const ubyte IME_EVENT_KEY_PASSTHROUGH = 5 ; Key passthrough
```

## Romanization Input Rules

### Basic Rules
- a-row: a, i, u, e, o
- ka-row: ka, ki, ku, ke, ko
- ga-row: ga, gi, gu, ge, go
- sa-row: sa, shi/si, su, se, so
- za-row: za, ji/zi, zu, ze, zo
- ta-row: ta, chi/ti, tsu/tu, te, to
- da-row: da, di, du, de, do
- na-row: na, ni, nu, ne, no
- ha-row: ha, hi, fu/hu, he, ho
- ba-row: ba, bi, bu, be, bo
- pa-row: pa, pi, pu, pe, po
- ma-row: ma, mi, mu, me, mo
- ya-row: ya, yu, yo
- ra-row: ra, ri, ru, re, ro
- wa-row: wa, wo, n

### Special Input
- Yōon: kya, kyu, kyo, sha, shu, sho, cha, chu, cho, etc.
- Sokuon: kk→っk, tt→っt, etc. (consonant duplication)
- Hatsuon: n (standalone), nn (explicit)
- Small letters: xa→ぁ, xi→ぃ, xya→ゃ, xtsu→っ, etc.

## Usage Example (from ime_test.p8)

```prog8
%import ime
%import jtxt

main {
    sub start() {
        ; Initialize
        ime.init()
        jtxt.init(64, 190, jtxt.BITMAP_MODE)
        
        bool exit_requested = false
        
        ; Main loop
        while not exit_requested {
            ubyte event = ime.process()
            
            when event {
                ime.IME_EVENT_CONFIRMED -> {
                    ; Display confirmed string
                    uword confirmed_text = ime.get_confirmed_text()
                    jtxt.bputs(confirmed_text)
                }
                ime.IME_EVENT_CANCELLED -> {
                    ; Cancel processing
                }
                ime.IME_EVENT_KEY_PASSTHROUGH -> {
                    ; Passthrough key processing
                    ubyte key = ime.get_passthrough_key()
                    when key {
                        20 -> {  ; BackSpace
                            jtxt.bputc(8)
                        }
                        13 -> {  ; Return
                            jtxt.bnewline()
                        }
                    }
                }
                ime.IME_EVENT_DEACTIVATED -> {
                    ; Processing when IME is deactivated
                }
            }
            
            sys.wait(1)  ; Frame control
        }
    }
}
```

## Memory Usage

- Romanized buffer: 8 bytes
- Hiragana buffer: 64 bytes
- Candidate buffer: 256 bytes
- Confirmed string buffer: 256 bytes

## Important Notes

1. **Dictionary Data**: Dictionary data is required in Banks 10-35 of the CRT file. Without dictionary, only Hiragana input is possible.
2. **Display Updates**: IME display uses bitmap mode functions of jtxt.p8.
3. **Non-blocking Processing**: The process() function must be called every frame.
4. **Character Encoding**: Internal processing is unified with Shift-JIS.