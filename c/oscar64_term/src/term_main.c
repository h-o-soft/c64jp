/*
 * C64 Japanese Telnet Terminal
 *
 * Connects to a Telnet BBS via Ultimate II+ network interface.
 * Uses jtxt bitmap mode for Japanese character display.
 * Supports u-term.seq phonebook file (ultimateterm compatible).
 *
 * Screen layout:
 *   Row 0:    Status line (connection info)
 *   Row 1-24: Terminal area (scrolling window)
 */

#include <c64/memmap.h>
#include <string.h>
#ifndef JTXT_CRT
#include <c64/kernalio.h>
#endif
#ifdef JTXT_EASYFLASH
#include <c64/easyflash.h>
#include <c64/cia.h>
#include <c64/vic.h>
#endif
#ifdef JTXT_MAGICDESK_CRT
#include <c64/cia.h>
#include <c64/vic.h>
#endif
#include "c64_oscar.h"
#include "jtxt.h"
#include "c64u_network.h"
#include "c64u_turbo.h"
#include "telnet.h"
#include "ime.h"
#include "xmodem.h"

#ifdef JTXT_EASYFLASH
// EasyFlash CRT: Memory layout
#pragma region(main, 0x0900, 0x5C00, , , { code, data, stack, heap })
#pragma region(extra, 0xC000, 0xD000, , , { bss })
jtxt_state_t jtxt_state;

#elif defined(JTXT_MAGICDESK_CRT)
// MagicDesk CRT: 2-bank code layout with ROM-to-RAM copy
// Bank 0: bootstrap (ROM) + main code (copied to $0900) + ccopy (copied to $0380)
// Bank 1: IME code (copied to $2300)
// Banks 2-10: fonts, Banks 11-36: dictionary
#pragma region(boot, 0x8080, 0x8600, , 0, { code, data })
#pragma section(ccode, 0)
#pragma region(crom, 0x9E00, 0xA000, , 0, { ccode }, 0x0380)
#pragma section(mcode, 0)
#pragma section(mdata, 0)
#pragma region(mrom, 0x8600, 0x9E00, , 0, { mcode, mdata }, 0x0900)
#pragma section(icode, 0)
#pragma section(idata, 0)
#pragma region(irom, 0x8000, 0xA000, , 1, { icode, idata }, 0x2300)
// Overlay B: XMODEM (Bank 37)
#pragma section(xcode, 0)
#pragma section(xdata, 0)
#pragma region(xrom, 0x8000, 0xA000, , 37, { xcode, xdata }, 0x2300)
// RAM-only regions (not copied from ROM)
#pragma region(ramreg, 0x4300, 0x5C00, , , { stack, heap })
#pragma region(bssreg, 0xC000, 0xD000, , , { bss })
#pragma stacksize(512)
jtxt_state_t jtxt_state;

#else
// PRG: Memory layout for bitmap mode
#pragma region(main, 0x0900, 0x5C00, , , { code, data, stack })
#pragma region(extra, 0xa000, 0xd000, , , { bss, heap })
#pragma stacksize(512)
#endif

// Host list
#define MAX_HOSTS 5
#define HOST_NAME_SIZE 32
#define DEFAULT_HOST "beryl.h-o-soft.com"
#define DEFAULT_PORT 2323

#ifndef JTXT_CRT
// Current disk device number (read from $BA at init)
static unsigned char disk_dev = 8;
#endif

static char hosts[MAX_HOSTS][HOST_NAME_SIZE];
static unsigned int ports[MAX_HOSTS];
static unsigned char host_count;

// Currently selected/connected host
static char connect_host[HOST_NAME_SIZE];
static unsigned int connect_port;

// C64 keyboard buffer
#define KEYBUF_COUNT 0xC6
#define KEYBUF_START 0x0277

// RUN/STOP key: row 7, column 7 of keyboard matrix
#define STOP_KEY_ROW 0x7F

// PETSCII cursor keys
#define PETSCII_DOWN  0x11
#define PETSCII_UP    0x91
#define PETSCII_RETURN 0x0D
#define PETSCII_DEL   0x14
#define PETSCII_F3    134

#ifdef JTXT_MAGICDESK_CRT
#pragma code(mcode)
#pragma data(mdata)
// Forward declaration: ccopy is in ccode section (RAM $0380)
void ccopy(char bank, char *dst, const char *src, unsigned n);
#endif

// Check if Ultimate II+ is present by reading the ID register
static int c64u_detect(void)
{
	volatile unsigned char *idreg = (volatile unsigned char *)ID_REG;
	unsigned char val = *idreg;
	return (val != 0xFF);
}

// Find the first interface with a valid IP address
static int find_active_interface(void)
{
	unsigned char iface;
	unsigned char ifcount;

	c64u_getinterfacecount();
	ifcount = (unsigned char)c64u_data[0];

	for (iface = 0; iface < ifcount; iface++) {
		c64u_getipaddress_iface(iface);
		if (c64u_success() && (c64u_data[0] != 0 || c64u_data[1] != 0 ||
		                       c64u_data[2] != 0 || c64u_data[3] != 0)) {
			return iface;
		}
	}
	return -1;
}

// Check RUN/STOP key via CIA keyboard matrix
static int check_runstop(void)
{
	unsigned char val;
	POKE(CIA1_PRA, STOP_KEY_ROW);
	val = PEEK(CIA1_PRB);
	POKE(CIA1_PRA, 0xFF);
	return ((val & 0x80) == 0);
}

// Read one key from keyboard buffer, return 0 if empty
static unsigned char read_key(void)
{
	unsigned char count, key;
	count = PEEK(KEYBUF_COUNT);
	if (count == 0) return 0;

	key = PEEK(KEYBUF_START);

	// Shift remaining keys down
	if (count > 1) {
		unsigned char i;
		for (i = 0; i < count - 1; i++) {
			POKE(KEYBUF_START + i, PEEK(KEYBUF_START + i + 1));
		}
	}
	POKE(KEYBUF_COUNT, count - 1);
	return key;
}

// Send a single ASCII character over the socket
static void send_ascii_char(unsigned char socketid, unsigned char c)
{
	c64u_socketwritechar(socketid, (char)c);
}

//=============================================================================
// Host list management
//=============================================================================

// Simple atoi for port numbers
static unsigned int parse_port(const char *s)
{
	unsigned int val = 0;
	while (*s >= '0' && *s <= '9') {
		val = val * 10 + (*s - '0');
		s++;
	}
	return val;
}

