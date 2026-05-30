
/*
 * Copyright (C) 2025 Masoud Bolhassani <masoud.bolhassani@gmail.com>
 *
 * This file is part of Trafix.
 *
 * Trafix is released under the GNU General Public License v3 (GPL-3.0).
 * See LICENSE file for details.
 */

#include <unistd.h>
#include <time.h>
#include <ncurses.h>
#include <stdbool.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include "trfx_threads.h"
#include "trfx_config.h"
#include "trfx_bandwidth.h"
#include "trfx_globals.h"
#include "trfx_runtime.h"
#include "trfx_utils.h"

#include "trfx_sysinfo.h"
#include "trfx_meminfo.h"
#include "trfx_disk.h"
#include "trfx_cpu.h"

#include "trfx_procinfo.h"
#include "trfx_connections.h"
#include "trfx_socket_owners.h"
#include "trfx_netinfo.h"
#include "trfx_wifi.h"

SortType current_sort_type = SORT_BY_MEM;

static pthread_mutex_t bandwidth_state_mutex = PTHREAD_MUTEX_INITIALIZER;
static TrfxNetworkSampleBuffer bandwidth_state_samples;
static TrfxBandwidthReport bandwidth_state_report;
static int bandwidth_state_initialized = 0;
static int bandwidth_focus_index = 0;

static int panel_has_room(int row, int max_lines) {
  return row < max_lines;
}

static void trfx_format_endpoint_for_tui(const char *value, char *buf,
                                         size_t bufsize) {
  if (!buf || bufsize == 0)
    return;

  if (!value) {
    snprintf(buf, bufsize, "-");
    return;
  }

  size_t len = strlen(value);
  if (len < bufsize) {
    snprintf(buf, bufsize, "%s", value);
    return;
  }

  if (bufsize <= 4) {
    snprintf(buf, bufsize, "%.*s", (int)(bufsize - 1), value);
    return;
  }

  snprintf(buf, bufsize, "%.*s...", (int)(bufsize - 4), value);
}

static int trfx_thread_sleep_ms(int milliseconds) {
  const int step_ms = 100;
  int elapsed = 0;

  while (!trfx_runtime_should_stop() && elapsed < milliseconds) {
    int remaining = milliseconds - elapsed;
    int sleep_ms = remaining < step_ms ? remaining : step_ms;
    usleep((useconds_t)sleep_ms * 1000);
    elapsed += sleep_ms;
  }

  return trfx_runtime_should_stop();
}

static int trfx_thread_should_stop(const volatile int *local_stop) {
  return trfx_runtime_should_stop() || (local_stop && *local_stop);
}

static int trfx_dynamic_thread_sleep_ms(const volatile int *local_stop,
                                        int milliseconds) {
  const int step_ms = 25;
  int elapsed = 0;

  while (!trfx_thread_should_stop(local_stop) && elapsed < milliseconds) {
    int remaining = milliseconds - elapsed;
    int sleep_ms = remaining < step_ms ? remaining : step_ms;
    usleep((useconds_t)sleep_ms * 1000);
    elapsed += sleep_ms;
  }

  return trfx_thread_should_stop(local_stop);
}

static int trfx_wait_for_static_refresh(int module_index, int milliseconds) {
  const int step_ms = 25;
  int elapsed = 0;

  while (!trfx_runtime_should_stop() && elapsed < milliseconds) {
    if (trfx_runtime_consume_static_refresh(module_index))
      return 1;

    int remaining = milliseconds - elapsed;
    int sleep_ms = remaining < step_ms ? remaining : step_ms;
    usleep((useconds_t)sleep_ms * 1000);
    elapsed += sleep_ms;
  }

  return trfx_runtime_consume_static_refresh(module_index);
}

static void format_connection_row(const ConnectionInfo *connection,
                                  int panel_width, char *line,
                                  size_t line_size) {
  if (!connection || !line || line_size == 0)
    return;

  char local[64];
  char remote[64];
  int inner_width = panel_width > 4 ? panel_width - 4 : panel_width;
  int endpoint_width = inner_width >= 100 ? 22 : inner_width >= 80 ? 18 : 14;
  int user_width = inner_width >= 100 ? 10 : inner_width >= 80 ? 9 : 8;
  int process_width = inner_width >= 100 ? 16 : inner_width >= 80 ? 14 : 12;

  trfx_format_endpoint_for_tui(connection->local_addr, local, sizeof(local));
  trfx_format_endpoint_for_tui(connection->remote_addr, remote, sizeof(remote));

  snprintf(line, line_size,
           "%-6.6s %-*s %-*s %-13.13s %-7u %-*.*s %-7.7s %-*.*s",
           connection->protocol, endpoint_width, local, endpoint_width, remote,
           connection->state, connection->uid, user_width, user_width,
           connection->user, connection->pid, process_width, process_width,
           connection->process);
}

static int connection_state_matches(const ConnectionInfo *connection,
                                    const char *state) {
  if (!connection || !state)
    return 0;

  return strcmp(connection->state, state) == 0;
}

