#ifndef C64_OSCAR_H
#define C64_OSCAR_H

#include <stdbool.h>
#include <stdint.h>

// Memory access macros
#define POKE(addr, val) (*(volatile uint8_t *)(addr) = (val))
#define PEEK(addr) (*(volatile uint8_t *)(addr))
#define POKEW(addr, val) (*(volatile uint16_t *)(addr) = (val))
#define PEEKW(addr) (*(volatile uint16_t *)(addr))

#define SEI() __asm { sei }
#define CLI() __asm { cli }

// VIC-II Registers
#define VIC_SPRITE0_X 0xD000
#define VIC_SPRITE0_Y 0xD001
#define VIC_SPRITE_MSB 0xD010
#define VIC_CTRL1 0xD011
#define VIC_RASTER 0xD012
#define VIC_SPRITE_EN 0xD015
#define VIC_CTRL2 0xD016
#define VIC_SPRITE_Y_EXP 0xD017
#define VIC_MEMORY 0xD018
#define VIC_IRQ_STATUS 0xD019
#define VIC_IRQ_EN 0xD01A
#define VIC_SPRITE_PRI 0xD01B
#define VIC_SPRITE_MC 0xD01C
#define VIC_SPRITE_X_EXP 0xD01D
#define VIC_SPRITE_COLL 0xD01E
#define VIC_SPRITE_BG_COLL 0xD01F
#define VIC_BORDER_COLOR 0xD020
#define VIC_BG_COLOR0 0xD021
#define VIC_BG_COLOR1 0xD022
#define VIC_BG_COLOR2 0xD023
#define VIC_BG_COLOR3 0xD024
#define VIC_SPRITE_COLOR0 0xD027

// SID Registers
#define SID_VOICE1_FREQ_LO 0xD400
#define SID_VOICE1_FREQ_HI 0xD401
#define SID_VOICE1_PW_LO 0xD402
#define SID_VOICE1_PW_HI 0xD403
#define SID_VOICE1_CTRL 0xD404
#define SID_VOICE1_AD 0xD405
#define SID_VOICE1_SR 0xD406

// CIA Registers
#define CIA1_PRA 0xDC00
#define CIA1_PRB 0xDC01
#define CIA1_DDRA 0xDC02
#define CIA1_DDRB 0xDC03
#define CIA1_TA_LO 0xDC04
#define CIA1_TA_HI 0xDC05
#define CIA1_TB_LO 0xDC06
#define CIA1_TB_HI 0xDC07
#define CIA1_TOD_10THS 0xDC08
#define CIA1_TOD_SEC 0xDC09
#define CIA1_TOD_MIN 0xDC0A
#define CIA1_TOD_HR 0xDC0B
#define CIA1_SDR 0xDC0C
#define CIA1_ICR 0xDC0D
#define CIA1_CRA 0xDC0E
#define CIA1_CRB 0xDC0F

#define CIA2_PRA 0xDD00
#define CIA2_PRB 0xDD01
#define CIA2_DDRA 0xDD02
#define CIA2_DDRB 0xDD03

// Colors
#define COLOR_BLACK 0
#define COLOR_WHITE 1
#define COLOR_RED 2
#define COLOR_CYAN 3
#define COLOR_PURPLE 4
#define COLOR_GREEN 5
#define COLOR_BLUE 6
#define COLOR_YELLOW 7
#define COLOR_ORANGE 8
#define COLOR_BROWN 9
#define COLOR_LIGHTRED 10
#define COLOR_DARKGREY 11
#define COLOR_GREY 12
#define COLOR_LIGHTGREEN 13
#define COLOR_LIGHTBLUE 14
#define COLOR_LIGHTGREY 15

#endif // C64_OSCAR_H