// Print port number at current cursor position
static void print_port(unsigned int port)
{
	char rev[6];
	unsigned char r = 0;
	if (port == 0) {
		jtxt_bputc('0');
		return;
	}
	while (port > 0) {
		rev[r++] = '0' + (port % 10);
		port /= 10;
	}
	while (r > 0) {
		jtxt_bputc(rev[--r]);
	}
}

// Print "host:port" at current cursor position
static void print_host_port(const char *host, unsigned int port)
{
	jtxt_bputs(host);
	jtxt_bputc(':');
	print_port(port);
}

// Set default host list
static void set_default_hosts(void)
{
	host_count = 1;
	strcpy(hosts[0], DEFAULT_HOST);
	ports[0] = DEFAULT_PORT;
}

// Load host list from u-term.seq file
static void load_hostlist(void)
{
#ifdef JTXT_CRT
	// CRT: no KERNAL file I/O, use default hosts
	set_default_hosts();
#else
	char line[48];
	int len;

	host_count = 0;

	krnio_setnam("0:u-term,s");
	if (!krnio_open(2, disk_dev, 0)) {
		set_default_hosts();
		return;
	}

	while (host_count < MAX_HOSTS) {
		len = krnio_gets(2, line, sizeof(line));
		if (len <= 0) break;

		// Strip trailing CR/LF
		while (len > 0 && (line[len - 1] == 0x0D || line[len - 1] == 0x0A)) {
			line[--len] = 0;
		}
		if (len == 0) continue;

		// Find last space -> separator between host and port
		{
			int sp = -1;
			int j;
			for (j = 0; j < len; j++) {
				if (line[j] == ' ') sp = j;
			}

			if (sp > 0 && sp < len - 1) {
				// Copy hostname
				{
					unsigned char k;
					for (k = 0; k < sp && k < HOST_NAME_SIZE - 1; k++) {
						hosts[host_count][k] = line[k];
					}
					hosts[host_count][k] = 0;
				}
				ports[host_count] = parse_port(&line[sp + 1]);
			} else {
				// No port specified, use default
				{
					unsigned char k;
					for (k = 0; k < len && k < HOST_NAME_SIZE - 1; k++) {
						hosts[host_count][k] = line[k];
					}
					hosts[host_count][k] = 0;
				}
				ports[host_count] = 23;
			}
			host_count++;
		}
	}

	krnio_close(2);

	if (host_count == 0) {
		set_default_hosts();
	}
#endif
}

//=============================================================================
// Host selection UI
//=============================================================================

// Draw the host selection menu
static void draw_host_menu(unsigned char selected)
{
	unsigned char i;

	jtxt_bwindow(0, 24);
	jtxt_bwindow_disable();
	jtxt_bcls();

	// Title
	jtxt_blocate(0, 0);
	jtxt_bcolor(COLOR_CYAN, COLOR_BLACK);
	jtxt_bputs("C64JP Terminal");

	jtxt_blocate(0, 2);
	jtxt_bcolor(COLOR_WHITE, COLOR_BLACK);
	jtxt_bputs("Select host:");

	// Host list
	for (i = 0; i < host_count; i++) {
		jtxt_blocate(0, (unsigned char)(4 + i));
		if (i == selected) {
			jtxt_bcolor(COLOR_WHITE, COLOR_BLACK);
			jtxt_bputs("> ");
		} else {
			jtxt_bcolor(COLOR_LIGHTGREY, COLOR_BLACK);
			jtxt_bputs("  ");
		}
		print_host_port(hosts[i], ports[i]);
	}

	// Manual input entry
	jtxt_blocate(0, (unsigned char)(4 + host_count));
	if (selected == host_count) {
		jtxt_bcolor(COLOR_WHITE, COLOR_BLACK);
		jtxt_bputs("> ");
	} else {
		jtxt_bcolor(COLOR_LIGHTGREY, COLOR_BLACK);
		jtxt_bputs("  ");
	}
	jtxt_bputs("[ Manual input ]");

	// Help text
	jtxt_blocate(0, 23);
	jtxt_bcolor(COLOR_YELLOW, COLOR_BLACK);
	jtxt_bputs("Up/Down:Select  Return:Connect");
}

// Read a line of text input at given screen position
// Returns length of input, 0 = cancelled (ESC)
static unsigned char read_line_input(unsigned char x, unsigned char y,
                                     char *buffer, unsigned char max_len)
{
	unsigned char pos = 0;
	unsigned char key;

	buffer[0] = 0;
	jtxt_blocate(x, y);
	jtxt_bcolor(COLOR_WHITE, COLOR_BLACK);

	// Show cursor
	jtxt_bputc('_');
	jtxt_blocate((unsigned char)(x + pos), y);

	for (;;) {
		key = read_key();
		if (key == 0) continue;

		if (key == PETSCII_RETURN) {
			// Erase cursor
			jtxt_blocate((unsigned char)(x + pos), y);
			jtxt_bputc(' ');
			return pos;
		}

		if (key == 0x1B) {
			// ESC - cancel
			return 0;
		}

		if (key == PETSCII_DEL) {
			// Backspace
			if (pos > 0) {
				pos--;
				buffer[pos] = 0;
				jtxt_blocate((unsigned char)(x + pos), y);
				jtxt_bputc('_');
				jtxt_bputc(' ');
			}
			continue;
		}

		// Convert PETSCII to ASCII
		{
			unsigned char ascii = petscii_to_ascii(key);
			if (ascii >= 0x20 && ascii < 0x7F && pos < max_len - 1) {
				buffer[pos] = (char)ascii;
				pos++;
				buffer[pos] = 0;
				jtxt_blocate((unsigned char)(x + pos - 1), y);
				jtxt_bputc(ascii);
				if (pos < max_len - 1) {
					jtxt_bputc('_');
				}
			}
		}
	}
}

// Manual host input screen
// Returns 1 if host was entered, 0 if cancelled
static int input_host_manual(void)
{
	char port_buf[6];
	unsigned char len;

	jtxt_bcls();

	jtxt_blocate(0, 0);
	jtxt_bcolor(COLOR_CYAN, COLOR_BLACK);
	jtxt_bputs("C64JP Terminal - Manual Input");

	jtxt_blocate(0, 2);
	jtxt_bcolor(COLOR_LIGHTGREY, COLOR_BLACK);
	jtxt_bputs("Host:");

	jtxt_blocate(0, 4);
	jtxt_bputs("Port:");

	jtxt_blocate(0, 6);
	jtxt_bcolor(COLOR_YELLOW, COLOR_BLACK);
	jtxt_bputs("Return:OK  ESC:Back");

	// Input hostname
	len = read_line_input(6, 2, connect_host, HOST_NAME_SIZE);
	if (len == 0) return 0;

	// Input port
	len = read_line_input(6, 4, port_buf, sizeof(port_buf));
	if (len == 0) {
		connect_port = 23;
	} else {
		connect_port = parse_port(port_buf);
		if (connect_port == 0) connect_port = 23;
	}

	return 1;
}

