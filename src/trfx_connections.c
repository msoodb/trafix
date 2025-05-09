/*
 * Copyright (C) 2025 Masoud Bolhassani <masoud.bolhassani@gmail.com>
 *
 * This file is part of Trafix.
 *
 * Trafix is released under the GNU General Public License v3 (GPL-3.0).
 * See LICENSE file for details.
 */

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "trfx_connections.h"

static void parse_ip_port(char *dest, const char *hex, int is_ipv6) {
    unsigned ip[4], port;
    if (is_ipv6) {
        snprintf(dest, 64, "[IPv6]");
    } else {
        sscanf(hex, "%2X%2X%2X%2X:%X", &ip[3], &ip[2], &ip[1], &ip[0], &port);
        snprintf(dest, 64, "%u.%u.%u.%u:%u", ip[0], ip[1], ip[2], ip[3], port);
    }
}

static int load_connections(const char *path, const char *proto, ConnectionInfo *list, int count, int max) {
    FILE *fp = fopen(path, "r");
    if (!fp) return count;

    char line[512];

    // Safely skip header line
    if (fgets(line, sizeof(line), fp) == NULL) {
        fclose(fp);
        return count;
    }

    while (fgets(line, sizeof(line), fp) && count < max) {
        char local[64], remote[64], state[16];
        char local_hex[128], remote_hex[128];
        int state_num;

        if (sscanf(line, "%*d: %64[0-9A-Fa-f]:%*x %64[0-9A-Fa-f]:%*x %x", local_hex, remote_hex, &state_num) != 3) {
            continue; // skip malformed lines
        }

        // Skip unwanted states
        if (state_num == 6 || state_num == 7 || state_num == 8 || state_num == 9 || state_num == 11) {
            continue; // Skip TIME_WAIT, CLOSE, CLOSE_WAIT, LAST_ACK, CLOSING
        }

        parse_ip_port(local, local_hex, 0);
        parse_ip_port(remote, remote_hex, 0);

        snprintf(list[count].protocol, sizeof(list[count].protocol), "%s", proto);
        snprintf(list[count].local_addr, sizeof(list[count].local_addr), "%s", local);
        snprintf(list[count].remote_addr, sizeof(list[count].remote_addr), "%s", remote);

        switch (state_num) {
            case 1: strcpy(state, "ESTABLISHED"); break;
            case 2: strcpy(state, "SYN_SENT"); break;
            case 3: strcpy(state, "SYN_RECV"); break;
            case 4: strcpy(state, "FIN_WAIT1"); break;
            case 5: strcpy(state, "FIN_WAIT2"); break;
            case 10: strcpy(state, "LISTEN"); break;
            default: strcpy(state, "UNKNOWN"); break;
        }

        snprintf(list[count].state, sizeof(list[count].state), "%s", state);
        count++;
    }

    fclose(fp);
    return count;
}

// Only ONE state_priority function
static int state_priority(const char *state) {
    if (strcmp(state, "ESTABLISHED") == 0) return 0;
    if (strcmp(state, "LISTEN") == 0) return 1;
    if (strcmp(state, "TIME_WAIT") == 0) return 2;
    // Add more if you want...
    return 100; // unknown states last
}

// Add THIS function
static int compare_connections(const void *a, const void *b) {
    const ConnectionInfo *conn_a = (const ConnectionInfo *)a;
    const ConnectionInfo *conn_b = (const ConnectionInfo *)b;

    return state_priority(conn_a->state) - state_priority(conn_b->state);
}

int get_connection_info(ConnectionInfo *connections, int max_conns) {
    int count = 0;
    count = load_connections("/proc/net/tcp", "TCP", connections, count, max_conns);
    count = load_connections("/proc/net/udp", "UDP", connections, count, max_conns);

    qsort(connections, count, sizeof(ConnectionInfo), compare_connections);

    return count;
}
