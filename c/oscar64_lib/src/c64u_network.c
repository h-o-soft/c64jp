/*
 * C64 Ultimate Network Communication for C64JP
 *
 * Independent implementation based on Ultimate II+ hardware register
 * interface ($DF1C-$DF1F) and network command protocol specification.
 *
 * Register map:
 *   $DF1C  Control (write) / Status (read)  -- dual-purpose register
 *   $DF1D  Command data (write)
 *   $DF1E  Response data (read)
 *   $DF1F  Status data (read)
 *
 * Uses static buffers (no malloc).
 */

#include "c64u_network.h"

#ifdef JTXT_MAGICDESK_CRT
#pragma code(mcode)
#pragma data(mdata)
#endif

/*
 * $DF1C is a dual-purpose register:
 *   READ  gives status bits
 *   WRITE sends control commands
 *
 * IMPORTANT: Always use direct assignment (=) for writes, never |=.
 * Using |= would read status bits and OR them into the control write,
 * causing unintended side effects.
 *
 * Status bits (read):
 *   bit 7 (0x80): response data available
 *   bit 6 (0x40): status data available
 *   bit 5 (0x20): command busy
 *   bit 4 (0x10): state (processing)
 *   bit 2 (0x04): error
 *   bit 1 (0x02): accept pending
 *
 * Control commands (write):
 *   0x01: push command
 *   0x02: accept (acknowledge response)
 *   0x04: abort
 *   0x08: clear error
 */

static volatile unsigned char * const reg_ctl  = (volatile unsigned char *)CONTROL_REG;
static volatile unsigned char * const reg_cmd  = (volatile unsigned char *)CMD_DATA_REG;
static volatile unsigned char * const reg_resp = (volatile unsigned char *)RESP_DATA_REG;
static volatile unsigned char * const reg_stat = (volatile unsigned char *)STATUS_DATA_REG;

/* Global data buffers */
char c64u_status[STATUS_QUEUE_SZ];
char c64u_data[DATA_QUEUE_SZ * 2];
int c64u_data_index;
int c64u_data_len;

/* Internal state */
static unsigned char cur_target = TARGET_NETWORK;

/* Static command construction buffers */
static char onechar[2];
static unsigned char conn_cmd[4 + C64U_CONNECT_HOST_MAX + 1];
static unsigned char wr_cmd[3 + C64U_WRITE_DATA_MAX];

/* ============================================================
 * Core hardware interface
 * ============================================================ */

void c64u_settarget(unsigned char id)
{
	cur_target = id;
}

int c64u_isdataavailable(void)
{
	return (*reg_ctl & 0x80) ? 1 : 0;
}

int c64u_isstatusdataavailable(void)
{
	return (*reg_ctl & 0x40) ? 1 : 0;
}

void c64u_sendcommand(unsigned char *bytes, int count)
{
	int i;

	/* First byte is always the target ID */
	bytes[0] = cur_target;

	for (;;) {
		/* Wait for idle: both busy (bit5) and state (bit4) must be clear */
		while (*reg_ctl & 0x30)
			;

		/* Write command bytes to command data register */
		for (i = 0; i < count; i++)
			*reg_cmd = bytes[i];

		/* Push the command */
		*reg_ctl = 0x01;

		/* Check for error (bit 2) */
		if (*reg_ctl & 0x04) {
			/* Clear the error and retry */
			*reg_ctl = 0x08;
			continue;
		}

		/* Wait for command to finish processing.
		 * While state (bit4) is set but busy (bit5) is clear,
		 * the UII+ is still working on the command. */
		while ((*reg_ctl & 0x30) == 0x10)
			;

		return;
	}
}

int c64u_readdata(void)
{
	int n = 0;
	c64u_data[0] = 0;
	while (c64u_isdataavailable())
		c64u_data[n++] = *reg_resp;
	c64u_data[n] = 0;
	return n;
}

int c64u_readstatus(void)
{
	int n = 0;
	c64u_status[0] = 0;
	while (c64u_isstatusdataavailable())
		c64u_status[n++] = *reg_stat;
	c64u_status[n] = 0;
	return n;
}

void c64u_accept(void)
{
	*reg_ctl = 0x02;
	/* Wait for accept to complete (bit 1 clears) */
	while (*reg_ctl & 0x02)
		;
}

void c64u_abort(void)
{
	*reg_ctl = 0x04;
}

/* ============================================================
 * Initialization
 * ============================================================ */

