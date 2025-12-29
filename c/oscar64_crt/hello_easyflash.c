/*
 * C64 Japanese Kanji ROM Cartridge
 * Oscar64 EasyFlash Demo
 *
 * This file embeds font data into an EasyFlash cartridge
 * and demonstrates Japanese text display with automatic RAM expansion.
 */

#include <c64/memmap.h>
#include <c64/cia.h>
#include <c64/vic.h>
#include <c64/easyflash.h>
#include <c64/keyboard.h>
#include <string.h>
#include "jtxt.h"

// Memory access macros
#define POKE(addr, val) (*(volatile unsigned char *)(addr) = (val))
#define PEEK(addr) (*(volatile unsigned char *)(addr))

// EasyFlash bank register
#define BANK_REG 0xDE00

// Color definitions
#define COLOR_BLACK   0
#define COLOR_WHITE   1
#define COLOR_BLUE    6

// RAM region for code copied from ROM
#define EXTRA_CODE_RAM   0xC000
#define EXTRA_CODE_SIZE  0x1000  // 4KB max

//=============================================================================
// Cross-bank utility functions and macros
//=============================================================================

// Generic bank call wrapper - forward declarations
void bankcall_0(unsigned char bank, void (*func)(void));
void bankcall_1(unsigned char bank, void (*func)(unsigned char), unsigned char arg1);
void bankcall_2(unsigned char bank, void (*func)(unsigned char, unsigned char),
                unsigned char arg1, unsigned char arg2);

// Macros for convenient usage
#define BANKCALL(bank, func) bankcall_0(bank, (void(*)(void))(func))
#define BANKCALL1(bank, func, arg1) bankcall_1(bank, (void(*)(unsigned char))(func), arg1)
#define BANKCALL2(bank, func, arg1, arg2) bankcall_2(bank, (void(*)(unsigned char, unsigned char))(func), arg1, arg2)

// Forward declarations for test functions in Bank 6
void test_from_bank6_first(void);
void test_from_bank6_second(void);

//=============================================================================
// Main Region: Automatically copied to RAM ($0900-$8000) at startup
//=============================================================================

#pragma region( main, 0x0900, 0x8000, , , { code, data, bss, heap, stack } )

// Define jtxt_state in main region (RAM)
jtxt_state_t jtxt_state;

//=============================================================================
// Cross-bank copy routine (ccopy)
// Copies data from a ROM bank to RAM
//=============================================================================
void ccopy(unsigned char bank, char* dst, const char* src, unsigned int n)
{
    unsigned char saved_bank = eflash.bank;
    eflash.bank = bank;
    while (n--) {
        *dst++ = *src++;
    }
    eflash.bank = saved_bank;
}

//=============================================================================
// Bank call wrappers - call functions in ROM banks
// These save/restore the bank and call the function
//=============================================================================
void bankcall_0(unsigned char bank, void (*func)(void))
{
    unsigned char saved_bank = eflash.bank;
    eflash.bank = bank;
    func();
    eflash.bank = saved_bank;
}

void bankcall_1(unsigned char bank, void (*func)(unsigned char), unsigned char arg1)
{
    unsigned char saved_bank = eflash.bank;
    eflash.bank = bank;
    func(arg1);
    eflash.bank = saved_bank;
}

void bankcall_2(unsigned char bank, void (*func)(unsigned char, unsigned char),
                unsigned char arg1, unsigned char arg2)
{
    unsigned char saved_bank = eflash.bank;
    eflash.bank = bank;
    func(arg1, arg2);
    eflash.bank = saved_bank;
}

// Initialize screen
void init_screen(void)
{
    // Clear screen
    memset((char*)0x0400, ' ', 1000);

    // Set screen color to white
    memset((char*)0xD800, COLOR_WHITE, 1000);

    // Set border and background
    POKE(0xD020, COLOR_BLUE);   // Border
    POKE(0xD021, COLOR_BLUE);   // Background
}

// Display test message with jtxt
void display_message(void)
{
    // Initialize jtxt library in text mode
    jtxt_init(JTXT_TEXT_MODE);

    // Set character range (start at character 128, use 64 characters)
    jtxt_set_range(128, 64);

    // Test jtxt_putc to display text
    jtxt_set_color(COLOR_WHITE);
    jtxt_locate(0, 10);
    jtxt_puts("HELLO WORLD - EASYFLASH");

    // Test Japanese text (Shift-JIS)
    jtxt_locate(0, 12);
    jtxt_set_color(COLOR_WHITE);

    // "こんにちは" in Shift-JIS
    const char* japanese_text = "\x82\xb1\x82\xf1\x82\xc9\x82\xbf\x82\xcd";
    jtxt_puts(japanese_text);

    // More Japanese text
    jtxt_locate(0, 14);
    jtxt_puts("\x82\xa0\x82\xa2\x82\xa4\x82\xa6\x82\xa8");  // "あいうえお"

    // Test characters from different banks
    jtxt_locate(0, 16);
    jtxt_puts("BANK1:");
    jtxt_puts("\x82\xa0");  // "あ" - Bank 1 front

    jtxt_locate(0, 17);
    jtxt_puts("BANK2:");
    jtxt_puts("\x8a\xbf");  // "漢" - Bank 2 area

    // jtxt_locate(0, 18);
    // jtxt_puts("BANK3:");
    // jtxt_puts("\x93\xfa");  // "日" - Bank 3 area

    // jtxt_locate(0, 19);
    // jtxt_puts("BANK4:");
    // jtxt_puts("\x96\x7b");  // "本" - Bank 4 area

    jtxt_locate(0, 20);
    jtxt_puts("BANK5:");
    jtxt_puts("\x9f\xb6");  // "涕" - Bank 5 rear (high codepoint)
}

