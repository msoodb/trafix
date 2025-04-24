/*
 * Copyright (C) 2025 Masoud Bolhassani <masoud.bolhassani@gmail.com>
 *
 * This file is part of Trafix.
 *
 * Trafix is released under the GNU General Public License v3 (GPL-3.0).
 * See LICENSE file for details.
 */

#ifndef TRFX_CONNECTIONS_H
#define TRFX_CONNECTIONS_H

#define MAX_CONNECTIONS 512

typedef struct {
    char protocol[8];
    char local_addr[64];
    char remote_addr[64];
    char state[32];
} ConnectionInfo;

int get_connection_info(ConnectionInfo *connections, int max_conns);

#endif // TRFX_CONNECTIONS_H
