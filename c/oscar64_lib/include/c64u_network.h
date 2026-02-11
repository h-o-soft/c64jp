/*
 * C64 Ultimate Network Communication for C64JP
 *
 * Independent implementation based on Ultimate II+ hardware register
 * interface ($DF1C-$DF1F) and network command protocol specification.
 * Uses static buffers (no malloc).
 */

#ifndef C64U_NETWORK_H
#define C64U_NETWORK_H

#include <string.h>

// Hardware registers
#define CONTROL_REG      0xDF1C
#define STATUS_REG       0xDF1C
#define CMD_DATA_REG     0xDF1D
#define ID_REG           0xDF1D
#define RESP_DATA_REG    0xDF1E
#define STATUS_DATA_REG  0xDF1F

// Buffer sizes
#define DATA_QUEUE_SZ    896
#define STATUS_QUEUE_SZ  256

// Target IDs
#define TARGET_DOS1      0x01
#define TARGET_NETWORK   0x03

// DOS command (for initialization)
#define DOS_CMD_IDENTIFY 0x01

// Network command codes
#define NET_CMD_GET_INTERFACE_COUNT  0x02
#define NET_CMD_GET_IP_ADDRESS       0x05
#define NET_CMD_TCP_SOCKET_CONNECT   0x07
#define NET_CMD_UDP_SOCKET_CONNECT   0x08
#define NET_CMD_SOCKET_CLOSE         0x09
#define NET_CMD_SOCKET_READ          0x10
#define NET_CMD_SOCKET_WRITE         0x11
#define NET_CMD_TCP_LISTENER_START   0x12
#define NET_CMD_TCP_LISTENER_STOP    0x13
#define NET_CMD_GET_LISTENER_STATE   0x14
#define NET_CMD_GET_LISTENER_SOCKET  0x15

// Listener states
#define NET_LISTENER_STATE_NOT_LISTENING  0x00
#define NET_LISTENER_STATE_LISTENING      0x01
#define NET_LISTENER_STATE_CONNECTED      0x02
#define NET_LISTENER_STATE_BIND_ERROR     0x03
#define NET_LISTENER_STATE_PORT_IN_USE    0x04

// Static buffer limits
#define C64U_CONNECT_HOST_MAX  128
#define C64U_WRITE_DATA_MAX    512

// Global data buffers
extern char c64u_status[STATUS_QUEUE_SZ];
extern char c64u_data[DATA_QUEUE_SZ * 2];
extern int c64u_data_index;
extern int c64u_data_len;

// Status check
inline int c64u_success(void) {
	return (c64u_status[0] == '0') && (c64u_status[1] == '0');
}

// Initialization
void c64u_identify(void);

// Core protocol functions
void c64u_settarget(unsigned char id);
void c64u_sendcommand(unsigned char *bytes, int count);
int  c64u_readdata(void);
int  c64u_readstatus(void);
void c64u_accept(void);
void c64u_abort(void);
int  c64u_isdataavailable(void);
int  c64u_isstatusdataavailable(void);

// Network functions
void c64u_getinterfacecount(void);
void c64u_getipaddress(void);
void c64u_getipaddress_iface(unsigned char iface);
unsigned char c64u_tcpconnect(const char *host, unsigned short port);
unsigned char c64u_udpconnect(const char *host, unsigned short port);
void c64u_socketclose(unsigned char socketid);
int  c64u_socketread(unsigned char socketid, unsigned short length);
void c64u_socketwrite(unsigned char socketid, const char *data);
void c64u_socketwritechar(unsigned char socketid, char one_char);
void c64u_socketwrite_ascii(unsigned char socketid, const char *data);

// Helper functions
char c64u_tcp_nextchar(unsigned char socketid);
int  c64u_tcp_nextline(unsigned char socketid, char *result);
int  c64u_tcp_nextline_ascii(unsigned char socketid, char *result);
void c64u_reset_data(void);
void c64u_tcp_emptybuffer(void);

#endif // C64U_NETWORK_H
