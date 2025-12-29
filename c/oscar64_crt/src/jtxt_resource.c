#include "jtxt.h"
#include <string.h>


#define POKE(addr, val) (*(volatile uint8_t*)(addr) = (val))
#define PEEK(addr) (*(volatile uint8_t*)(addr))

bool jtxt_load_string_resource(uint8_t resource_number) {
    // Begin ROM access with $01 register backup
    jtxt_rom_access_begin();

    // Switch to string resource bank
    POKE(JTXT_BANK_REG, JTXT_STRING_RESOURCE_BANK);

    // Get number of strings (little-endian 4 bytes, but we only need the low word)
    uint16_t num_strings = PEEK(JTXT_STRING_RESOURCE_BASE) |
                          ((uint16_t)PEEK(JTXT_STRING_RESOURCE_BASE + 1) << 8);

    // Range check
    if (resource_number >= (num_strings & 0xFF)) {
        POKE(JTXT_BANK_REG, 0);
        jtxt_rom_access_end();
        return false;
    }

    // Read offset table entry (4 bytes per entry)
    uint16_t offset_table_addr = JTXT_STRING_RESOURCE_BASE + 4 + (uint16_t)resource_number * 4;
    uint8_t target_bank = PEEK(offset_table_addr);
    // uint8_t reserved = PEEK(offset_table_addr + 1); // Not used
    uint16_t string_offset = PEEK(offset_table_addr + 2) |
                            ((uint16_t)PEEK(offset_table_addr + 3) << 8);

    // Check for undefined string
    if (target_bank == 0 && string_offset == 0) {
        POKE(JTXT_BANK_REG, 0);
        jtxt_rom_access_end();
        return false;
    }

    // Switch to bank containing the string
    POKE(JTXT_BANK_REG, target_bank);

    // Copy string to RAM buffer
    uint16_t string_addr = JTXT_ROM_BASE + string_offset;
    uint8_t buffer_pos = 0;
    uint8_t current_bank = target_bank;
    uint16_t current_addr = string_addr;

    while (buffer_pos < JTXT_STRING_BUFFER_SIZE) {
        // Check for 8KB boundary crossing
        if (current_addr > 0x9FFF) {
            current_bank++;
            current_addr = JTXT_ROM_BASE;
            POKE(JTXT_BANK_REG, current_bank);
        }

        uint8_t char_data = PEEK(current_addr);
        POKE(JTXT_STRING_BUFFER + buffer_pos, char_data);

        if (char_data == 0) {
            break; // NULL terminator
        }

        buffer_pos++;
        current_addr++;
    }

    // Ensure NULL termination
    if (buffer_pos >= JTXT_STRING_BUFFER_SIZE) {
        POKE(JTXT_STRING_BUFFER + JTXT_STRING_BUFFER_SIZE, 0);
    }

    // Return to bank 0
    POKE(JTXT_BANK_REG, 0);

    // End ROM access with $01 register restore
    jtxt_rom_access_end();
    return true;
}

void jtxt_putr(uint8_t resource_number) {
    if (jtxt_load_string_resource(resource_number)) {
        jtxt_puts((const char*)JTXT_STRING_BUFFER);
    }
}

void jtxt_bputr(uint8_t resource_number) {
    if (jtxt_load_string_resource(resource_number)) {
        jtxt_bputs((const char*)JTXT_STRING_BUFFER);
    }
}