// Host selection screen
// Returns 1 if a host was selected, 0 if user wants to quit
static int select_host(void)
{
	unsigned char selected = 0;
	unsigned char total = (unsigned char)(host_count + 1); // +1 for manual
	unsigned char key;
	unsigned char prev_selected;

	draw_host_menu(selected);

	for (;;) {
		if (check_runstop()) return 0;

		key = read_key();
		if (key == 0) continue;

		prev_selected = selected;

		if (key == PETSCII_DOWN) {
			if (selected < total - 1) selected++;
		} else if (key == PETSCII_UP) {
			if (selected > 0) selected--;
		} else if (key == PETSCII_RETURN) {
			if (selected < host_count) {
				// Preset host selected
				strcpy(connect_host, hosts[selected]);
				connect_port = ports[selected];
				return 1;
			} else {
				// Manual input
				if (input_host_manual()) {
					return 1;
				}
				// Cancelled - redraw menu
				draw_host_menu(selected);
			}
		}

		if (prev_selected != selected) {
			draw_host_menu(selected);
		}
	}
}

//=============================================================================
// ANSI escape sequence parser
//=============================================================================

#define ANSI_STATE_NORMAL 0
#define ANSI_STATE_ESC    1  // Got ESC
#define ANSI_STATE_CSI    2  // Got ESC [

static unsigned char ansi_state = ANSI_STATE_NORMAL;

// CSI parameter buffer
#define ANSI_MAX_PARAMS 4
static unsigned char ansi_params[ANSI_MAX_PARAMS];
static unsigned char ansi_param_count;
static unsigned int ansi_current_param;
static bool ansi_has_digit;

// ANSI 8-color -> C64 color mapping
static const unsigned char ansi_to_c64_color[8] = {
	COLOR_BLACK, COLOR_RED, COLOR_GREEN, COLOR_YELLOW,
	COLOR_BLUE, COLOR_PURPLE, COLOR_CYAN, COLOR_WHITE
};

// BS erase pattern detection state machine
// Halfwidth erase: BS SP BS         -> jtxt_bbackspace() once
// Fullwidth erase: BS BS SP SP BS BS -> jtxt_bbackspace() once
// Other patterns are discarded
#define BS_STATE_NORMAL         0
#define BS_STATE_BS1            1  // Got BS
#define BS_STATE_BS_SP          2  // Got BS SP (halfwidth: expect BS)
#define BS_STATE_BS_BS          3  // Got BS BS (fullwidth: expect SP)
#define BS_STATE_BS_BS_SP       4  // Got BS BS SP (expect SP)
#define BS_STATE_BS_BS_SP_SP    5  // Got BS BS SP SP (expect BS)
#define BS_STATE_BS_BS_SP_SP_BS 6  // Got BS BS SP SP BS (expect BS)

static unsigned char bs_state = BS_STATE_NORMAL;

// CSI command dispatch
static void ansi_dispatch(unsigned char final_byte)
{
	unsigned char p0 = (ansi_param_count > 0) ? ansi_params[0] : 0;
	unsigned char p1 = (ansi_param_count > 1) ? ansi_params[1] : 0;
	unsigned char n;

	switch (final_byte) {
	case 'A': // Cursor Up
		n = (p0 > 0) ? p0 : 1;
		if (jtxt_state.cursor_y >= jtxt_state.bitmap_top_row + n)
			jtxt_state.cursor_y -= n;
		else
			jtxt_state.cursor_y = jtxt_state.bitmap_top_row;
		jtxt_state.wrap_pending = false;
		break;
	case 'B': // Cursor Down
		n = (p0 > 0) ? p0 : 1;
		if (jtxt_state.cursor_y + n <= jtxt_state.bitmap_bottom_row)
			jtxt_state.cursor_y += n;
		else
			jtxt_state.cursor_y = jtxt_state.bitmap_bottom_row;
		jtxt_state.wrap_pending = false;
		break;
	case 'C': // Cursor Forward
		n = (p0 > 0) ? p0 : 1;
		jtxt_state.cursor_x += n;
		if (jtxt_state.cursor_x > 39) jtxt_state.cursor_x = 39;
		jtxt_state.wrap_pending = false;
		break;
	case 'D': // Cursor Back
		n = (p0 > 0) ? p0 : 1;
		if (jtxt_state.cursor_x >= n)
			jtxt_state.cursor_x -= n;
		else
			jtxt_state.cursor_x = 0;
		jtxt_state.wrap_pending = false;
		break;
	case 'H': // Cursor Position (row;col, 1-based)
	case 'f':
		{
			unsigned char row = (p0 > 0) ? p0 - 1 : 0;
			unsigned char col = (p1 > 0) ? p1 - 1 : 0;
			row += jtxt_state.bitmap_top_row;
			if (row > jtxt_state.bitmap_bottom_row)
				row = jtxt_state.bitmap_bottom_row;
			if (col > 39) col = 39;
			jtxt_blocate(col, row);
		}
		break;
	case 'J': // Erase in Display
		if (p0 == 0 || ansi_param_count == 0) {
			jtxt_bclear_to_eol();
			for (unsigned char r = jtxt_state.cursor_y + 1;
			     r <= jtxt_state.bitmap_bottom_row; r++) {
				jtxt_bclear_line(r);
			}
		} else if (p0 == 2) {
			jtxt_bcls();
		}
		break;
	case 'K': // Erase in Line
		if (p0 == 0 || ansi_param_count == 0) {
			jtxt_bclear_to_eol();
		} else if (p0 == 2) {
			jtxt_bclear_line(jtxt_state.cursor_y);
		}
		break;
	case 'm': // SGR
		if (ansi_param_count == 0) {
			jtxt_bcolor(COLOR_WHITE, COLOR_BLACK);
		}
		for (unsigned char i = 0; i < ansi_param_count; i++) {
			unsigned char p = ansi_params[i];
			if (p == 0) {
				jtxt_bcolor(COLOR_WHITE, COLOR_BLACK);
			} else if (p >= 30 && p <= 37) {
				unsigned char bg = jtxt_state.bitmap_color & 0x0F;
				jtxt_bcolor(ansi_to_c64_color[p - 30], bg);
			} else if (p >= 40 && p <= 47) {
				unsigned char fg = jtxt_state.bitmap_color >> 4;
				jtxt_bcolor(fg, ansi_to_c64_color[p - 40]);
			}
		}
		break;
	}
}

