/*
 * XMODEM Transfer for C64JP Terminal
 *
 * Download: Receives files via XMODEM and saves to disk.
 * Upload:   Reads files from disk and sends via XMODEM.
 *
 * Implements the standard XMODEM/XMODEM-CRC protocol (Ward Christensen, 1977).
 * File I/O uses the C64 KERNAL jump table ($FFBA-$FFD2).
 *
 * Placed in overlay slot (Bank 37, $2300) for MagicDesk CRT.
 */

#include <string.h>
#include "c64_oscar.h"
#include "jtxt.h"
#include "c64u_network.h"
#include "xmodem.h"

#ifdef JTXT_MAGICDESK_CRT
#pragma code(xcode)
#pragma data(xdata)
#endif

// ============================================================
// XMODEM protocol constants
// ============================================================

#define SOH       0x01
#define EOT       0x04
#define ACK       0x06
#define NAK       0x15
#define CAN       0x18

#define SECSIZE   128
#define MAXERRORS 10

// File access modes
#define CBM_READ  0
#define CBM_WRITE 1

// ============================================================
// KERNAL file I/O wrappers (using standard C64 jump table)
// ============================================================

static unsigned char io_status;

static unsigned char read_kernal_status(void)
{
	return __asm {
		jsr $FFB7
		sta accu
		lda #0
		sta accu + 1
	};
}

static void kernal_setnam(const char *name, unsigned char len)
{
	__asm {
		lda len
		ldx name
		ldy name + 1
		jsr $FFBD
	}
}

static void kernal_setlfs(unsigned char lfn, unsigned char device, unsigned char sec_addr)
{
	__asm {
		lda lfn
		ldx device
		ldy sec_addr
		jsr $FFBA
	}
}

static unsigned char kernal_open(void)
{
	return __asm {
		jsr $FFC0
		bcc ok
		sta accu
		lda #0
		sta accu + 1
		jmp done
	ok:
		lda #0
		sta accu
		sta accu + 1
	done:
	};
}

static void kernal_close(unsigned char lfn)
{
	__asm {
		lda lfn
		jsr $FFC3
	}
}

static unsigned char kernal_chkin(unsigned char lfn)
{
	return __asm {
		ldx lfn
		jsr $FFC6
		bcc ok
		sta accu
		lda #0
		sta accu + 1
		jmp done
	ok:
		lda #0
		sta accu
		sta accu + 1
	done:
	};
}

static unsigned char kernal_chkout(unsigned char lfn)
{
	return __asm {
		ldx lfn
		jsr $FFC9
		bcc ok
		sta accu
		lda #0
		sta accu + 1
		jmp done
	ok:
		lda #0
		sta accu
		sta accu + 1
	done:
	};
}

static void kernal_clrchn(void)
{
	__asm {
		jsr $FFCC
	}
}

static unsigned char kernal_chrin(void)
{
	return __asm {
		jsr $FFCF
		sta accu
		lda #0
		sta accu + 1
	};
}

static void kernal_chrout(unsigned char c)
{
	__asm {
		lda c
		jsr $FFD2
	}
}

static unsigned char cbm_open(unsigned char lfn, unsigned char device,
                               unsigned char sec_addr, const char *name)
{
	unsigned char len = 0;
	if (name != (void *)0) {
		len = strlen(name);
	}
	kernal_setnam(name, len);
	kernal_setlfs(lfn, device, sec_addr);
	return kernal_open();
}

static void cbm_close(unsigned char lfn)
{
	kernal_close(lfn);
}

static int cbm_read(unsigned char lfn, void *buffer, unsigned int size)
{
	unsigned char *buf = (unsigned char *)buffer;
	unsigned int count = 0;
	unsigned char c;

	if (kernal_chkin(lfn) != 0)
		return -1;

	while (count < size) {
		c = kernal_chrin();
		io_status = read_kernal_status();
		buf[count++] = c;
		if (io_status & 0x40) break;
		if (io_status & 0x83) {
			kernal_clrchn();
			return -1;
		}
	}
	kernal_clrchn();
	return count;
}