// Test bitmap mode display
void test_bitmap_mode(void)
{
    // Wait for SPACE key press
    while (true) {
        keyb_poll();
        if (key_pressed(KSCAN_SPACE)) {
            break;
        }
    }
    // Wait for SPACE key release
    while (true) {
        keyb_poll();
        if (!key_pressed(KSCAN_SPACE)) {
            break;
        }
    }

    // Switch to bitmap mode
    jtxt_set_mode(JTXT_BITMAP_MODE);
    jtxt_bcls();

    // Set colors
    jtxt_bcolor(COLOR_WHITE, COLOR_BLUE);

    // Display ASCII text
    jtxt_blocate(0, 0);
    jtxt_bputs("BITMAP MODE - EASYFLASH");

    jtxt_blocate(0, 2);
    jtxt_bputs("HELLO WORLD");

    // Display Japanese text
    jtxt_blocate(0, 4);
    const char* japanese_text = "\x82\xb1\x82\xf1\x82\xc9\x82\xbf\x82\xcd";
    jtxt_bputs(japanese_text);

    // Display more Japanese
    jtxt_blocate(0, 6);
    jtxt_bputs("\x82\xa0\x82\xa2\x82\xa4\x82\xa6\x82\xa8");  // "あいうえお"

    // Test characters from different banks (bitmap mode)
    jtxt_blocate(0, 8);
    jtxt_bputs("BANK TEST:");

    jtxt_blocate(0, 9);
    jtxt_bputs("\x8a\xbf\x8e\x9a");  // "漢字" - Bank 2

    jtxt_blocate(0, 10);
    jtxt_bputs("\x93\xfa\x96\x7b");  // "日本" - Bank 3-4

    jtxt_blocate(0, 11);
    jtxt_bputs("\x8c\xea");  // "語" - Bank 2-3 area
}

int main(void)
{
    // Enable ROM
    mmap_set(MMAP_ROM);

    // Init CIAs (no kernal rom was executed so far)
    cia_init();

    // Init VIC
    vic_setmode(VICM_TEXT, (char *)0x0400, (char *)0x1800);

    // Initialize screen
    init_screen();

    // Display message
    display_message();

    // Test bitmap mode
    test_bitmap_mode();

    // Test cross-bank code execution
    // 1. Copy code from Bank 6 ROM ($8000) to RAM ($C000)
    ccopy(6, (char*)EXTRA_CODE_RAM, (char*)0x8000, EXTRA_CODE_SIZE);

    // 2. Call functions from Bank 6 (now at $C000)
    // Test that both functions work - first is at start, second is somewhere in the middle
    test_from_bank6_first();
    test_from_bank6_second();

    // Infinite loop
    while (1) {
        // All code runs from RAM, so no bank switching issues
    }

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

//=============================================================================
// Bank 6: Extra code region (can be copied to $C000 and executed)
// The 7th parameter (0xC000) specifies the relocation address
//=============================================================================

#pragma section( code6, 0 )
#pragma region(bank6, 0x8000, 0xc000, , 6, { code6 }, 0xc000)
#pragma code( code6 )

// First test function - at the start of Bank 6
__export void test_from_bank6_first(void)
{
    // This function is at the beginning of the bank
    jtxt_blocate(0, 13);
    jtxt_bputs("FIRST FUNC OK!");

    jtxt_blocate(0, 14);
    jtxt_bputs("\x82\xb1\x82\xf1\x82\xc9\x82\xbf\x82\xcd");  // "こんにちは"

    // Flash border WHITE
    for (unsigned char i = 0; i < 5; i++) {
        POKE(0xD020, COLOR_WHITE);
        for (unsigned int j = 0; j < 3000; j++) {}
        POKE(0xD020, COLOR_BLUE);
        for (unsigned int j = 0; j < 3000; j++) {}
    }
}

// Second test function - somewhere in the middle of Bank 6
// This tests that functions not at the start are also correctly relocated
__export void test_from_bank6_second(void)
{
    // This function is NOT at the beginning - tests relocation of non-first functions
    jtxt_blocate(0, 16);
    jtxt_bputs("SECOND FUNC OK!");

    jtxt_blocate(0, 17);
    jtxt_bputs("\x93\xfa\x96\x7b\x8c\xea");  // "日本語"

    // Flash border different color (RED)
    for (unsigned char i = 0; i < 5; i++) {
        POKE(0xD020, 2);  // Red
        for (unsigned int j = 0; j < 3000; j++) {}
        POKE(0xD020, COLOR_BLUE);
        for (unsigned int j = 0; j < 3000; j++) {}
    }
}

#pragma code( code )
