/*
 * Copyright (C) 2025 Masoud Bolhassani <masoud.bolhassani@gmail.com>
 *
 * This file is part of Trafix.
 *
 * Trafix is released under the GNU General Public License v3 (GPL-3.0).
 * See LICENSE file for details.
 */

#include "trfx_dashboard.h"

#include "trfx_connections.h"
#include <ncurses.h>
#include <pthread.h>
#include <signal.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <ctype.h>
#include "trfx_bandwidth.h"
#include "trfx_cpu.h"
#include "trfx_disk.h"
#include "trfx_meminfo.h"
#include "trfx_netinfo.h"
#include "trfx_procinfo.h"
#include "trfx_sysinfo.h"
#include "trfx_utils.h"
#include "trfx_wifi.h"

#define COLOR_TITLE 1
#define COLOR_SECTION 2
#define COLOR_DATA 3
#define COLOR_DATA_RED 4
#define COLOR_DATA_YELLOW 5
#define COLOR_DATA_GREEN 6
#define COLOR_HEADER 7
#define COLOR_BORDER 8

#define TOTAL_ROWS 3
#define ROW1_COLS 4
#define ROW2_COLS 4
#define ROW3_COLS 2

#define FIXED_ROW1_HEIGHT 6
#define FIXED_ROW2_HEIGHT 0

#define MAX_DISKS 16

#define KPI_USAGE_WARN 70.0
#define KPI_USAGE_CRIT 90.0
#define BAR_WIDTH 16

#define NUM_MODULES 8

volatile int force_refresh_flags[NUM_MODULES] = {1, 1, 1, 1, 1, 1, 1, 1};
enum ModuleIndex {
  MODULE_SYSINFO = 0,
  MODULE_CPUINFO,
  MODULE_MEMINFO,
  MODULE_DISKINFO,
  MODULE_CONNINFO,
  MODULE_NETINFO,
  MODULE_PROCINFO,
  MODULE_HELPINFO
};

typedef struct {
  const char *name;
  void *(*thread_func)(void *); // Pointer to thread function
} Module;

Module modules[] = {
    {"Connections", connection_info_thread},
    {"Network", network_info_thread},
    {"Processes", process_info_thread},
    {"Banswidths", bandwidth_info_thread},
    {NULL, NULL} // Sentinel
};

typedef struct {
  pthread_t thread_id;
  int module_index; // -1 = none
  WINDOW *window;
} WindowSlot;

WindowSlot row2_slots[3]; // 3 windows in row 2

volatile int ready = 0;
pthread_mutex_t ready_mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t ncurses_mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t global_var_mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t memory_info_mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t disk_info_mutex = PTHREAD_MUTEX_INITIALIZER;

volatile sig_atomic_t screen_paused = 0;
typedef struct {
  int module_index;
  WINDOW *window;
} ThreadArg;

void wait_until_ready() {
  pthread_mutex_lock(&ready_mutex); // Lock before checking the ready flag
  while (!ready) {
    pthread_mutex_unlock(&ready_mutex); // Unlock before sleeping
    usleep(10000);                      // Sleep before checking again
    pthread_mutex_lock(&ready_mutex);   // Lock again before checking
  }
  pthread_mutex_unlock(&ready_mutex); // Unlock after we're done
}

void *system_info_thread(void *arg) {
  WINDOW *win = (WINDOW *)arg;
  wait_until_ready();

  while (1) {
    int row = 1;
    int line = 2;
    int label_width = 16;
    SystemOverview sysinfo = get_system_overview();

    pthread_mutex_lock(&ncurses_mutex);

    werase(win);

    wattron(win, COLOR_PAIR(COLOR_BORDER));
    box(win, 0, 0);
    wattroff(win, COLOR_PAIR(COLOR_BORDER));

    mvwprintw(win, row++, line, "%*s: %s", label_width, "Hostname",
              sysinfo.hostname);
    mvwprintw(win, row++, line, "%*s: %s", label_width, "OS",
              sysinfo.os_version);
    mvwprintw(win, row++, line, "%*s: %s", label_width, "Kernel",
              sysinfo.kernel_version);
    mvwprintw(win, row++, line, "%*s: %s", label_width, "Uptime",
              sysinfo.uptime);
    mvwprintw(win, row++, line, "%*s: %s", label_width, "Load Avg",
              sysinfo.load_avg);
    mvwprintw(win, row++, line, "%*s: %s", label_width, "Logged-in Users",
              sysinfo.logged_in_users);

    wrefresh(win);

    pthread_mutex_unlock(&ncurses_mutex);

    for (int i = 0; i < 50; i++) {
      if (force_refresh_flags[MODULE_SYSINFO]) {
        force_refresh_flags[MODULE_SYSINFO] = 0;
        break;
      }
      sleep(1);
    }
  }
  return NULL;
}

