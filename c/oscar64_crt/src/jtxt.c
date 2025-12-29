#include "jtxt.h"
#include <c64/easyflash.h>
#include <string.h>

// Global library state - defined in hello_easyflash.c in main region (RAM)
// (extern declaration is in jtxt.h)

// ROM access management - in RAM (main region)
static uint8_t saved_01_register = 0;

// Internal macros for memory access
#define POKE(addr, val) (*(volatile uint8_t*)(addr) = (val))
#define PEEK(addr) (*(volatile uint8_t*)(addr))
#define POKEW(addr, val) (*(volatile uint16_t*)(addr) = (val))
#define PEEKW(addr) (*(volatile uint16_t*)(addr))

void jtxt_init(uint8_t mode) {
    // Initialize state (same as llvm-mos version initializer)
    jtxt_state.chr_start = 128;
    jtxt_state.chr_count = 64;
    jtxt_state.current_index = 128;
    jtxt_state.screen_pos = JTXT_SCREEN_RAM;
    jtxt_state.color_pos = JTXT_COLOR_RAM;
    jtxt_state.current_color = 1;
    jtxt_state.sjis_first_byte = 0;
    jtxt_state.display_mode = JTXT_TEXT_MODE;
    jtxt_state.cursor_x = 0;
    jtxt_state.cursor_y = 0;
    jtxt_state.bitmap_color = (1 << 4) | 0;  // fg=white, bg=black
    jtxt_state.bitmap_top_row = 0;
    jtxt_state.bitmap_bottom_row = 24;
    jtxt_state.bitmap_window_enabled = false;

    // Set display mode
    jtxt_set_mode(mode);

    // Set EasyFlash to bank 0
    eflash.bank = 0;

    // Disable interrupts
    POKE(0xDC0E, PEEK(0xDC0E) & 0xFE);

    // Memory map: make character ROM visible
    POKE(0x01, 0x33);

    // Copy charset to RAM in text mode only
    if (jtxt_state.display_mode == JTXT_TEXT_MODE) {
        jtxt_copy_charset_to_ram();
    }

    // Restore memory map
    POKE(0x01, 0x37);

    // Re-enable interrupts
    POKE(0xDC0E, PEEK(0xDC0E) | 0x01);

    // Switch VIC to RAM charset in text mode
    if (jtxt_state.display_mode == JTXT_TEXT_MODE) {
        POKE(0xD018, (PEEK(0xD018) & 0xF0) | 0x0C);
    }
}

void jtxt_cleanup(void) {
    // Return to text mode if in bitmap mode
    if (jtxt_state.display_mode == JTXT_BITMAP_MODE) {
        jtxt_set_mode(JTXT_TEXT_MODE);
    }

    // Reset VIC bank to 0
    POKE(0xDD00, (PEEK(0xDD00) & 0xFC) | 0x03);

    // Reset VIC registers
    POKE(0xD018, (PEEK(0xD018) & 0x0F) | 0x10);

    // Reset EasyFlash to bank 0
    eflash.bank = 0;
}

void jtxt_set_mode(uint8_t mode) {
    jtxt_state.display_mode = mode;

    if (mode == JTXT_BITMAP_MODE) {
        // Clear bitmap screen
        jtxt_bcls();

        // Set VIC bank 1 (0x4000-0x7FFF)
        POKE(0xDD00, (PEEK(0xDD00) & 0xFC) | 0x02);

        // Enable bitmap mode
        POKE(0xD011, PEEK(0xD011) | 0x20);

        // Set bitmap at 0x6000, screen at 0x5C00
        POKE(0xD018, 0x79);
    } else {
        // Disable bitmap mode
        POKE(0xD011, PEEK(0xD011) & 0xDF);

        // Reset to VIC bank 0
        POKE(0xDD00, (PEEK(0xDD00) & 0xFC) | 0x03);

        // Set character RAM at 0x3000
        POKE(0xD018, (PEEK(0xD018) & 0xF0) | 0x0C);
    }
}

void jtxt_set_range(uint8_t start_char, uint8_t char_count) {
    jtxt_state.chr_start = start_char;
    jtxt_state.chr_count = char_count;
    jtxt_state.current_index = start_char;
    jtxt_state.screen_pos = JTXT_SCREEN_RAM;
    jtxt_state.color_pos = JTXT_COLOR_RAM;
}