static void format_connection_summary(const TrfxNetworkSnapshot *snapshot,
                                      char *line, size_t line_size) {
  int tcp_count = 0;
  int udp_count = 0;
  int established_count = 0;
  int listen_count = 0;
  int owned_count = 0;

  if (!snapshot || !line || line_size == 0)
    return;

  for (int i = 0; i < snapshot->connection_count; i++) {
    const ConnectionInfo *connection = &snapshot->connections[i];

    if (strcmp(connection->protocol, "TCP") == 0)
      tcp_count++;
    else if (strcmp(connection->protocol, "UDP") == 0)
      udp_count++;

    if (connection_state_matches(connection, "ESTABLISHED"))
      established_count++;
    else if (connection_state_matches(connection, "LISTEN") ||
             connection_state_matches(connection, "UNCONN"))
      listen_count++;

    if (strcmp(connection->pid, "-") != 0 ||
        strcmp(connection->process, "-") != 0)
      owned_count++;
  }

  if (snapshot->connection_count == 0) {
    snprintf(line, line_size, "Summary: no visible connections");
    return;
  }

  snprintf(line, line_size,
           "Summary: TCP %d | UDP %d | Established %d | Listen/Unconn %d | Owned %d",
           tcp_count, udp_count, established_count, listen_count,
           owned_count);
}

static int connection_has_owner(const ConnectionInfo *connection) {
  if (!connection)
    return 0;

  return strcmp(connection->pid, "-") != 0 ||
         strcmp(connection->process, "-") != 0;
}

static void format_socket_inventory_line(const ConnectionInfo *connection,
                                         int panel_width, char *line,
                                         size_t line_size) {
  if (!connection || !line || line_size == 0)
    return;

  char local[64];
  char remote[64];
  int inner_width = panel_width > 4 ? panel_width - 4 : panel_width;
  int endpoint_width = inner_width >= 90 ? 22 : inner_width >= 70 ? 18 : 12;
  int process_width = inner_width >= 90 ? 16 : inner_width >= 70 ? 14 : 12;

  trfx_format_endpoint_for_tui(connection->local_addr, local, sizeof(local));
  trfx_format_endpoint_for_tui(connection->remote_addr, remote, sizeof(remote));

  snprintf(line, line_size, "%-6.6s %-7u %-7.7s %-*.*s %-*.*s %-*.*s",
           connection->protocol, connection->uid, connection->pid,
           process_width, process_width, connection->process, endpoint_width,
           endpoint_width, local, endpoint_width, endpoint_width, remote);
}

static void format_network_route_line(const TrfxNetworkSnapshot *snapshot,
                                      char *line, size_t line_size) {
  if (!snapshot || !line || line_size == 0)
    return;

  if (snapshot->route_status == TRFX_COLLECTOR_OK &&
      snapshot->route.has_default) {
    snprintf(line, line_size, "Route: default via %s dev %s metric %s",
             snapshot->route.gateway, snapshot->route.interface,
             snapshot->route.metric);
    return;
  }

  snprintf(line, line_size, "Route: unavailable");
}

static void format_network_dns_line(const TrfxNetworkSnapshot *snapshot,
                                    char *line, size_t line_size) {
  if (!snapshot || !line || line_size == 0)
    return;

  if (snapshot->dns_status == TRFX_COLLECTOR_OK && snapshot->dns.count > 0) {
    char dns[256];
    dns[0] = '\0';
    for (int i = 0; i < snapshot->dns.count; i++) {
      if (i > 0)
        strncat(dns, ", ", sizeof(dns) - strlen(dns) - 1);
      strncat(dns, snapshot->dns.servers[i],
              sizeof(dns) - strlen(dns) - 1);
    }
    snprintf(line, line_size, "DNS: %s", dns);
    return;
  }

  snprintf(line, line_size, "DNS: unavailable");
}

static void format_network_active_line(const TrfxNetworkSnapshot *snapshot,
                                       char *line, size_t line_size) {
  if (!snapshot || !line || line_size == 0)
    return;

  if (!snapshot->has_active_interface) {
    snprintf(line, line_size, "Active: no interface with an IPv4 address");
    return;
  }

  if (snapshot->active_ssid[0] != '\0' && strcmp(snapshot->active_ssid, "N/A") != 0) {
    snprintf(line, line_size,
             "Active: %s (%s) | IP: %s | SSID: %s | MAC: %s",
             snapshot->active_interface, snapshot->active_type,
             snapshot->active_ip, snapshot->active_ssid,
             snapshot->active_mac[0] ? snapshot->active_mac : "N/A");
    return;
  }

  snprintf(line, line_size, "Active: %s (%s) | IP: %s | MAC: %s",
           snapshot->active_interface, snapshot->active_type,
           snapshot->active_ip, snapshot->active_mac[0] ? snapshot->active_mac
                                                        : "N/A");
}

static void format_network_vpn_line(const TrfxNetworkSnapshot *snapshot,
                                    char *line, size_t line_size) {
  if (!snapshot || !line || line_size == 0)
    return;

  if (!snapshot->has_vpn_interface) {
    snprintf(line, line_size, "VPN: none detected");
    return;
  }

  snprintf(line, line_size, "VPN: %s | IP: %s", snapshot->vpn_interface,
           snapshot->vpn_ip);
}

