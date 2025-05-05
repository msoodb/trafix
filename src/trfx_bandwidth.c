/*
 * Copyright (C) 2025 Masoud Bolhassani <masoud.bolhassani@gmail.com>
 *
 * This file is part of Trafix.
 *
 * Trafix is released under the GNU General Public License v3 (GPL-3.0).
 * See LICENSE file for details.
 */

#include "trfx_bandwidth.h"
#include <arpa/inet.h>
#include <ctype.h>
#include <dirent.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
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
        name[len - 1] = '\0'; // Remove trailing newline
    } else {
      snprintf(name, name_size, "-");
    }
    fclose(f);
  } else {
    snprintf(name, name_size, "-");
  }
}

static int parse_proc_net(const char *path, const char *proto,
                          BandwidthInfo *list, int max_count) {
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

    sscanf(line, "%*d: %8s:%4s %8s:%4s %02X %*s %*s %*s %u %*d %u",
           local_ip_hex, local_port_hex, remote_ip_hex, remote_port_hex, &state,
           &uid, &inode);

    if (inode == 0)
      continue;

    // Now find the process using this inode
    DIR *proc = opendir("/proc");
    if (!proc)
      break;

    struct dirent *entry;
    while ((entry = readdir(proc)) != NULL) {
      if (!isdigit(entry->d_name[0]))
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
            BandwidthInfo *c = &list[count];
            snprintf(c->pid, sizeof(c->pid), "%.*s", (int)(sizeof(c->pid) - 1),
                     entry->d_name);
            get_process_name_by_pid(c->pid, c->process, sizeof(c->process));
            hex_to_ip_port(local_ip_hex, local_port_hex, c->laddr, c->lport);
            hex_to_ip_port(remote_ip_hex, remote_port_hex, c->raddr, c->rport);
            snprintf(c->proto, sizeof(c->proto), "%s", proto);
            c->sent_kb = 0;
            c->recv_kb = 0;
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

int get_bandwidth_info(BandwidthInfo *bandwidths, int max_conns) {
  int count = 0;
  count +=
      parse_proc_net("/proc/net/tcp", "TCP", bandwidths + count, max_conns - count);
  count +=
      parse_proc_net("/proc/net/udp", "UDP", bandwidths + count, max_conns - count);
  return count;
}
