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

#include <stdio.h>

#define MAX_CONNECTIONS 512

typedef struct {
    char protocol[8];
    char local_addr[64];
    char remote_addr[64];
    char state[32];
} ConnectionInfo;

int trfx_parse_connection_file(FILE *fp, const char *proto,
                               ConnectionInfo *connections, int count,
                               int max_conns);
int trfx_parse_connection_path(const char *path, const char *proto,
                               ConnectionInfo *connections, int count,
                               int max_conns);
const char *trfx_tcp_state_name(int state_num);
const char *trfx_udp_state_name(int state_num);
int get_connection_info(ConnectionInfo *connections, int max_conns);

#endif // TRFX_CONNECTIONS_H