// Process received data and display on terminal
static void process_received(unsigned char socketid, int datacount)
{
	int i;
	unsigned char c;
	int result;

	for (i = 0; i < datacount; i++) {
		c = (unsigned char)c64u_data[i + 2];

		result = telnet_process_byte(c);

		if (result == TELNET_CONSUMED) {
			continue;
		}

		if (result == TELNET_ESCAPED) {
			// IAC IAC -> literal 0xFF, pass to jtxt as data
			jtxt_bputc(0xFF);
			continue;
		}

		// ANSI escape sequence handling
		if (ansi_state == ANSI_STATE_ESC) {
			if (c == 0x5B) {
				// ESC [ -> CSI sequence
				ansi_state = ANSI_STATE_CSI;
				ansi_param_count = 0;
				ansi_current_param = 0;
				ansi_has_digit = false;
			} else {
				// ESC + something else, ignore and reset
				ansi_state = ANSI_STATE_NORMAL;
			}
			continue;
		}

		if (ansi_state == ANSI_STATE_CSI) {
			if (c >= '0' && c <= '9') {
				ansi_current_param = ansi_current_param * 10 + (c - '0');
				ansi_has_digit = true;
			} else if (c == ';') {
				if (ansi_param_count < ANSI_MAX_PARAMS) {
					ansi_params[ansi_param_count++] =
						(ansi_current_param > 255) ? 255 : (unsigned char)ansi_current_param;
				}
				ansi_current_param = 0;
				ansi_has_digit = false;
			} else if (c >= 0x40 && c <= 0x7E) {
				if (ansi_has_digit && ansi_param_count < ANSI_MAX_PARAMS) {
					ansi_params[ansi_param_count++] =
						(ansi_current_param > 255) ? 255 : (unsigned char)ansi_current_param;
				}
				ansi_dispatch(c);
				ansi_state = ANSI_STATE_NORMAL;
			}
			continue;
		}

		// BS erase pattern detection
		if (bs_state != BS_STATE_NORMAL) {
			switch (bs_state) {
			case BS_STATE_BS1:
				if (c == 0x20) { bs_state = BS_STATE_BS_SP; continue; }
				if (c == 0x08) { bs_state = BS_STATE_BS_BS; continue; }
				bs_state = BS_STATE_NORMAL;
				break; // pattern broken, fall through to process c
			case BS_STATE_BS_SP:
				bs_state = BS_STATE_NORMAL;
				if (c == 0x08) { jtxt_bbackspace(); continue; }
				break; // pattern broken
			case BS_STATE_BS_BS:
				if (c == 0x20) { bs_state = BS_STATE_BS_BS_SP; continue; }
				bs_state = BS_STATE_NORMAL;
				break;
			case BS_STATE_BS_BS_SP:
				if (c == 0x20) { bs_state = BS_STATE_BS_BS_SP_SP; continue; }
				bs_state = BS_STATE_NORMAL;
				break;
			case BS_STATE_BS_BS_SP_SP:
				if (c == 0x08) { bs_state = BS_STATE_BS_BS_SP_SP_BS; continue; }
				bs_state = BS_STATE_NORMAL;
				break;
			case BS_STATE_BS_BS_SP_SP_BS:
				bs_state = BS_STATE_NORMAL;
				if (c == 0x08) { jtxt_bbackspace(); continue; }
				break;
			}
		}

		// Normal character processing
		if (c == 0x1B) {
			// ESC
			ansi_state = ANSI_STATE_ESC;
			continue;
		} else if (c == 0x0D) {
			// CR - ignore (use LF for newline)
			continue;
		} else if (c == 0x0A) {
			// LF - newline
			jtxt_bnewline();
		} else if (c == 0x08) {
			// BS - start pattern detection (don't erase yet)
			bs_state = BS_STATE_BS1;
		} else if (c >= 0x20) {
			// Printable ASCII + high bytes (Shift-JIS, half-width kana, etc.)
			// jtxt_bputc handles Shift-JIS multi-byte internally
			jtxt_bputc(c);
		}
		// Control characters (0x00-0x1F except above) are ignored
	}
}

//=============================================================================
// Terminal session
//=============================================================================

// Run a terminal session with the given host
// Returns 1 to go back to host selection, 0 to exit
static int terminal_session(void)
{
	unsigned char socketid;
	int datacount;
	unsigned char key;

	// Set up terminal window (Row 0-23: terminal, Row 24: IME)
	jtxt_bcls();
	jtxt_bwindow(0, 23);
	jtxt_bwindow_enable();
	jtxt_bautowrap_enable();

	jtxt_blocate(0, 0);
	jtxt_bcolor(COLOR_LIGHTGREEN, COLOR_BLACK);

	jtxt_bputs("Connecting to ");
	print_host_port(connect_host, connect_port);
	jtxt_bputs("...");
	jtxt_bnewline();

	// Connect
	socketid = c64u_tcpconnect(connect_host, connect_port);

	if (!c64u_success()) {
		jtxt_bcolor(COLOR_RED, COLOR_BLACK);
		jtxt_bputs("Connection failed: ");
		jtxt_bputs(c64u_status);
		jtxt_bnewline();
		jtxt_bnewline();
		jtxt_bcolor(COLOR_WHITE, COLOR_BLACK);
		jtxt_bputs("Press any key to go back.");
		while (PEEK(KEYBUF_COUNT) == 0) {}
		POKE(KEYBUF_COUNT, 0);
		return 1;
	}

	// Connected
	jtxt_bcolor(COLOR_LIGHTGREEN, COLOR_BLACK);
	jtxt_bputs("Connected! (RUN/STOP to disconnect)");
	jtxt_bnewline();
	jtxt_bcolor(COLOR_WHITE, COLOR_BLACK);

	// Initialize telnet and IME
	telnet_init(socketid);
#ifdef JTXT_MAGICDESK_CRT
	// Load IME overlay from Bank 1 to RAM
	ccopy(1, (char *)0x2300, (char *)0x8000, 0x2000);
#endif
	ime_init();
	ansi_state = ANSI_STATE_NORMAL;
	bs_state = BS_STATE_NORMAL;

	// Main terminal loop
	while (1) {
		// Check RUN/STOP key for disconnect
		if (check_runstop()) {
			break;
		}

		// Receive data
		datacount = c64u_socketread(socketid, 512);

		if (datacount == 0) {
			// Connection closed by remote
			break;
		}

		if (datacount > 0) {
			process_received(socketid, datacount);
		}
		// datacount == -1 means no data available (wait state)

		// Handle keyboard input through IME
		{
			unsigned char ime_event = ime_process();

			if (ime_event == IME_EVENT_CONFIRMED) {
				// IME confirmed text: send Shift-JIS bytes
				const unsigned char *text = ime_get_result_text();
				unsigned char len = ime_get_result_length();
				if (text && len > 0) {
					unsigned char i;
					for (i = 0; i < len; i++) {
						c64u_socketwritechar(socketid, (char)text[i]);
					}
				}
				ime_clear_output();
			} else if (ime_event == IME_EVENT_KEY_PASSTHROUGH) {
				// IME passed through a key: handle normally
				key = ime_get_passthrough_key();
				if (key == 0x0D) {
					send_ascii_char(socketid, 0x0D);
				} else if (key == 0x14) {
					send_ascii_char(socketid, 0x08);
				}
			} else if (ime_event == IME_EVENT_NONE && !ime_is_active()) {
				// IME not active: use normal key handling
				key = read_key();
				if (key != 0) {
					if (key == PETSCII_F3) {
						// F3: XMODEM transfer menu
#ifdef JTXT_MAGICDESK_CRT
						ccopy(37, (char *)0x2300, (char *)0x8000, 0x2000);
#endif
						xmodem_menu(socketid);
#ifdef JTXT_MAGICDESK_CRT
						// Reload IME overlay
						ccopy(1, (char *)0x2300, (char *)0x8000, 0x2000);
						ime_init();
#endif
						continue;
					} else if (key == 0x0D) {
						send_ascii_char(socketid, 0x0D);
					} else if (key == 0x14) {
						send_ascii_char(socketid, 0x08);
					} else {
						unsigned char ascii = petscii_to_ascii(key);
						if (ascii >= 0x20 && ascii < 0x7F) {
							send_ascii_char(socketid, ascii);
						}
					}
				}
			}
			// Other events (MODE_CHANGED, CANCELLED, DEACTIVATED): ignore
		}
	}

	// Deactivate IME if active
	if (ime_is_active()) {
		ime_deactivate();
	}

	// Disconnect
	c64u_socketclose(socketid);

	jtxt_bcolor(COLOR_YELLOW, COLOR_BLACK);
	jtxt_bnewline();
	jtxt_bputs("Disconnected.");
	jtxt_bnewline();
	jtxt_bputs("Press any key...");

	while (PEEK(KEYBUF_COUNT) == 0) {}
	POKE(KEYBUF_COUNT, 0);

	return 1; // Back to host selection
}

