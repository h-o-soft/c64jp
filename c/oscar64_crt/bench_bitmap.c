/*
 * C64 Japanese Kanji ROM - Bitmap Rendering Benchmark
 * Oscar64 EasyFlash Version
 *
 * Measures rendering performance of bitmap mode character drawing
 * using CIA2 Timer A for cycle-accurate measurement.
 *
 * Tests:
 *   1. Single ASCII draw (draw_font_to_bitmap direct)
 *   2. Single Kanji draw (draw_font_to_bitmap direct)
 *   3. bputs 10 ASCII chars (includes SJIS state machine)
 *   4. bputs 10 Kanji chars (includes SJIS parsing)
 *   5. Line fill 40 ASCII (draw_font_to_bitmap x40)
 *   6. Line fill 40 Kanji (draw_font_to_bitmap x40)
 *   7. Scroll up (full 25-row scroll)
 *   8. Full screen ASCII fill (1000 chars, 32-bit accumulation)
 *   9. Full screen Kanji fill (1000 chars, 32-bit accumulation)
 */

#include <c64/memmap.h>
#include <c64/cia.h>
#include <c64/vic.h>
#include <c64/easyflash.h>
#include <c64/keyboard.h>
#include <string.h>
#include "jtxt.h"

#define POKE(addr, val) (*(volatile unsigned char *)(addr) = (val))
#define PEEK(addr) (*(volatile unsigned char *)(addr))

#define COLOR_BLACK   0
#define COLOR_WHITE   1
#define COLOR_RED     2
#define COLOR_GREEN   5
#define COLOR_BLUE    6
#define COLOR_YELLOW  7

//=============================================================================
// Main Region: RAM ($0900-$8000)
//=============================================================================

#pragma region( main, 0x0900, 0x8000, , , { code, data, bss, heap, stack } )

jtxt_state_t jtxt_state;

//=============================================================================
// CIA2 Timer A for cycle counting ($DD04/$DD05/$DD0E)
//
// CIA2 timers are free in EasyFlash mode (no KERNAL/NMI handler).
// Timer counts down from latch value at PHI2 clock rate (~1MHz).
// Elapsed cycles = 0xFFFF - timer_value + overhead correction.
//=============================================================================

// Start CIA2 Timer A counting down from $FFFF
static void timer_start(void)
{
    // Stop timer first
    *(volatile unsigned char *)0xDD0E &= 0xFE;
    // Set latch to $FFFF
    *(volatile unsigned char *)0xDD04 = 0xFF;
    *(volatile unsigned char *)0xDD05 = 0xFF;
    // Force load latch (bit4) + start continuous (bit0), count PHI2 (bit5=0)
    *(volatile unsigned char *)0xDD0E = 0x11;
}

// Stop CIA2 Timer A and return elapsed cycles
static unsigned int timer_stop(void)
{
    // Stop timer
    *(volatile unsigned char *)0xDD0E &= 0xFE;
    // Read value (high byte first for consistency)
    unsigned char hi = *(volatile unsigned char *)0xDD05;
    unsigned char lo = *(volatile unsigned char *)0xDD04;
    return 0xFFFFu - (((unsigned int)hi << 8) | lo);
}

//=============================================================================
// Display utilities
//=============================================================================

// Display right-aligned 5-digit decimal number
static void put_uint16(unsigned int num)
{
    char buf[6];
    signed char i;
    buf[5] = 0;

    for (i = 4; i >= 0; i--) {
        if (num > 0 || i == 4) {
            buf[i] = '0' + (unsigned char)(num % 10);
            num /= 10;
        } else {
            buf[i] = ' ';
        }
    }
    jtxt_bputs(buf);
}

// Display right-aligned 7-digit decimal number (for 32-bit values)
static void put_uint32(unsigned long num)
{
    char buf[8];
    signed char i;
    buf[7] = 0;

    for (i = 6; i >= 0; i--) {
        if (num > 0 || i == 6) {
            buf[i] = '0' + (unsigned char)(num % 10);
            num /= 10;
        } else {
            buf[i] = ' ';
        }
    }
    jtxt_bputs(buf);
}

// Wait for SPACE key press and release
static void wait_space(void)
{
    while (true) {
        keyb_poll();
        if (key_pressed(KSCAN_SPACE)) break;
    }
    while (true) {
        keyb_poll();
        if (!key_pressed(KSCAN_SPACE)) break;
    }
}

