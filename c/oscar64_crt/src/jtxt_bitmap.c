#include "jtxt.h"
#include <c64/easyflash.h>
#include <string.h>

#define POKE(addr, val) (*(volatile uint8_t*)(addr) = (val))
#define PEEK(addr) (*(volatile uint8_t*)(addr))

// Use inline assembly for 8-byte font copy (saves ~28 cycles/kanji)
// Set to 0 to use original C volatile pointer copy
#define USE_ASM_COPY 1

// TODO どうにかする
const bool is_auto_scroll = false;

//=============================================================================
// Pre-computed lookup tables (eliminates all multiplication in draw path)
//=============================================================================

// Bitmap row base addresses: BITMAP_BASE + (y/8)*2560 + (y&7)*320
static const uint16_t bitmap_row_addr[25] = {
    0x6000, 0x6140, 0x6280, 0x63C0, 0x6500, 0x6640, 0x6780, 0x68C0,  // rows 0-7
    0x6A00, 0x6B40, 0x6C80, 0x6DC0, 0x6F00, 0x7040, 0x7180, 0x72C0,  // rows 8-15
    0x7400, 0x7540, 0x7680, 0x77C0, 0x7900, 0x7A40, 0x7B80, 0x7CC0,  // rows 16-23
    0x7E00                                                              // row 24
};

// Screen/color RAM row base addresses: BITMAP_SCREEN_RAM + y*40
static const uint16_t screen_row_addr[25] = {
    0x5C00, 0x5C28, 0x5C50, 0x5C78, 0x5CA0, 0x5CC8, 0x5CF0, 0x5D18,  // rows 0-7
    0x5D40, 0x5D68, 0x5D90, 0x5DB8, 0x5DE0, 0x5E08, 0x5E30, 0x5E58,  // rows 8-15
    0x5E80, 0x5EA8, 0x5ED0, 0x5EF8, 0x5F20, 0x5F48, 0x5F70, 0x5F98,  // rows 16-23
    0x5FC0                                                              // row 24
};

void jtxt_bcls(void) {
    uint8_t top = jtxt_state.bitmap_top_row;
    uint8_t bottom = jtxt_state.bitmap_bottom_row;

    for (uint8_t row = top; row <= bottom; row++) {
        memset((void*)bitmap_row_addr[row], 0, 320);
        memset((void*)screen_row_addr[row], jtxt_state.bitmap_color, 40);
    }

    // Reset cursor position
    jtxt_state.cursor_x = 0;
    jtxt_state.cursor_y = jtxt_state.bitmap_top_row;
    jtxt_state.sjis_first_byte = 0;
}

void jtxt_blocate(uint8_t x, uint8_t y) {
    jtxt_state.cursor_x = x;
    jtxt_state.cursor_y = y;
}

void jtxt_bcolor(uint8_t fg, uint8_t bg) {
    jtxt_state.bitmap_color = ((fg & 0x0F) << 4) | (bg & 0x0F);
}

void jtxt_bwindow(uint8_t top_row, uint8_t bottom_row) {
    jtxt_state.bitmap_top_row = top_row;
    jtxt_state.bitmap_bottom_row = bottom_row;
}

void jtxt_bwindow_enable(void) {
    jtxt_state.bitmap_window_enabled = true;
}

void jtxt_bwindow_disable(void) {
    jtxt_state.bitmap_window_enabled = false;
}

void jtxt_bnewline(void) {
    jtxt_state.cursor_x = 0;

    if (jtxt_state.cursor_y >= jtxt_state.bitmap_bottom_row) {
        if (jtxt_state.bitmap_window_enabled) {
            jtxt_bscroll_up();
        }
        jtxt_state.cursor_y = jtxt_state.bitmap_bottom_row;
    } else {
        jtxt_state.cursor_y++;
    }
}

void jtxt_bbackspace(void) {
    // Cancel Shift-JIS state if active
    if (jtxt_state.sjis_first_byte != 0) {
        jtxt_state.sjis_first_byte = 0;
        return;
    }

    // Move cursor back
    if (jtxt_state.cursor_x != 0) {
        jtxt_state.cursor_x--;
    } else {
        if (jtxt_state.cursor_y > jtxt_state.bitmap_top_row) {
            jtxt_state.cursor_y--;
            jtxt_state.cursor_x = 39;
        } else {
            return;
        }
    }

    // Check window bounds
    if (jtxt_state.cursor_y < jtxt_state.bitmap_top_row ||
        jtxt_state.cursor_y > jtxt_state.bitmap_bottom_row) {
        return;
    }

    // Draw space character to erase
    jtxt_draw_font_to_bitmap(32);
}