static void render_network_summary(WINDOW *win,
                                   const TrfxNetworkSnapshot *snapshot,
                                   int *row, int line, int max_lines) {
  char summary[512];

  if (!win || !snapshot || !row)
    return;

  format_network_route_line(snapshot, summary, sizeof(summary));
  if (panel_has_room(*row, max_lines))
    trfx_print_clipped(win, (*row)++, line, summary);

  format_network_dns_line(snapshot, summary, sizeof(summary));
  if (panel_has_room(*row, max_lines))
    trfx_print_clipped(win, (*row)++, line, summary);

  format_network_active_line(snapshot, summary, sizeof(summary));
  if (panel_has_room(*row, max_lines))
    trfx_print_clipped(win, (*row)++, line, summary);

  format_network_vpn_line(snapshot, summary, sizeof(summary));
  if (panel_has_room(*row, max_lines))
    trfx_print_clipped(win, (*row)++, line, summary);
}

static void render_bandwidth_talkers_summary(
    WINDOW *win, const TrfxBandwidthReport *report, int *row, int line,
    int max_lines) {
  char summary[256];
  int selected_index;

  if (!win || !report || !row)
    return;

  pthread_mutex_lock(&bandwidth_state_mutex);
  selected_index = bandwidth_focus_index;
  pthread_mutex_unlock(&bandwidth_state_mutex);

  if (!panel_has_room(*row, max_lines))
    return;

  mvwprintw(win, (*row)++, line, "Top talkers (%s):",
            trfx_bandwidth_mode_name(report->mode));

  if (report->mode == TRFX_BW_MODE_UNSUPPORTED ||
      report->flow_count == 0) {
    snprintf(summary, sizeof(summary), "Bandwidth: %s", report->source);
    if (panel_has_room(*row, max_lines))
      trfx_print_clipped(win, (*row)++, line, summary);
    return;
  }

  for (int i = 0; i < report->flow_count && i < 3 && panel_has_room(*row, max_lines);
       i++) {
    const TrfxBandwidthFlow *flow = &report->flows[i];
    char rx[32];
    char tx[32];
    char linebuf[256];

    trfx_format_net_bytes(flow->rx_bytes_per_sec, rx, sizeof(rx));
    trfx_format_net_bytes(flow->tx_bytes_per_sec, tx, sizeof(tx));

    snprintf(linebuf, sizeof(linebuf), "%c %s %s [%s] %s -> %s | rx %s/s tx %s/s",
             i == selected_index ? '>' : ' ', flow->pid[0] ? flow->pid : "-",
             flow->process[0] ? flow->process : "-",
             flow->detail[0] ? flow->detail : flow->label,
             flow->local[0] ? flow->local : "-", flow->remote[0] ? flow->remote
                                                                   : "-",
             rx, tx);
    trfx_print_clipped(win, (*row)++, line, linebuf);
  }
}

static void format_bandwidth_history_time(time_t value, char *buf,
                                          size_t buf_size) {
  struct tm tm_value;

  if (!buf || buf_size == 0)
    return;

  if (localtime_r(&value, &tm_value) == NULL) {
    snprintf(buf, buf_size, "unknown");
    return;
  }

  strftime(buf, buf_size, "%H:%M:%S", &tm_value);
}

static void render_bandwidth_history_summary(WINDOW *win,
                                             const TrfxBandwidthTrend *trend,
                                             int *row, int line,
                                             int max_lines) {
  char history_line[256];

  if (!win || !trend || !row)
    return;

  if (!panel_has_room(*row, max_lines))
    return;

  mvwprintw(win, (*row)++, line, "Recent trend:");

  if (trend->point_count <= 0) {
    snprintf(history_line, sizeof(history_line), "Trend: %s", trend->source);
    if (panel_has_room(*row, max_lines))
      trfx_print_clipped(win, (*row)++, line, history_line);
    return;
  }

  for (int i = 0; i < trend->point_count && panel_has_room(*row, max_lines);
       i++) {
    char rx[32];
    char tx[32];
    char time_line[32];

    trfx_format_net_bytes(trend->rx_bytes_per_sec[i], rx, sizeof(rx));
    trfx_format_net_bytes(trend->tx_bytes_per_sec[i], tx, sizeof(tx));
    format_bandwidth_history_time(trend->captured_at[i], time_line,
                                  sizeof(time_line));

    snprintf(history_line, sizeof(history_line), "%s | rx %s/s tx %s/s",
             time_line, rx, tx);
    trfx_print_clipped(win, (*row)++, line, history_line);
  }
}

void trfx_bandwidth_state_init(void) {
  pthread_mutex_lock(&bandwidth_state_mutex);
  if (!bandwidth_state_initialized) {
    trfx_init_network_sample_buffer(&bandwidth_state_samples);
    trfx_init_bandwidth_report(&bandwidth_state_report);
    bandwidth_state_initialized = 1;
  }
  pthread_mutex_unlock(&bandwidth_state_mutex);
}

