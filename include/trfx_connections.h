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
    unsigned long inode;
    unsigned int uid;
    char user[64];
    char pid[16];
    char process[64];
} ConnectionInfo;

typedef struct {
    char protocol[8];
    char state[32];
    char local_endpoint[64];
    char remote_endpoint[64];
    char uid[16];
    char user[64];
    char pid[16];
    char process[64];
    int has_owner;
    int is_listener;
    int is_established;
    int is_ipv6;
} TrfxConnectionSummary;

typedef struct {
    int status;
    int count;
    char error[128];
    TrfxConnectionSummary rows[MAX_CONNECTIONS];
} TrfxConnectionSummaryResult;

int trfx_parse_connection_file(FILE *fp, const char *proto,
                               ConnectionInfo *connections, int count,
                               int max_conns);
int trfx_parse_connection_path(const char *path, const char *proto,
                               ConnectionInfo *connections, int count,
                               int max_conns);
void trfx_init_connection_summary_result(TrfxConnectionSummaryResult *result);
int trfx_collect_connection_summary(const ConnectionInfo *connections, int count,
                                    TrfxConnectionSummaryResult *result,
                                    char *error, size_t error_size);
const char *trfx_tcp_state_name(int state_num);
const char *trfx_udp_state_name(int state_num);
int get_connection_info(ConnectionInfo *connections, int max_conns);

#endif // TRFX_CONNECTIONS_H