void jtxt_cls(void) {
    // Clear screen with spaces
    memset((void*)JTXT_SCREEN_RAM, 32, 1000);

    // Reset state
    jtxt_state.current_index = jtxt_state.chr_start;
    jtxt_locate(0, 0);
    jtxt_state.sjis_first_byte = 0;
}

void jtxt_locate(uint8_t x, uint8_t y) {
    jtxt_state.screen_pos = JTXT_SCREEN_RAM + (uint16_t)y * JTXT_CHAR_WIDTH + x;
    jtxt_state.color_pos = jtxt_state.screen_pos + (JTXT_COLOR_RAM - JTXT_SCREEN_RAM);
}

void jtxt_set_color(uint8_t color) {
    jtxt_state.current_color = color & 0x0F;
}

void jtxt_set_bgcolor(uint8_t bgcolor, uint8_t bordercolor) {
    POKE(0xD021, bgcolor & 0x0F);
    POKE(0xD020, bordercolor & 0x0F);
}

// Internal function to output a character (handles actual drawing)
static void jtxt_putc_internal(uint16_t char_code) {
    // Range check
    if (jtxt_state.current_index >= jtxt_state.chr_start + jtxt_state.chr_count) {
        return; // Overflow
    }

    // Define character
    jtxt_define_char(jtxt_state.current_index, char_code);

    // Display on screen
    POKE(jtxt_state.screen_pos, jtxt_state.current_index);
    POKE(jtxt_state.color_pos, jtxt_state.current_color);

    // Update position
    jtxt_state.current_index++;
    if (jtxt_state.screen_pos < JTXT_SCREEN_RAM + 999) {
        jtxt_state.screen_pos++;
        jtxt_state.color_pos++;
    }
}

void jtxt_putc(uint8_t char_code) {
    // Handle Shift-JIS second byte
    if (jtxt_state.sjis_first_byte != 0) {
        if ((char_code >= 0x40 && char_code <= 0x7E) ||
            (char_code >= 0x80 && char_code <= 0xFC)) {
            // Valid Shift-JIS character
            uint16_t sjis_code = ((uint16_t)jtxt_state.sjis_first_byte << 8) | char_code;
            jtxt_putc_internal(sjis_code);
            jtxt_state.sjis_first_byte = 0;
            return;
        } else {
            // Invalid second byte, output first byte alone
            jtxt_putc_internal(jtxt_state.sjis_first_byte);
            jtxt_state.sjis_first_byte = 0;
        }
    }

    // Check for Shift-JIS first byte
    if ((char_code >= 0x81 && char_code <= 0x9F) ||
        (char_code >= 0xE0 && char_code <= 0xFC)) {
        jtxt_state.sjis_first_byte = char_code;
        return;
    }

    // Handle newline
    if (char_code == 0x0A || char_code == 0x0D) {
        jtxt_newline();
        return;
    }

    // Normal single-byte character
    if ((char_code >= 0xA1 && char_code <= 0xDF) ||
        (char_code >= 0x20 && char_code <= 0x7E)) {
      jtxt_putc_internal(char_code);
    }
}

void jtxt_puts(const char* str) {
    while (*str) {
        jtxt_putc(*str);
        str++;
    }
}

void jtxt_newline(void) {
    uint16_t pos = jtxt_state.screen_pos - JTXT_SCREEN_RAM;
    uint8_t row = pos / 40;
    if (row < 24) {
        row++;
    }
    jtxt_state.screen_pos = JTXT_SCREEN_RAM + (uint16_t)row * 40;
    jtxt_state.color_pos = jtxt_state.screen_pos + (JTXT_COLOR_RAM - JTXT_SCREEN_RAM);
}

bool jtxt_is_firstsjis(uint8_t char_code) {
    return (char_code >= 0x81 && char_code <= 0x9F) ||
           (char_code >= 0xE0 && char_code <= 0xFC);
}

// ROM access management functions
void jtxt_rom_access_begin(void) {
    // Save current $01 register state
    saved_01_register = PEEK(0x01);

    // Enable BASIC ROM and LO ROM (bit 0 = 1) to access EasyFlash cartridge
    POKE(0x01, saved_01_register | 0x01);
}

void jtxt_rom_access_end(void) {
    // Restore original $01 register state
    POKE(0x01, saved_01_register);
}
