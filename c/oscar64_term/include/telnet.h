/*
 * Telnet protocol handling for C64 Japanese Terminal
 *
 * Minimal Telnet NVT (Network Virtual Terminal) implementation.
 * Handles IAC negotiation sequences and PETSCII<->ASCII conversion.
 */

#ifndef _TELNET_H_
#define _TELNET_H_

#include "c64u_network.h"

// Telnet NVT command codes
#define NVT_SE   240
#define NVT_NOP  241
#define NVT_BRK  243
#define NVT_GA   249
#define NVT_SB   250
#define NVT_WILL 251
#define NVT_WONT 252
#define NVT_DO   253
#define NVT_DONT 254
#define NVT_IAC  255

// Telnet option codes
#define NVT_OPT_TRANSMIT_BINARY    0
#define NVT_OPT_ECHO               1
#define NVT_OPT_SUPPRESS_GO_AHEAD  3
#define NVT_OPT_TERMINAL_TYPE      24
#define NVT_OPT_NAWS               31
#define NVT_OPT_LINEMODE           34

// Telnet IAC parser states
#define IAC_STATE_NORMAL  0
#define IAC_STATE_IAC     1
#define IAC_STATE_VERB    2
#define IAC_STATE_SB      3

// Return codes from telnet_process_byte
#define TELNET_CHAR     0   // Normal character, output it
#define TELNET_CONSUMED 1   // Byte consumed by IAC processing
#define TELNET_ESCAPED  2   // IAC IAC -> output 0xFF as data

// Telnet state
typedef struct {
	unsigned char iac_state;
	unsigned char iac_verb;
	unsigned char socketid;
} telnet_state_t;

extern telnet_state_t telnet;

// Initialize telnet state
void telnet_init(unsigned char socketid);

// Process one received byte through telnet filter.
// Returns TELNET_CHAR if c should be displayed,
// TELNET_CONSUMED if it was part of IAC sequence,
// TELNET_ESCAPED if IAC IAC was received (display 0xFF).
int telnet_process_byte(unsigned char c);

// Send IAC response (3 bytes: IAC verb opt)
void telnet_send_iac(unsigned char verb, unsigned char opt);

// Convert PETSCII key to ASCII for sending
unsigned char petscii_to_ascii(unsigned char c);

#endif // _TELNET_H_
