/*
 * Runtime Library Placement Test
 *
 * This program tests where Oscar64 places runtime library functions
 * (like mul16by8) when using multiple ROM banks.
 */

#include <c64/memmap.h>
#include <c64/vic.h>

// Memory access macros
#define POKE(addr, val) (*(volatile unsigned char *)(addr) = (val))
#define PEEK(addr) (*(volatile unsigned char *)(addr))

// MagicDesk bank register
#define BANK_REG 0xDE00

// Color definitions
#define COLOR_BLACK   0
#define COLOR_WHITE   1
#define COLOR_RED     2
#define COLOR_GREEN   5

//=============================================================================
// Bank 0: Startup Code
//=============================================================================

#pragma region(rom, 0x8080, 0x9000, , 0, { code, data } )
#pragma region(main, 0xc000, 0xd000, , , { bss, stack, heap })
#pragma stacksize( 256 )

// Test function in Bank 0 that uses multiplication
unsigned int test_mul_bank0(unsigned char a, unsigned char b) {
    return (unsigned int)a * b;
}

int main(void)
{
    // Enable ROM
    mmap_set(MMAP_ROM);

    // Clear screen
    for (unsigned int i = 0; i < 1000; i++) {
        POKE(0x0400 + i, ' ');
        POKE(0xD800 + i, COLOR_WHITE);
    }

    // Set border and background
    POKE(0xD020, COLOR_BLACK);
    POKE(0xD021, COLOR_BLACK);

    // Test multiplication in Bank 0
    unsigned int result0 = test_mul_bank0(10, 20);

    // Display result as background color
    POKE(0xD021, result0 & 0x0F);  // Should be 200 & 0x0F = 8 (orange)

    // Display some marker on screen
    POKE(0x0400, 1);  // 'A'
    POKE(0x0400 + 1, result0 & 0xFF);

    // Infinite loop
    while (1) {
    }

    return 0;
}

//=============================================================================
// Bank 1: Test code with multiplication
//=============================================================================

#pragma section( code1, 0 )
#pragma region(bank1, 0x8000, 0x9000, , 1, { code1 })
#pragma code( code1 )

// Test function in Bank 1 that uses multiplication
__export unsigned int test_mul_bank1(unsigned char a, unsigned char b) {
    return (unsigned int)a * b;
}

#pragma code( code )

//=============================================================================
// Bank 2: Another test with multiplication
//=============================================================================

#pragma section( code2, 0 )
#pragma region(bank2, 0x8000, 0x9000, , 2, { code2 })
#pragma code( code2 )

// Test function in Bank 2 that uses multiplication
__export unsigned int test_mul_bank2(unsigned char a, unsigned char b) {
    return (unsigned int)a * b;
}

#pragma code( code )