void *cpu_info_thread(void *arg) {
  WINDOW *win = (WINDOW *)arg;
  wait_until_ready();

  extern int temp_warn_yellow;
  extern int temp_warn_red;

  while (1) {
    CPUInfo cpu = get_cpu_info();

    pthread_mutex_lock(&ncurses_mutex);

    werase(win);

    int h, w;
    getmaxyx(win, h, w);
    (void)w;

    // Apply color before drawing border
    wattron(win, COLOR_PAIR(COLOR_BORDER));
    box(win, 0, 0);
    wattroff(win, COLOR_PAIR(COLOR_BORDER));

    int row = 0;
    int line = 3;

    // Title
    if (row < h - 1) {
      wattron(win, A_BOLD);
      mvwprintw(win, row++, line, " CPU Information ");
      wattroff(win, A_BOLD);
    }

    pthread_mutex_lock(&global_var_mutex);
    int temp_color = 0;
    if (cpu.temperature >= temp_warn_red) {
      temp_color = COLOR_DATA_RED;
    } else if (cpu.temperature >= temp_warn_yellow) {
      temp_color = COLOR_DATA_YELLOW;
    }
    pthread_mutex_unlock(&global_var_mutex);

    // Avg usage and temperature
    if (row < h - 1) {
      wmove(win, row, line);
      wprintw(win, "Average: ");

      wattron(win, A_BOLD);
      wprintw(win, "%.1f%%", cpu.avg_usage);
      wattroff(win, A_BOLD);

      wprintw(win, " Temperature: ");

      wattron(win, A_BOLD);
      if (temp_color) {
        wattron(win, COLOR_PAIR(temp_color));
        wprintw(win, "%.1f °C", cpu.temperature);
        wattroff(win, COLOR_PAIR(temp_color));
      } else {
        wprintw(win, "%.1f °C", cpu.temperature);
      }
      wattroff(win, A_BOLD);

      row++;
    }

    char filled_char = '=';
    char empty_char = ' ';

    for (int i = 0; i < cpu.num_cores && row < h - 1; ++i) {
      float usage = cpu.usage_per_core[i];
      float freq = cpu.frequency_per_core[i];

      int filled = (int)((usage / 100.0) * BAR_WIDTH);
      char bar[BAR_WIDTH + 1];
      for (int j = 0; j < BAR_WIDTH; ++j) {
        bar[j] = j < filled ? filled_char : empty_char;
      }
      bar[BAR_WIDTH] = '\0';

      int usage_color = COLOR_PAIR(0);
      if (usage >= KPI_USAGE_CRIT) {
        usage_color = COLOR_PAIR(COLOR_DATA_RED);
      } else if (usage >= KPI_USAGE_WARN) {
        usage_color = COLOR_PAIR(COLOR_DATA_YELLOW);
      } else {
        usage_color = COLOR_PAIR(COLOR_DATA_GREEN);
      }

      if (row < h - 1) {
        mvwprintw(win, row++, line, "C%-2d [", i);
        wattron(win, usage_color);
        wprintw(win, "%s", bar);
        wattroff(win, usage_color);
        wprintw(win, "] %5.1f%%  %4.0f MHz", usage, freq);
      }
    }

    wrefresh(win);
    pthread_mutex_unlock(&ncurses_mutex);
    for (int i = 0; i < 2; i++) {
      if (force_refresh_flags[MODULE_CPUINFO]) {
        force_refresh_flags[MODULE_CPUINFO] = 0;
        break;
      }
      sleep(1);
    }
  }  
  return NULL;
}