void c64u_identify(void)
{
	unsigned char cmd[2];
	cmd[0] = 0x00;
	cmd[1] = DOS_CMD_IDENTIFY;

	c64u_settarget(TARGET_DOS1);
	c64u_sendcommand(cmd, 2);
	c64u_readdata();
	c64u_readstatus();
	c64u_accept();
}

/* ============================================================
 * Network operations
 *
 * All network commands use TARGET_NETWORK (0x03).
 * Functions save/restore the current target so callers
 * don't need to worry about it.
 * ============================================================ */

void c64u_getinterfacecount(void)
{
	unsigned char prev = cur_target;
	unsigned char cmd[2];
	cmd[0] = 0x00;
	cmd[1] = NET_CMD_GET_INTERFACE_COUNT;

	c64u_settarget(TARGET_NETWORK);
	c64u_sendcommand(cmd, 2);
	c64u_readdata();
	c64u_readstatus();
	c64u_accept();
	cur_target = prev;
}

void c64u_getipaddress(void)
{
	c64u_getipaddress_iface(0x00);
}

void c64u_getipaddress_iface(unsigned char iface)
{
	unsigned char prev = cur_target;
	unsigned char cmd[3];
	cmd[0] = 0x00;
	cmd[1] = NET_CMD_GET_IP_ADDRESS;
	cmd[2] = iface;

	c64u_settarget(TARGET_NETWORK);
	c64u_sendcommand(cmd, 3);
	c64u_readdata();
	c64u_readstatus();
	c64u_accept();
	cur_target = prev;
}

/* Open a TCP or UDP socket to host:port */
static unsigned char open_socket(const char *host, unsigned short port,
                                  unsigned char cmdcode)
{
	unsigned char prev = cur_target;
	int hlen = strlen(host);
	int i;

	if (hlen > C64U_CONNECT_HOST_MAX)
		hlen = C64U_CONNECT_HOST_MAX;

	conn_cmd[0] = 0x00;
	conn_cmd[1] = cmdcode;
	conn_cmd[2] = (unsigned char)(port & 0xFF);
	conn_cmd[3] = (unsigned char)((port >> 8) & 0xFF);
	for (i = 0; i < hlen; i++)
		conn_cmd[4 + i] = host[i];
	conn_cmd[4 + hlen] = 0x00;

	c64u_settarget(TARGET_NETWORK);
	c64u_sendcommand(conn_cmd, 4 + hlen + 1);
	c64u_readdata();
	c64u_readstatus();
	c64u_accept();
	cur_target = prev;

	c64u_data_index = 0;
	c64u_data_len = 0;
	return c64u_data[0];
}

unsigned char c64u_tcpconnect(const char *host, unsigned short port)
{
	return open_socket(host, port, NET_CMD_TCP_SOCKET_CONNECT);
}

unsigned char c64u_udpconnect(const char *host, unsigned short port)
{
	return open_socket(host, port, NET_CMD_UDP_SOCKET_CONNECT);
}

void c64u_socketclose(unsigned char socketid)
{
	unsigned char prev = cur_target;
	unsigned char cmd[3];
	cmd[0] = 0x00;
	cmd[1] = NET_CMD_SOCKET_CLOSE;
	cmd[2] = socketid;

	c64u_settarget(TARGET_NETWORK);
	c64u_sendcommand(cmd, 3);
	c64u_readdata();
	c64u_readstatus();
	c64u_accept();
	cur_target = prev;
}

int c64u_socketread(unsigned char socketid, unsigned short length)
{
	unsigned char prev = cur_target;
	unsigned char cmd[5];
	cmd[0] = 0x00;
	cmd[1] = NET_CMD_SOCKET_READ;
	cmd[2] = socketid;
	cmd[3] = (unsigned char)(length & 0xFF);
	cmd[4] = (unsigned char)((length >> 8) & 0xFF);

	c64u_settarget(TARGET_NETWORK);
	c64u_sendcommand(cmd, 5);
	c64u_readdata();
	c64u_readstatus();
	c64u_accept();
	cur_target = prev;

	return (unsigned char)c64u_data[0] | ((unsigned char)c64u_data[1] << 8);
}