static int cbm_write(unsigned char lfn, const void *buffer, unsigned int size)
{
	const unsigned char *buf = (const unsigned char *)buffer;
	unsigned int count = 0;

	if (kernal_chkout(lfn) != 0)
		return -1;

	while (count < size) {
		kernal_chrout(buf[count++]);
		io_status = read_kernal_status();
		if (io_status & 0x83) {
			kernal_clrchn();
			return -1;
		}
	}
	kernal_clrchn();
	return count;
}

// ============================================================
// Helpers
// ============================================================

#define KEYBUF_COUNT 0xC6
#define KEYBUF_START 0x0277
#define PEEK(a) (*(volatile unsigned char *)(a))
#define POKE(a,v) (*(volatile unsigned char *)(a) = (v))
#define CIA1_PRA 0xDC00
#define CIA1_PRB 0xDC01
#define STOP_KEY_ROW 0x7F

static void print_number(unsigned char n)
{
	char rev[4];
	unsigned char r = 0;
	if (n == 0) { jtxt_bputc('0'); return; }
	while (n > 0) { rev[r++] = '0' + (n % 10); n /= 10; }
	while (r > 0) { jtxt_bputc(rev[--r]); }
}

static unsigned char xm_read_key(void)
{
	unsigned char count, key;
	count = PEEK(KEYBUF_COUNT);
	if (count == 0) return 0;
	key = PEEK(KEYBUF_START);
	if (count > 1) {
		unsigned char i;
		for (i = 0; i < count - 1; i++)
			POKE(KEYBUF_START + i, PEEK(KEYBUF_START + i + 1));
	}
	POKE(KEYBUF_COUNT, count - 1);
	return key;
}

static int xm_check_runstop(void)
{
	unsigned char val;
	POKE(CIA1_PRA, STOP_KEY_ROW);
	val = PEEK(CIA1_PRB);
	POKE(CIA1_PRA, 0xFF);
	return ((val & 0x80) == 0);
}

static void wait_key(void)
{
	POKE(KEYBUF_COUNT, 0);
	while (PEEK(KEYBUF_COUNT) == 0) {}
	POKE(KEYBUF_COUNT, 0);
}

static unsigned char pet_to_asc(unsigned char key)
{
	if (key >= 0xC1 && key <= 0xDA) return key - 0xC1 + 'a';
	if (key >= 0x41 && key <= 0x5A) return key;
	if (key >= 0x30 && key <= 0x39) return key;
	if (key == 0x2E || key == 0x2D || key == 0x5F) return key;
	return 0;
}

static unsigned char read_filename(char *buffer, unsigned char max_len)
{
	unsigned char pos = 0;
	unsigned char key;

	buffer[0] = 0;
	jtxt_bputc('_');

	for (;;) {
		key = xm_read_key();
		if (key == 0) continue;

		if (key == 0x0D) {
			jtxt_bbackspace();
			jtxt_bputc(' ');
			jtxt_bbackspace();
			return pos;
		}
		if (key == 0x1B) return 0;
		if (key == 0x14) {
			if (pos > 0) {
				pos--;
				buffer[pos] = 0;
				jtxt_bbackspace();
				jtxt_bputc('_');
				jtxt_bbackspace();
			}
			continue;
		}
		{
			unsigned char ascii = pet_to_asc(key);
			if (ascii >= 0x20 && ascii < 0x7F && pos < max_len - 1) {
				buffer[pos] = (char)ascii;
				pos++;
				buffer[pos] = 0;
				jtxt_bbackspace();
				jtxt_bputc(ascii);
				if (pos < max_len - 1) jtxt_bputc('_');
			}
		}
	}
}

static void sanitize_filename(char *name)
{
	unsigned char i;
	unsigned char len = strlen(name);
	for (i = 0; i < len; i++) {
		char c = name[i];
		if (c >= 'a' && c <= 'z') c -= 32;
		switch (c) {
			case ':': case ',': case '?': case '*': case '@': case '$':
				c = '.';
		}
		name[i] = c;
	}
}

// Normalize PETSCII key to uppercase ASCII
static unsigned char key_to_upper(unsigned char key)
{
	if (key >= 0xC1 && key <= 0xDA) return key - 0xC1 + 'A';
	if (key >= 'a' && key <= 'z') return key - 32;
	return key;
}

// ============================================================
// Common UI: device, filename, type, confirmation
// Returns 1 if user confirmed, 0 if cancelled
// ============================================================

