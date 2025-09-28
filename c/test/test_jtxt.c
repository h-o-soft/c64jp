#include <c64.h>
#include <string.h>
#include "jtxt.h"

#define PEEK(addr) (*(volatile unsigned char*)(addr))
#define POKE(addr, val) (*(volatile unsigned char*)(addr) = (val))

// Test bitmap mode
static void test_bitmap_mode(void) {
    jtxt_set_mode(JTXT_BITMAP_MODE);
    jtxt_bcls();

    // Set colors
    jtxt_bcolor(1, 0);  // White on black

    // Test ASCII text
    jtxt_blocate(5, 2);
    jtxt_bputs("Bitmap Mode Test");

    // Test Japanese text
    jtxt_blocate(5, 4);
    // const char japanese[] = {
    //     0x93, 0xFA, 0x96, 0x7B, 0x8C, 0xEA, // “ú–{Œê
    //     0x00
    // };
    const char japanese[] = "‚±‚ñ‚É‚¿‚ÍŠ¿ŽšãOŸ~åKåN—Ú—ž";
    jtxt_bputs(japanese);

    // Test window functionality
    jtxt_bwindow(10, 20);
    jtxt_bwindow_enable();

    jtxt_blocate(0, 10);
    jtxt_bputs("Window test line 1");
    jtxt_bnewline();
    jtxt_bputs("Window test line 2");

    // Test hex and decimal output
    jtxt_blocate(0, 15);
    jtxt_bputs("Hex: ");
    jtxt_bput_hex2(0xAB);
    jtxt_bputs(" Dec: ");
    jtxt_bput_dec3(255);

    // Wait for key
    while (PEEK(0xC6) == 0);
    POKE(0xC6, 0);  // Clear key buffer
}

// Test text mode
static void test_text_mode(void) {
    jtxt_set_mode(JTXT_TEXT_MODE);
    jtxt_cls();

    // Test colors
    jtxt_set_bgcolor(6, 14);  // Blue background, light blue border

    // Test different colors
    jtxt_set_color(1);
    jtxt_locate(5, 2);
    jtxt_puts("Text Mode Test");

    // Test kanji
    jtxt_set_color(7);
    jtxt_locate(5, 4);
    const char kanji[] = {
        0x8A, 0xBF, 0x8E, 0x9A, // Š¿Žš
        0x00
    };
    jtxt_puts(kanji);

    // Test mixed text
    jtxt_set_color(3);
    jtxt_locate(5, 6);
    jtxt_puts("Mix: ");
    const char mixed[] = {
        'A', 'B', 'C', ' ',
        0xB1, 0xB2, 0xB3, ' ',  // Half-width katakana ƒA ƒC ƒE
        0x82, 0x50, 0x82, 0x51, // Full-width ‚P‚Q
        0x00
    };
    jtxt_puts(mixed);

    // Test newline
    jtxt_set_color(5);
    jtxt_locate(5, 8);
    jtxt_puts("Line 1");
    jtxt_newline();
    jtxt_puts("     Line 2 after newline");

    // Test character range setting
    jtxt_set_range(64, 32);
    jtxt_locate(5, 12);
    jtxt_puts("Custom range test");

    // Wait for key
    while (PEEK(0xC6) == 0);
    POKE(0xC6, 0);
}

// Test Shift-JIS state handling
static void test_sjis_state(void) {
    jtxt_cls();
    jtxt_locate(5, 2);
    jtxt_puts("Shift-JIS State Test");

    // Test incomplete Shift-JIS sequence
    jtxt_locate(5, 4);
    jtxt_putc(0x82);  // First byte of Shift-JIS
    jtxt_putc(0x20);  // Invalid second byte (should reset)
    jtxt_puts("ABC");  // Should display normally

    // Test valid Shift-JIS sequence
    jtxt_locate(5, 6);
    jtxt_putc(0x82);  // First byte
    jtxt_putc(0xA0);  // Valid second byte (‚ )
    jtxt_puts(" OK");

    // Test is_firstsjis function
    jtxt_locate(5, 8);
    if (jtxt_is_firstsjis(0x82)) {
        jtxt_puts("0x82 is first byte: OK");
    }

    jtxt_locate(5, 10);
    if (!jtxt_is_firstsjis(0x41)) {
        jtxt_puts("0x41 is not first byte: OK");
    }

    // Wait for key
    while (PEEK(0xC6) == 0);
    POKE(0xC6, 0);
}

int main(void) {
    // Initialize library
    jtxt_init(JTXT_TEXT_MODE);

    // Run tests
    test_text_mode();
    test_bitmap_mode();
    test_sjis_state();

    // Final cleanup
    jtxt_cleanup();

    // Show completion message
    jtxt_cls();
    jtxt_locate(10, 10);
    jtxt_puts("All tests completed!");
    jtxt_locate(10, 12);
    jtxt_puts("Press any key to exit");

    while (PEEK(0xC6) == 0);

    return 0;
}