/*
 * PETSCII <-> ASCII character conversion.
 *
 * PETSCII character ranges (standard C64 charset):
 *   0x41-0x5A: uppercase A-Z
 *   0xC1-0xDA: lowercase a-z (shifted)
 *
 * This swaps case for terminal communication:
 *   PETSCII uppercase (0x41-0x5A) -> ASCII lowercase (0x61-0x7A)
 *   PETSCII lowercase (0xC1-0xDA) -> ASCII uppercase (0x41-0x5A)
 *   ASCII lowercase (0x61-0x7A) -> uppercase (0x41-0x5A)
 *   CR (0x0D) -> LF (0x0A)
 */
static char petscii_swap_case(char c)
{
	/* ASCII/PETSCII lowercase range or PETSCII shifted lowercase */
	if ((c >= 97 && c <= 122) || (c >= 193 && c <= 218))
		return c & 95;     /* -> uppercase */
	/* ASCII/PETSCII uppercase range */
	if (c >= 65 && c <= 90)
		return c | 32;     /* -> lowercase */
	return c;
}

/* Write data to socket, with optional PETSCII-ASCII conversion */
static void socket_write_data(unsigned char socketid, const char *data,
                               int convert)
{
	unsigned char prev = cur_target;
	int dlen = strlen(data);
	int i;
	char c;

	if (dlen > C64U_WRITE_DATA_MAX)
		dlen = C64U_WRITE_DATA_MAX;

	wr_cmd[0] = 0x00;
	wr_cmd[1] = NET_CMD_SOCKET_WRITE;
	wr_cmd[2] = socketid;

	for (i = 0; i < dlen; i++) {
		c = data[i];
		if (convert) {
			if (c == 0x0D)
				c = 0x0A;
			else
				c = petscii_swap_case(c);
		}
		wr_cmd[3 + i] = c;
	}

	c64u_settarget(TARGET_NETWORK);
	c64u_sendcommand(wr_cmd, 3 + dlen);
	c64u_readdata();
	c64u_readstatus();
	c64u_accept();
	cur_target = prev;

	c64u_data_index = 0;
	c64u_data_len = 0;
}

void c64u_socketwrite(unsigned char socketid, const char *data)
{
	socket_write_data(socketid, data, 0);
}

void c64u_socketwrite_ascii(unsigned char socketid, const char *data)
{
	socket_write_data(socketid, data, 1);
}

void c64u_socketwritechar(unsigned char socketid, char one_char)
{
	onechar[0] = one_char;
	onechar[1] = 0;
	c64u_socketwrite(socketid, onechar);
}

/* ============================================================
 * Buffered TCP read helpers
 * ============================================================ */

/*
 * Read a single byte from the TCP socket.
 * Uses an internal buffer to minimize command overhead.
 * Returns 0 if no data is available.
 */
char c64u_tcp_nextchar(unsigned char socketid)
{
	char ch;

	if (c64u_data_index < c64u_data_len) {
		/* Serve from buffer (data starts at offset 2) */
		ch = c64u_data[c64u_data_index + 2];
		c64u_data_index++;
		return ch;
	}

	/* Buffer exhausted, fetch more data from socket */
	do {
		c64u_data_len = c64u_socketread(socketid, DATA_QUEUE_SZ - 4);
		if (c64u_data_len == 0)
			return 0;
	} while (c64u_data_len == -1);

	ch = c64u_data[2];
	c64u_data_index = 1;
	return ch;
}

/*
 * Read a line (until LF) from the TCP socket.
 * CR characters are skipped.
 * Returns 1 if data was read, 0 if connection closed.
 */
static int read_line(unsigned char socketid, char *buf, int convert)
{
	int c;
	int pos = 0;

	buf[0] = 0;

	while ((c = c64u_tcp_nextchar(socketid)) != 0 && c != 0x0A) {
		if (c == 0x0D)
			continue;
		if (convert)
			c = petscii_swap_case(c);
		buf[pos++] = c;
	}

	buf[pos] = 0;
	return (c != 0 || pos > 0);
}

int c64u_tcp_nextline(unsigned char socketid, char *result)
{
	return read_line(socketid, result, 0);
}

int c64u_tcp_nextline_ascii(unsigned char socketid, char *result)
{
	return read_line(socketid, result, 1);
}

/* ============================================================
 * Buffer management
 * ============================================================ */

void c64u_reset_data(void)
{
	c64u_data_len = 0;
	c64u_data_index = 0;
	memset(c64u_data, 0, DATA_QUEUE_SZ * 2);
	memset(c64u_status, 0, STATUS_QUEUE_SZ);
}

void c64u_tcp_emptybuffer(void)
{
	c64u_data_index = 0;
}