static void bandwidth_state_update(const TrfxNetworkSampleBuffer *samples,
                                   const TrfxBandwidthReport *report) {
  int visible_count;

  pthread_mutex_lock(&bandwidth_state_mutex);
  if (samples)
    bandwidth_state_samples = *samples;
  if (report)
    bandwidth_state_report = *report;
  visible_count = bandwidth_state_report.flow_count < 3
                      ? bandwidth_state_report.flow_count
                      : 3;
  if (visible_count <= 0) {
    bandwidth_focus_index = 0;
  } else if (bandwidth_focus_index >= visible_count) {
    bandwidth_focus_index = visible_count - 1;
  }
  pthread_mutex_unlock(&bandwidth_state_mutex);
}

int trfx_bandwidth_state_copy(TrfxNetworkSampleBuffer *samples,
                              TrfxBandwidthReport *report, int *focus_index) {
  int available = 0;

  pthread_mutex_lock(&bandwidth_state_mutex);
  if (bandwidth_state_initialized) {
    if (samples)
      *samples = bandwidth_state_samples;
    if (report)
      *report = bandwidth_state_report;
    if (focus_index)
      *focus_index = bandwidth_focus_index;
    available = 1;
  }
  pthread_mutex_unlock(&bandwidth_state_mutex);

  return available;
}

void trfx_bandwidth_state_move_focus(int delta) {
  int visible_count;

  pthread_mutex_lock(&bandwidth_state_mutex);
  visible_count = bandwidth_state_report.flow_count < 3
                      ? bandwidth_state_report.flow_count
                      : 3;
  if (visible_count > 0) {
    bandwidth_focus_index += delta;
    if (bandwidth_focus_index < 0)
      bandwidth_focus_index = visible_count - 1;
    else if (bandwidth_focus_index >= visible_count)
      bandwidth_focus_index = 0;
  }
  pthread_mutex_unlock(&bandwidth_state_mutex);
}

void wait_until_ready() {
  while (!trfx_runtime_is_ready() && !trfx_runtime_should_stop())
    usleep((useconds_t)TUI_READY_CHECK_INTERVAL_MS * 1000);
}

void *system_info_thread(void *arg) {
  WINDOW *win = (WINDOW *)arg;
  wait_until_ready();

  while (!trfx_runtime_should_stop()) {
    if (trfx_runtime_is_paused()) {
      trfx_thread_sleep_ms(TUI_PAUSE_INTERVAL_MS);
      continue;
    }

    if (!SHOW_TOP_PANELS) {
      trfx_thread_sleep_ms(TUI_REFRESH_INTERVAL_MS);
      continue;
    }

    int row = 1;
    int line = 2;
    int label_width = 16;
    SystemOverview sysinfo = get_system_overview();

    pthread_mutex_lock(&ncurses_mutex);

    werase(win);

    wattron(win, trfx_color_attr(COLOR_BORDER));
    box(win, 0, 0);
    wattroff(win, trfx_color_attr(COLOR_BORDER));

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

    trfx_wait_for_static_refresh(STATIC_MODULE_SYSINFO,
                                 TUI_REFRESH_INTERVAL_MS);
  }
  return NULL;
}