static unsigned char ui_device;
static char ui_filename[32];
static char ui_open_name[40];
static char ui_filetype;

static int xmodem_ui(const char *title, const char *action_verb)
{
	unsigned char key;

	jtxt_bnewline();
	jtxt_bcolor(COLOR_CYAN, COLOR_BLACK);
	jtxt_bputs(title);
	jtxt_bnewline();
	jtxt_bcolor(COLOR_WHITE, COLOR_BLACK);

	// Device number
	ui_device = 8;
	jtxt_bputs("Device#: ");
	{
		unsigned char dx = jtxt_state.cursor_x;
		unsigned char dy = jtxt_state.cursor_y;
		for (;;) {
			jtxt_blocate(dx, dy);
			print_number(ui_device);
			jtxt_bputs(" +/-/Ret ");

			do { key = xm_read_key(); } while (key == 0);
			if (key == 0x0D) { jtxt_bnewline(); break; }
			if (key == 0x1B) {
				jtxt_bnewline();
				jtxt_bputs("Cancelled.");
				jtxt_bnewline();
				return 0;
			}
			if (key == '+' || key == 0x2B) {
				if (ui_device < 30) ui_device++;
			} else if (key == '-' || key == 0x2D) {
				if (ui_device > 8) ui_device--;
			}
		}
	}

	// Filename
	jtxt_bputs("Filename: ");
	{
		unsigned char len = read_filename(ui_filename, sizeof(ui_filename));
		if (len == 0) {
			jtxt_bnewline();
			jtxt_bputs("Cancelled.");
			jtxt_bnewline();
			return 0;
		}
	}
	jtxt_bnewline();

	// File type
	jtxt_bputs("Type (P/S/U): ");
	ui_filetype = 0;
	for (;;) {
		key = xm_read_key();
		if (key == 0) continue;
		if (key == 0x1B) {
			jtxt_bnewline();
			jtxt_bputs("Cancelled.");
			jtxt_bnewline();
			return 0;
		}
		key = key_to_upper(key);
		if (key == 'P') { ui_filetype = 'P'; break; }
		if (key == 'S') { ui_filetype = 'S'; break; }
		if (key == 'U') { ui_filetype = 'U'; break; }
	}
	jtxt_bputc(ui_filetype);
	jtxt_bnewline();

	// Build open_name: "FILENAME,p"
	sanitize_filename(ui_filename);
	strcpy(ui_open_name, ui_filename);
	{
		unsigned char nlen = strlen(ui_open_name);
		ui_open_name[nlen] = ',';
		ui_open_name[nlen + 1] = (ui_filetype == 'P') ? 'p' :
		                          (ui_filetype == 'S') ? 's' : 'u';
		ui_open_name[nlen + 2] = 0;
	}

	// Confirmation
	jtxt_bputs(action_verb);
	jtxt_bputs(" DEV#");
	print_number(ui_device);
	jtxt_bputc(' ');
	jtxt_bputs(ui_open_name);
	jtxt_bputs("  OK? (Y/N) ");

	for (;;) {
		key = xm_read_key();
		if (key == 0) continue;
		key = key_to_upper(key);
		if (key == 'N') {
			jtxt_bputc('N');
			jtxt_bnewline();
			jtxt_bputs("Cancelled.");
			jtxt_bnewline();
			return 0;
		}
		if (key == 'Y') {
			jtxt_bputc('Y');
			jtxt_bnewline();
			return 1;
		}
	}
}

// ============================================================
// Process received XMODEM sector: write to disk
// ============================================================

static void process_sector(char sector[], char is_eot)
{
	int len = SECSIZE;
	if (is_eot) {
		while (len > 0 && sector[len - 1] == 0x1A)
			--len;
	}
	if (len > 0)
		cbm_write(2, sector, len);
}

// ============================================================
// Drain TCP receive buffer
// ============================================================

static void drain_tcp(unsigned char socketid)
{
	int pending;
	c64u_reset_data();
	do {
		pending = c64u_socketread(socketid, 512);
	} while (pending > 0);
	c64u_reset_data();
}

// ============================================================
// XMODEM Download
// ============================================================

