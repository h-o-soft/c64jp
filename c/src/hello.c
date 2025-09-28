#include <c64.h>
#include "jtxt.h"

#define PEEK(addr) (*(volatile unsigned char*)(addr))

int main(void) {
    // Initialize jtxt library in text mode
    jtxt_init(JTXT_TEXT_MODE);

    // Clear screen
    jtxt_cls();

    // Set colors
    jtxt_set_bgcolor(0, 0);  // Black background and border
    jtxt_set_color(1);       // White text

    // Display messages
    jtxt_locate(10, 5);
    jtxt_puts("Hello from C!");

    jtxt_locate(5, 8);
    // Test with Japanese text (Shift-JIS encoded)
    const char japanese_text[] = {
        0x82, 0xB1, 0x82, 0xF1, 0x82, 0xC9, 0x82, 0xBF, 0x82, 0xCD, // こんにちは
        0x21, 0x00  // !
    };
    jtxt_puts(japanese_text);

    jtxt_locate(5, 10);
    jtxt_puts("C64 Japanese Text Library");

    jtxt_locate(5, 12);
    // Test half-width katakana
    const char katakana_text[] = {
        0xC6, 0xCE, 0xDD, 0xBA, 0xDE, // ニホンゴ
        0x00
    };
    jtxt_puts(katakana_text);

    // Wait for key press
    jtxt_locate(5, 20);
    jtxt_puts("Press any key...");

    // Simple wait loop
    while (PEEK(0xC6) == 0) {
        // Wait for key press
    }

    // Cleanup
    jtxt_cleanup();

    return 0;
}