//=============================================================================
// Main
//=============================================================================

//=============================================================================
// MagicDesk CRT: ccopy function (runs from RAM at $0380)
//=============================================================================

#ifdef JTXT_MAGICDESK_CRT
#pragma code(ccode)

__export void ccopy(char bank, char *dst, const char *src, unsigned n)
{
	*((volatile char *)0xDE00) = bank;
	while (n) {
		*dst++ = *src++;
		n--;
	}
	*((volatile char *)0xDE00) = 0;
}

#pragma code(code)
#endif

//=============================================================================
// MagicDesk CRT: real_main (runs from RAM after bootstrap copy)
//=============================================================================

static void terminal_app(void)
{
	int active_iface;

	// Enable Ultimate 64 turbo mode (max speed)
	c64u_turbo_set(C64U_SPEED_MAX);

#ifdef JTXT_CRT
	// CRT: Zero-initialize BSS region ($C000-$CFFF)
	memset((void *)0xC000, 0, 0x1000);
#else
	// Bank out BASIC ROM to expose RAM at $A000-$BFFF (BSS region)
	mmap_set(MMAP_NO_BASIC);

	// Get current disk device from $BA (last LOAD device)
	{
		unsigned char ba = PEEK(0xBA);
		disk_dev = (ba < 8) ? 8 : ba;
	}
#endif

	// Initialize jtxt in bitmap mode
	jtxt_init(JTXT_BITMAP_MODE);
	jtxt_bcls();
	jtxt_bcolor(COLOR_WHITE, COLOR_BLACK);
	jtxt_bautowrap_enable();

	// Detection phase (full screen, no window)
	jtxt_blocate(0, 0);
	jtxt_bcolor(COLOR_CYAN, COLOR_BLACK);
	jtxt_bputs("C64JP Terminal - Initializing...");

	jtxt_blocate(0, 2);
	jtxt_bcolor(COLOR_LIGHTGREEN, COLOR_BLACK);

	// Detect Ultimate II+
	if (!c64u_detect()) {
		jtxt_bputs("Ultimate II+ not detected.");
		jtxt_bnewline();
		jtxt_bputs("Press any key to exit.");
		while (PEEK(KEYBUF_COUNT) == 0) {}
		POKE(KEYBUF_COUNT, 0);
		jtxt_cleanup();
		return;
	}

	jtxt_bputs("Ultimate II+ detected.");
	jtxt_bnewline();

	// Initialize command interface
#ifndef JTXT_CRT
	// PRG: identify via DOS subsystem
	c64u_identify();
#endif
	c64u_settarget(TARGET_NETWORK);

	// Find active network interface
	jtxt_bputs("Searching network...");
	jtxt_bnewline();

	active_iface = find_active_interface();
	if (active_iface < 0) {
		jtxt_bputs("No active network found.");
		jtxt_bnewline();
		jtxt_bputs("Press any key to exit.");
		while (PEEK(KEYBUF_COUNT) == 0) {}
		POKE(KEYBUF_COUNT, 0);
		jtxt_cleanup();
		return;
	}

	jtxt_bputs("Network OK.");
	jtxt_bnewline();

	// Load host list from u-term.seq
	jtxt_bputs("Loading phonebook...");
	jtxt_bnewline();
	load_hostlist();

	// Main loop: select host -> connect -> session -> repeat
	while (1) {
		if (!select_host()) {
#ifdef JTXT_CRT
			continue; // CRT: no exit, return to host selection
#else
			break;
#endif
		}

		if (!terminal_session()) {
#ifdef JTXT_CRT
			continue; // CRT: no exit, return to host selection
#else
			break;
#endif
		}
	}

	c64u_turbo_disable();
	jtxt_cleanup();
}

// Cold restart: restore KERNAL state and jump to reset vector
static void cold_restart(void)
{
	// Restore normal memory configuration (BASIC + KERNAL + I/O visible)
	POKE(0x01, 0x37);

	__asm {
		jsr $FF8A  // RESTOR: Restore default KERNAL vectors
		jsr $FF81  // CINT: Initialize screen editor
		jsr $FF84  // IOINIT: Initialize I/O devices
		ldx #$ff
		txs        // Reset stack pointer
		jmp $FCE2  // Cold start
	}
}

#ifdef JTXT_MAGICDESK_CRT
#pragma code(code)
#pragma data(data)
#endif