void *memory_info_thread(void *arg) {
  WINDOW *win = (WINDOW *)arg;
  wait_until_ready();

  while (1) {
    pthread_mutex_lock(&ncurses_mutex);

    werase(win);

    wattron(win, COLOR_PAIR(COLOR_BORDER));
    box(win, 0, 0);
    wattroff(win, COLOR_PAIR(COLOR_BORDER));

    pthread_mutex_lock(&memory_info_mutex);
    MemoryInfo mem = get_memory_info();
    pthread_mutex_unlock(&memory_info_mutex);

    float total = mem.total_ram / 1024.0f;
    float free = mem.free_ram / 1024.0f;
    float used = mem.used_ram / 1024.0f;
    float swap_used = mem.used_swap / 1024.0f;
    float swap_total = mem.total_swap / 1024.0f;

    int row = 0;
    int line = 3;

    // Headers
    wattron(win, A_BOLD);
    mvwprintw(win, row++, 2, " Memory Usage ");
    wattroff(win, A_BOLD);
    wattron(win, COLOR_PAIR(COLOR_HEADER));
    mvwprintw(win, row++, line, "%-12s  %-10s  %-10s  %-10s", "Type",
              "Total(MiB)", "Used(MiB)", "Free(MiB)");
    wattroff(win, COLOR_PAIR(COLOR_HEADER));
    mvwprintw(win, row++, line, "%-12s  %-10.1f  %-10.1f  %-10.1f", "RAM",
              total, used, free);
    mvwprintw(win, row++, line, "%-12s  %-10.1f  %-10.1f  %-10s", "Swap",
              swap_total, swap_used, "-");

    // Additional percentage
    mvwprintw(win, row++, 2, "RAM used: %.1f%%", mem.mem_percent);

    wrefresh(win);
    pthread_mutex_unlock(&ncurses_mutex);
    for (int i = 0; i < 2; i++) {
      if (force_refresh_flags[MODULE_MEMINFO]) {
        force_refresh_flags[MODULE_MEMINFO] = 0;
        break;
      }
      sleep(1);
    }

  }

  return NULL;
}

void *disk_info_thread(void *arg) {
  WINDOW *win = (WINDOW *)arg;
  wait_until_ready();

  while (1) {
    DiskInfo disks[MAX_DISKS];
    double total_used = 0.0, total_total = 0.0;

    pthread_mutex_lock(&disk_info_mutex);
    int ndisk = get_disk_info(disks, MAX_DISKS, &total_used, &total_total);
    pthread_mutex_unlock(&disk_info_mutex);

    pthread_mutex_lock(&ncurses_mutex);

    werase(win);

    int h, w;
    getmaxyx(win, h, w);

    wattron(win, COLOR_PAIR(COLOR_BORDER));
    box(win, 0, 0);
    wattroff(win, COLOR_PAIR(COLOR_BORDER));

    int row = 0;              // Start after top border
    int col = 2;              // Two-space indent
    int usable_width = w - 5; // 2 spaces + border on each side

    // Title
    wattron(win, A_BOLD);
    mvwprintw(win, row++, col, "%.*s", usable_width, " Disk Information ");
    wattroff(win, A_BOLD);

    // Header
    wattron(win, COLOR_PAIR(COLOR_HEADER));
    mvwprintw(win, row++, col, "%.*s", usable_width,
              "Mount      Filesystem              Used     Total    Usage");
    wattroff(win, COLOR_PAIR(COLOR_HEADER));

    // Disk rows
    for (int i = 0; i < ndisk && row < h - 2; ++i) {
      if (disks[i].total_mb <= 0.0)
        continue;

      char used_buf[16], total_buf[16], line[256];
      format_bytes(disks[i].used_mb, used_buf, sizeof(used_buf));
      format_bytes(disks[i].total_mb, total_buf, sizeof(total_buf));

      snprintf(line, sizeof(line), "%-10.10s %-20.20s %7s %8s  %5.1f%%",
               disks[i].mount_point, disks[i].filesystem, used_buf, total_buf,
               disks[i].usage_percent);

      mvwprintw(win, row++, col, "%.*s", usable_width, line);
    }

    // Totals
    if (row < h - 1 && total_total > 0.0) {
      char used_buf[16], total_buf[16], line[256];
      format_bytes(total_used, used_buf, sizeof(used_buf));
      format_bytes(total_total, total_buf, sizeof(total_buf));
      double usage_percent = (total_used / total_total) * 100.0;

      snprintf(line, sizeof(line), "%-10s %-20s %7s %8s  %5.1f%%", "Total", "-",
               used_buf, total_buf, usage_percent);

      mvwprintw(win, row++, col, "%.*s", usable_width, line);
    }

    wrefresh(win);
    pthread_mutex_unlock(&ncurses_mutex);
    for (int i = 0; i < 10; i++) {
      if (force_refresh_flags[MODULE_DISKINFO]) {
        force_refresh_flags[MODULE_DISKINFO] = 0;
        break;
      }
      sleep(1);
    }
  }
  return NULL;
}

