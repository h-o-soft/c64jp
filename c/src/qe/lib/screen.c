#include "screen.h"
#include <c64.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include <stdbool.h>
#include "jtxt.h"

#define SCREEN_COLS 40
#define SCREEN_ROWS 25
#define SCREEN_SIZE (SCREEN_COLS * SCREEN_ROWS)

static uint8_t cursor_x;
static uint8_t cursor_y;
static uint8_t current_style;
static const uint8_t normal_fg_color = 1;  // White text
static const uint8_t normal_bg_color = 6;  // Blue background
static const uint8_t status_fg_color = 0;  // Black text
static const uint8_t status_bg_color = 1;  // White background
static bool sjis_lead_pending = false;

extern uint8_t kernal_getin(void);

// MagicDesk cartridge detection
static bool check_magicdesk_cartridge(void)
{
    // Try to read from MagicDesk bank register
    volatile uint8_t* bank_reg = (volatile uint8_t*)0xDE00;

    // Save original value
    uint8_t original = *bank_reg;

    // Try to switch banks and verify
    *bank_reg = 0;
    uint8_t test1 = *bank_reg;

    *bank_reg = 1;
    uint8_t test2 = *bank_reg;

    // Restore original value
    *bank_reg = original;

    // If we can't switch banks, cartridge might not be present
    return (test1 != test2) || (original == 0);
}

// カーソル位置の文字を反転表示（外部から呼び出し用）
void screen_invert_cursor(uint8_t x, uint8_t y)
{
    if (x < SCREEN_COLS && y < SCREEN_ROWS)
    {
        uint16_t pos = y * SCREEN_COLS + x;
        uint8_t* color_ram = (uint8_t*)0x5C00;
        uint8_t color = color_ram[pos];
        color_ram[pos] = ((color & 0x0F) << 4) | ((color & 0xF0) >> 4);
    }
}

static void mark_cell(uint8_t x, uint8_t y, uint8_t ch)
{
    if (current_style == 1) {
        jtxt_bcolor(status_fg_color, status_bg_color);
    } else {
        jtxt_bcolor(normal_fg_color, normal_bg_color);
    }

    jtxt_blocate(x, y);
    jtxt_bputc(ch);
}

static uint8_t ascii_to_screen(uint8_t c)
{
    // jtxt handles ASCII directly, no conversion needed
    return c;
}

static uint8_t petscii_to_ascii(uint8_t c)
{
    if (c >= 65 && c <= 90)
        return (uint8_t)(c + 32); /* unshifted letters -> lowercase ASCII */
    if (c >= 193 && c <= 218)
        return (uint8_t)(c - 128); /* shifted letters -> uppercase ASCII */
    if (c == 13)
        return 13;
    if (c == 20)
        return 127;
    return c;
}

uint8_t screen_init(void)
{
    jtxt_init(JTXT_BITMAP_MODE);

    cursor_x = 0;
    cursor_y = 0;
    current_style = 0;

    // Set default colors
    jtxt_bcolor(normal_fg_color, normal_bg_color);

    screen_clear();
    screen_showcursor(1);
    return 1;
}

void screen_shutdown(void)
{
    screen_showcursor(1);
    jtxt_cleanup();
}

void screen_clear(void)
{
    jtxt_bcls();
    cursor_x = 0;
    cursor_y = 0;
    sjis_lead_pending = false;
}

uint16_t _screen_getsize(void)
{
    return (uint16_t)(((SCREEN_ROWS - 1) << 8) | (SCREEN_COLS - 1));
}

void _screen_setcursor(uint16_t cursor)
{
    cursor_x = cursor & 0xff;
    cursor_y = (cursor >> 8) & 0xff;
    sjis_lead_pending = false;
}

uint16_t _screen_getcursor(void)
{
    return (uint16_t)(((uint16_t)cursor_y << 8) | cursor_x);
}

static void advance_cursor(void)
{
    cursor_x++;
    if (cursor_x >= SCREEN_COLS)
    {
        cursor_x = 0;
        if (cursor_y < SCREEN_ROWS - 1)
            cursor_y++;
    }
}

void screen_putchar(char c)
{
    if (c == '\n')
    {
        cursor_x = 0;
        if (cursor_y < SCREEN_ROWS - 1)
            cursor_y++;
        sjis_lead_pending = false;
        return;
    }
    if (c == '\r')
    {
        cursor_x = 0;
        sjis_lead_pending = false;
        return;
    }

    uint8_t mapped = ascii_to_screen((uint8_t)c);
    bool is_lead = jtxt_is_firstsjis(mapped);
    mark_cell(cursor_x, cursor_y, mapped);

    if (sjis_lead_pending)
    {
        sjis_lead_pending = false;
        advance_cursor();
    }
    else if (is_lead)
    {
        sjis_lead_pending = true;
    }
    else
    {
        advance_cursor();
    }
}

void screen_putstring(const char* s)
{
    while (*s)
        screen_putchar(*s++);
}

static uint8_t translate_key(uint8_t c)
{
    switch (c)
    {
        case 145: return SCREEN_KEY_UP;
        case 17:  return SCREEN_KEY_DOWN;
        case 157: return SCREEN_KEY_LEFT;
        case 29:  return SCREEN_KEY_RIGHT;
        case 20:  return 127; /* delete */
        case 3:   return 27;  /* RUN/STOP as ESC */
        case 95:  return 27;  /* ← キー */
        case 223: return 27;  /* SHIFT+← */
        default:  return c;
    }
}

uint16_t screen_getchar(uint16_t timeout_cs)
{
    (void)timeout_cs;
    uint8_t value = kernal_getin();
    if (value == 0)
        return 0;
    uint8_t key = translate_key(value);
    if (key == SCREEN_KEY_UP || key == SCREEN_KEY_DOWN ||
        key == SCREEN_KEY_LEFT || key == SCREEN_KEY_RIGHT)
        return key;
    if (key == 27 || key == 127)
        return key;
    uint8_t ascii = petscii_to_ascii(key);
    if (ascii == 0)
        return 0;
    return ascii;
}

uint8_t screen_waitchar(void)
{
    uint16_t c;
    while ((c = screen_getchar(0)) == 0)
        ;
    return (uint8_t)c;
}

void screen_scrollup(void)
{
    jtxt_bscroll_up();
}

void screen_scrolldown(void)
{
    /* Not used; provide a stub */
}

void screen_clear_to_eol(void)
{
    // Set current color and position
    if (current_style == 1) {
        jtxt_bcolor(status_fg_color, status_bg_color);
    } else {
        jtxt_bcolor(normal_fg_color, normal_bg_color);
    }

    for (uint8_t x = cursor_x; x < SCREEN_COLS; x++) {
        jtxt_blocate(x, cursor_y);
        jtxt_bputc(' ');
    }
}

void screen_setstyle(uint8_t style)
{
    current_style = style;
}

void screen_showcursor(uint8_t show)
{
    // カーソル表示制御は呼び出し側で管理
}