int main(void)
{
#ifdef JTXT_MAGICDESK_CRT
	// MagicDesk CRT bootstrap: copy code from ROM to RAM, then run
	mmap_set(MMAP_ROM);
	cia_init();
	vic_setmode(VICM_TEXT, (char *)0x0400, (char *)0x1800);

	// Initialize KERNAL IRQ for keyboard scanning
	// cia_init() disables all IRQs; we need CIA1 Timer A driving the
	// KERNAL handler which scans the keyboard matrix into $C6/$0277.
	__asm { jsr $FF8A }         // RESTOR: set default software vectors ($0314=$EA31)
	cia1.ta = 16421;            // Timer A period for ~60Hz (PAL)
	cia1.icr = 0x81;            // Enable CIA1 Timer A interrupt
	cia1.cra = 0x11;            // Start Timer A, continuous mode
	__asm { cli }               // Enable CPU interrupts

	// 1. Copy ccopy function to RAM ($9E00 -> $0380, 512 bytes)
	{
		unsigned i;
		for (i = 0; i < 0x200; i++)
			((char *)0x0380)[i] = ((char *)0x9E00)[i];
	}

	// 2. Copy main code to RAM ($8400 -> $0900, 6656 bytes)
	{
		unsigned i;
		for (i = 0; i < 0x1800; i++)
			((char *)0x0900)[i] = ((char *)0x8600)[i];
	}

	// 3. Jump to terminal app (now in RAM)
	// IME overlay (Bank 1) is loaded on-demand in terminal_session()
	terminal_app();

#elif defined(JTXT_EASYFLASH)
	// EasyFlash CRT: hardware init
	mmap_set(MMAP_ROM);
	cia_init();
	vic_setmode(VICM_TEXT, (char *)0x0400, (char *)0x1800);
	terminal_app();

#else
	// PRG
	terminal_app();
#endif

	cold_restart();
	return 0; // unreachable
}

//=============================================================================
// EasyFlash CRT: Embedded font and dictionary data
//=============================================================================

#ifdef JTXT_EASYFLASH

//--- Bank 1: JIS X 0201 Half-width Font (2KB) + Misaki Gothic Part 1 (14KB) ---
#pragma section( data1, 0 )
#pragma region(font1, 0x8000, 0xc000, , 1, { data1 })
#pragma data( data1 )

__export const unsigned char font_jisx0201[] = {
    #embed "../../../fontconv/font_jisx0201.bin"
};
__export const unsigned char font_gothic_0[] = {
    #embed 14336 0 "../../../fontconv/font_misaki_gothic.bin"
};

#pragma data( data )

//--- Bank 2: Misaki Gothic Part 2 (16KB) ---
#pragma section( data2, 0 )
#pragma region(font2, 0x8000, 0xc000, , 2, { data2 })
#pragma data( data2 )

__export const unsigned char font_gothic_1[] = {
    #embed 16384 14336 "../../../fontconv/font_misaki_gothic.bin"
};

#pragma data( data )

//--- Bank 3: Misaki Gothic Part 3 (16KB) ---
#pragma section( data3, 0 )
#pragma region(font3, 0x8000, 0xc000, , 3, { data3 })
#pragma data( data3 )

__export const unsigned char font_gothic_2[] = {
    #embed 16384 30720 "../../../fontconv/font_misaki_gothic.bin"
};

#pragma data( data )

//--- Bank 4: Misaki Gothic Part 4 (16KB) ---
#pragma section( data4, 0 )
#pragma region(font4, 0x8000, 0xc000, , 4, { data4 })
#pragma data( data4 )

__export const unsigned char font_gothic_3[] = {
    #embed 16384 47104 "../../../fontconv/font_misaki_gothic.bin"
};

#pragma data( data )

//--- Bank 5: Misaki Gothic Part 5 (remaining 7200B) ---
#pragma section( data5, 0 )
#pragma region(font5, 0x8000, 0xc000, , 5, { data5 })
#pragma data( data5 )

__export const unsigned char font_gothic_4[] = {
    #embed 7200 63488 "../../../fontconv/font_misaki_gothic.bin"
};

#pragma data( data )

//--- Banks 6-18: SKK Dictionary (210,789 bytes in 13 banks) ---

#pragma section( dic6, 0 )
#pragma region(dict6, 0x8000, 0xc000, , 6, { dic6 })
#pragma data( dic6 )
__export const unsigned char dict_0[] = {
    #embed 16384 0 "../../../dicconv/skkdic.bin"
};
#pragma data( data )

#pragma section( dic7, 0 )
#pragma region(dict7, 0x8000, 0xc000, , 7, { dic7 })
#pragma data( dic7 )
__export const unsigned char dict_1[] = {
    #embed 16384 16384 "../../../dicconv/skkdic.bin"
};
#pragma data( data )

#pragma section( dic8, 0 )
#pragma region(dict8, 0x8000, 0xc000, , 8, { dic8 })
#pragma data( dic8 )
__export const unsigned char dict_2[] = {
    #embed 16384 32768 "../../../dicconv/skkdic.bin"
};
#pragma data( data )

#pragma section( dic9, 0 )
#pragma region(dict9, 0x8000, 0xc000, , 9, { dic9 })
#pragma data( dic9 )
__export const unsigned char dict_3[] = {
    #embed 16384 49152 "../../../dicconv/skkdic.bin"
};
#pragma data( data )

#pragma section( dic10, 0 )
#pragma region(dict10, 0x8000, 0xc000, , 10, { dic10 })
#pragma data( dic10 )
__export const unsigned char dict_4[] = {
    #embed 16384 65536 "../../../dicconv/skkdic.bin"
};
#pragma data( data )

#pragma section( dic11, 0 )
#pragma region(dict11, 0x8000, 0xc000, , 11, { dic11 })
#pragma data( dic11 )
__export const unsigned char dict_5[] = {
    #embed 16384 81920 "../../../dicconv/skkdic.bin"
};
#pragma data( data )

#pragma section( dic12, 0 )
#pragma region(dict12, 0x8000, 0xc000, , 12, { dic12 })
#pragma data( dic12 )
__export const unsigned char dict_6[] = {
    #embed 16384 98304 "../../../dicconv/skkdic.bin"
};
#pragma data( data )

#pragma section( dic13, 0 )
#pragma region(dict13, 0x8000, 0xc000, , 13, { dic13 })
#pragma data( dic13 )
__export const unsigned char dict_7[] = {
    #embed 16384 114688 "../../../dicconv/skkdic.bin"
};
#pragma data( data )

#pragma section( dic14, 0 )
#pragma region(dict14, 0x8000, 0xc000, , 14, { dic14 })
#pragma data( dic14 )
__export const unsigned char dict_8[] = {
    #embed 16384 131072 "../../../dicconv/skkdic.bin"
};
#pragma data( data )