void *process_info_thread(void *arg) {
  ThreadArg *thread_arg = (ThreadArg *)arg;
  int my_index = thread_arg->module_index;
  WINDOW *win = thread_arg->window;
  wait_until_ready();

  while (1) {

    pthread_mutex_lock(&ncurses_mutex);
    int h, w;
    getmaxyx(win, h, w);
    werase(win);

    wattron(win, COLOR_PAIR(COLOR_BORDER));
    box(win, 0, 0);
    wattroff(win, COLOR_PAIR(COLOR_BORDER));

    if (h < 5) {
      mvwprintw(win, 1, 2, "Window too small");
      wrefresh(win);
      pthread_mutex_unlock(&ncurses_mutex);
      sleep(2);
      continue;
    }

    pthread_mutex_unlock(&ncurses_mutex);

    ProcessInfo list[MAX_PROCESSES];
    int count = get_top_processes_by_mem(list, MAX_PROCESSES);

    pthread_mutex_lock(&ncurses_mutex);

    int row = 0;

    wattron(win, A_BOLD);
    mvwprintw(win, row++, 2, " [%d] Processes ", my_index + 1);
    wattroff(win, A_BOLD);

    int compact_mode = 1; // 0 = full, 1 = compact
    int max_rows = h - 2;

    if (compact_mode) {
      const char *header = "  PID    USER        %CPU  %MEM   COMMAND";
      char clipped_header[1024];
      strncpy(clipped_header, header, w - 2);
      clipped_header[w - 2] = '\0';

      wattron(win, COLOR_PAIR(COLOR_HEADER));
      mvwprintw(win, row++, 1, "%s", clipped_header);
      wattroff(win, COLOR_PAIR(COLOR_HEADER));

      for (int i = 0; i < count && row < max_rows; i++) {
        char line[1024];
        snprintf(line, sizeof(line), "%7.7s %-10.10s %5.5s %5.5s %-20.20s",
                 list[i].pid, list[i].user, list[i].cpu, list[i].mem,
                 list[i].command);

        line[w - 2] = '\0';
        mvwprintw(win, row++, 1, "%s", line);
      }

    } else {
      const char *header = "  PID    USER      PR  NI    VIRT    RES      SHR "
                           "S   %%CPU %%MEM   TIME+     COMMAND               ";
      char clipped_header[1024];
      strncpy(clipped_header, header, w - 2);
      clipped_header[w - 2] = '\0';

      wattron(win, COLOR_PAIR(COLOR_HEADER));
      mvwprintw(win, row++, 1, "%s", clipped_header);
      wattroff(win, COLOR_PAIR(COLOR_HEADER));

      for (int i = 0; i < count && row < max_rows; i++) {
        char line[1024];
        snprintf(line, sizeof(line),
                 "%7.7s %-10.10s %2.2s %2.2s %8.8s %7.7s %7.7s %1.1s %5.5s "
                 "%5.5s %10.10s %-20.20s",
                 list[i].pid, list[i].user, list[i].pr, list[i].ni,
                 list[i].virt, list[i].res, list[i].shr, list[i].state,
                 list[i].cpu, list[i].mem, list[i].time, list[i].command);

        line[w - 2] = '\0';
        mvwprintw(win, row++, 1, "%s", line);
      }
    }

    wrefresh(win);
    pthread_mutex_unlock(&ncurses_mutex);
    sleep(2);
    free(arg);
    return NULL;
  }
}

