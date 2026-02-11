#ifndef XMODEM_H
#define XMODEM_H

// XMODEM menu: D)ownload / U)pload selection + transfer
// Returns 1 on success, 0 on cancel/error
int xmodem_menu(unsigned char socketid);

#endif
