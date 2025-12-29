#include "jtxt.h"
#include <string.h>


#define POKE(addr, val) (*(volatile uint8_t*)(addr) = (val))
#define PEEK(addr) (*(volatile uint8_t*)(addr))

// TODO どうにかする
const bool is_auto_scroll = false;

void jtxt_bcls(void) {
    // Clear bitmap data in specified range
    uint16_t bitmap_addr = JTXT_BITMAP_BASE + (uint16_t)jtxt_state.bitmap_top_row * 320;
    uint16_t screen_addr = JTXT_BITMAP_SCREEN_RAM + (uint16_t)jtxt_state.bitmap_top_row * 40;

    for (uint8_t row = jtxt_state.bitmap_top_row; row <= jtxt_state.bitmap_bottom_row; row++) {
        // Clear bitmap data (320 bytes per row)
        memset((void*)bitmap_addr, 0, 320);
        bitmap_addr += 320;

        // Clear color information
        memset((void*)screen_addr, jtxt_state.bitmap_color, 40);
        screen_addr += 40;
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
    uint16_t dst_bitmap = JTXT_BITMAP_BASE + (uint16_t)jtxt_state.bitmap_top_row * 320;
    uint16_t src_bitmap = dst_bitmap + 320;
    uint16_t dst_screen = JTXT_BITMAP_SCREEN_RAM + (uint16_t)jtxt_state.bitmap_top_row * 40;
    uint16_t src_screen = dst_screen + 40;

    // Scroll each row
    for (uint8_t i = jtxt_state.bitmap_top_row; i < jtxt_state.bitmap_bottom_row; i++) {
        // Copy bitmap data
        memcpy((void*)dst_bitmap, (void*)src_bitmap, 320);
        dst_bitmap += 320;
        src_bitmap += 320;

        // Copy color data
        memcpy((void*)dst_screen, (void*)src_screen, 40);
        dst_screen += 40;
        src_screen += 40;
    }

    // Clear last row
    memset((void*)dst_bitmap, 0, 320);
    memset((void*)dst_screen, jtxt_state.bitmap_color, 40);
}

void jtxt_draw_font_to_bitmap(uint16_t char_code) {
    // Set color information
    POKE(JTXT_BITMAP_SCREEN_RAM + (uint16_t)jtxt_state.cursor_y * 40 + jtxt_state.cursor_x, jtxt_state.bitmap_color);

    // Calculate bitmap address
    uint16_t bitmap_addr = JTXT_BITMAP_BASE +
                          ((uint16_t)(jtxt_state.cursor_y / 8) * 320 * 8) +
                          ((uint16_t)(jtxt_state.cursor_y & 7) * 320) +
                          ((uint16_t)jtxt_state.cursor_x * 8);

    // Write font data to bitmap
    jtxt_define_font(bitmap_addr, char_code);
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