//=============================================================================
// Kanji code table (40 unique Shift-JIS codes spanning multiple ROM banks)
//=============================================================================

static const unsigned int kanji_40[] = {
    0x8ABF, 0x8E9A, 0x93FA, 0x967B, 0x8CEA,  // 漢字日本語 (bank 2-4)
    0x82A0, 0x82A2, 0x82A4, 0x82A6, 0x82A8,  // あいうえお (bank 1-2)
    0x82A9, 0x82AB, 0x82AD, 0x82AF, 0x82B1,  // かきくけこ
    0x82B3, 0x82B5, 0x82B7, 0x82B9, 0x82BB,  // さしすせそ
    0x82BD, 0x82BF, 0x82C2, 0x82C4, 0x82C6,  // たちつてと
    0x82C8, 0x82C9, 0x82CA, 0x82CB, 0x82CC,  // なにぬねの
    0x82CD, 0x82D0, 0x82D3, 0x82D6, 0x82D9,  // はひふへほ
    0x82DC, 0x82DD, 0x82DE, 0x82DF, 0x82E0   // まみむめも
};

//=============================================================================
// Benchmark functions
//=============================================================================

// Test 1: Single ASCII char (direct draw, no SJIS overhead)
static unsigned int bench_draw_ascii_1(void)
{
    jtxt_blocate(0, 24);
    timer_start();
    jtxt_draw_font_to_bitmap('A');
    return timer_stop();
}

// Test 2: Single Kanji char (direct draw, includes sjis_to_offset + bank switch)
static unsigned int bench_draw_kanji_1(void)
{
    jtxt_blocate(0, 24);
    timer_start();
    jtxt_draw_font_to_bitmap(0x8ABF);  // "漢"
    return timer_stop();
}

// Test 3: 10 ASCII chars via bputs (SJIS state machine + draw)
static unsigned int bench_bputs_ascii_10(void)
{
    jtxt_blocate(0, 24);
    timer_start();
    jtxt_bputs("ABCDEFGHIJ");
    return timer_stop();
}

// Test 4: 10 Kanji chars via bputs (SJIS parsing + sjis_to_offset + draw)
static unsigned int bench_bputs_kanji_10(void)
{
    jtxt_blocate(0, 24);
    timer_start();
    // "あいうえおかきくけこ" in Shift-JIS (20 bytes)
    jtxt_bputs("\x82\xa0\x82\xa2\x82\xa4\x82\xa6\x82\xa8"
               "\x82\xa9\x82\xab\x82\xad\x82\xaf\x82\xb1");
    return timer_stop();
}

// Test 5: 40 ASCII draw_font_to_bitmap (one full row, direct calls)
static unsigned int bench_line_ascii_40(void)
{
    unsigned char x;
    timer_start();
    for (x = 0; x < 40; x++) {
        jtxt_blocate(x, 24);
        jtxt_draw_font_to_bitmap('A' + (x % 26));
    }
    return timer_stop();
}

// Test 6: 40 Kanji draw_font_to_bitmap (one full row, various ROM banks)
static unsigned int bench_line_kanji_40(void)
{
    unsigned char x;
    timer_start();
    for (x = 0; x < 40; x++) {
        jtxt_blocate(x, 24);
        jtxt_draw_font_to_bitmap(kanji_40[x]);
    }
    return timer_stop();
}

// Test 7: Scroll up (25-row region, 320 bytes/row bitmap + 40 bytes/row color)
static unsigned int bench_scroll_up(void)
{
    timer_start();
    jtxt_bscroll_up();
    return timer_stop();
}

// Test 8: Full screen ASCII fill (1000 chars) - accumulated row by row
static unsigned long bench_fullscreen_ascii(void)
{
    unsigned long total = 0;
    unsigned char x, y;

    for (y = 0; y < 25; y++) {
        timer_start();
        for (x = 0; x < 40; x++) {
            jtxt_blocate(x, y);
            jtxt_draw_font_to_bitmap('A' + ((x + y) % 26));
        }
        total += timer_stop();
    }
    return total;
}

