
/*
 * Copyright (C) 2025 Masoud Bolhassani <masoud.bolhassani@gmail.com>
 *
 * This file is part of Trafix.
 *
 * Trafix is released under the GNU General Public License v3 (GPL-3.0).
 * See LICENSE file for details.
 */

#include <unistd.h>
#include <ncurses.h>
#include <stdlib.h>

#include "trfx_threads.h"
#include "trfx_globals.h"
#include "trfx_utils.h"

#include "trfx_sysinfo.h"
#include "trfx_meminfo.h"
#include "trfx_disk.h"
#include "trfx_cpu.h"

#include "trfx_procinfo.h"
#include "trfx_connections.h"
#include "trfx_bandwidth.h"
#include "trfx_netinfo.h"
#include "trfx_wifi.h"

SortType current_sort_type = SORT_BY_MEM;

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
    if (screen_paused) {
      usleep(100000);
      continue;
    }

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
      if (force_refresh_flags[STATIC_MODULE_SYSINFO]) {
        force_refresh_flags[STATIC_MODULE_SYSINFO] = 0;
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

  extern int TEMP_WARN_YELLOW;
  extern int TEMP_WARN_RED;

  while (1) {
    if (screen_paused) {
      usleep(100000);
      continue;
    }

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
    if (cpu.temperature >= TEMP_WARN_RED) {
      temp_color = COLOR_DATA_RED;
    } else if (cpu.temperature >= TEMP_WARN_YELLOW) {
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

      int filled = (int)((usage / 100.0) * CPU_BAR_WIDTH);
      char bar[CPU_BAR_WIDTH + 1];
      for (int j = 0; j < CPU_BAR_WIDTH; ++j) {
        bar[j] = j < filled ? filled_char : empty_char;
      }
      bar[CPU_BAR_WIDTH] = '\0';

      int usage_color = COLOR_PAIR(0);
      if (usage >= CPU_USAGE_CRIT) {
        usage_color = COLOR_PAIR(COLOR_DATA_RED);
      } else if (usage >= CPU_USAGE_WARN) {
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
      if (force_refresh_flags[STATIC_MODULE_CPUINFO]) {
        force_refresh_flags[STATIC_MODULE_CPUINFO] = 0;
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
    if (screen_paused) {
      usleep(100000);
      continue;
    }

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
    mvwprintw(win, row++, line, "%-10s %10s %10s %10s", "Type", "Total", "Used",
              "Free");
    wattroff(win, COLOR_PAIR(COLOR_HEADER));

    // Format memory values
    char total_buf[16], used_buf[16], free_buf[16];
    format_bytes(total, total_buf, sizeof(total_buf));
    format_bytes(used, used_buf, sizeof(used_buf));
    format_bytes(free, free_buf, sizeof(free_buf));

    // RAM row
    mvwprintw(win, row++, line, "%-10s %10s %10s %10s", "RAM", total_buf,
              used_buf, free_buf);

    // Swap row
    char swap_total_buf[16], swap_used_buf[16];
    format_bytes(swap_total, swap_total_buf, sizeof(swap_total_buf));
    format_bytes(swap_used, swap_used_buf, sizeof(swap_used_buf));
    mvwprintw(win, row++, line, "%-10s %10s %10s %10s", "Swap", swap_total_buf,
              swap_used_buf, "-");

    // RAM usage percent
    mvwprintw(win, row++, 2, "RAM used: %.1f%%", mem.mem_percent);

    wrefresh(win);
    pthread_mutex_unlock(&ncurses_mutex);
    for (int i = 0; i < 2; i++) {
      if (force_refresh_flags[STATIC_MODULE_MEMINFO]) {
        force_refresh_flags[STATIC_MODULE_MEMINFO] = 0;
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
    if (screen_paused) {
      usleep(100000);
      continue;
    }

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
              "Mount      Filesystem                  Total    Used    Usage");
    wattroff(win, COLOR_PAIR(COLOR_HEADER));

    // Disk rows
    for (int i = 0; i < ndisk && row < h - 2; ++i) {
      if (disks[i].total_mb <= 0.0)
        continue;

      char used_buf[16], total_buf[16], line[256];
      format_bytes(disks[i].used_mb, used_buf, sizeof(used_buf));
      format_bytes(disks[i].total_mb, total_buf, sizeof(total_buf));

      snprintf(line, sizeof(line), "%-10.10s %-24.24s %8s %8s  %5.1f%%",
               disks[i].mount_point, disks[i].filesystem, total_buf, used_buf,
               disks[i].usage_percent);

      mvwprintw(win, row++, col, "%.*s", usable_width, line);
    }

    // Totals
    if (row < h - 1 && total_total > 0.0) {
      char used_buf[16], total_buf[16], line[256];
      format_bytes(total_total, total_buf, sizeof(total_buf));
      format_bytes(total_used, used_buf, sizeof(used_buf));
      double usage_percent = (total_used / total_total) * 100.0;

      snprintf(line, sizeof(line), "%-10s %-24s %8s %8s  %5.1f%%", "Total", "-",
               total_buf, used_buf, usage_percent);

      mvwprintw(win, row++, col, "%.*s", usable_width, line);
    }

    wrefresh(win);
    pthread_mutex_unlock(&ncurses_mutex);
    for (int i = 0; i < 10; i++) {
      if (force_refresh_flags[STATIC_MODULE_DISKINFO]) {
        force_refresh_flags[STATIC_MODULE_DISKINFO] = 0;
        break;
      }
      sleep(1);
    }
  }
  return NULL;
}

/*
  dynamic modules
*/
void *process_info_thread(void *arg) {
  ThreadArg *thread_arg = (ThreadArg *)arg;
  int my_index = thread_arg->module_index;
  WINDOW *win = thread_arg->window;

  free(arg);
  wait_until_ready();

  while (1) {

    if (screen_paused) {
      usleep(100000);
      continue;
    }

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
    int count = get_top_processes(list, MAX_PROCESSES, current_sort_type);

    pthread_mutex_lock(&ncurses_mutex);

    int row = 0;

    wattron(win, A_BOLD);
    mvwprintw(win, row++, 2, " [%d] Processes ", my_index + 1);
    wattroff(win, A_BOLD);


    int max_rows = h - 2;
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
               list[i].pid, list[i].user, list[i].pr, list[i].ni, list[i].virt,
               list[i].res, list[i].shr, list[i].state, list[i].cpu,
               list[i].mem, list[i].time, list[i].command);

      line[w - 2] = '\0';
      mvwprintw(win, row++, 1, "%s", line);
    }

    wrefresh(win);
    pthread_mutex_unlock(&ncurses_mutex);
    sleep(1);
  }
  return NULL;
}

void *process_compact_info_thread(void *arg) {
  ThreadArg *thread_arg = (ThreadArg *)arg;
  int my_index = thread_arg->module_index;
  WINDOW *win = thread_arg->window;

  free(arg);
  wait_until_ready();

  while (1) {

    if (screen_paused) {
      usleep(100000);
      continue;
    }

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
    int count = get_top_processes(list, MAX_PROCESSES, current_sort_type);

    pthread_mutex_lock(&ncurses_mutex);

    int row = 0;

    wattron(win, A_BOLD);
    mvwprintw(win, row++, 2, " [%d] Processes ", my_index + 1);
    wattroff(win, A_BOLD);

    int max_rows = h - 2;

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

    wrefresh(win);
    pthread_mutex_unlock(&ncurses_mutex);
    sleep(1);
  }
  return NULL;
}

void *connection_info_thread(void *arg) {
  ThreadArg *thread_arg = (ThreadArg *)arg;
  int my_index = thread_arg->module_index;
  WINDOW *win = thread_arg->window;

  free(arg);
  wait_until_ready();

  while (1) {

    if (screen_paused) {
      usleep(100000);
      continue;
    }

    ConnectionInfo connections[MAX_CONNECTIONS];
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
    sleep(1);
  }
  return NULL;
}

void *bandwidth_info_thread(void *arg) {
  ThreadArg *thread_arg = (ThreadArg *)arg;
  int my_index = thread_arg->module_index;
  WINDOW *win = thread_arg->window;

  free(arg);
  wait_until_ready();

  while (1) {

    if (screen_paused) {
      usleep(100000);
      continue;
    }

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

  free(arg);
  wait_until_ready();

  while (1) {

    if (screen_paused) {
      usleep(100000);
      continue;
    }

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

    sleep(1);
  }
  return NULL;

}

/*
  Help
*/
void *help_info_thread(void *arg) {
  WINDOW *win = (WINDOW *)arg;
  wait_until_ready();

  /*const char *help_text[] = {
    "[1-3] Switch Panel", "[z] Zoom Focus", "[p] Pause",
      "[s] Sort",           "[r] Refresh",    "[f] Filter",
      "[h] Help",           "[q] Quit",       NULL};*/
  
  const char *help_text[] = {
    "[1-3] Switch Panel",
    "[s] Sort Processes",
    "[r] Refresh",
    "[c] Columns",
    "[p] Pause",
    "[q] Quit",
    "edit /etc/trafix/config.cfg to customize all settings.",
    NULL};
  
  while (1) {
    if (screen_paused) {
      usleep(100000);
      continue;
    }

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
    int col4 = help_start_col + 5 * col_spacing;

    // First row
    mvwprintw(win, row, col1, "%s", help_text[0]);
    mvwprintw(win, row, col2, "%s", help_text[1]);
    mvwprintw(win, row, col3, "%s", help_text[2]);
    
    // Second row
    row++;    
    mvwprintw(win, row, col1, "%s", help_text[3]);
    mvwprintw(win, row, col2, "%s", help_text[4]);
    mvwprintw(win, row, col3, "%s", help_text[5]);
    mvwprintw(win, row, col4, "%s", help_text[6]);

    wrefresh(win);

    pthread_mutex_unlock(&ncurses_mutex);

    sleep(1000);
  }
  return NULL;
}
