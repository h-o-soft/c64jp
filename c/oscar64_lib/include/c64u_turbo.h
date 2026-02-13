/*
 * Ultimate 64 Turbo Mode Control
 *
 * Two turbo control modes (configured in U64 menu):
 *
 * 1. "Turbo Enable Bit" mode:
 *    $D030 bit 0 toggles between 1 MHz and the speed set in U64 menu.
 *    Use c64u_turbo_enable() / c64u_turbo_disable().
 *
 * 2. "U64 Turbo Registers" mode:
 *    $D031 bits 0-3 select CPU speed (0=1MHz .. 15=48MHz).
 *    $D031 bit 7 controls badline timing (1=disabled for more CPU cycles).
 *    $D030 bit 0 must also be set to activate.
 *    Use c64u_turbo_set(speed) / c64u_turbo_disable().
 *
 * Reference: https://1541u-documentation.readthedocs.io/en/latest/config/turbo_mode.html
 */

#ifndef C64U_TURBO_H
#define C64U_TURBO_H

#define C64U_TURBO_ENABLE_REG   0xD030  // Bit 0: turbo enable
#define C64U_TURBO_CONTROL_REG  0xD031  // Bits 0-3: speed, Bit 7: badline disable

// Speed constants for c64u_turbo_set()
#define C64U_SPEED_1MHZ    0
#define C64U_SPEED_2MHZ    1
#define C64U_SPEED_3MHZ    2
#define C64U_SPEED_4MHZ    3
#define C64U_SPEED_5MHZ    4
#define C64U_SPEED_8MHZ    7
#define C64U_SPEED_10MHZ   9
#define C64U_SPEED_16MHZ  12
#define C64U_SPEED_20MHZ  13
#define C64U_SPEED_48MHZ  15
#define C64U_SPEED_MAX    15

// Enable turbo mode (for "Turbo Enable Bit" mode)
// Switches to speed configured in U64 menu
inline void c64u_turbo_enable(void)
{
	*(volatile unsigned char *)C64U_TURBO_ENABLE_REG |= 0x01;
}

// Set turbo speed (for "U64 Turbo Registers" mode)
// speed: 0-15 (use C64U_SPEED_* constants)
inline void c64u_turbo_set(unsigned char speed)
{
	*(volatile unsigned char *)C64U_TURBO_CONTROL_REG = speed & 0x0F;
	*(volatile unsigned char *)C64U_TURBO_ENABLE_REG |= 0x01;
}

// Disable turbo mode (return to 1 MHz)
inline void c64u_turbo_disable(void)
{
	*(volatile unsigned char *)C64U_TURBO_ENABLE_REG &= ~0x01;
	*(volatile unsigned char *)C64U_TURBO_CONTROL_REG = 0;
}

#endif // C64U_TURBO_H