void *connection_info_thread(void *arg) {
  ThreadArg *thread_arg = (ThreadArg *)arg;
  int my_index = thread_arg->module_index;
  WINDOW *win = thread_arg->window;

  free(arg);
  wait_until_ready();

  ConnectionInfo connections[MAX_CONNECTIONS];
  while (1) {
    int nconn = get_connection_info(connections, MAX_CONNECTIONS);

    pthread_mutex_lock(&ncurses_mutex);

    werase(win);

    wattron(win, COLOR_PAIR(COLOR_BORDER));
    box(win, 0, 0);
    wattroff(win, COLOR_PAIR(COLOR_BORDER));

    wattron(win, A_BOLD);
    mvwprintw(win, 0, 2, " [%d] Connections ", my_index + 1);
    wattroff(win, A_BOLD);

    wattron(win, COLOR_PAIR(COLOR_HEADER));
    mvwprintw(win, 1, 2, "%-6s %-22s %-22s %-15s", "Proto", "Local Address",
              "Remote Address", "State");
    wattroff(win, COLOR_PAIR(COLOR_HEADER));

    int y = 2;
    for (int i = 0; i < nconn && y < getmaxy(win) - 1; ++i) {
      mvwprintw(win, y++, 2, "%-6s %-22s %-22s %-15s", connections[i].protocol,
                connections[i].local_addr, connections[i].remote_addr,
                connections[i].state);
    }

    wrefresh(win);
    pthread_mutex_unlock(&ncurses_mutex);
    sleep(2);
    return NULL;
  }
}
void *bandwidth_info_thread(void *arg) {
  ThreadArg *thread_arg = (ThreadArg *)arg;
  int my_index = thread_arg->module_index;
  WINDOW *win = thread_arg->window;

  free(arg);
  wait_until_ready();

  while (1) {
    // Array of connections (max limit)
    BandwidthInfo bandwidths[MAX_BANDWIDTH_CONNECTIONS];
    int nconn = get_bandwidth_info(bandwidths, MAX_BANDWIDTH_CONNECTIONS);

    pthread_mutex_lock(&ncurses_mutex);

    werase(win); // Clear the window

    // Draw the border
    wattron(win, COLOR_PAIR(COLOR_BORDER));
    box(win, 0, 0);
    wattroff(win, COLOR_PAIR(COLOR_BORDER));

    // Print the title
    wattron(win, A_BOLD);
    mvwprintw(win, 0, 2, " [%d] Bandwidth Usage ", my_index + 1);
    wattroff(win, A_BOLD);

    // Print column headers (updated)
    wattron(win, COLOR_PAIR(COLOR_HEADER));
    mvwprintw(win, 1, 2, "%-10s %-6s %-22s %-22s %-5s %-5s", "Proto", "PID",
              "Local", "Remote", "Sent", "Recv");
    wattroff(win, COLOR_PAIR(COLOR_HEADER));

    // Display connection info
    int y = 2;
    for (int i = 0; i < nconn && y < getmaxy(win) - 1; ++i) {
      // Format local address as IP:Port
      char local_info[64];
      snprintf(local_info, sizeof(local_info), "%s:%s", bandwidths[i].laddr,
               bandwidths[i].lport);

      // Format remote address as IP:Port
      char remote_info[64];
      snprintf(remote_info, sizeof(remote_info), "%s:%s", bandwidths[i].raddr,
               bandwidths[i].rport);

      // Display the connection info in the window
      mvwprintw(win, y++, 2, "%-10s %-6s %-22s %-22s %-5lu %-5lu",
                bandwidths[i].proto, bandwidths[i].pid, local_info,
                remote_info, bandwidths[i].sent_kb, bandwidths[i].recv_kb);
    }

    wrefresh(win); // Refresh the window to show new data
    pthread_mutex_unlock(&ncurses_mutex);

    sleep(1); // Sleep for a second before refreshing
  }

  return NULL;
}

void *network_info_thread(void *arg) {
  ThreadArg *thread_arg = (ThreadArg *)arg;
  int my_index = thread_arg->module_index;
  WINDOW *win = thread_arg->window;

  wait_until_ready();

  while (1) {
    int num_interfaces = 0;
    int max_rows, max_cols;

    char **interfaces_usage = get_interfaces_usage(&num_interfaces);

    const char *connected_if = NULL;
    const char *ssid = NULL;
    char *ip = NULL;

    for (int i = 0; i < num_interfaces; i++) {
      char name[32];
      sscanf(interfaces_usage[i], " %31s", name);

      if (strcmp(name, "lo") == 0 || strncmp(name, "br-", 3) == 0 ||
          strncmp(name, "docker", 6) == 0 || strncmp(name, "veth", 4) == 0 ||
          strncmp(name, "virbr", 5) == 0 || strncmp(name, "vmnet", 5) == 0) {
        continue;
      }

      ip = get_ip_address(name);
      if (ip) {
        connected_if = name;
        ssid = get_wifi_ssid(name);
        break;
      }
    }

    const char *vpn_if = NULL;
    const char *vpn_ip = NULL;

    for (int i = 0; i < num_interfaces; i++) {
      char name[32];
      sscanf(interfaces_usage[i], " %31s", name);

      if (is_vpn_interface(name)) {
        char *vip = get_ip_address(name);
        if (vip) {
          vpn_if = name;
          vpn_ip = vip;
          break;
        }
      }
    }

    const char *dns = get_dns_servers();
    char gateway[64] = {0};
    char metric[64] = {0};
    get_default_gateway_and_metric(gateway, metric);

    char *mac = get_mac_address(connected_if);
    WifiInfo wifi = get_wifi_info(connected_if);

    // Lock only for ncurses rendering
    pthread_mutex_lock(&ncurses_mutex);

    getmaxyx(win, max_rows, max_cols);
    (void)max_cols;
    int row = 0;
    int line = 4;
    int max_lines = max_rows - 1;

    werase(win);
    wattron(win, COLOR_PAIR(COLOR_BORDER));
    box(win, 0, 0);
    wattroff(win, COLOR_PAIR(COLOR_BORDER));

    wattron(win, A_BOLD);
    mvwprintw(win, row++, 2, " [%d] Network Information ", my_index + 1);
    wattroff(win, A_BOLD);

    mvwprintw(win, row++, line, "Default Gateway: %s | Metric: %s", gateway,
              metric);
    mvwprintw(win, row++, line, "DNS Servers: %s", dns);
    row++;

    if (connected_if) {
      const char *type = is_wifi_interface(connected_if) ? "Wi-Fi" : "Ethernet";
      if (is_wifi_interface(connected_if) && ssid)
        mvwprintw(win, row++, line, "> Connected: %s (%s: %s)  |  IP: %s",
                  connected_if, type, ssid, ip);
      else
        mvwprintw(win, row++, line, "> Connected: %s (%s)       |  IP: %s",
                  connected_if, type, ip);
    } else {
      mvwprintw(win, row++, line, "No active network connection detected.");
    }

    if (vpn_if && vpn_ip) {
      wattron(win, COLOR_PAIR(COLOR_DATA_RED));
      mvwprintw(win, row++, line, "VPN Active: %s  |  IP: %s", vpn_if, vpn_ip);
      wattroff(win, COLOR_PAIR(COLOR_DATA_RED));
    }

    row++;
    if (ssid) {
      mvwprintw(win, row++, line, "  Wi-Fi: %s", ssid);
      mvwprintw(win, row++, line, "  Signal Strength: %s dBm",
                wifi.signal_strength);
      mvwprintw(win, row++, line, "  Bitrate: %s", wifi.bitrate);
      mvwprintw(win, row++, line, "  Frequency: %s MHz", wifi.freq);
      mvwprintw(win, row++, line, "  MAC Address: %s", mac);
    }

    row++;
    wattron(win, A_BOLD);
    mvwprintw(win, row++, line, "%-15s | %10s | %10s", "Interface", "Sent",
              "Received");
    wattroff(win, A_BOLD);
    mvwprintw(win, row++, line,
              "---------------------------------------------");

    for (int i = 0; i < num_interfaces && row < max_lines; i++) {
      mvwprintw(win, row++, line, "%s", interfaces_usage[i]);
    }

    wrefresh(win);
    pthread_mutex_unlock(&ncurses_mutex);

    free_interfaces_usage(interfaces_usage, num_interfaces);

    sleep(2);
    free(arg);
    return NULL;
  }
}