static int xmodem_download(unsigned char socketid)
{
	char scratch_cmd[36];
	char error_buf[40];
	unsigned char status;
	char sector[SECSIZE];
	char c, b, not_b, checksum;
	unsigned char blocknumber;
	unsigned char i, errorcount, errorfound;
	char firstread;

	if (!xmodem_ui("XMODEM Download", "Save"))
		return 0;

	// Build scratch command
	strcpy(scratch_cmd, "s:");
	strcat(scratch_cmd, ui_filename);

	// Open file for writing
	jtxt_bputs("Opening file...");
	cbm_close(15);
	cbm_close(2);
	cbm_open(15, ui_device, 15, scratch_cmd);
	cbm_close(15);

	status = cbm_open(2, ui_device, CBM_WRITE, ui_open_name);
	if (status) {
		jtxt_bnewline();
		jtxt_bcolor(COLOR_RED, COLOR_BLACK);
		jtxt_bputs("I/O ERROR. Aborted.");
		jtxt_bnewline();
		jtxt_bcolor(COLOR_WHITE, COLOR_BLACK);
		jtxt_bputs("Press any key...");
		wait_key();
		return 0;
	}

	// Start XMODEM receive
	jtxt_bnewline();
	jtxt_bputs("Waiting for XMODEM...");
	jtxt_bnewline();

	errorcount = 0;
	blocknumber = 1;

	drain_tcp(socketid);
	c64u_socketwritechar(socketid, NAK);

	firstread = 1;
	do {
		errorfound = 0;
		c = c64u_tcp_nextchar(socketid);

		if (!firstread) {
			jtxt_bputc('.');
			process_sector(sector, c == EOT);
		}

		if (c != EOT) {
			if (c != SOH) {
				jtxt_bnewline();
				jtxt_bputs("ERR: bad SOH");
				jtxt_bnewline();
				if (++errorcount < MAXERRORS) {
					errorfound = 1;
					continue;
				} else {
					jtxt_bputs("FATAL: too many errors");
					jtxt_bnewline();
					break;
				}
			}

			b = c64u_tcp_nextchar(socketid);
			not_b = ~c64u_tcp_nextchar(socketid);

			if (b != not_b) {
				jtxt_bnewline();
				jtxt_bputs("ERR: block parity");
				jtxt_bnewline();
				errorfound = 1;
				++errorcount;
				continue;
			}

			if (b != blocknumber) {
				jtxt_bnewline();
				jtxt_bputs("ERR: wrong block#");
				jtxt_bnewline();
				errorfound = 1;
				++errorcount;
				continue;
			}

			checksum = 0;
			for (i = 0; i < SECSIZE; i++) {
				sector[i] = c64u_tcp_nextchar(socketid);
				checksum += sector[i];
			}

			if (checksum != c64u_tcp_nextchar(socketid)) {
				jtxt_bputs("ERR: checksum");
				jtxt_bnewline();
				errorfound = 1;
				++errorcount;
			}

			if (xm_check_runstop()) {
				c64u_socketwritechar(socketid, CAN);
				jtxt_bnewline();
				jtxt_bputs("Cancelling...");
				jtxt_bnewline();
				cbm_close(2);
				c64u_reset_data();
				cbm_open(15, ui_device, 15, scratch_cmd);
				cbm_read(15, error_buf, sizeof(error_buf));
				cbm_close(15);
				jtxt_bputs("BREAK. Press any key...");
				wait_key();
				return 0;
			}

			if (errorfound != 0) {
				c64u_socketwritechar(socketid, NAK);
			} else {
				c64u_socketwritechar(socketid, ACK);
				++blocknumber;
			}

			firstread = 0;
		}
	} while (c != EOT);

	cbm_close(2);
	cbm_open(15, ui_device, 15, "");
	cbm_read(15, error_buf, sizeof(error_buf));
	cbm_close(15);

	c64u_socketwritechar(socketid, ACK);
	c64u_reset_data();

	jtxt_bnewline();
	jtxt_bcolor(COLOR_LIGHTGREEN, COLOR_BLACK);
	jtxt_bputs("Download complete!");
	jtxt_bnewline();
	jtxt_bcolor(COLOR_WHITE, COLOR_BLACK);
	jtxt_bputs("Press any key...");
	wait_key();
	return 1;
}

// ============================================================
// Binary socket write (sends exact bytes, no strlen)
// Builds UII+ SOCKET_WRITE command directly.
// ============================================================

