#ifndef SCREEN_H
#define SCREEN_H

#include <stdint.h>

#define SCREEN_KEY_UP    0x8b
#define SCREEN_KEY_DOWN  0x8a
#define SCREEN_KEY_LEFT  0x88
#define SCREEN_KEY_RIGHT 0x89

uint8_t screen_init(void);
void screen_shutdown(void);

void screen_clear(void);
uint16_t _screen_getsize(void);
void _screen_setcursor(uint16_t c);
uint16_t _screen_getcursor(void);
void screen_putchar(char c);
void screen_putstring(const char* s);
uint16_t screen_getchar(uint16_t timeout_cs);
uint8_t screen_waitchar(void);
void screen_scrollup(void);
void screen_scrolldown(void);
void screen_clear_to_eol(void);
void screen_setstyle(uint8_t style);
void screen_showcursor(uint8_t show);
void screen_invert_cursor(uint8_t x, uint8_t y);

#define screen_setcursor(x, y) \
    _screen_setcursor((uint16_t)((x) & 0xff) | ((uint16_t)(y) << 8))

#define screen_getsize(wp, hp) \
    do { \
        uint16_t c = _screen_getsize(); \
        *(wp) = c & 0xff; \
        *(hp) = c >> 8; \
    } while (0)

#define screen_getcursor(wp, hp) \
    do { \
        uint16_t c = _screen_getcursor(); \
        *(wp) = c & 0xff; \
        *(hp) = c >> 8; \
    } while (0)

#endif