void *help_info_thread(void *arg) {
  WINDOW *win = (WINDOW *)arg;
  wait_until_ready();

  const char *help_text[] = {
      "[1-3] Switch Panel", "[z] Zoom Focus", "[p] Pause",
      "[s] Sort",           "[r] Refresh",    "[f] Filter",
      "[h] Help",           "[q] Quit",       NULL};

  while (1) {
    pthread_mutex_lock(&ncurses_mutex);

    werase(win);

    wattron(win, COLOR_PAIR(COLOR_BORDER));
    box(win, 0, 0);
    wattroff(win, COLOR_PAIR(COLOR_BORDER));

    int row = 1;
    int title_col = 2;
    int help_start_col = title_col + 20; // after "Trafix - Hotkeys:"

    // Print title
    mvwprintw(win, row, title_col, " Hotkeys:");

    // Define starting column for each column
    int col_spacing = 25; // space between columns
    int col1 = help_start_col;
    int col2 = help_start_col + col_spacing;
    int col3 = help_start_col + 2 * col_spacing;
    int col4 = help_start_col + 3 * col_spacing;

    // First row
    mvwprintw(win, row, col1, "%s", help_text[0]);
    mvwprintw(win, row, col2, "%s", help_text[1]);
    mvwprintw(win, row, col3, "%s", help_text[2]);
    mvwprintw(win, row, col4, "%s", help_text[3]);

    // Second row
    row++;
    mvwprintw(win, row, col1, "%s", help_text[4]);
    mvwprintw(win, row, col2, "%s", help_text[5]);
    mvwprintw(win, row, col3, "%s", help_text[6]);
    mvwprintw(win, row, col4, "%s", help_text[7]);

    wrefresh(win);

    pthread_mutex_unlock(&ncurses_mutex);

    sleep(1000);
  }
  return NULL;
}

void refresh_all_windows(WINDOW *sys_win, WINDOW *cpu_win, WINDOW *mem_win, WINDOW *disk_win) {
  pthread_mutex_lock(&ncurses_mutex);

  werase(sys_win);
  werase(cpu_win);
  werase(mem_win);
  werase(disk_win);

  box(sys_win, 0, 0);
  box(cpu_win, 0, 0);
  box(mem_win, 0, 0);
  box(disk_win, 0, 0);

  wrefresh(sys_win);
  wrefresh(cpu_win);
  wrefresh(mem_win);
  wrefresh(disk_win);

  pthread_mutex_unlock(&ncurses_mutex);
}

