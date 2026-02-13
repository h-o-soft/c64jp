#ifndef JTXT_H
#define JTXT_H

#include <stdint.h>
#include <stdbool.h>

// Cartridge constants (both MagicDesk and EasyFlash use $DE00)
#define JTXT_ROM_BASE         0x8000U
#define JTXT_BANK_REG         0xDE00U

// Character constants
#define JTXT_CHARSET_ROM      0xD000U
#define JTXT_CHARSET_RAM      0x3000U
#define JTXT_SCREEN_RAM       0x0400U
#define JTXT_BITMAP_SCREEN_RAM 0x5C00U
#define JTXT_COLOR_RAM        0xD800U
#define JTXT_CHAR_WIDTH       40
#define JTXT_CHAR_HEIGHT      25

// Bitmap mode constants
#define JTXT_BITMAP_BASE      0x6000U
#define JTXT_TEXT_MODE        0
#define JTXT_BITMAP_MODE      1

// CRT mode detection
#if defined(JTXT_EASYFLASH) || defined(JTXT_MAGICDESK_CRT)
  #define JTXT_CRT 1
#endif

// Bank offset: MagicDesk CRT uses banks 0-1 for code, so fonts/dict shift by 1
#ifdef JTXT_MAGICDESK_CRT
  #define JTXT_BANK_OFFSET 1
#else
  #define JTXT_BANK_OFFSET 0
#endif

// Font offsets
#define JTXT_JISX0201_SIZE    2048U
#ifdef JTXT_EASYFLASH
  // EasyFlash: kanji offset starts at 0 (JIS X 0201 handled separately in bank layout)
  #define JTXT_JISX0208_OFFSET  0U
#else
  // MagicDesk: kanji data follows JIS X 0201 in same bank sequence
  #define JTXT_JISX0208_OFFSET  JTXT_JISX0201_SIZE
#endif

// String resource constants
#define JTXT_STRING_RESOURCE_BANK 36
#define JTXT_STRING_RESOURCE_BASE JTXT_ROM_BASE
#define JTXT_STRING_BUFFER    0x0340U
#define JTXT_STRING_BUFFER_SIZE 191

// Library state
typedef struct {
    uint8_t chr_start;
    uint8_t chr_count;
    uint8_t current_index;
    uint16_t screen_pos;
    uint16_t color_pos;
    uint8_t current_color;

    // Shift-JIS state
    uint8_t sjis_first_byte;

    // Display mode
    uint8_t display_mode;

    // Bitmap mode variables
    uint8_t cursor_x;
    uint8_t cursor_y;
    uint8_t bitmap_color;

    // Bitmap window control
    uint8_t bitmap_top_row;
    uint8_t bitmap_bottom_row;
    bool bitmap_window_enabled;

    // Deferred wrap: cursor stays at column 39 until next character
    bool wrap_pending;
} jtxt_state_t;

// Main API functions
void jtxt_init(uint8_t mode);
void jtxt_cleanup(void);
void jtxt_set_mode(uint8_t mode);
void jtxt_set_range(uint8_t start_char, uint8_t char_count);

// Text mode functions
void jtxt_cls(void);
void jtxt_locate(uint8_t x, uint8_t y);
void jtxt_putc(uint8_t char_code);
void jtxt_puts(const char* str);
void jtxt_newline(void);
void jtxt_set_color(uint8_t color);
void jtxt_set_bgcolor(uint8_t bgcolor, uint8_t bordercolor);

// Bitmap mode functions
void jtxt_bcls(void);
void jtxt_blocate(uint8_t x, uint8_t y);
void jtxt_bputc(uint8_t char_code);
void jtxt_bputs(const char* str);
void jtxt_bputs_fast(const char* str);
void jtxt_bnewline(void);
void jtxt_bbackspace(void);
void jtxt_bcolor(uint8_t fg, uint8_t bg);
void jtxt_bwindow(uint8_t top_row, uint8_t bottom_row);
void jtxt_bwindow_enable(void);
void jtxt_bwindow_disable(void);
void jtxt_bautowrap_enable(void);
void jtxt_bautowrap_disable(void);
void jtxt_bscroll_up(void);
void jtxt_bclear_to_eol(void);
void jtxt_bclear_line(uint8_t row);

// String resource functions
bool jtxt_load_string_resource(uint8_t resource_number);
void jtxt_putr(uint8_t resource_number);
void jtxt_bputr(uint8_t resource_number);

// Utility functions
bool jtxt_is_firstsjis(uint8_t char_code);
void jtxt_bput_hex2(uint8_t value);
void jtxt_bput_dec2(uint8_t value);
void jtxt_bput_dec3(uint8_t value);

// ROM access management functions
void jtxt_rom_access_begin(void);
void jtxt_rom_access_end(void);

// Internal functions (exposed for modularity)
void jtxt_copy_charset_to_ram(void);
void jtxt_define_char(uint8_t char_code, uint16_t code);
void jtxt_define_font(uint16_t dest_addr, uint16_t code);
void jtxt_define_jisx0201(uint8_t jisx0201_code);
void jtxt_define_kanji(uint16_t sjis_code);
uint16_t jtxt_sjis_to_offset(uint16_t sjis_code);
void jtxt_draw_font_to_bitmap(uint16_t char_code);

// Global state access
extern jtxt_state_t jtxt_state;

#endif // JTXT_H