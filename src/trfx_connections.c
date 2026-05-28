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
#include "trfx_socket_owners.h"

static void parse_ip_port(char *dest, const char *hex, int is_ipv6) {
    unsigned ip[4], port;
    if (is_ipv6) {
        snprintf(dest, 64, "[IPv6]");
    } else {
        sscanf(hex, "%2X%2X%2X%2X:%X", &ip[3], &ip[2], &ip[1], &ip[0], &port);
        snprintf(dest, 64, "%u.%u.%u.%u:%u", ip[0], ip[1], ip[2], ip[3], port);
    }
}

const char *trfx_tcp_state_name(int state_num) {
    switch (state_num) {
        case 1: return "ESTABLISHED";
        case 2: return "SYN_SENT";
        case 3: return "SYN_RECV";
        case 4: return "FIN_WAIT1";
        case 5: return "FIN_WAIT2";
        case 6: return "TIME_WAIT";
        case 7: return "CLOSE";
        case 8: return "CLOSE_WAIT";
        case 9: return "LAST_ACK";
        case 10: return "LISTEN";
        case 11: return "CLOSING";
        case 12: return "NEW_SYN_RECV";
        default: return "UNKNOWN";
    }
}

const char *trfx_udp_state_name(int state_num) {
    /*
     * Linux exposes UDP socket states in the same hex field as TCP, but the
     * common listening/unconnected UDP state is 07. Display it as UNCONN to
     * avoid implying TCP CLOSE semantics.
     */
    switch (state_num) {
        case 1: return "ESTABLISHED";
        case 7: return "UNCONN";
        default: return "UNKNOWN";
    }
}

static const char *socket_state_name(const char *proto, int state_num) {
    if (strcmp(proto, "UDP") == 0) {
        return trfx_udp_state_name(state_num);
    }

    return trfx_tcp_state_name(state_num);
}

static int should_skip_connection_state(const char *proto, int state_num) {
    if (strcmp(proto, "TCP") != 0) {
        return 0;
    }

    return state_num == 6 || state_num == 7 || state_num == 8 ||
           state_num == 9 || state_num == 11;
}

int trfx_parse_connection_file(FILE *fp, const char *proto,
                               ConnectionInfo *list, int count, int max) {
    char line[512];

    if (!fp || !proto || !list || max <= 0)
        return count;

    // Safely skip header line
    if (fgets(line, sizeof(line), fp) == NULL) {
        return count;
    }

    while (fgets(line, sizeof(line), fp) && count < max) {
        char local[64], remote[64];
        char local_hex[128], remote_hex[128];
        int state_num;
        unsigned long inode;

        if (sscanf(line,
                   "%*d: %127[0-9A-Fa-f:] %127[0-9A-Fa-f:] %x %*s %*s %*s %*u %*u %lu",
                   local_hex, remote_hex, &state_num, &inode) != 4) {
            continue; // skip malformed lines
        }

        // Skip noisy closed TCP states, but keep UDP 07 as UNCONN.
        if (should_skip_connection_state(proto, state_num)) {
            continue; // Skip TIME_WAIT, CLOSE, CLOSE_WAIT, LAST_ACK, CLOSING
        }

        parse_ip_port(local, local_hex, 0);
        parse_ip_port(remote, remote_hex, 0);

        snprintf(list[count].protocol, sizeof(list[count].protocol), "%s", proto);
        snprintf(list[count].local_addr, sizeof(list[count].local_addr), "%s", local);
        snprintf(list[count].remote_addr, sizeof(list[count].remote_addr), "%s", remote);

        snprintf(list[count].state, sizeof(list[count].state), "%s",
                 socket_state_name(proto, state_num));
        list[count].inode = inode;
        snprintf(list[count].pid, sizeof(list[count].pid), "-");
        snprintf(list[count].process, sizeof(list[count].process), "-");
        count++;
    }

    return count;
}

int trfx_parse_connection_path(const char *path, const char *proto,
                               ConnectionInfo *list, int count, int max) {
    FILE *fp = fopen(path, "r");
    if (!fp)
        return count;

    count = trfx_parse_connection_file(fp, proto, list, count, max);
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

static void apply_socket_owner_map(ConnectionInfo *connections, int count) {
    TrfxSocketOwnerMapEntry owners[MAX_SOCKET_OWNER_MAP_ENTRIES];
    int owner_count = trfx_scan_socket_owner_map(owners,
                                                 MAX_SOCKET_OWNER_MAP_ENTRIES);

    for (int i = 0; connections && i < count; i++) {
        trfx_find_socket_owner_by_inode(owners, owner_count,
                                        connections[i].inode,
                                        connections[i].pid,
                                        sizeof(connections[i].pid),
                                        connections[i].process,
                                        sizeof(connections[i].process));
    }
}

int get_connection_info(ConnectionInfo *connections, int max_conns) {
    int count = 0;
    count = trfx_parse_connection_path("/proc/net/tcp", "TCP", connections,
                                       count, max_conns);
    count = trfx_parse_connection_path("/proc/net/udp", "UDP", connections,
                                       count, max_conns);

    apply_socket_owner_map(connections, count);
    qsort(connections, count, sizeof(ConnectionInfo), compare_connections);

    return count;
}
