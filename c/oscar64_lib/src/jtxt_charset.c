#include "c64_oscar.h"
#include "jtxt.h"
#include <string.h>

#define POKE(addr, val) (*(volatile uint8_t *)(addr) = (val))
#define PEEK(addr) (*(volatile uint8_t *)(addr))

void jtxt_copy_charset_to_ram(void) {
  // Copy 2KB of character ROM to RAM
  memcpy((void *)JTXT_CHARSET_RAM, (void *)JTXT_CHARSET_ROM, 2048);
}

uint16_t jtxt_sjis_to_offset(uint16_t sjis_code) {
  uint8_t ch = (sjis_code >> 8) & 0xFF;
  uint8_t ch2 = sjis_code & 0xFF;

  // Shift-JIS to Ku/Ten conversion
  if (ch <= 0x9F) {
    ch = ((uint16_t)ch << 1) - 0x102;
  } else {
    ch = ((uint16_t)ch << 1) - 0x182;
  }

  if (ch2 >= 0x9F) {
    ch++;
  }

  if (ch2 < 0x7F) {
    ch2 -= 0x40;
  } else if (ch2 < 0x9F) {
    ch2 -= 0x41;
  } else {
    ch2 -= 0x9F;
  }

  return ((uint16_t)ch * 94 + ch2) * 8 + JTXT_JISX0208_OFFSET;
}

void jtxt_define_jisx0201(uint8_t jisx0201_code) {
  // Calculate source address in ROM
  uint16_t src_addr = JTXT_ROM_BASE + ((uint16_t)jisx0201_code * 8);

  // Begin ROM access with $01 register backup
  jtxt_rom_access_begin();

  // Switch to bank 1 for JIS X 0201 font
  // POKE(JTXT_BANK_REG, 1);
  *((volatile char *)JTXT_BANK_REG) = 1;

  // Copy 8 bytes of font data
  uint16_t dst_addr = jtxt_state.screen_pos;

  if (jisx0201_code == 0x20) {
    *(volatile uint32_t *)(dst_addr) = 0;
    dst_addr += 4;
    *(volatile uint32_t *)(dst_addr) = 0;
  } else {
    *(volatile uint8_t *)(dst_addr++) = *(volatile uint8_t *)(src_addr++);
    *(volatile uint8_t *)(dst_addr++) = *(volatile uint8_t *)(src_addr++);
    *(volatile uint8_t *)(dst_addr++) = *(volatile uint8_t *)(src_addr++);
    *(volatile uint8_t *)(dst_addr++) = *(volatile uint8_t *)(src_addr++);
    *(volatile uint8_t *)(dst_addr++) = *(volatile uint8_t *)(src_addr++);
    *(volatile uint8_t *)(dst_addr++) = *(volatile uint8_t *)(src_addr++);
    *(volatile uint8_t *)(dst_addr++) = *(volatile uint8_t *)(src_addr++);
    *(volatile uint8_t *)(dst_addr) = *(volatile uint8_t *)(src_addr);
  }

  // Switch back to bank 0
  // POKE(JTXT_BANK_REG, 0);
  *((volatile char *)JTXT_BANK_REG) = 0;

  // End ROM access with $01 register restore
  jtxt_rom_access_end();
}

void jtxt_define_kanji(uint16_t sjis_code) {
  uint16_t kanji_offset = jtxt_sjis_to_offset(sjis_code);
  uint8_t bank = (kanji_offset / 8192) + 1;
  uint16_t in_bank_offset = kanji_offset % 8192;

  // Begin ROM access with $01 register backup
  jtxt_rom_access_begin();

  // Switch to appropriate bank
  // POKE(JTXT_BANK_REG, bank);
  *((volatile char *)JTXT_BANK_REG) = bank;

  uint16_t rom_addr = JTXT_ROM_BASE + in_bank_offset;

  // Copy 8 bytes of font data
  uint16_t dst_addr = jtxt_state.screen_pos;

  *(volatile uint8_t *)(dst_addr++) = *(volatile uint8_t *)(rom_addr++);
  *(volatile uint8_t *)(dst_addr++) = *(volatile uint8_t *)(rom_addr++);
  *(volatile uint8_t *)(dst_addr++) = *(volatile uint8_t *)(rom_addr++);
  *(volatile uint8_t *)(dst_addr++) = *(volatile uint8_t *)(rom_addr++);
  *(volatile uint8_t *)(dst_addr++) = *(volatile uint8_t *)(rom_addr++);
  *(volatile uint8_t *)(dst_addr++) = *(volatile uint8_t *)(rom_addr++);
  *(volatile uint8_t *)(dst_addr++) = *(volatile uint8_t *)(rom_addr++);
  *(volatile uint8_t *)(dst_addr) = *(volatile uint8_t *)(rom_addr);

  // Switch back to bank 0
  // POKE(JTXT_BANK_REG, 0);
  *((volatile char *)JTXT_BANK_REG) = 0;

  // End ROM access with $01 register restore
  jtxt_rom_access_end();
}

void jtxt_define_font(uint16_t dest_addr, uint16_t code) {
  // Save destination address temporarily
  uint16_t saved_pos = jtxt_state.screen_pos;
  jtxt_state.screen_pos = dest_addr;

  if ((code & 0xFF00) == 0) {
    // Single-byte character (ASCII, half-width kana)
    jtxt_define_jisx0201(code & 0xFF);
  } else {
    // Double-byte character (kanji)
    jtxt_define_kanji(code);
  }

  // Restore original position
  jtxt_state.screen_pos = saved_pos;
}

void jtxt_define_char(uint8_t char_code, uint16_t code) {
  jtxt_define_font(JTXT_CHARSET_RAM + (uint16_t)char_code * 8, code);
}