#include "c64_oscar.h"
#include "jtxt.h"
#include <string.h>

#define POKE(addr, val) (*(volatile uint8_t *)(addr) = (val))
#define PEEK(addr) (*(volatile uint8_t *)(addr))

void jtxt_copy_charset_to_ram(void) {
  // Copy 2KB of character ROM to RAM
  memcpy((void *)JTXT_CHARSET_RAM, (void *)JTXT_CHARSET_ROM, 2048);
}

// Lookup table: ch * 94 for ch = 0..83 (JIS X 0208 row indices)
// Eliminates ~90 cycle shift-based multiply per kanji character
static const uint16_t row_times_94[84] = {
       0,   94,  188,  282,  376,  470,  564,  658,  //  0- 7
     752,  846,  940, 1034, 1128, 1222, 1316, 1410,  //  8-15
    1504, 1598, 1692, 1786, 1880, 1974, 2068, 2162,  // 16-23
    2256, 2350, 2444, 2538, 2632, 2726, 2820, 2914,  // 24-31
    3008, 3102, 3196, 3290, 3384, 3478, 3572, 3666,  // 32-39
    3760, 3854, 3948, 4042, 4136, 4230, 4324, 4418,  // 40-47
    4512, 4606, 4700, 4794, 4888, 4982, 5076, 5170,  // 48-55
    5264, 5358, 5452, 5546, 5640, 5734, 5828, 5922,  // 56-63
    6016, 6110, 6204, 6298, 6392, 6486, 6580, 6674,  // 64-71
    6768, 6862, 6956, 7050, 7144, 7238, 7332, 7426,  // 72-79
    7520, 7614, 7708, 7802                            // 80-83
};

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

  // ch * 94 via lookup table (eliminates ~90 cycle shift multiply)
  uint16_t row = row_times_94[ch];

  // (row + ch2) * 8 = (row + ch2) << 3
  return ((row + (uint16_t)ch2) << 3) + JTXT_JISX0208_OFFSET;
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
  uint8_t bank = (uint8_t)(kanji_offset >> 13) + 1;   // / 8192
  uint16_t in_bank_offset = kanji_offset & 0x1FFF;    // % 8192

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