int select_module() {
  screen_paused = 1; // <--- PAUSE background threads

  const char *module_names[] = {" Connections ", " Network Information ",
                                " Processes ", " Bandwidths ", };
  const char module_keys[] = {'C', 'N', 'P', 'B'};
  int module_count = sizeof(module_names) / sizeof(module_names[0]);

  int screen_height, screen_width;
  getmaxyx(stdscr, screen_height, screen_width);

  int popup_height = module_count + 4;
  int popup_width = 40;
  int popup_y = (screen_height - popup_height) / 2;
  int popup_x = (screen_width - popup_width) / 2;

  WINDOW *popup = newwin(popup_height, popup_width, popup_y, popup_x);
  if (!popup) {
    screen_paused = 0; // resume if failed
    return -1;
  }

  pthread_mutex_lock(&ncurses_mutex);
  wattron(popup, COLOR_PAIR(COLOR_BORDER));
  box(popup, 0, 0);
  wattroff(popup, COLOR_PAIR(COLOR_BORDER));

  mvwprintw(popup, 1, 2, "Select a module:");
  for (int i = 0; i < module_count; i++) {
    mvwprintw(popup, i + 2, 4, "[%c] %s", module_keys[i], module_names[i]);
  }
  wrefresh(popup);
  pthread_mutex_unlock(&ncurses_mutex);

  int ch;
  int selected = -1;
  while (1) {
    ch = getch();
    ch = toupper(ch); // Accept lower or upper case
    for (int i = 0; i < module_count; i++) {
      if (ch == module_keys[i]) {
        selected = i;
        goto done;
      }
    }
    if (ch == 27) { // ESC
      selected = -1;
      break;
    }
  }

done:
  pthread_mutex_lock(&ncurses_mutex);
  werase(popup);
  wrefresh(popup);
  delwin(popup);
  pthread_mutex_unlock(&ncurses_mutex);

  screen_paused = 0; // <--- RESUME background threads
  return selected;
}

void change_window_module(int slot_idx) {
  if (row2_slots[slot_idx].module_index != -1) {
    pthread_cancel(row2_slots[slot_idx].thread_id);
    pthread_join(row2_slots[slot_idx].thread_id, NULL);
    row2_slots[slot_idx].module_index = -1;
  }

  int selected_module = select_module();
  if (selected_module == -1) {
    return; // User canceled
  }

  row2_slots[slot_idx].module_index = selected_module;

  ThreadArg *arg = malloc(sizeof(ThreadArg));
  arg->module_index = selected_module;
  arg->window = row2_slots[slot_idx].window;

  pthread_create(&row2_slots[slot_idx].thread_id, NULL,
                 modules[selected_module].thread_func, arg);
}

void handle_keypress(int ch, int screen_height, int screen_width,
                     WINDOW *sys_win, WINDOW *cpu_win, WINDOW *mem_win,
                     WINDOW *disk_win, WINDOW *conn_win, WINDOW *net_win,
                     WINDOW *proc_win) {
  Hotkey hotkeys[] = {
      {'1', "Switch Panel 1"},       {'2', "Switch Panel 2"},
      {'3', "Switch Panel 3"},       {'z', "Zoom Focus"},
      {'Z', "Zoom Focus"},           {'h', "Show/Hide Help"},
      {'H', "Show/Hide Help"},       {'r', "Refresh Now"},
      {'R', "Refresh Now"},          {'f', "Filter Connections"},
      {'F', "Filter Connections"},   {'s', "Sort Processes"},
      {'S', "Sort Processes"},       {'p', "Pause/Resume Updates"},
      {'P', "Pause/Resume Updates"}, {0, NULL}};

  if (ch == 'r' || ch == 'R') {
    for (int i = 0; i < NUM_MODULES; i++) {
      force_refresh_flags[i] = 1;
    }
    refresh_all_windows(sys_win, cpu_win, mem_win, disk_win);    
    return;
  }

  if (ch == '1' || ch == '2' || ch == '3') {
    int slot_idx = ch - '1';
    change_window_module(slot_idx);
    return;
  }

  for (int i = 0; hotkeys[i].key != 0; i++) {
    if (ch == hotkeys[i].key) {
      int popup_height = 5;
      int popup_width = 50;
      int popup_y = (screen_height - popup_height) / 2;
      int popup_x = (screen_width - popup_width) / 2;

      WINDOW *popup = newwin(popup_height, popup_width, popup_y, popup_x);
      if (!popup)
        return;

      pthread_mutex_lock(&ncurses_mutex);
      wattron(popup, COLOR_PAIR(COLOR_BORDER));
      box(popup, 0, 0);
      wattroff(popup, COLOR_PAIR(COLOR_BORDER));
      mvwprintw(popup, 1, 2, "Hotkey Pressed:");
      mvwprintw(popup, 2, 2, "[%c] %s", ch, hotkeys[i].description);
      wrefresh(popup);
      pthread_mutex_unlock(&ncurses_mutex);

      napms(1000);

      pthread_mutex_lock(&ncurses_mutex);
      werase(popup);
      wrefresh(popup);
      delwin(popup);
      pthread_mutex_unlock(&ncurses_mutex);

      break;
    }
  }
}

