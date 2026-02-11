/*
 * Telnet protocol handling for C64 Japanese Terminal
 *
 * Minimal IAC negotiation: accept SUPPRESS_GO_AHEAD, reject everything else.
 * State machine processes bytes one at a time for streaming use.
 */

#include "telnet.h"

#ifdef JTXT_MAGICDESK_CRT
#pragma code(mcode)
#pragma data(mdata)
#endif

telnet_state_t telnet;

void telnet_init(unsigned char socketid)
{
	telnet.iac_state = IAC_STATE_NORMAL;
	telnet.iac_verb = 0;
	telnet.socketid = socketid;
}

void telnet_send_iac(unsigned char verb, unsigned char opt)
{
	c64u_socketwritechar(telnet.socketid, (char)NVT_IAC);
	c64u_socketwritechar(telnet.socketid, (char)verb);
	c64u_socketwritechar(telnet.socketid, (char)opt);
}

// Handle WILL/DO/WONT/DONT negotiation
static void telnet_negotiate(unsigned char verb, unsigned char opt)
{
	switch (verb) {
	case NVT_WILL:
		if (opt == NVT_OPT_SUPPRESS_GO_AHEAD || opt == NVT_OPT_ECHO) {
			telnet_send_iac(NVT_DO, opt);
		} else {
			telnet_send_iac(NVT_DONT, opt);
		}
		break;

	case NVT_DO:
		if (opt == NVT_OPT_SUPPRESS_GO_AHEAD) {
			telnet_send_iac(NVT_WILL, opt);
		} else {
			telnet_send_iac(NVT_WONT, opt);
		}
		break;

	case NVT_WONT:
		telnet_send_iac(NVT_DONT, opt);
		break;

	case NVT_DONT:
		telnet_send_iac(NVT_WONT, opt);
		break;
	}
}

int telnet_process_byte(unsigned char c)
{
	switch (telnet.iac_state) {
	case IAC_STATE_NORMAL:
		if (c == NVT_IAC) {
			telnet.iac_state = IAC_STATE_IAC;
			return TELNET_CONSUMED;
		}
		return TELNET_CHAR;

	case IAC_STATE_IAC:
		if (c == NVT_IAC) {
			// IAC IAC = escaped 0xFF
			telnet.iac_state = IAC_STATE_NORMAL;
			return TELNET_ESCAPED;
		}
		if (c == NVT_SB) {
			// Start of subnegotiation - skip until SE
			telnet.iac_state = IAC_STATE_SB;
			return TELNET_CONSUMED;
		}
		if (c >= NVT_WILL && c <= NVT_DONT) {
			telnet.iac_verb = c;
			telnet.iac_state = IAC_STATE_VERB;
			return TELNET_CONSUMED;
		}
		// Other commands (NOP, BRK, GA etc.) - ignore
		telnet.iac_state = IAC_STATE_NORMAL;
		return TELNET_CONSUMED;

	case IAC_STATE_VERB:
		// c is the option byte
		telnet_negotiate(telnet.iac_verb, c);
		telnet.iac_state = IAC_STATE_NORMAL;
		return TELNET_CONSUMED;

	case IAC_STATE_SB:
		// Inside subnegotiation, wait for IAC SE
		if (c == NVT_IAC) {
			// Next byte should be SE
			telnet.iac_state = IAC_STATE_IAC;
		}
		return TELNET_CONSUMED;
	}

	// Should not reach here
	telnet.iac_state = IAC_STATE_NORMAL;
	return TELNET_CONSUMED;
}

// PETSCII to ASCII conversion for keyboard input
unsigned char petscii_to_ascii(unsigned char c)
{
	// Lowercase PETSCII (a-z: $41-$5A) -> ASCII lowercase (a-z: $61-$7A)
	if (c >= 0x41 && c <= 0x5A) {
		return c + 0x20;
	}
	// Uppercase PETSCII (A-Z: $C1-$DA) -> ASCII uppercase (A-Z: $41-$5A)
	if (c >= 0xC1 && c <= 0xDA) {
		return c - 0x80;
	}
	// Most other printable characters are the same
	return c;
}