void jtxt_bscroll_up(void) {
    uint8_t top = jtxt_state.bitmap_top_row;
    uint8_t bottom = jtxt_state.bitmap_bottom_row;

    for (uint8_t i = top; i < bottom; i++) {
        memcpy((void*)bitmap_row_addr[i], (void*)bitmap_row_addr[i + 1], 320);
        memcpy((void*)screen_row_addr[i], (void*)screen_row_addr[i + 1], 40);
    }

    // Clear last row
    memset((void*)bitmap_row_addr[bottom], 0, 320);
    memset((void*)screen_row_addr[bottom], jtxt_state.bitmap_color, 40);
}

void jtxt_draw_font_to_bitmap(uint16_t char_code) {
    uint8_t cx = jtxt_state.cursor_x;
    uint8_t cy = jtxt_state.cursor_y;

    // Color RAM: table lookup (no multiplication)
    *(volatile uint8_t *)(screen_row_addr[cy] + cx) = jtxt_state.bitmap_color;

    // Bitmap address: table lookup + shift (no multiplication)
    uint16_t dst = bitmap_row_addr[cy] + ((uint16_t)cx << 3);

    // --- Flattened font copy (no define_font/define_kanji call chain) ---

    uint16_t src;
    uint8_t bank;
    uint8_t saved_01;

    if ((char_code & 0xFF00) == 0) {
        // Single-byte: ASCII / half-width kana (Bank 1)
        uint8_t code = (uint8_t)char_code;

        if (code == 0x20) {
            // Space: zero-fill without ROM access
            *(volatile uint32_t *)(dst)     = 0;
            *(volatile uint32_t *)(dst + 4) = 0;
            return;
        }

        src = JTXT_ROM_BASE + ((uint16_t)code << 3);
        bank = 1;
    } else {
        // Double-byte: Kanji
        uint16_t kanji_offset = jtxt_sjis_to_offset(char_code);

        if (kanji_offset < 14336) {
            bank = 1;
            src = JTXT_ROM_BASE + kanji_offset + 2048;
        } else {
            uint16_t adjusted = kanji_offset - 14336;
            bank = (uint8_t)(adjusted >> 14) + 2;
            src = JTXT_ROM_BASE + (adjusted & 0x3FFF);
        }
    }

    // ROM access + bank switch + 8-byte copy (all inline)
    saved_01 = *(volatile uint8_t *)0x01;
    *(volatile uint8_t *)0x01 = saved_01 | 0x01;
    eflash.bank = bank;

#if USE_ASM_COPY
    __asm volatile {
        ldy #0
        lda (src),y
        sta (dst),y
        iny
        lda (src),y
        sta (dst),y
        iny
        lda (src),y
        sta (dst),y
        iny
        lda (src),y
        sta (dst),y
        iny
        lda (src),y
        sta (dst),y
        iny
        lda (src),y
        sta (dst),y
        iny
        lda (src),y
        sta (dst),y
        iny
        lda (src),y
        sta (dst),y
    }
#else
    *(volatile uint8_t *)(dst)     = *(volatile uint8_t *)(src);
    *(volatile uint8_t *)(dst + 1) = *(volatile uint8_t *)(src + 1);
    *(volatile uint8_t *)(dst + 2) = *(volatile uint8_t *)(src + 2);
    *(volatile uint8_t *)(dst + 3) = *(volatile uint8_t *)(src + 3);
    *(volatile uint8_t *)(dst + 4) = *(volatile uint8_t *)(src + 4);
    *(volatile uint8_t *)(dst + 5) = *(volatile uint8_t *)(src + 5);
    *(volatile uint8_t *)(dst + 6) = *(volatile uint8_t *)(src + 6);
    *(volatile uint8_t *)(dst + 7) = *(volatile uint8_t *)(src + 7);
#endif

    eflash.bank = 0;
    *(volatile uint8_t *)0x01 = saved_01;
}

// Internal function for bitmap character output
static void jtxt_bputc_internal(uint16_t char_code) {
    // Check window bounds
    if (jtxt_state.bitmap_window_enabled &&
        (jtxt_state.cursor_y < jtxt_state.bitmap_top_row ||
         jtxt_state.cursor_y > jtxt_state.bitmap_bottom_row)) {
        return;
    }

    // Draw character to bitmap
    jtxt_draw_font_to_bitmap(char_code);

    // Move to next position
    jtxt_state.cursor_x++;
    if (is_auto_scroll && jtxt_state.cursor_x >= 40) {
        jtxt_bnewline();
    }
}

