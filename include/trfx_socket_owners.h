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

#include <stddef.h>

#define MAX_SOCKET_OWNERS 256
#define MAX_SOCKET_OWNER_MAP_ENTRIES 4096

typedef struct {
    char pid[16];
    char process[64];
    char laddr[64];
    char lport[8];
    char raddr[64];
    char rport[8];
    char proto[8]; // "TCP" or "UDP"
} SocketOwnerInfo;

typedef struct {
    unsigned long inode;
    char pid[16];
    char process[64];
} TrfxSocketOwnerMapEntry;

int get_socket_owner_info(SocketOwnerInfo *owners, int max_owners);
int trfx_parse_socket_owner_path(const char *path, const char *proto,
                                 const TrfxSocketOwnerMapEntry *entries,
                                 int entry_count, SocketOwnerInfo *list,
                                 int start_count, int max_count);
int trfx_scan_socket_owner_map(TrfxSocketOwnerMapEntry *entries,
                               int max_entries);
int trfx_find_socket_owner_by_inode(const TrfxSocketOwnerMapEntry *entries,
                                    int entry_count, unsigned long inode,
                                    char *pid, size_t pid_size, char *process,
                                    size_t process_size);

#endif // TRFX_SOCKET_OWNERS_H