#pragma section( dic15, 0 )
#pragma region(dict15, 0x8000, 0xc000, , 15, { dic15 })
#pragma data( dic15 )
__export const unsigned char dict_9[] = {
    #embed 16384 147456 "../../../dicconv/skkdic.bin"
};
#pragma data( data )

#pragma section( dic16, 0 )
#pragma region(dict16, 0x8000, 0xc000, , 16, { dic16 })
#pragma data( dic16 )
__export const unsigned char dict_10[] = {
    #embed 16384 163840 "../../../dicconv/skkdic.bin"
};
#pragma data( data )

#pragma section( dic17, 0 )
#pragma region(dict17, 0x8000, 0xc000, , 17, { dic17 })
#pragma data( dic17 )
__export const unsigned char dict_11[] = {
    #embed 16384 180224 "../../../dicconv/skkdic.bin"
};
#pragma data( data )

#pragma section( dic18, 0 )
#pragma region(dict18, 0x8000, 0xc000, , 18, { dic18 })
#pragma data( dic18 )
__export const unsigned char dict_12[] = {
    #embed 14181 196608 "../../../dicconv/skkdic.bin"
};
#pragma data( data )

#endif /* JTXT_EASYFLASH */

//=============================================================================
// MagicDesk CRT: Embedded font and dictionary data (8KB banks)
//=============================================================================

#ifdef JTXT_MAGICDESK_CRT

//--- Bank 2: JIS X 0201 (2KB) + Misaki Gothic Part 1 (6144B) = 8KB ---
#pragma section( md2, 0 )
#pragma region( mdbank2, 0x8000, 0xA000, , 2, { md2 })
#pragma data( md2 )
__export const unsigned char md_font_jisx0201[] = {
    #embed "../../../fontconv/font_jisx0201.bin"
};
__export const unsigned char md_font_gothic_0[] = {
    #embed 6144 0 "../../../fontconv/font_misaki_gothic.bin"
};
#pragma data( data )

//--- Bank 3: Misaki Gothic Part 2 (8KB) ---
#pragma section( md3, 0 )
#pragma region( mdbank3, 0x8000, 0xA000, , 3, { md3 })
#pragma data( md3 )
__export const unsigned char md_font_gothic_1[] = {
    #embed 8192 6144 "../../../fontconv/font_misaki_gothic.bin"
};
#pragma data( data )

//--- Bank 4: Misaki Gothic Part 3 (8KB) ---
#pragma section( md4, 0 )
#pragma region( mdbank4, 0x8000, 0xA000, , 4, { md4 })
#pragma data( md4 )
__export const unsigned char md_font_gothic_2[] = {
    #embed 8192 14336 "../../../fontconv/font_misaki_gothic.bin"
};
#pragma data( data )

//--- Bank 5: Misaki Gothic Part 4 (8KB) ---
#pragma section( md5, 0 )
#pragma region( mdbank5, 0x8000, 0xA000, , 5, { md5 })
#pragma data( md5 )
__export const unsigned char md_font_gothic_3[] = {
    #embed 8192 22528 "../../../fontconv/font_misaki_gothic.bin"
};
#pragma data( data )

//--- Bank 6: Misaki Gothic Part 5 (8KB) ---
#pragma section( md6, 0 )
#pragma region( mdbank6, 0x8000, 0xA000, , 6, { md6 })
#pragma data( md6 )
__export const unsigned char md_font_gothic_4[] = {
    #embed 8192 30720 "../../../fontconv/font_misaki_gothic.bin"
};
#pragma data( data )

//--- Bank 7: Misaki Gothic Part 6 (8KB) ---
#pragma section( md7, 0 )
#pragma region( mdbank7, 0x8000, 0xA000, , 7, { md7 })
#pragma data( md7 )
__export const unsigned char md_font_gothic_5[] = {
    #embed 8192 38912 "../../../fontconv/font_misaki_gothic.bin"
};
#pragma data( data )

//--- Bank 8: Misaki Gothic Part 7 (8KB) ---
#pragma section( md8, 0 )
#pragma region( mdbank8, 0x8000, 0xA000, , 8, { md8 })
#pragma data( md8 )
__export const unsigned char md_font_gothic_6[] = {
    #embed 8192 47104 "../../../fontconv/font_misaki_gothic.bin"
};
#pragma data( data )

//--- Bank 9: Misaki Gothic Part 8 (8KB) ---
#pragma section( md9, 0 )
#pragma region( mdbank9, 0x8000, 0xA000, , 9, { md9 })
#pragma data( md9 )
__export const unsigned char md_font_gothic_7[] = {
    #embed 8192 55296 "../../../fontconv/font_misaki_gothic.bin"
};
#pragma data( data )

//--- Bank 10: Misaki Gothic Part 9 (remaining 7200B) ---
#pragma section( md10, 0 )
#pragma region( mdbank10, 0x8000, 0xA000, , 10, { md10 })
#pragma data( md10 )
__export const unsigned char md_font_gothic_8[] = {
    #embed 7200 63488 "../../../fontconv/font_misaki_gothic.bin"
};
#pragma data( data )

//--- Banks 11-36: SKK Dictionary (210,789 bytes in 26 x 8KB banks) ---

#pragma section( mddic11, 0 )
#pragma region( mddict11, 0x8000, 0xA000, , 11, { mddic11 })
#pragma data( mddic11 )
__export const unsigned char md_dict_0[] = {
    #embed 8192 0 "../../../dicconv/skkdic.bin"
};
#pragma data( data )

#pragma section( mddic12, 0 )
#pragma region( mddict12, 0x8000, 0xA000, , 12, { mddic12 })
#pragma data( mddic12 )
__export const unsigned char md_dict_1[] = {
    #embed 8192 8192 "../../../dicconv/skkdic.bin"
};
#pragma data( data )

#pragma section( mddic13, 0 )
#pragma region( mddict13, 0x8000, 0xA000, , 13, { mddic13 })
#pragma data( mddic13 )
__export const unsigned char md_dict_2[] = {
    #embed 8192 16384 "../../../dicconv/skkdic.bin"
};
#pragma data( data )

#pragma section( mddic14, 0 )
#pragma region( mddict14, 0x8000, 0xA000, , 14, { mddic14 })
#pragma data( mddic14 )
__export const unsigned char md_dict_3[] = {
    #embed 8192 24576 "../../../dicconv/skkdic.bin"
};
#pragma data( data )

#pragma section( mddic15, 0 )
#pragma region( mddict15, 0x8000, 0xA000, , 15, { mddic15 })
#pragma data( mddic15 )
__export const unsigned char md_dict_4[] = {
    #embed 8192 32768 "../../../dicconv/skkdic.bin"
};
#pragma data( data )

