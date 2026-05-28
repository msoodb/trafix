/*
 * Copyright (C) 2025 Masoud Bolhassani <masoud.bolhassani@gmail.com>
 *
 * This file is part of Trafix.
 *
 * Trafix is released under the GNU General Public License v3 (GPL-3.0).
 * See LICENSE file for details.
 */

#ifndef TRFX_SOCKET_OWNERS_H
#define TRFX_SOCKET_OWNERS_H

#define MAX_SOCKET_OWNERS 256

typedef struct {
    char pid[16];
    char process[64];
    char laddr[64];
    char lport[8];
    char raddr[64];
    char rport[8];
    char proto[8]; // "TCP" or "UDP"
} SocketOwnerInfo;

int get_socket_owner_info(SocketOwnerInfo *owners, int max_owners);

#endif // TRFX_SOCKET_OWNERS_H