// NET_CMD_SOCKET_WRITE (0x11) is defined in c64u_network.h

static unsigned char xm_write_buf[3 + SECSIZE + 5]; // header(3) + SOH+blk+~blk+data+CRC(max 133)

static void socket_write_binary(unsigned char socketid, const unsigned char *data, unsigned char len)
{
	unsigned char x;

	xm_write_buf[0] = 0x00;
	xm_write_buf[1] = NET_CMD_SOCKET_WRITE;
	xm_write_buf[2] = socketid;

	for (x = 0; x < len; x++)
		xm_write_buf[x + 3] = data[x];

	c64u_settarget(TARGET_NETWORK);
	c64u_sendcommand(xm_write_buf, 3 + len);

	c64u_readdata();
	c64u_readstatus();
	c64u_accept();
}

// ============================================================
// CRC-16 for XMODEM-CRC (polynomial 0x1021)
// ============================================================

static unsigned int crc16_xmodem(const char *data, unsigned char len)
{
	unsigned int crc = 0;
	unsigned char i, j;
	for (i = 0; i < len; i++) {
		crc ^= ((unsigned int)(unsigned char)data[i] << 8);
		for (j = 0; j < 8; j++) {
			if (crc & 0x8000)
				crc = (crc << 1) ^ 0x1021;
			else
				crc <<= 1;
		}
	}
	return crc;
}

// ============================================================
// XMODEM Upload (supports both checksum and CRC-16 mode)
// ============================================================

#define XMODEM_START_C 0x43  // 'C' = XMODEM-CRC mode

