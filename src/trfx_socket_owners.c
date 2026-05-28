/*
 * Copyright (C) 2025 Masoud Bolhassani <masoud.bolhassani@gmail.com>
 *
 * This file is part of Trafix.
 *
 * Trafix is released under the GNU General Public License v3 (GPL-3.0).
 * See LICENSE file for details.
 */

#include "trfx_socket_owners.h"

#include <arpa/inet.h>
#include <ctype.h>
#include <dirent.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

static void hex_to_ip_port(const char *hex_ip, const char *hex_port,
                           char *ip_str, char *port_str) {
  struct in_addr addr;
  unsigned int ip, port;
  sscanf(hex_ip, "%X", &ip);
  sscanf(hex_port, "%X", &port);
  addr.s_addr = htonl(ip);
  inet_ntop(AF_INET, &addr, ip_str, 64);
  snprintf(port_str, 8, "%u", port);
}

static void get_process_name_by_pid(const char *pid, char *name,
                                    size_t name_size) {
  char path[256];
  snprintf(path, sizeof(path), "/proc/%s/comm", pid);
  FILE *f = fopen(path, "r");
  if (f) {
    if (fgets(name, name_size, f)) {
      size_t len = strlen(name);
      if (len > 0 && name[len - 1] == '\n')
        name[len - 1] = '\0';
    } else {
      snprintf(name, name_size, "-");
    }
    fclose(f);
  } else {
    snprintf(name, name_size, "-");
  }
}

static int parse_proc_net(const char *path, const char *proto,
                          SocketOwnerInfo *list, int max_count) {
  FILE *fp = fopen(path, "r");
  if (!fp)
    return 0;

  char line[512];
  int count = 0;

  if (fgets(line, sizeof(line), fp) == NULL) {
    fclose(fp);
    return 0;
  }

  while (fgets(line, sizeof(line), fp) && count < max_count) {
    char local_ip_hex[9], local_port_hex[5];
    char remote_ip_hex[9], remote_port_hex[5];
    unsigned int state, uid, inode;

    if (sscanf(line, "%*d: %8s:%4s %8s:%4s %02X %*s %*s %*s %u %*d %u",
               local_ip_hex, local_port_hex, remote_ip_hex, remote_port_hex,
               &state, &uid, &inode) != 7) {
      continue;
    }
    (void)state;
    (void)uid;

    if (inode == 0)
      continue;

    DIR *proc = opendir("/proc");
    if (!proc)
      break;

    struct dirent *entry;
    while ((entry = readdir(proc)) != NULL) {
      if (!isdigit((unsigned char)entry->d_name[0]))
        continue;

      char fd_dir[300];
      snprintf(fd_dir, sizeof(fd_dir), "/proc/%.*s/fd", 200, entry->d_name);

      DIR *fd = opendir(fd_dir);
      if (!fd)
        continue;

      struct dirent *fd_entry;
      while ((fd_entry = readdir(fd)) != NULL) {
        if (fd_entry->d_name[0] == '.')
          continue;

        char linkpath[512];
        char target[512];
        if (strlen(fd_dir) + 1 + strlen(fd_entry->d_name) < sizeof(linkpath)) {
          strcpy(linkpath, fd_dir);
          strcat(linkpath, "/");
          strcat(linkpath, fd_entry->d_name);
        } else {
          continue;
        }

        ssize_t len = readlink(linkpath, target, sizeof(target) - 1);
        if (len != -1) {
          target[len] = '\0';
          char inode_str[32];
          snprintf(inode_str, sizeof(inode_str), "socket:[%u]", inode);
          if (strstr(target, inode_str)) {
            SocketOwnerInfo *owner = &list[count];
            snprintf(owner->pid, sizeof(owner->pid), "%.*s",
                     (int)(sizeof(owner->pid) - 1), entry->d_name);
            get_process_name_by_pid(owner->pid, owner->process,
                                    sizeof(owner->process));
            hex_to_ip_port(local_ip_hex, local_port_hex, owner->laddr,
                           owner->lport);
            hex_to_ip_port(remote_ip_hex, remote_port_hex, owner->raddr,
                           owner->rport);
            snprintf(owner->proto, sizeof(owner->proto), "%s", proto);
            count++;
            if (count >= max_count) {
              closedir(fd);
              closedir(proc);
              fclose(fp);
              return count;
            }
            break;
          }
        }
      }
      closedir(fd);
    }
    closedir(proc);
  }

  fclose(fp);
  return count;
}

int get_socket_owner_info(SocketOwnerInfo *owners, int max_owners) {
  int count = 0;
  count += parse_proc_net("/proc/net/tcp", "TCP", owners + count,
                          max_owners - count);
  count += parse_proc_net("/proc/net/udp", "UDP", owners + count,
                          max_owners - count);
  return count;
}

int trfx_find_socket_owner_by_inode(const TrfxSocketOwnerMapEntry *entries,
                                    int entry_count, unsigned long inode,
                                    char *pid, size_t pid_size, char *process,
                                    size_t process_size) {
  if (pid && pid_size > 0)
    snprintf(pid, pid_size, "-");
  if (process && process_size > 0)
    snprintf(process, process_size, "-");

  if (!entries || entry_count <= 0 || inode == 0)
    return 0;

  for (int i = 0; i < entry_count; i++) {
    if (entries[i].inode == inode) {
      if (pid && pid_size > 0)
        snprintf(pid, pid_size, "%s", entries[i].pid);
      if (process && process_size > 0)
        snprintf(process, process_size, "%s", entries[i].process);
      return 1;
    }
  }

  return 0;
}

int trfx_scan_socket_owner_map(TrfxSocketOwnerMapEntry *entries,
                               int max_entries) {
  if (!entries || max_entries <= 0)
    return 0;

  DIR *proc = opendir("/proc");
  if (!proc)
    return 0;

  int count = 0;
  struct dirent *entry;
  while ((entry = readdir(proc)) != NULL && count < max_entries) {
    if (!isdigit((unsigned char)entry->d_name[0]))
      continue;

    char fd_dir[300];
    snprintf(fd_dir, sizeof(fd_dir), "/proc/%.*s/fd", 200, entry->d_name);

    DIR *fd = opendir(fd_dir);
    if (!fd)
      continue;

    struct dirent *fd_entry;
    while ((fd_entry = readdir(fd)) != NULL && count < max_entries) {
      if (fd_entry->d_name[0] == '.')
        continue;

      char linkpath[512];
      char target[512];
      if (strlen(fd_dir) + 1 + strlen(fd_entry->d_name) >= sizeof(linkpath))
        continue;

      strcpy(linkpath, fd_dir);
      strcat(linkpath, "/");
      strcat(linkpath, fd_entry->d_name);

      ssize_t len = readlink(linkpath, target, sizeof(target) - 1);
      if (len == -1)
        continue;

      target[len] = '\0';

      unsigned long inode;
      if (sscanf(target, "socket:[%lu]", &inode) != 1 || inode == 0)
        continue;

      entries[count].inode = inode;
      snprintf(entries[count].pid, sizeof(entries[count].pid), "%.*s",
               (int)(sizeof(entries[count].pid) - 1), entry->d_name);
      get_process_name_by_pid(entries[count].pid, entries[count].process,
                              sizeof(entries[count].process));
      count++;
    }

    closedir(fd);
  }

  closedir(proc);
  return count;
}