void jtxt_bputc(uint8_t char_code) {
    // Handle Shift-JIS second byte
    if (jtxt_state.sjis_first_byte != 0) {
        if ((char_code >= 0x40 && char_code <= 0x7E) ||
            (char_code >= 0x80 && char_code <= 0xFC)) {
            // Valid Shift-JIS character
            uint16_t sjis_code = ((uint16_t)jtxt_state.sjis_first_byte << 8) | char_code;
            jtxt_bputc_internal(sjis_code);
            jtxt_state.sjis_first_byte = 0;
            return;
        } else {
            // Invalid second byte
            jtxt_bputc_internal(jtxt_state.sjis_first_byte);
            jtxt_state.sjis_first_byte = 0;
        }
    }

    // Check for Shift-JIS first byte
    if ((char_code >= 0x81 && char_code <= 0x9F) ||
        (char_code >= 0xE0 && char_code <= 0xFC)) {
        jtxt_state.sjis_first_byte = char_code;
        return;
    }

    // Handle backspace
    if (char_code == 0x08) {
        jtxt_bbackspace();
        return;
    }

    // Handle newline
    if (char_code == 0x0D) {
        jtxt_bnewline();
        return;
    }

    // Normal single-byte character
    if ((char_code >= 0xA1 && char_code <= 0xDF) ||
        (char_code >= 0x20 && char_code <= 0x7E)) {
        jtxt_bputc_internal(char_code);
    }
}

void jtxt_bputs(const char* str) {
    while (*str) {
        jtxt_bputc(*str);
        str++;
    }
}

//=============================================================================
// bputs_fast: High-performance string rendering (Phase 3 optimization)
//
// Key optimizations vs bputs:
//   1. ROM access ($01 save/restore) done ONCE for entire string
//   2. Bank 0 reset NOT done between characters
//   3. No function calls: entire bputs→bputc→bputc_internal→draw chain inlined
//   4. No window bounds check per character
//   5. Local variables (cx, cy, row bases, color) cached from struct
//
// Limitations:
//   - No backspace (0x08) handling
//   - No window bounds checking
//   - No auto-scroll at bottom
//   - Control characters other than printable are skipped
//=============================================================================

// Zero-page registers for bputs_fast inner loop
static __zeropage uint8_t _fast_ch;
static __zeropage uint8_t _fast_cx;
static __zeropage uint8_t _fast_sjis;

