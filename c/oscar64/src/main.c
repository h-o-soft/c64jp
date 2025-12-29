#include "c64_oscar.h"
#include "jtxt.h"
#include <string.h>

#pragma region(main, 0x900, 0x8000, , , { code, data, bss, stack, heap })

int main(void) {
  // Initialize jtxt library in text mode
  // jtxt_init(JTXT_TEXT_MODE);

  // Temporary debug: just print something using standard IO to see if it runs
  // Note: standard printf might not work if we mess with memory too early
  // But let's try to see if we get here.
  (*(volatile unsigned char *)0x0400) = 'A'; // Put 'A' on screen directly
  (*(volatile unsigned char *)0xD800) = 1;   // White color

  jtxt_init(JTXT_TEXT_MODE);

  // Clear screen
  jtxt_cls();

  // Set colors
  jtxt_set_bgcolor(COLOR_BLACK, COLOR_BLACK);
  jtxt_set_color(COLOR_WHITE);

  // Display messages
  jtxt_locate(10, 5);
  jtxt_puts("Hello from Oscar64!");

  jtxt_set_bgcolor(COLOR_BLUE, COLOR_BLACK);

  jtxt_locate(5, 8);
  // Test with Japanese text (Shift-JIS encoded)
  // こんにちは
  const char japanese_text[] = {0x82, 0xB1, 0x82, 0xF1, 0x82, 0xC9,
                                0x82, 0xBF, 0x82, 0xCD, 0x21, 0x00};
  jtxt_puts(japanese_text);

  jtxt_locate(5, 10);
  jtxt_puts("C64 Japanese Text Library");

  // Wait for key press (simple loop)
  jtxt_locate(5, 20);
  jtxt_puts("Looping forever...");

  while (1) {
    // Infinite loop
  }

  return 0;
}