void *cpu_info_thread(void *arg) {
  WINDOW *win = (WINDOW *)arg;
  wait_until_ready();

  extern int TEMP_WARN_YELLOW;
  extern int TEMP_WARN_RED;

  while (!trfx_runtime_should_stop()) {
    if (trfx_runtime_is_paused()) {
      trfx_thread_sleep_ms(TUI_PAUSE_INTERVAL_MS);
      continue;
    }

    if (!SHOW_TOP_PANELS) {
      trfx_thread_sleep_ms(TUI_REFRESH_INTERVAL_MS);
      continue;
    }

    CPUInfo cpu = get_cpu_info();

    pthread_mutex_lock(&ncurses_mutex);

    werase(win);

    int h, w;
    getmaxyx(win, h, w);
    (void)w;

    // Apply color before drawing border
    wattron(win, trfx_color_attr(COLOR_BORDER));
    box(win, 0, 0);
    wattroff(win, trfx_color_attr(COLOR_BORDER));

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
        wattron(win, trfx_color_attr(temp_color));
        wprintw(win, "%.1f °C", cpu.temperature);
        wattroff(win, trfx_color_attr(temp_color));
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

      int usage_color = trfx_color_attr(0);
      if (usage >= CPU_USAGE_CRIT) {
        usage_color = trfx_color_attr(COLOR_DATA_RED);
      } else if (usage >= CPU_USAGE_WARN) {
        usage_color = trfx_color_attr(COLOR_DATA_YELLOW);
      } else {
        usage_color = trfx_color_attr(COLOR_DATA_GREEN);
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
    trfx_wait_for_static_refresh(STATIC_MODULE_CPUINFO,
                                 TUI_REFRESH_INTERVAL_MS);
  }  
  return NULL;
}

void *memory_info_thread(void *arg) {
  WINDOW *win = (WINDOW *)arg;
  wait_until_ready();

  while (!trfx_runtime_should_stop()) {
    if (trfx_runtime_is_paused()) {
      trfx_thread_sleep_ms(TUI_PAUSE_INTERVAL_MS);
      continue;
    }

    if (!SHOW_TOP_PANELS) {
      trfx_thread_sleep_ms(TUI_REFRESH_INTERVAL_MS);
      continue;
    }

    pthread_mutex_lock(&ncurses_mutex);

    werase(win);

    wattron(win, trfx_color_attr(COLOR_BORDER));
    box(win, 0, 0);
    wattroff(win, trfx_color_attr(COLOR_BORDER));

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

    wattron(win, trfx_color_attr(COLOR_HEADER));
    mvwprintw(win, row++, line, "%-10s %10s %10s %10s", "Type", "Total", "Used",
              "Free");
    wattroff(win, trfx_color_attr(COLOR_HEADER));

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
    trfx_wait_for_static_refresh(STATIC_MODULE_MEMINFO,
                                 TUI_REFRESH_INTERVAL_MS);

  }

  return NULL;
}

void *disk_info_thread(void *arg) {
  WINDOW *win = (WINDOW *)arg;
  wait_until_ready();

  while (!trfx_runtime_should_stop()) {
    if (trfx_runtime_is_paused()) {
      trfx_thread_sleep_ms(TUI_PAUSE_INTERVAL_MS);
      continue;
    }

    if (!SHOW_TOP_PANELS) {
      trfx_thread_sleep_ms(TUI_REFRESH_INTERVAL_MS);
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

    wattron(win, trfx_color_attr(COLOR_BORDER));
    box(win, 0, 0);
    wattroff(win, trfx_color_attr(COLOR_BORDER));

    int row = 0;              // Start after top border
    int col = 2;              // Two-space indent
    int usable_width = w - 5; // 2 spaces + border on each side

    // Title
    wattron(win, A_BOLD);
    mvwprintw(win, row++, col, "%.*s", usable_width, " Disk Information ");
    wattroff(win, A_BOLD);

    // Header
    wattron(win, trfx_color_attr(COLOR_HEADER));
    mvwprintw(win, row++, col, "%.*s", usable_width,
              "Mount      Filesystem                  Total    Used    Usage");
    wattroff(win, trfx_color_attr(COLOR_HEADER));

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
    trfx_wait_for_static_refresh(STATIC_MODULE_DISKINFO,
                                 TUI_REFRESH_INTERVAL_MS);
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
  volatile int *local_stop = thread_arg->stop_requested;

  free(arg);
  wait_until_ready();

  while (!trfx_thread_should_stop(local_stop)) {

    if (trfx_runtime_is_paused()) {
      trfx_dynamic_thread_sleep_ms(local_stop, TUI_PAUSE_INTERVAL_MS);
      continue;
    }

    pthread_mutex_lock(&ncurses_mutex);
    int h, w;
    getmaxyx(win, h, w);
    (void)w;
    werase(win);

    wattron(win, trfx_color_attr(COLOR_BORDER));
    box(win, 0, 0);
    wattroff(win, trfx_color_attr(COLOR_BORDER));

    if (h < 5) {
      mvwprintw(win, 1, 2, "Window too small");
      wrefresh(win);
      pthread_mutex_unlock(&ncurses_mutex);
      trfx_dynamic_thread_sleep_ms(local_stop, TUI_SMALL_PANEL_REFRESH_MS);
      continue;
    }

    pthread_mutex_unlock(&ncurses_mutex);

    TrfxProcessResult processes = trfx_collect_processes(current_sort_type);

    pthread_mutex_lock(&ncurses_mutex);

    int row = 0;

    wattron(win, A_BOLD);
    mvwprintw(win, row++, 2, " [%d] Processes ", my_index + 1);
    wattroff(win, A_BOLD);


    int max_rows = h - 2;
    const char *header = "  PID    USER      PR  NI    VIRT    RES      SHR "
                         "S   %%CPU %%MEM   TIME+     COMMAND               ";
    wattron(win, trfx_color_attr(COLOR_HEADER));
    trfx_print_clipped(win, row++, 1, header);
    wattroff(win, trfx_color_attr(COLOR_HEADER));

    if (processes.status != TRFX_PROCESS_COLLECTOR_OK) {
      trfx_print_empty_state(win, processes.error[0] ? processes.error
                                                     : "Process data unavailable");
    } else if (processes.count == 0) {
      trfx_print_empty_state(win, "No process rows available");
    }

    for (int i = 0; i < processes.count && row < max_rows; i++) {
      char line[1024];
      snprintf(line, sizeof(line),
               "%7.7s %-10.10s %2.2s %2.2s %8.8s %7.7s %7.7s %1.1s %5.5s "
               "%5.5s %10.10s %-20.20s",
               processes.processes[i].pid, processes.processes[i].user,
               processes.processes[i].pr, processes.processes[i].ni,
               processes.processes[i].virt, processes.processes[i].res,
               processes.processes[i].shr, processes.processes[i].state,
               processes.processes[i].cpu, processes.processes[i].mem,
               processes.processes[i].time, processes.processes[i].command);

      trfx_print_clipped(win, row++, 1, line);
    }

    wrefresh(win);
    pthread_mutex_unlock(&ncurses_mutex);
    trfx_dynamic_thread_sleep_ms(local_stop, TUI_REFRESH_INTERVAL_MS);
  }
  return NULL;
}

void *process_compact_info_thread(void *arg) {
  ThreadArg *thread_arg = (ThreadArg *)arg;
  int my_index = thread_arg->module_index;
  WINDOW *win = thread_arg->window;
  volatile int *local_stop = thread_arg->stop_requested;

  free(arg);
  wait_until_ready();

  while (!trfx_thread_should_stop(local_stop)) {

    if (trfx_runtime_is_paused()) {
      trfx_dynamic_thread_sleep_ms(local_stop, TUI_PAUSE_INTERVAL_MS);
      continue;
    }

    pthread_mutex_lock(&ncurses_mutex);
    int h, w;
    getmaxyx(win, h, w);
    (void)w;
    werase(win);

    wattron(win, trfx_color_attr(COLOR_BORDER));
    box(win, 0, 0);
    wattroff(win, trfx_color_attr(COLOR_BORDER));

    if (h < 5) {
      mvwprintw(win, 1, 2, "Window too small");
      wrefresh(win);
      pthread_mutex_unlock(&ncurses_mutex);
      trfx_dynamic_thread_sleep_ms(local_stop, TUI_SMALL_PANEL_REFRESH_MS);
      continue;
    }

    pthread_mutex_unlock(&ncurses_mutex);

    TrfxProcessResult processes = trfx_collect_processes(current_sort_type);

    pthread_mutex_lock(&ncurses_mutex);

    int row = 0;

    wattron(win, A_BOLD);
    mvwprintw(win, row++, 2, " [%d] Processes ", my_index + 1);
    wattroff(win, A_BOLD);

    int max_rows = h - 2;

    const char *header = "  PID    USER        %CPU  %MEM   COMMAND";
    wattron(win, trfx_color_attr(COLOR_HEADER));
    trfx_print_clipped(win, row++, 1, header);
    wattroff(win, trfx_color_attr(COLOR_HEADER));

    if (processes.status != TRFX_PROCESS_COLLECTOR_OK) {
      trfx_print_empty_state(win, processes.error[0] ? processes.error
                                                     : "Process data unavailable");
    } else if (processes.count == 0) {
      trfx_print_empty_state(win, "No process rows available");
    }

    for (int i = 0; i < processes.count && row < max_rows; i++) {
      char line[1024];
      snprintf(line, sizeof(line), "%7.7s %-10.10s %5.5s %5.5s %-20.20s",
               processes.processes[i].pid, processes.processes[i].user,
               processes.processes[i].cpu, processes.processes[i].mem,
               processes.processes[i].command);

      trfx_print_clipped(win, row++, 1, line);
    }

    wrefresh(win);
    pthread_mutex_unlock(&ncurses_mutex);
    trfx_dynamic_thread_sleep_ms(local_stop, TUI_REFRESH_INTERVAL_MS);
  }
  return NULL;
}

void *connection_info_thread(void *arg) {
  ThreadArg *thread_arg = (ThreadArg *)arg;
  int my_index = thread_arg->module_index;
  WINDOW *win = thread_arg->window;
  volatile int *local_stop = thread_arg->stop_requested;

  free(arg);
  wait_until_ready();

  while (!trfx_thread_should_stop(local_stop)) {

    if (trfx_runtime_is_paused()) {
      trfx_dynamic_thread_sleep_ms(local_stop, TUI_PAUSE_INTERVAL_MS);
      continue;
    }

    TrfxNetworkSnapshot snapshot;
    char snapshot_error[128];
    trfx_init_network_snapshot(&snapshot);
    TrfxCollectorStatus snapshot_status = trfx_collect_network_snapshot(
        &snapshot, snapshot_error, sizeof(snapshot_error));

    pthread_mutex_lock(&ncurses_mutex);

    werase(win);

    wattron(win, trfx_color_attr(COLOR_BORDER));
    box(win, 0, 0);
    wattroff(win, trfx_color_attr(COLOR_BORDER));

    wattron(win, A_BOLD);
    mvwprintw(win, 0, 2, " [%d] Connections ", my_index + 1);
    wattroff(win, A_BOLD);

    char header[256];
    int panel_width = getmaxx(win);
    int inner_width = panel_width > 4 ? panel_width - 4 : panel_width;
    int endpoint_width = inner_width >= 100 ? 22 : inner_width >= 80 ? 18 : 14;
    int user_width = inner_width >= 100 ? 10 : inner_width >= 80 ? 9 : 8;
    int process_width = inner_width >= 100 ? 16 : inner_width >= 80 ? 14 : 12;
    char summary[256];
    format_connection_summary(&snapshot, summary, sizeof(summary));
    trfx_print_clipped(win, 1, 2, summary);
    snprintf(header, sizeof(header), "%-6s %-*s %-*s %-13s %-7s %-*s %-7s %-*s",
             "Proto", endpoint_width, "Local", endpoint_width, "Remote",
             "State", "UID", user_width, "User", "PID", process_width,
             "Process");
    wattron(win, trfx_color_attr(COLOR_HEADER));
    trfx_print_clipped(win, 2, 2, header);
    wattroff(win, trfx_color_attr(COLOR_HEADER));

    int y = 3;
    if (snapshot_status != TRFX_COLLECTOR_OK && snapshot_error[0] != '\0' &&
        panel_has_room(y, getmaxy(win) - 1)) {
      mvwprintw(win, y++, 2, "Connection snapshot: %s", snapshot_error);
    }

    if (snapshot.connection_count == 0)
      trfx_print_empty_state(win, "No connection rows available");

    for (int i = 0; i < snapshot.connection_count && y < getmaxy(win) - 1; ++i) {
      char line[256];
      format_connection_row(&snapshot.connections[i], panel_width, line,
                            sizeof(line));
      trfx_print_clipped(win, y++, 2, line);
    }

    wrefresh(win);
    pthread_mutex_unlock(&ncurses_mutex);
    trfx_dynamic_thread_sleep_ms(local_stop, TUI_REFRESH_INTERVAL_MS);
  }
  return NULL;
}

void *socket_owner_info_thread(void *arg) {
  ThreadArg *thread_arg = (ThreadArg *)arg;
  int my_index = thread_arg->module_index;
  WINDOW *win = thread_arg->window;
  volatile int *local_stop = thread_arg->stop_requested;

  free(arg);
  wait_until_ready();

  while (!trfx_thread_should_stop(local_stop)) {

    if (trfx_runtime_is_paused()) {
      trfx_dynamic_thread_sleep_ms(local_stop, TUI_PAUSE_INTERVAL_MS);
      continue;
    }

    TrfxNetworkSnapshot snapshot;
    char snapshot_error[128];
    trfx_init_network_snapshot(&snapshot);
    TrfxCollectorStatus snapshot_status = trfx_collect_network_snapshot(
        &snapshot, snapshot_error, sizeof(snapshot_error));

    pthread_mutex_lock(&ncurses_mutex);

    werase(win);

    wattron(win, trfx_color_attr(COLOR_BORDER));
    box(win, 0, 0);
    wattroff(win, trfx_color_attr(COLOR_BORDER));

    wattron(win, A_BOLD);
    mvwprintw(win, 0, 2, " [%d] Socket Inventory ", my_index + 1);
    wattroff(win, A_BOLD);

    int panel_width = getmaxx(win);
    int inner_width = panel_width > 4 ? panel_width - 4 : panel_width;
    int process_width = inner_width >= 90 ? 16 : inner_width >= 70 ? 14 : 12;
    int endpoint_width = inner_width >= 90 ? 22 : inner_width >= 70 ? 18 : 12;

    char summary[256];
    snprintf(summary, sizeof(summary),
             "Owned sockets: %d | Connections: %d", snapshot.socket_owner_count,
             snapshot.connection_count);
    trfx_print_clipped(win, 1, 2, summary);

    wattron(win, trfx_color_attr(COLOR_HEADER));
    {
      char header[128];
      snprintf(header, sizeof(header), "%-6s %-7s %-7s %-*s %-*s %-*s",
               "Proto", "UID", "PID", process_width, "Process",
               endpoint_width, "Local", endpoint_width, "Remote");
      trfx_print_clipped(win, 2, 2, header);
    }
    wattroff(win, trfx_color_attr(COLOR_HEADER));

    int y = 3;
    int owned_count = 0;
    for (int i = 0; i < snapshot.connection_count; ++i) {
      if (connection_has_owner(&snapshot.connections[i]))
        owned_count++;
    }

    if (snapshot_status != TRFX_COLLECTOR_OK && snapshot_error[0] != '\0' &&
        panel_has_room(y, getmaxy(win) - 1)) {
      mvwprintw(win, y++, 2, "Socket snapshot: %s", snapshot_error);
    }

    if (owned_count == 0) {
      trfx_print_empty_state(win, "No owned sockets visible");
    }

    for (int i = 0; i < snapshot.connection_count && y < getmaxy(win) - 1; ++i) {
      if (!connection_has_owner(&snapshot.connections[i]))
        continue;

      char line[256];
      format_socket_inventory_line(&snapshot.connections[i], panel_width, line,
                                   sizeof(line));
      trfx_print_clipped(win, y++, 2, line);
    }

    wrefresh(win);
    pthread_mutex_unlock(&ncurses_mutex);

    trfx_dynamic_thread_sleep_ms(local_stop, TUI_REFRESH_INTERVAL_MS);
  }

  return NULL;
}

void *network_info_thread(void *arg) {
  ThreadArg *thread_arg = (ThreadArg *)arg;
  int my_index = thread_arg->module_index;
  WINDOW *win = thread_arg->window;
  volatile int *local_stop = thread_arg->stop_requested;
  TrfxNetworkSampleBuffer bandwidth_samples;

  free(arg);
  wait_until_ready();
  trfx_bandwidth_state_init();
  trfx_init_network_sample_buffer(&bandwidth_samples);

  while (!trfx_thread_should_stop(local_stop)) {

    if (trfx_runtime_is_paused()) {
      trfx_dynamic_thread_sleep_ms(local_stop, TUI_PAUSE_INTERVAL_MS);
      continue;
    }

    int num_interfaces = 0;
    int max_rows, max_cols;
    TrfxNetworkSnapshot snapshot;
    char snapshot_error[128];
    char bandwidth_error[128];
    char trend_error[128];
    TrfxBandwidthReport bandwidth_report;
    TrfxBandwidthTrend bandwidth_trend;

    trfx_init_network_snapshot(&snapshot);
    TrfxCollectorStatus snapshot_status = trfx_collect_network_snapshot(
        &snapshot, snapshot_error, sizeof(snapshot_error));
    trfx_network_sample_buffer_push(&bandwidth_samples, &snapshot, time(NULL));
    trfx_init_bandwidth_report(&bandwidth_report);
    trfx_collect_bandwidth_report(&bandwidth_samples, &bandwidth_report,
                                  bandwidth_error, sizeof(bandwidth_error));
    trfx_init_bandwidth_trend(&bandwidth_trend);
    trfx_collect_bandwidth_trend(&bandwidth_samples, &bandwidth_trend,
                                 trend_error, sizeof(trend_error));

    char **interfaces_usage = get_interfaces_usage(&num_interfaces);
    bool interface_collect_failed = interfaces_usage == NULL;

    // Lock only for ncurses rendering
    pthread_mutex_lock(&ncurses_mutex);

    getmaxyx(win, max_rows, max_cols);
    (void)max_cols;
    int row = 0;
    int line = 4;
    int max_lines = max_rows - 1;

    werase(win);
    wattron(win, trfx_color_attr(COLOR_BORDER));
    box(win, 0, 0);
    wattroff(win, trfx_color_attr(COLOR_BORDER));

    wattron(win, A_BOLD);
    mvwprintw(win, row++, 2, " [%d] Network Overview ", my_index + 1);
    wattroff(win, A_BOLD);

    render_network_summary(win, &snapshot, &row, line, max_lines);

    if (panel_has_room(row, max_lines))
      row++;
    render_bandwidth_talkers_summary(win, &bandwidth_report, &row, line,
                                     max_lines);

    if (panel_has_room(row, max_lines))
      row++;
    render_bandwidth_history_summary(win, &bandwidth_trend, &row, line,
                                     max_lines);

    if (snapshot_status != TRFX_COLLECTOR_OK && snapshot_error[0] != '\0' &&
        panel_has_room(row, max_lines)) {
      mvwprintw(win, row++, line, "Network snapshot: %s", snapshot_error);
    }

    if (bandwidth_error[0] != '\0' && panel_has_room(row, max_lines)) {
      mvwprintw(win, row++, line, "Bandwidth: %s", bandwidth_error);
    }

    if (trend_error[0] != '\0' && bandwidth_trend.point_count <= 0 &&
        panel_has_room(row, max_lines)) {
      mvwprintw(win, row++, line, "Trend: %s", trend_error);
    }

    if (panel_has_room(row, max_lines))
      row++;
    wattron(win, A_BOLD);
    if (panel_has_room(row, max_lines))
      mvwprintw(win, row++, line, "%-15s | %10s | %10s", "Interface", "Sent/s",
                "Recv/s");
    wattroff(win, A_BOLD);
    if (panel_has_room(row, max_lines))
      mvwprintw(win, row++, line,
                "---------------------------------------------");

    if (interface_collect_failed) {
      trfx_print_empty_state(win, "Interface counters unavailable");
    } else if (num_interfaces == 0) {
      trfx_print_empty_state(win, "No interface counters available");
    }

    for (int i = 0; i < num_interfaces && row < max_lines; i++) {
      mvwprintw(win, row++, line, "%s", interfaces_usage[i]);
    }

    wrefresh(win);
    pthread_mutex_unlock(&ncurses_mutex);

    free_interfaces_usage(interfaces_usage, num_interfaces);
    bandwidth_state_update(&bandwidth_samples, &bandwidth_report);

    trfx_dynamic_thread_sleep_ms(local_stop, TUI_REFRESH_INTERVAL_MS);
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
  
  while (!trfx_runtime_should_stop()) {
    if (trfx_runtime_is_paused()) {
      trfx_thread_sleep_ms(TUI_PAUSE_INTERVAL_MS);
      continue;
    }

    pthread_mutex_lock(&ncurses_mutex);

    werase(win);

    wattron(win, trfx_color_attr(COLOR_BORDER));
    box(win, 0, 0);
    wattroff(win, trfx_color_attr(COLOR_BORDER));

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

    trfx_thread_sleep_ms(TUI_REFRESH_INTERVAL_MS);
  }
  return NULL;
}