#pragma section( mddic16, 0 )
#pragma region( mddict16, 0x8000, 0xA000, , 16, { mddic16 })
#pragma data( mddic16 )
__export const unsigned char md_dict_5[] = {
    #embed 8192 40960 "../../../dicconv/skkdic.bin"
};
#pragma data( data )

#pragma section( mddic17, 0 )
#pragma region( mddict17, 0x8000, 0xA000, , 17, { mddic17 })
#pragma data( mddic17 )
__export const unsigned char md_dict_6[] = {
    #embed 8192 49152 "../../../dicconv/skkdic.bin"
};
#pragma data( data )

#pragma section( mddic18, 0 )
#pragma region( mddict18, 0x8000, 0xA000, , 18, { mddic18 })
#pragma data( mddic18 )
__export const unsigned char md_dict_7[] = {
    #embed 8192 57344 "../../../dicconv/skkdic.bin"
};
#pragma data( data )

#pragma section( mddic19, 0 )
#pragma region( mddict19, 0x8000, 0xA000, , 19, { mddic19 })
#pragma data( mddic19 )
__export const unsigned char md_dict_8[] = {
    #embed 8192 65536 "../../../dicconv/skkdic.bin"
};
#pragma data( data )

#pragma section( mddic20, 0 )
#pragma region( mddict20, 0x8000, 0xA000, , 20, { mddic20 })
#pragma data( mddic20 )
__export const unsigned char md_dict_9[] = {
    #embed 8192 73728 "../../../dicconv/skkdic.bin"
};
#pragma data( data )

#pragma section( mddic21, 0 )
#pragma region( mddict21, 0x8000, 0xA000, , 21, { mddic21 })
#pragma data( mddic21 )
__export const unsigned char md_dict_10[] = {
    #embed 8192 81920 "../../../dicconv/skkdic.bin"
};
#pragma data( data )

#pragma section( mddic22, 0 )
#pragma region( mddict22, 0x8000, 0xA000, , 22, { mddic22 })
#pragma data( mddic22 )
__export const unsigned char md_dict_11[] = {
    #embed 8192 90112 "../../../dicconv/skkdic.bin"
};
#pragma data( data )

#pragma section( mddic23, 0 )
#pragma region( mddict23, 0x8000, 0xA000, , 23, { mddic23 })
#pragma data( mddic23 )
__export const unsigned char md_dict_12[] = {
    #embed 8192 98304 "../../../dicconv/skkdic.bin"
};
#pragma data( data )

#pragma section( mddic24, 0 )
#pragma region( mddict24, 0x8000, 0xA000, , 24, { mddic24 })
#pragma data( mddic24 )
__export const unsigned char md_dict_13[] = {
    #embed 8192 106496 "../../../dicconv/skkdic.bin"
};
#pragma data( data )

#pragma section( mddic25, 0 )
#pragma region( mddict25, 0x8000, 0xA000, , 25, { mddic25 })
#pragma data( mddic25 )
__export const unsigned char md_dict_14[] = {
    #embed 8192 114688 "../../../dicconv/skkdic.bin"
};
#pragma data( data )

#pragma section( mddic26, 0 )
#pragma region( mddict26, 0x8000, 0xA000, , 26, { mddic26 })
#pragma data( mddic26 )
__export const unsigned char md_dict_15[] = {
    #embed 8192 122880 "../../../dicconv/skkdic.bin"
};
#pragma data( data )

#pragma section( mddic27, 0 )
#pragma region( mddict27, 0x8000, 0xA000, , 27, { mddic27 })
#pragma data( mddic27 )
__export const unsigned char md_dict_16[] = {
    #embed 8192 131072 "../../../dicconv/skkdic.bin"
};
#pragma data( data )

#pragma section( mddic28, 0 )
#pragma region( mddict28, 0x8000, 0xA000, , 28, { mddic28 })
#pragma data( mddic28 )
__export const unsigned char md_dict_17[] = {
    #embed 8192 139264 "../../../dicconv/skkdic.bin"
};
#pragma data( data )

#pragma section( mddic29, 0 )
#pragma region( mddict29, 0x8000, 0xA000, , 29, { mddic29 })
#pragma data( mddic29 )
__export const unsigned char md_dict_18[] = {
    #embed 8192 147456 "../../../dicconv/skkdic.bin"
};
#pragma data( data )

#pragma section( mddic30, 0 )
#pragma region( mddict30, 0x8000, 0xA000, , 30, { mddic30 })
#pragma data( mddic30 )
__export const unsigned char md_dict_19[] = {
    #embed 8192 155648 "../../../dicconv/skkdic.bin"
};
#pragma data( data )

#pragma section( mddic31, 0 )
#pragma region( mddict31, 0x8000, 0xA000, , 31, { mddic31 })
#pragma data( mddic31 )
__export const unsigned char md_dict_20[] = {
    #embed 8192 163840 "../../../dicconv/skkdic.bin"
};
#pragma data( data )

#pragma section( mddic32, 0 )
#pragma region( mddict32, 0x8000, 0xA000, , 32, { mddic32 })
#pragma data( mddic32 )
__export const unsigned char md_dict_21[] = {
    #embed 8192 172032 "../../../dicconv/skkdic.bin"
};
#pragma data( data )

#pragma section( mddic33, 0 )
#pragma region( mddict33, 0x8000, 0xA000, , 33, { mddic33 })
#pragma data( mddic33 )
__export const unsigned char md_dict_22[] = {
    #embed 8192 180224 "../../../dicconv/skkdic.bin"
};
#pragma data( data )

#pragma section( mddic34, 0 )
#pragma region( mddict34, 0x8000, 0xA000, , 34, { mddic34 })
#pragma data( mddic34 )
__export const unsigned char md_dict_23[] = {
    #embed 8192 188416 "../../../dicconv/skkdic.bin"
};
#pragma data( data )

#pragma section( mddic35, 0 )
#pragma region( mddict35, 0x8000, 0xA000, , 35, { mddic35 })
#pragma data( mddic35 )
__export const unsigned char md_dict_24[] = {
    #embed 8192 196608 "../../../dicconv/skkdic.bin"
};
#pragma data( data )

#pragma section( mddic36, 0 )
#pragma region( mddict36, 0x8000, 0xA000, , 36, { mddic36 })
#pragma data( mddic36 )
__export const unsigned char md_dict_25[] = {
    #embed 5989 204800 "../../../dicconv/skkdic.bin"
};
#pragma data( data )

#endif /* JTXT_MAGICDESK_CRT */