void start_dashboard() { 
  initscr();
  noecho();
  curs_set(FALSE);
  mousemask(0, NULL);
  start_color();
  use_default_colors();
  init_pair(COLOR_HEADER, COLOR_CYAN, -1);
  init_pair(COLOR_DATA_GREEN, COLOR_GREEN, -1);
  init_pair(COLOR_DATA_RED, COLOR_RED, -1);
  init_pair(COLOR_DATA_YELLOW, COLOR_YELLOW, -1);
  init_pair(COLOR_BORDER, COLOR_CYAN, -1);

  int screen_height, screen_width;
  getmaxyx(stdscr, screen_height, screen_width);

  // Fixed heights for each row
  const int row1_height = 11;
  const int row3_height = 4;
  const int row2_height = screen_height - row1_height - row3_height;

  // Row 1 widths (4 columns)
  int row1_widths[4] = {(int)(screen_width * 0.25), (int)(screen_width * 0.20),
                        (int)(screen_width * 0.25),
                        screen_width - ((int)(screen_width * 0.25) +
                                        (int)(screen_width * 0.20) +
                                        (int)(screen_width * 0.25))};

  // Row 2 widths (3 columns)
  int row2_widths[3] = {
      screen_width / 3, screen_width / 3,
      screen_width - ((int)(screen_width / 3) + (int)(screen_width / 3))};

  // Row 3 widths (1 column — full width)
  int row3_widths[1] = {screen_width};

  // Y offsets
  int row1_y = 0;
  int row2_y = row1_height;
  int row3_y = row1_height + row2_height;

  // Create row 1 windows
  WINDOW *sys_win = newwin(row1_height, row1_widths[0], row1_y, 0);
  WINDOW *cpu_win = newwin(row1_height, row1_widths[1], row1_y, row1_widths[0]);
  WINDOW *mem_win = newwin(row1_height, row1_widths[2], row1_y,
                           row1_widths[0] + row1_widths[1]);
  WINDOW *disk_win = newwin(row1_height, row1_widths[3], row1_y,
                            row1_widths[0] + row1_widths[1] + row1_widths[2]);

  // Create row 2 windows
  WINDOW *conn_win = newwin(row2_height, row2_widths[0], row2_y, 0);
  WINDOW *net_win = newwin(row2_height, row2_widths[1], row2_y, row2_widths[0]);
  WINDOW *proc_win = newwin(row2_height, row2_widths[2], row2_y,
                            row2_widths[0] + row2_widths[1]);

  // Create row 3 window (help box)
  WINDOW *help_win = newwin(row3_height, row3_widths[0], row3_y, 0);

  // Launch threads
  pthread_t sys_tid, cpu_tid, mem_tid, disk_tid;
  // pthread_t net_tid, conn_tid, proc_tid;
  pthread_t help_tid;

  pthread_create(&sys_tid, NULL, system_info_thread, sys_win);
  pthread_create(&cpu_tid, NULL, cpu_info_thread, cpu_win);
  pthread_create(&mem_tid, NULL, memory_info_thread, mem_win);
  pthread_create(&disk_tid, NULL, disk_info_thread, disk_win);

  // pthread_create(&conn_tid, NULL, connection_info_thread, conn_win);
  // pthread_create(&net_tid, NULL, network_info_thread, net_win);
  // pthread_create(&proc_tid, NULL, process_info_thread, proc_win);
  row2_slots[0].window = conn_win;
  row2_slots[1].window = net_win;
  row2_slots[2].window = proc_win;

  for (int i = 0; i < 3; i++) {
    row2_slots[i].module_index = i; // Default to Connection, Network, Process

    ThreadArg *arg = malloc(sizeof(ThreadArg));
    arg->module_index = i;
    arg->window = row2_slots[i].window;

    pthread_create(&row2_slots[i].thread_id, NULL, modules[i].thread_func,
                   arg); // <-- pass the `arg`, not `window`
  }
  //  Launch the help thread
  pthread_create(&help_tid, NULL, help_info_thread, help_win);

  sleep(1);
  ready = 1;

  int ch;
  while ((ch = getch()) != 'q' && ch != 'Q') {
    /*for (int i = 0; i < NUM_MODULES; i++) {
      force_refresh_flags[i] = 1;
      }*/
    handle_keypress(ch, screen_height, screen_width, sys_win, cpu_win, mem_win,
                    disk_win, conn_win, net_win, proc_win);
  }
  endwin();
}