void jtxt_bputs_fast(const char* str) {
    _fast_cx = jtxt_state.cursor_x;
    _fast_sjis = 0;
    uint8_t cy = jtxt_state.cursor_y;
    uint8_t color = jtxt_state.bitmap_color;

    // Cache row base addresses (updated on row change)
    uint16_t bmp_base = bitmap_row_addr[cy];
    uint16_t scr_base = screen_row_addr[cy];

    // Batch ROM access: begin once for entire string
    uint8_t saved_01 = *(volatile uint8_t *)0x01;
    *(volatile uint8_t *)0x01 = saved_01 | 0x01;

    while ((_fast_ch = (uint8_t)*str++) != 0) {
        uint16_t char_code;

        // Inline SJIS state machine
        if (_fast_sjis != 0) {
            // [PERF] Range check removed - trust caller provides valid SJIS data
            // Original: if ((ch >= 0x40 && ch <= 0x7E) || (ch >= 0x80 && ch <= 0xFC)) {
            char_code = ((uint16_t)_fast_sjis << 8) | _fast_ch;
            _fast_sjis = 0;
        } else if ((_fast_ch >= 0x81 && _fast_ch <= 0x9F) || (_fast_ch >= 0xE0 && _fast_ch <= 0xFC)) {
            _fast_sjis = _fast_ch;
            continue;
        } else {
            // [PERF] Printable range check removed - trust caller provides valid data
            // Original: if ((ch >= 0x20 && ch <= 0x7E) || (ch >= 0xA1 && ch <= 0xDF))
            char_code = _fast_ch;
        }

        // --- Inline draw (no function call, no per-char ROM access) ---

        // Color RAM
        *(volatile uint8_t *)(scr_base + _fast_cx) = color;

        // Bitmap address
        uint16_t dst = bmp_base + ((uint16_t)_fast_cx << 3);

        if ((char_code & 0xFF00) == 0) {
            // Single-byte: ASCII / half-width kana
            uint8_t code = (uint8_t)char_code;
            if (code == 0x20) {
                *(volatile uint32_t *)(dst)     = 0;
                *(volatile uint32_t *)(dst + 4) = 0;
            } else {
                eflash.bank = 1;
                uint16_t src = JTXT_ROM_BASE + ((uint16_t)code << 3);
#if USE_ASM_COPY
                __asm volatile {
                    ldy #0
                    lda (src),y
                    sta (dst),y
                    iny
                    lda (src),y
                    sta (dst),y
                    iny
                    lda (src),y
                    sta (dst),y
                    iny
                    lda (src),y
                    sta (dst),y
                    iny
                    lda (src),y
                    sta (dst),y
                    iny
                    lda (src),y
                    sta (dst),y
                    iny
                    lda (src),y
                    sta (dst),y
                    iny
                    lda (src),y
                    sta (dst),y
                }
#else
                *(volatile uint8_t *)(dst)     = *(volatile uint8_t *)(src);
                *(volatile uint8_t *)(dst + 1) = *(volatile uint8_t *)(src + 1);
                *(volatile uint8_t *)(dst + 2) = *(volatile uint8_t *)(src + 2);
                *(volatile uint8_t *)(dst + 3) = *(volatile uint8_t *)(src + 3);
                *(volatile uint8_t *)(dst + 4) = *(volatile uint8_t *)(src + 4);
                *(volatile uint8_t *)(dst + 5) = *(volatile uint8_t *)(src + 5);
                *(volatile uint8_t *)(dst + 6) = *(volatile uint8_t *)(src + 6);
                *(volatile uint8_t *)(dst + 7) = *(volatile uint8_t *)(src + 7);
#endif
            }
        } else {
            // Double-byte: Kanji
            uint16_t kanji_offset = jtxt_sjis_to_offset(char_code);
            uint8_t bank;
            uint16_t src;
            if (kanji_offset < 14336) {
                bank = 1;
                src = JTXT_ROM_BASE + kanji_offset + 2048;
            } else {
                uint16_t adjusted = kanji_offset - 14336;
                bank = (uint8_t)(adjusted >> 14) + 2;
                src = JTXT_ROM_BASE + (adjusted & 0x3FFF);
            }
            eflash.bank = bank;
#if USE_ASM_COPY
            __asm volatile {
                ldy #0
                lda (src),y
                sta (dst),y
                iny
                lda (src),y
                sta (dst),y
                iny
                lda (src),y
                sta (dst),y
                iny
                lda (src),y
                sta (dst),y
                iny
                lda (src),y
                sta (dst),y
                iny
                lda (src),y
                sta (dst),y
                iny
                lda (src),y
                sta (dst),y
                iny
                lda (src),y
                sta (dst),y
            }
#else
            *(volatile uint8_t *)(dst)     = *(volatile uint8_t *)(src);
            *(volatile uint8_t *)(dst + 1) = *(volatile uint8_t *)(src + 1);
            *(volatile uint8_t *)(dst + 2) = *(volatile uint8_t *)(src + 2);
            *(volatile uint8_t *)(dst + 3) = *(volatile uint8_t *)(src + 3);
            *(volatile uint8_t *)(dst + 4) = *(volatile uint8_t *)(src + 4);
            *(volatile uint8_t *)(dst + 5) = *(volatile uint8_t *)(src + 5);
            *(volatile uint8_t *)(dst + 6) = *(volatile uint8_t *)(src + 6);
            *(volatile uint8_t *)(dst + 7) = *(volatile uint8_t *)(src + 7);
#endif
        }

        // Advance cursor
        _fast_cx++;
        if (_fast_cx >= 40) {
            _fast_cx = 0;
            if (cy < 24) cy++;
            bmp_base = bitmap_row_addr[cy];
            scr_base = screen_row_addr[cy];
        }
    }

    // Batch ROM access: end once for entire string
    eflash.bank = 0;
    *(volatile uint8_t *)0x01 = saved_01;

    // Update state
    jtxt_state.cursor_x = _fast_cx;
    jtxt_state.cursor_y = cy;
    jtxt_state.sjis_first_byte = _fast_sjis;
}

void jtxt_bput_hex2(uint8_t value) {
    uint8_t hi = value >> 4;
    uint8_t lo = value & 0x0F;

    jtxt_bputc((hi < 10) ? ('0' + hi) : ('A' + hi - 10));
    jtxt_bputc((lo < 10) ? ('0' + lo) : ('A' + lo - 10));
}

void jtxt_bput_dec2(uint8_t value) {
    jtxt_bputc('0' + (value / 10));
    jtxt_bputc('0' + (value % 10));
}

void jtxt_bput_dec3(uint8_t value) {
    jtxt_bputc('0' + (value / 100));
    jtxt_bputc('0' + ((value / 10) % 10));
    jtxt_bputc('0' + (value % 10));
}