static int xmodem_upload(unsigned char socketid)
{
	char error_buf[40];
	unsigned char status;
	char sector[SECSIZE];
	unsigned char blocknumber;
	char checksum;
	unsigned int crc;
	unsigned char i, errorcount;
	int bytes_read;
	char c;
	char eof_reached;
	char use_crc;

	if (!xmodem_ui("XMODEM Upload", "Send"))
		return 0;

	// Open file for reading
	jtxt_bputs("Opening file...");
	cbm_close(15);
	cbm_close(2);

	status = cbm_open(2, ui_device, CBM_READ, ui_open_name);
	if (status) {
		jtxt_bnewline();
		jtxt_bcolor(COLOR_RED, COLOR_BLACK);
		jtxt_bputs("I/O ERROR. Aborted.");
		jtxt_bnewline();
		jtxt_bcolor(COLOR_WHITE, COLOR_BLACK);
		jtxt_bputs("Press any key...");
		wait_key();
		return 0;
	}

	// Wait for receiver's start signal
	jtxt_bnewline();
	jtxt_bputs("Waiting for receiver...");
	jtxt_bnewline();

	drain_tcp(socketid);

	// Wait for NAK (checksum mode) or 'C' (CRC-16 mode)
	use_crc = 0;
	errorcount = 0;
	for (;;) {
		c = c64u_tcp_nextchar(socketid);
		if (c == NAK) { use_crc = 0; break; }
		if (c == XMODEM_START_C) { use_crc = 1; break; }
		if (c == CAN || c == 0) {
			jtxt_bcolor(COLOR_RED, COLOR_BLACK);
			jtxt_bputs("Receiver cancelled.");
			jtxt_bnewline();
			jtxt_bcolor(COLOR_WHITE, COLOR_BLACK);
			cbm_close(2);
			jtxt_bputs("Press any key...");
			wait_key();
			return 0;
		}
		if (++errorcount >= MAXERRORS) {
			jtxt_bcolor(COLOR_RED, COLOR_BLACK);
			jtxt_bputs("No start signal.");
			jtxt_bnewline();
			jtxt_bcolor(COLOR_WHITE, COLOR_BLACK);
			cbm_close(2);
			jtxt_bputs("Press any key...");
			wait_key();
			return 0;
		}
	}

	if (use_crc)
		jtxt_bputs("CRC-16 mode");
	else
		jtxt_bputs("Checksum mode");
	jtxt_bnewline();

	// Drain any extra C/NAK chars buffered by receiver
	drain_tcp(socketid);

	// Send file block by block
	jtxt_bputs("Sending...");
	blocknumber = 1;
	errorcount = 0;
	eof_reached = 0;

	while (!eof_reached) {
		// Read 128 bytes from disk
		bytes_read = cbm_read(2, sector, SECSIZE);
		if (bytes_read <= 0) break;

		// Check if this is the last block
		if (bytes_read < SECSIZE || (io_status & 0x40)) {
			eof_reached = 1;
			// Pad remainder with 0x1A (CP/M EOF)
			for (i = (unsigned char)bytes_read; i < SECSIZE; i++)
				sector[i] = 0x1A;
		}

		// Build XMODEM block in xm_write_buf (static) and send
		for (;;) {
			{
				unsigned char pktlen;

				xm_write_buf[0] = SOH;
				xm_write_buf[1] = blocknumber;
				xm_write_buf[2] = ~blocknumber;

				for (i = 0; i < SECSIZE; i++)
					xm_write_buf[3 + i] = sector[i];

				if (use_crc) {
					crc = crc16_xmodem(sector, SECSIZE);
					xm_write_buf[3 + SECSIZE] = (unsigned char)(crc >> 8);
					xm_write_buf[3 + SECSIZE + 1] = (unsigned char)(crc & 0xFF);
					pktlen = 3 + SECSIZE + 2; // 133
				} else {
					checksum = 0;
					for (i = 0; i < SECSIZE; i++)
						checksum += sector[i];
					xm_write_buf[3 + SECSIZE] = checksum;
					pktlen = 3 + SECSIZE + 1; // 132
				}

				socket_write_binary(socketid, xm_write_buf, pktlen);
			}

			// Wait for ACK/NAK
			c = c64u_tcp_nextchar(socketid);
			if (c == ACK) {
				jtxt_bputc('.');
				++blocknumber;
				errorcount = 0;
				break;
			}
			if (c == CAN) {
				jtxt_bnewline();
				jtxt_bputs("Receiver cancelled.");
				jtxt_bnewline();
				cbm_close(2);
				jtxt_bputs("Press any key...");
				wait_key();
				return 0;
			}
			// NAK or unexpected: retry
			if (++errorcount >= MAXERRORS) {
				jtxt_bnewline();
				jtxt_bputs("FATAL: too many errors");
				jtxt_bnewline();
				cbm_close(2);
				c64u_socketwritechar(socketid, CAN);
				jtxt_bputs("Press any key...");
				wait_key();
				return 0;
			}

			// Check RUN/STOP for cancel
			if (xm_check_runstop()) {
				c64u_socketwritechar(socketid, CAN);
				jtxt_bnewline();
				jtxt_bputs("Cancelling...");
				jtxt_bnewline();
				cbm_close(2);
				c64u_reset_data();
				jtxt_bputs("BREAK. Press any key...");
				wait_key();
				return 0;
			}
		}
	}

	// Send EOT
	for (;;) {
		c64u_socketwritechar(socketid, EOT);
		c = c64u_tcp_nextchar(socketid);
		if (c == ACK) break;
		if (++errorcount >= MAXERRORS) break;
	}

	cbm_close(2);

	// Read drive error channel
	cbm_open(15, ui_device, 15, "");
	cbm_read(15, error_buf, sizeof(error_buf));
	cbm_close(15);

	c64u_reset_data();

	jtxt_bnewline();
	jtxt_bcolor(COLOR_LIGHTGREEN, COLOR_BLACK);
	jtxt_bputs("Upload complete!");
	jtxt_bnewline();
	jtxt_bcolor(COLOR_WHITE, COLOR_BLACK);
	jtxt_bputs("Press any key...");
	wait_key();
	return 1;
}

// ============================================================
// XMODEM Menu: D)ownload / U)pload
// ============================================================

int xmodem_menu(unsigned char socketid)
{
	unsigned char key;

	jtxt_bnewline();
	jtxt_bcolor(COLOR_YELLOW, COLOR_BLACK);
	jtxt_bputs("XMODEM: D)ownload U)pload ESC=Cancel");
	jtxt_bcolor(COLOR_WHITE, COLOR_BLACK);

	for (;;) {
		key = xm_read_key();
		if (key == 0) continue;
		key = key_to_upper(key);
		if (key == 'D') return xmodem_download(socketid);
		if (key == 'U') return xmodem_upload(socketid);
		if (key == 0x1B) {
			jtxt_bnewline();
			return 0;
		}
	}
}