// Test 9: Full screen Kanji fill (1000 chars) - accumulated row by row
static unsigned long bench_fullscreen_kanji(void)
{
    unsigned long total = 0;
    unsigned char x, y;

    for (y = 0; y < 25; y++) {
        timer_start();
        for (x = 0; x < 40; x++) {
            jtxt_blocate(x, y);
            jtxt_draw_font_to_bitmap(kanji_40[x]);
        }
        total += timer_stop();
    }
    return total;
}

//=============================================================================
// Phase 3: bputs_fast benchmarks
//=============================================================================

// 40-char ASCII line for full screen bputs_fast test
static const char ascii_line_40[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789ABCD";

// 40 kanji as SJIS byte string (matching kanji_40[] codes)
static const char kanji_line_40[] =
    "\x8a\xbf\x8e\x9a\x93\xfa\x96\x7b\x8c\xea"
    "\x82\xa0\x82\xa2\x82\xa4\x82\xa6\x82\xa8"
    "\x82\xa9\x82\xab\x82\xad\x82\xaf\x82\xb1"
    "\x82\xb3\x82\xb5\x82\xb7\x82\xb9\x82\xbb"
    "\x82\xbd\x82\xbf\x82\xc2\x82\xc4\x82\xc6"
    "\x82\xc8\x82\xc9\x82\xca\x82\xcb\x82\xcc"
    "\x82\xcd\x82\xd0\x82\xd3\x82\xd6\x82\xd9"
    "\x82\xdc\x82\xdd\x82\xde\x82\xdf\x82\xe0";

// Test 10: 10 ASCII chars via bputs_fast
static unsigned int bench_bputs_fast_ascii_10(void)
{
    jtxt_blocate(0, 24);
    timer_start();
    jtxt_bputs_fast("ABCDEFGHIJ");
    return timer_stop();
}

// Test 11: 10 Kanji chars via bputs_fast
static unsigned int bench_bputs_fast_kanji_10(void)
{
    jtxt_blocate(0, 24);
    timer_start();
    jtxt_bputs_fast("\x82\xa0\x82\xa2\x82\xa4\x82\xa6\x82\xa8"
                    "\x82\xa9\x82\xab\x82\xad\x82\xaf\x82\xb1");
    return timer_stop();
}

// Test 12: Full screen ASCII via bputs_fast (1000 chars)
static unsigned long bench_fullscreen_ascii_fast(void)
{
    unsigned long total = 0;
    unsigned char y;

    for (y = 0; y < 25; y++) {
        timer_start();
        jtxt_blocate(0, y);
        jtxt_bputs_fast(ascii_line_40);
        total += timer_stop();
    }
    return total;
}

// Test 13: Full screen Kanji via bputs_fast (1000 chars)
static unsigned long bench_fullscreen_kanji_fast(void)
{
    unsigned long total = 0;
    unsigned char y;

    for (y = 0; y < 25; y++) {
        timer_start();
        jtxt_blocate(0, y);
        jtxt_bputs_fast(kanji_line_40);
        total += timer_stop();
    }
    return total;
}

//=============================================================================
// Main
//=============================================================================

int main(void)
{
    unsigned int r1, r2, r3, r4, r5, r6, r7;
    unsigned long r8, r9;
    unsigned int r10, r11;
    unsigned long r12, r13;

    // Hardware initialization (EasyFlash, no KERNAL)
    mmap_set(MMAP_ROM);
    cia_init();
    vic_setmode(VICM_TEXT, (char *)0x0400, (char *)0x1800);

    // Initialize jtxt bitmap mode
    jtxt_init(JTXT_BITMAP_MODE);
    jtxt_bcolor(COLOR_WHITE, COLOR_BLACK);

    // Title screen
    jtxt_blocate(0, 0);
    jtxt_bputs("C64JP BITMAP BENCHMARK V1");
    jtxt_blocate(0, 1);
    jtxt_bputs("========================");
    jtxt_blocate(0, 3);
    jtxt_bputs("MEASURES RENDER CYCLES");
    jtxt_blocate(0, 4);
    jtxt_bputs("USING CIA2 TIMER A");
    jtxt_blocate(0, 6);
    jtxt_bputs("PRESS SPACE TO START");

    wait_space();

    //=========================================================================
    // Run all individual benchmarks (results stored in variables)
    //=========================================================================

    jtxt_bcls();
    jtxt_bcolor(COLOR_WHITE, COLOR_BLACK);
    jtxt_blocate(0, 0);
    jtxt_bputs("RUNNING BENCHMARKS...");

    POKE(0xD020, COLOR_RED);    // Visual: red border during bench

    r1 = bench_draw_ascii_1();
    r2 = bench_draw_kanji_1();
    r3 = bench_bputs_ascii_10();
    r4 = bench_bputs_kanji_10();
    r5 = bench_line_ascii_40();
    r6 = bench_line_kanji_40();

    // Fill screen with data for scroll test
    {
        unsigned char y;
        for (y = 2; y < 25; y++) {
            jtxt_blocate(0, y);
            jtxt_bputs("SCROLL TEST DATA 0123456789");
        }
    }
    r7 = bench_scroll_up();

    // Phase 3: bputs_fast tests
    r10 = bench_bputs_fast_ascii_10();
    r11 = bench_bputs_fast_kanji_10();

    POKE(0xD020, COLOR_BLACK);

    //=========================================================================
    // Display Page 1: Individual test results
    //=========================================================================

    jtxt_bcls();
    jtxt_bcolor(COLOR_WHITE, COLOR_BLACK);

    jtxt_blocate(0, 0);
    jtxt_bputs("=== INDIVIDUAL TESTS (CYC) ===");

    jtxt_blocate(0, 2);
    jtxt_bputs("1 DRAW ASCII x1 :");
    jtxt_blocate(19, 2);
    put_uint16(r1);

    jtxt_blocate(0, 3);
    jtxt_bputs("2 DRAW KANJI x1 :");
    jtxt_blocate(19, 3);
    put_uint16(r2);

    jtxt_blocate(0, 4);
    jtxt_bputs("3 BPUTS ASC x10 :");
    jtxt_blocate(19, 4);
    put_uint16(r3);

    jtxt_blocate(0, 5);
    jtxt_bputs("4 BPUTS KNJ x10 :");
    jtxt_blocate(19, 5);
    put_uint16(r4);

    jtxt_blocate(0, 6);
    jtxt_bputs("5 LINE ASC  x40 :");
    jtxt_blocate(19, 6);
    put_uint16(r5);

    jtxt_blocate(0, 7);
    jtxt_bputs("6 LINE KNJ  x40 :");
    jtxt_blocate(19, 7);
    put_uint16(r6);

    jtxt_blocate(0, 8);
    jtxt_bputs("7 SCROLL UP     :");
    jtxt_blocate(19, 8);
    put_uint16(r7);

    // Per-character averages
    jtxt_blocate(0, 10);
    jtxt_bputs("--- PER CHAR AVG (CYC) ---");

    jtxt_blocate(0, 11);
    jtxt_bputs("DRAW ASC/CHAR  :");
    jtxt_blocate(19, 11);
    put_uint16(r1);

    jtxt_blocate(0, 12);
    jtxt_bputs("DRAW KNJ/CHAR  :");
    jtxt_blocate(19, 12);
    put_uint16(r2);

    jtxt_blocate(0, 13);
    jtxt_bputs("BPUTS ASC/CHAR :");
    jtxt_blocate(19, 13);
    put_uint16(r3 / 10);

    jtxt_blocate(0, 14);
    jtxt_bputs("BPUTS KNJ/CHAR :");
    jtxt_blocate(19, 14);
    put_uint16(r4 / 10);

    jtxt_blocate(0, 15);
    jtxt_bputs("LINE ASC/CHAR  :");
    jtxt_blocate(19, 15);
    put_uint16(r5 / 40);

    jtxt_blocate(0, 16);
    jtxt_bputs("LINE KNJ/CHAR  :");
    jtxt_blocate(19, 16);
    put_uint16(r6 / 40);

    // Overhead analysis
    jtxt_blocate(0, 18);
    jtxt_bputs("SJIS OVERHEAD:");
    jtxt_blocate(0, 19);
    jtxt_bputs(" BPUTS-DRAW ASC:");
    jtxt_blocate(19, 19);
    put_uint16(r3 / 10 - r1);

    jtxt_blocate(0, 20);
    jtxt_bputs(" BPUTS-DRAW KNJ:");
    jtxt_blocate(19, 20);
    put_uint16(r4 / 10 - r2);

    jtxt_blocate(0, 21);
    jtxt_bputs("KANJI-ASCII DIFF:");
    jtxt_blocate(19, 21);
    put_uint16(r2 - r1);

    jtxt_blocate(0, 23);
    jtxt_bputs("PRESS SPACE FOR PAGE 2");

    wait_space();

    //=========================================================================
    // Page 2: Full screen fill benchmarks
    //=========================================================================

    jtxt_bcls();
    jtxt_bcolor(COLOR_WHITE, COLOR_BLACK);
    jtxt_blocate(0, 0);
    jtxt_bputs("=== FULL SCREEN (1000CH) ===");
    jtxt_blocate(0, 2);
    jtxt_bputs("RUNNING ASCII FILL...");

    POKE(0xD020, COLOR_RED);
    r8 = bench_fullscreen_ascii();
    POKE(0xD020, COLOR_BLACK);

    jtxt_bcls();
    jtxt_bcolor(COLOR_WHITE, COLOR_BLACK);
    jtxt_blocate(0, 0);
    jtxt_bputs("=== FULL SCREEN (1000CH) ===");
    jtxt_blocate(0, 2);
    jtxt_bputs("RUNNING KANJI FILL...");

    POKE(0xD020, COLOR_GREEN);
    r9 = bench_fullscreen_kanji();
    POKE(0xD020, COLOR_BLACK);

    //=========================================================================
    // Display Page 2 results
    //=========================================================================

    jtxt_bcls();
    jtxt_bcolor(COLOR_WHITE, COLOR_BLACK);

    jtxt_blocate(0, 0);
    jtxt_bputs("=== FULL SCREEN RESULTS ===");

    jtxt_blocate(0, 2);
    jtxt_bputs("ASCII 1000CH:");
    jtxt_blocate(14, 2);
    put_uint32(r8);
    jtxt_bputs(" CYC");

    jtxt_blocate(0, 3);
    jtxt_bputs("KANJI 1000CH:");
    jtxt_blocate(14, 3);
    put_uint32(r9);
    jtxt_bputs(" CYC");

    // Per-character average from full screen
    jtxt_blocate(0, 5);
    jtxt_bputs("--- PER CHAR FROM FULL ---");

    jtxt_blocate(0, 6);
    jtxt_bputs("ASCII/CHAR:");
    jtxt_blocate(14, 6);
    put_uint16((unsigned int)(r8 / 1000));
    jtxt_bputs(" CYC");

    jtxt_blocate(0, 7);
    jtxt_bputs("KANJI/CHAR:");
    jtxt_blocate(14, 7);
    put_uint16((unsigned int)(r9 / 1000));
    jtxt_bputs(" CYC");

    // Frame equivalents (PAL: 19656 cycles/frame @ 50Hz)
    jtxt_blocate(0, 9);
    jtxt_bputs("--- FRAME EQUIVALENTS ---");
    jtxt_blocate(0, 10);
    jtxt_bputs("(PAL=19656 CYC/FRAME)");

    jtxt_blocate(0, 12);
    jtxt_bputs("ASCII FILL:");
    jtxt_blocate(14, 12);
    {
        unsigned int frames = (unsigned int)(r8 / 19656);
        unsigned int frac = (unsigned int)((r8 % 19656) * 10 / 19656);
        put_uint16(frames);
        jtxt_bputs(".");
        jtxt_bputc('0' + (unsigned char)frac);
        jtxt_bputs(" FRM");
    }

    jtxt_blocate(0, 13);
    jtxt_bputs("KANJI FILL:");
    jtxt_blocate(14, 13);
    {
        unsigned int frames = (unsigned int)(r9 / 19656);
        unsigned int frac = (unsigned int)((r9 % 19656) * 10 / 19656);
        put_uint16(frames);
        jtxt_bputs(".");
        jtxt_bputc('0' + (unsigned char)frac);
        jtxt_bputs(" FRM");
    }

    // Summary
    jtxt_blocate(0, 15);
    jtxt_bputs("--- SUMMARY ---");

    jtxt_blocate(0, 16);
    jtxt_bputs("SINGLE DRAW ASCII :");
    jtxt_blocate(22, 16);
    put_uint16(r1);

    jtxt_blocate(0, 17);
    jtxt_bputs("SINGLE DRAW KANJI :");
    jtxt_blocate(22, 17);
    put_uint16(r2);

    jtxt_blocate(0, 18);
    jtxt_bputs("KANJI OVERHEAD    :");
    jtxt_blocate(22, 18);
    put_uint16(r2 - r1);

    jtxt_blocate(0, 20);
    jtxt_bputs("SCROLL UP         :");
    jtxt_blocate(22, 20);
    put_uint16(r7);

    jtxt_blocate(0, 22);
    jtxt_bputs("PRESS SPACE FOR PAGE 3");

    wait_space();

    //=========================================================================
    // Page 3: bputs_fast comparison (Phase 3)
    //=========================================================================

    // Run full screen bputs_fast tests
    jtxt_bcls();
    jtxt_bcolor(COLOR_WHITE, COLOR_BLACK);
    jtxt_blocate(0, 0);
    jtxt_bputs("RUNNING FAST ASCII FILL...");

    POKE(0xD020, COLOR_RED);
    r12 = bench_fullscreen_ascii_fast();
    POKE(0xD020, COLOR_BLACK);

    jtxt_bcls();
    jtxt_bcolor(COLOR_WHITE, COLOR_BLACK);
    jtxt_blocate(0, 0);
    jtxt_bputs("RUNNING FAST KANJI FILL...");

    POKE(0xD020, COLOR_GREEN);
    r13 = bench_fullscreen_kanji_fast();
    POKE(0xD020, COLOR_BLACK);

    // Display Page 3
    jtxt_bcls();
    jtxt_bcolor(COLOR_WHITE, COLOR_BLACK);

    jtxt_blocate(0, 0);
    jtxt_bputs("=== BPUTS_FAST (PHASE 3) ===");

    // bputs vs bputs_fast x10 comparison
    jtxt_blocate(0, 2);
    jtxt_bputs("--- BPUTS x10 COMPARE ---");

    jtxt_blocate(0, 3);
    jtxt_bputs("         TOTAL  /CH");

    jtxt_blocate(0, 4);
    jtxt_bputs("BPUTS ASC");
    jtxt_blocate(10, 4);
    put_uint16(r3);
    jtxt_blocate(16, 4);
    put_uint16(r3 / 10);

    jtxt_blocate(0, 5);
    jtxt_bputs("FAST  ASC");
    jtxt_blocate(10, 5);
    put_uint16(r10);
    jtxt_blocate(16, 5);
    put_uint16(r10 / 10);

    jtxt_blocate(0, 6);
    jtxt_bputs("BPUTS KNJ");
    jtxt_blocate(10, 6);
    put_uint16(r4);
    jtxt_blocate(16, 6);
    put_uint16(r4 / 10);

    jtxt_blocate(0, 7);
    jtxt_bputs("FAST  KNJ");
    jtxt_blocate(10, 7);
    put_uint16(r11);
    jtxt_blocate(16, 7);
    put_uint16(r11 / 10);

    // Full screen comparison
    jtxt_blocate(0, 9);
    jtxt_bputs("--- FULL SCREEN 1000CH ---");

    jtxt_blocate(0, 10);
    jtxt_bputs("          TOTAL   /CH");

    jtxt_blocate(0, 11);
    jtxt_bputs("DRAW ASC");
    jtxt_blocate(9, 11);
    put_uint32(r8);
    jtxt_blocate(17, 11);
    put_uint16((unsigned int)(r8 / 1000));

    jtxt_blocate(0, 12);
    jtxt_bputs("FAST ASC");
    jtxt_blocate(9, 12);
    put_uint32(r12);
    jtxt_blocate(17, 12);
    put_uint16((unsigned int)(r12 / 1000));

    jtxt_blocate(0, 13);
    jtxt_bputs("DRAW KNJ");
    jtxt_blocate(9, 13);
    put_uint32(r9);
    jtxt_blocate(17, 13);
    put_uint16((unsigned int)(r9 / 1000));

    jtxt_blocate(0, 14);
    jtxt_bputs("FAST KNJ");
    jtxt_blocate(9, 14);
    put_uint32(r13);
    jtxt_blocate(17, 14);
    put_uint16((unsigned int)(r13 / 1000));

    // Frame equivalents
    jtxt_blocate(0, 16);
    jtxt_bputs("--- FRAMES (PAL) ---");

    jtxt_blocate(0, 17);
    jtxt_bputs("          DRAW   FAST");

    jtxt_blocate(0, 18);
    jtxt_bputs("ASCII:");
    {
        unsigned int f1 = (unsigned int)(r8 / 19656);
        unsigned int d1 = (unsigned int)((r8 % 19656) * 10 / 19656);
        unsigned int f2 = (unsigned int)(r12 / 19656);
        unsigned int d2 = (unsigned int)((r12 % 19656) * 10 / 19656);
        jtxt_blocate(10, 18);
        put_uint16(f1);
        jtxt_bputs(".");
        jtxt_bputc('0' + (unsigned char)d1);
        jtxt_blocate(17, 18);
        put_uint16(f2);
        jtxt_bputs(".");
        jtxt_bputc('0' + (unsigned char)d2);
    }

    jtxt_blocate(0, 19);
    jtxt_bputs("KANJI:");
    {
        unsigned int f1 = (unsigned int)(r9 / 19656);
        unsigned int d1 = (unsigned int)((r9 % 19656) * 10 / 19656);
        unsigned int f2 = (unsigned int)(r13 / 19656);
        unsigned int d2 = (unsigned int)((r13 % 19656) * 10 / 19656);
        jtxt_blocate(10, 19);
        put_uint16(f1);
        jtxt_bputs(".");
        jtxt_bputc('0' + (unsigned char)d1);
        jtxt_blocate(17, 19);
        put_uint16(f2);
        jtxt_bputs(".");
        jtxt_bputc('0' + (unsigned char)d2);
    }

    // Savings summary
    jtxt_blocate(0, 21);
    jtxt_bputs("SAVED ASC/CH:");
    jtxt_blocate(14, 21);
    put_uint16(r3 / 10 - r10 / 10);

    jtxt_blocate(0, 22);
    jtxt_bputs("SAVED KNJ/CH:");
    jtxt_blocate(14, 22);
    put_uint16(r4 / 10 - r11 / 10);

    jtxt_blocate(0, 24);
    jtxt_bputs("BENCHMARK COMPLETE");

    // Halt
    while (1) {}

    return 0;
}

//=============================================================================
// Bank 1: JIS X 0201 Half-width Font (2KB) + Misaki Gothic Part 1 (14KB)
//=============================================================================

#pragma section( data1, 0 )
#pragma region(font1, 0x8000, 0xc000, , 1, { data1 })
#pragma data( data1 )

__export const unsigned char font_jisx0201[] = {
    #embed "../../fontconv/font_jisx0201.bin"
};

__export const unsigned char font_gothic_0[] = {
    #embed 14336 0 "../../fontconv/font_misaki_gothic.bin"
};

#pragma data( data )

//=============================================================================
// Bank 2: Misaki Gothic Part 2 (16KB)
//=============================================================================

#pragma section( data2, 0 )
#pragma region(font2, 0x8000, 0xc000, , 2, { data2 })
#pragma data( data2 )

__export const unsigned char font_gothic_1[] = {
    #embed 16384 14336 "../../fontconv/font_misaki_gothic.bin"
};

#pragma data( data )

//=============================================================================
// Bank 3: Misaki Gothic Part 3 (16KB)
//=============================================================================

#pragma section( data3, 0 )
#pragma region(font3, 0x8000, 0xc000, , 3, { data3 })
#pragma data( data3 )

__export const unsigned char font_gothic_2[] = {
    #embed 16384 30720 "../../fontconv/font_misaki_gothic.bin"
};

#pragma data( data )

//=============================================================================
// Bank 4: Misaki Gothic Part 4 (16KB)
//=============================================================================

#pragma section( data4, 0 )
#pragma region(font4, 0x8000, 0xc000, , 4, { data4 })
#pragma data( data4 )

__export const unsigned char font_gothic_3[] = {
    #embed 16384 47104 "../../fontconv/font_misaki_gothic.bin"
};

#pragma data( data )

//=============================================================================
// Bank 5: Misaki Gothic Part 5 (remaining ~6KB)
//=============================================================================

#pragma section( data5, 0 )
#pragma region(font5, 0x8000, 0xc000, , 5, { data5 })
#pragma data( data5 )

__export const unsigned char font_gothic_4[] = {
    #embed 6528 63488 "../../fontconv/font_misaki_gothic.bin"
};

#pragma data( data )
