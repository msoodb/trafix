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
#include <pwd.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include "trfx_connections.h"
#include "trfx_socket_owners.h"

static pthread_mutex_t connection_state_mutex = PTHREAD_MUTEX_INITIALIZER;
static TrfxConnectionSummaryResult connection_state_connections;
static int connection_state_initialized = 0;
static int connection_focus_index = 0;

static int hex_byte(const char *hex, unsigned char *byte) {
    unsigned value;

    if (sscanf(hex, "%2x", &value) != 1)
        return 0;

    *byte = (unsigned char)value;
    return 1;
}

static int parse_ipv6_hex(const char *hex, char *dest, size_t dest_size) {
    unsigned char bytes[16];
    struct in6_addr addr;

    if (!hex || strlen(hex) != 32)
        return 0;

    for (int word = 0; word < 4; word++) {
        for (int byte = 0; byte < 4; byte++) {
            int src = word * 8 + (3 - byte) * 2;
            if (!hex_byte(hex + src, &bytes[word * 4 + byte]))
                return 0;
        }
    }

    memcpy(&addr, bytes, sizeof(addr));
    return inet_ntop(AF_INET6, &addr, dest, dest_size) != NULL;
}

static void parse_ip_port(char *dest, const char *hex) {
    char addr_hex[64];
    const char *sep;
    unsigned ip[4], port;

    if (!dest || !hex) {
        return;
    }

    sep = strrchr(hex, ':');
    if (!sep || sscanf(sep + 1, "%X", &port) != 1) {
        snprintf(dest, 64, "-");
        return;
    }

    size_t addr_len = (size_t)(sep - hex);
    if (addr_len >= sizeof(addr_hex)) {
        snprintf(dest, 64, "-");
        return;
    }

    memcpy(addr_hex, hex, addr_len);
    addr_hex[addr_len] = '\0';

    if (addr_len == 32) {
        char ip_str[INET6_ADDRSTRLEN];
        if (parse_ipv6_hex(addr_hex, ip_str, sizeof(ip_str))) {
            snprintf(dest, 64, "[%s]:%u", ip_str, port);
        } else {
            snprintf(dest, 64, "-");
        }
        return;
    }

    if (sscanf(hex, "%2X%2X%2X%2X:%X", &ip[3], &ip[2], &ip[1], &ip[0],
               &port) == 5) {
        snprintf(dest, 64, "%u.%u.%u.%u:%u", ip[0], ip[1], ip[2], ip[3],
                 port);
    } else {
        snprintf(dest, 64, "-");
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

void trfx_connection_state_init(void) {
    pthread_mutex_lock(&connection_state_mutex);
    if (!connection_state_initialized) {
        trfx_init_connection_summary_result(&connection_state_connections);
        connection_state_initialized = 1;
    }
    pthread_mutex_unlock(&connection_state_mutex);
}

void trfx_connection_state_update(const TrfxConnectionSummaryResult *connections) {
    int visible_count;

    pthread_mutex_lock(&connection_state_mutex);
    if (!connection_state_initialized) {
        trfx_init_connection_summary_result(&connection_state_connections);
        connection_state_initialized = 1;
    }
    if (connections)
        connection_state_connections = *connections;
    visible_count = connection_state_connections.count;
    if (visible_count <= 0) {
        connection_focus_index = 0;
    } else if (connection_focus_index >= visible_count) {
        connection_focus_index = visible_count - 1;
    }
    pthread_mutex_unlock(&connection_state_mutex);
}

int trfx_connection_state_copy(TrfxConnectionSummaryResult *connections,
                               int *focus_index) {
    int available = 0;

    pthread_mutex_lock(&connection_state_mutex);
    if (connection_state_initialized) {
        if (connections)
            *connections = connection_state_connections;
        if (focus_index)
            *focus_index = connection_focus_index;
        available = 1;
    }
    pthread_mutex_unlock(&connection_state_mutex);

    return available;
}

void trfx_connection_state_move_focus(int delta) {
    int visible_count;

    pthread_mutex_lock(&connection_state_mutex);
    visible_count = connection_state_connections.count;
    if (visible_count > 0) {
        connection_focus_index += delta;
        if (connection_focus_index < 0)
            connection_focus_index = visible_count - 1;
        else if (connection_focus_index >= visible_count)
            connection_focus_index = 0;
    }
    pthread_mutex_unlock(&connection_state_mutex);
}

static const char *socket_state_name(const char *proto, int state_num) {
    if (strcmp(proto, "UDP") == 0) {
        return trfx_udp_state_name(state_num);
    }

    return trfx_tcp_state_name(state_num);
}

static void resolve_uid_name(unsigned int uid, char *dest, size_t dest_size) {
    struct passwd *pw;

    if (!dest || dest_size == 0)
        return;

    pw = getpwuid((uid_t)uid);
    if (pw && pw->pw_name) {
        snprintf(dest, dest_size, "%s", pw->pw_name);
    } else {
        snprintf(dest, dest_size, "-");
    }
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
        unsigned int uid;

        if (sscanf(line,
                   "%*d: %127[0-9A-Fa-f:] %127[0-9A-Fa-f:] %x %*s %*s %*s %u %*u %lu",
                   local_hex, remote_hex, &state_num, &uid, &inode) != 5) {
            continue; // skip malformed lines
        }

        // Skip noisy closed TCP states, but keep UDP 07 as UNCONN.
        if (should_skip_connection_state(proto, state_num)) {
            continue; // Skip TIME_WAIT, CLOSE, CLOSE_WAIT, LAST_ACK, CLOSING
        }

        parse_ip_port(local, local_hex);
        parse_ip_port(remote, remote_hex);

        snprintf(list[count].protocol, sizeof(list[count].protocol), "%s", proto);
        snprintf(list[count].local_addr, sizeof(list[count].local_addr), "%s", local);
        snprintf(list[count].remote_addr, sizeof(list[count].remote_addr), "%s", remote);

        snprintf(list[count].state, sizeof(list[count].state), "%s",
                 socket_state_name(proto, state_num));
        list[count].inode = inode;
        list[count].uid = uid;
        resolve_uid_name(uid, list[count].user, sizeof(list[count].user));
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

void trfx_init_connection_summary_result(TrfxConnectionSummaryResult *result) {
    if (!result)
        return;

    memset(result, 0, sizeof(*result));
    result->status = 0;
    result->error[0] = '\0';
}

static int connection_is_ipv6(const ConnectionInfo *connection) {
    if (!connection)
        return 0;

    return strchr(connection->local_addr, '[') != NULL ||
           strchr(connection->remote_addr, '[') != NULL;
}

static int connection_is_owned(const ConnectionInfo *connection) {
    if (!connection)
        return 0;

    return connection->pid[0] != '\0' && strcmp(connection->pid, "-") != 0 &&
           connection->process[0] != '\0' &&
           strcmp(connection->process, "-") != 0;
}

int trfx_collect_connection_summary(const ConnectionInfo *connections, int count,
                                    TrfxConnectionSummaryResult *result,
                                    char *error, size_t error_size) {
    int i;

    if (!result)
        return 0;

    if (error && error_size > 0)
        error[0] = '\0';

    trfx_init_connection_summary_result(result);

    if (!connections || count < 0) {
        snprintf(result->error, sizeof(result->error),
                 "invalid connection snapshot");
        result->status = -1;
        if (error && error_size > 0)
            snprintf(error, error_size, "%s", result->error);
        return 0;
    }

    for (i = 0; i < count && i < MAX_CONNECTIONS; i++) {
        const ConnectionInfo *src = &connections[i];
        TrfxConnectionSummary *dst = &result->rows[result->count];

        snprintf(dst->protocol, sizeof(dst->protocol), "%s", src->protocol);
        snprintf(dst->state, sizeof(dst->state), "%s", src->state);
        snprintf(dst->local_endpoint, sizeof(dst->local_endpoint), "%s",
                 src->local_addr);
        snprintf(dst->remote_endpoint, sizeof(dst->remote_endpoint), "%s",
                 src->remote_addr);
        snprintf(dst->uid, sizeof(dst->uid), "%u", src->uid);
        snprintf(dst->user, sizeof(dst->user), "%s", src->user);
        snprintf(dst->pid, sizeof(dst->pid), "%s", src->pid);
        snprintf(dst->process, sizeof(dst->process), "%s", src->process);
        dst->has_owner = connection_is_owned(src);
        dst->is_listener = strcmp(src->state, "LISTEN") == 0 ||
                           strcmp(src->state, "UNCONN") == 0;
        dst->is_established = strcmp(src->state, "ESTABLISHED") == 0;
        dst->is_ipv6 = connection_is_ipv6(src);
        result->count++;
    }

    result->status = 0;
    return result->count;
}

int get_connection_info(ConnectionInfo *connections, int max_conns) {
    int count = 0;
    count = trfx_parse_connection_path("/proc/net/tcp", "TCP", connections,
                                       count, max_conns);
    count = trfx_parse_connection_path("/proc/net/udp", "UDP", connections,
                                       count, max_conns);
    count = trfx_parse_connection_path("/proc/net/tcp6", "TCP", connections,
                                       count, max_conns);
    count = trfx_parse_connection_path("/proc/net/udp6", "UDP", connections,
                                       count, max_conns);

    apply_socket_owner_map(connections, count);
    qsort(connections, count, sizeof(ConnectionInfo), compare_connections);

    return count;
}
