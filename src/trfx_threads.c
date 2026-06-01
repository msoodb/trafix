
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
#include "trfx_actions.h"
#include "trfx_config.h"
#include "trfx_diagnostics.h"
#include "trfx_bandwidth.h"
#include "trfx_globals.h"
#include "trfx_runtime.h"
#include "trfx_utils.h"
#include "trfx_support_views.h"

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

static void render_support_view_header(WINDOW *win, int *row, int max_lines) {
  const TrfxSupportViewSpec *selected_view;

  if (!win || !row)
    return;

  selected_view = trfx_support_view_selected();
  if (selected_view && panel_has_room(*row, max_lines)) {
    char active_line[256];
    snprintf(active_line, sizeof(active_line), "View: %s", selected_view->title);
    trfx_print_clipped(win, (*row)++, 2, active_line);
  }
}

static void format_network_route_line(const TrfxNetworkSnapshot *snapshot,
                                      char *line, size_t line_size);
static void format_network_dns_line(const TrfxNetworkSnapshot *snapshot,
                                    char *line, size_t line_size);
static void format_network_active_line(const TrfxNetworkSnapshot *snapshot,
                                       char *line, size_t line_size);
static void format_network_vpn_line(const TrfxNetworkSnapshot *snapshot,
                                    char *line, size_t line_size);
static void render_bandwidth_totals_summary(WINDOW *win,
                                            const TrfxBandwidthReport *report,
                                            int *row, int line,
                                            int max_lines);
static void render_bandwidth_talkers_summary(WINDOW *win,
                                             const TrfxBandwidthReport *report,
                                             int *row, int line,
                                             int max_lines);
static void render_bandwidth_history_summary(WINDOW *win,
                                             const TrfxBandwidthTrend *trend,
                                             int *row, int line,
                                             int max_lines);
static const TrfxBandwidthFlow *connection_find_hot_flow(
    const TrfxConnectionSummary *connection, const TrfxBandwidthReport *report);

static void format_support_time(time_t value, char *buf, size_t buf_size) {
  struct tm tm_value;

  if (!buf || buf_size == 0)
    return;

  if (localtime_r(&value, &tm_value) == NULL) {
    snprintf(buf, buf_size, "unknown");
    return;
  }

  strftime(buf, buf_size, "%H:%M:%S", &tm_value);
}

static void render_support_log_lines(WINDOW *win, const TrfxDiagnosticsLogSnapshot *logs,
                                     int *row, int max_lines) {
  if (!win || !logs || !row)
    return;

  if (trfx_diagnostics_log_count(logs) == 0) {
    trfx_print_empty_state(win, "No readable log lines available");
    return;
  }

  for (size_t i = 0; i < trfx_diagnostics_log_count(logs) &&
                     panel_has_room(*row, max_lines);
       i++) {
    const TrfxDiagnosticsLogLine *entry = trfx_diagnostics_log_at(logs, i);
    char line[384];

    if (!entry)
      continue;

    snprintf(line, sizeof(line), "[%s] %s", entry->source, entry->text);
    trfx_print_clipped(win, (*row)++, 2, line);
  }
}

static void render_support_overview_view(
    WINDOW *win, const TrfxDiagnosticsSnapshot *snapshot,
    const TrfxAlertSummary *alerts, TrfxCollectorStatus status,
    const char *error, int *row, int max_lines) {
  char health_line[256];
  char alerts_line[384];

  if (!win || !snapshot || !alerts || !row)
    return;

  snprintf(health_line, sizeof(health_line),
           "Status: %s | route %s | DNS %s | active %s | VPN %s",
           status == TRFX_COLLECTOR_OK ? "ok" : "partial",
           snapshot->network.route.has_default ? "ok" : "missing",
           snapshot->network.dns.count > 0 ? "ok" : "missing",
           snapshot->network.has_active_interface ? "ok" : "missing",
           snapshot->network.has_vpn_interface ? "ok" : "missing");
  trfx_print_clipped(win, (*row)++, 2, health_line);

  snprintf(alerts_line, sizeof(alerts_line), "Alerts: ");
  if (trfx_diagnostics_alert_count(alerts) == 0) {
    strncat(alerts_line, "none", sizeof(alerts_line) - strlen(alerts_line) - 1);
  } else {
    for (size_t i = 0; i < trfx_diagnostics_alert_count(alerts); i++) {
      const char *alert = trfx_diagnostics_alert_at(alerts, i);
      if (!alert)
        continue;
      if (i > 0)
        strncat(alerts_line, "; ",
                sizeof(alerts_line) - strlen(alerts_line) - 1);
      strncat(alerts_line, alert,
              sizeof(alerts_line) - strlen(alerts_line) - 1);
    }
  }
  trfx_print_clipped(win, (*row)++, 2, alerts_line);

  if (error[0] != '\0')
    trfx_print_clipped(win, (*row)++, 2, error);

  if (panel_has_room(*row, max_lines))
    trfx_print_clipped(win, (*row)++, 2, "Recent logs:");

  render_support_log_lines(win, &snapshot->logs, row, max_lines);
}

static void render_support_logs_view(WINDOW *win,
                                     const TrfxDiagnosticsSnapshot *snapshot,
                                     const char *error, int *row,
                                     int max_lines) {
  char header[256];

  if (!win || !snapshot || !row)
    return;

  snprintf(header, sizeof(header), "Log source: %s",
           snapshot->logs.count > 0 ? snapshot->logs.lines[0].source : "logs");
  trfx_print_clipped(win, (*row)++, 2, header);
  if (error[0] != '\0')
    trfx_print_clipped(win, (*row)++, 2, error);
  render_support_log_lines(win, &snapshot->logs, row, max_lines);
}

static void render_support_diagnostics_view(
    WINDOW *win, const TrfxDiagnosticsSnapshot *snapshot,
    const TrfxAlertSummary *alerts, const char *error, int *row,
    int max_lines) {
  char line[256];

  if (!win || !snapshot || !alerts || !row)
    return;

  snprintf(line, sizeof(line), "Route: %s",
           snapshot->network.route.has_default ? "present" : "missing");
  trfx_print_clipped(win, (*row)++, 2, line);
  snprintf(line, sizeof(line), "DNS: %s",
           snapshot->network.dns.count > 0 ? "present" : "missing");
  trfx_print_clipped(win, (*row)++, 2, line);
  snprintf(line, sizeof(line), "Active interface: %s",
           snapshot->network.has_active_interface ? "present" : "missing");
  trfx_print_clipped(win, (*row)++, 2, line);

  if (trfx_diagnostics_alert_count(alerts) == 0) {
    trfx_print_clipped(win, (*row)++, 2, "Alerts: none");
  } else {
    for (size_t i = 0; i < trfx_diagnostics_alert_count(alerts) &&
                       panel_has_room(*row, max_lines);
         i++) {
      const char *alert = trfx_diagnostics_alert_at(alerts, i);
      if (alert)
        trfx_print_clipped(win, (*row)++, 2, alert);
    }
  }

  if (error[0] != '\0')
    trfx_print_clipped(win, (*row)++, 2, error);
  if (panel_has_room(*row, max_lines))
    trfx_print_clipped(win, (*row)++, 2, "Recent logs:");
  render_support_log_lines(win, &snapshot->logs, row, max_lines);
}

static void render_support_route_dns_view(WINDOW *win,
                                          const TrfxDiagnosticsSnapshot *snapshot,
                                          const char *error, int *row,
                                          int max_lines) {
  char route_line[256];
  char dns_line[256];
  char active_line[256];
  char vpn_line[256];
  char dns_body[192] = "";
  int shown;

  if (!win || !snapshot || !row)
    return;

  (void)max_lines;

  if (snapshot->network.route.has_default) {
    snprintf(route_line, sizeof(route_line), "Route: default via %s dev %s metric %s",
             snapshot->network.route.gateway, snapshot->network.route.interface,
             snapshot->network.route.metric);
  } else {
    snprintf(route_line, sizeof(route_line), "Route: unavailable");
  }

  if (snapshot->network.dns.count > 0) {
    shown = snapshot->network.dns.count < 3 ? snapshot->network.dns.count : 3;
    for (int i = 0; i < shown; i++) {
      if (i > 0)
        strncat(dns_body, ", ", sizeof(dns_body) - strlen(dns_body) - 1);
      strncat(dns_body, snapshot->network.dns.servers[i],
              sizeof(dns_body) - strlen(dns_body) - 1);
    }
    if (snapshot->network.dns.count > shown)
      strncat(dns_body, ", ...", sizeof(dns_body) - strlen(dns_body) - 1);
    snprintf(dns_line, sizeof(dns_line), "DNS: %d server%s detected | %s",
             snapshot->network.dns.count,
             snapshot->network.dns.count == 1 ? "" : "s", dns_body);
  } else {
    snprintf(dns_line, sizeof(dns_line), "DNS: unavailable");
  }

  if (snapshot->network.has_active_interface) {
    snprintf(active_line, sizeof(active_line),
             "Active: %s (%s) | IP: %s | MAC: %s", snapshot->network.active_interface,
             snapshot->network.active_type, snapshot->network.active_ip,
             snapshot->network.active_mac[0] ? snapshot->network.active_mac
                                             : "N/A");
  } else {
    snprintf(active_line, sizeof(active_line), "Active: unavailable");
  }

  if (!snapshot->network.has_vpn_interface) {
    snprintf(vpn_line, sizeof(vpn_line), "VPN: none detected");
  } else if (snapshot->network.vpn_ip[0] == '\0') {
    snprintf(vpn_line, sizeof(vpn_line), "VPN: %s detected | IP unavailable",
             snapshot->network.vpn_interface);
  } else {
    snprintf(vpn_line, sizeof(vpn_line), "VPN: %s detected | IP %s",
             snapshot->network.vpn_interface, snapshot->network.vpn_ip);
  }

  trfx_print_clipped(win, (*row)++, 2, route_line);
  trfx_print_clipped(win, (*row)++, 2, dns_line);
  trfx_print_clipped(win, (*row)++, 2, active_line);
  trfx_print_clipped(win, (*row)++, 2, vpn_line);

  if (error[0] != '\0')
    trfx_print_clipped(win, (*row)++, 2, error);
}

static void render_support_network_health_view(
    WINDOW *win, const TrfxDiagnosticsSnapshot *snapshot, int *row,
    int max_lines) {
  char cpu_line[256];
  char memory_line[256];
  char disk_line[256];
  char process_line[256];
  char network_line[256];

  if (!win || !snapshot || !row)
    return;

  snprintf(cpu_line, sizeof(cpu_line), "CPU: avg %.1f%% | temp %.1fC | cores %d",
           snapshot->cpu.avg_usage, snapshot->cpu.temperature,
           snapshot->cpu.num_cores);
  snprintf(memory_line, sizeof(memory_line),
           "Memory: %.1f%% | RAM %ld/%ld | SWAP %ld/%ld",
           snapshot->memory.mem_percent, snapshot->memory.used_ram,
           snapshot->memory.total_ram, snapshot->memory.used_swap,
           snapshot->memory.total_swap);
  snprintf(disk_line, sizeof(disk_line),
           "Disk: %d mounts | %.1f/%.1f MB used", snapshot->disk_count,
           snapshot->disk_total_used_mb, snapshot->disk_total_mb);
  snprintf(process_line, sizeof(process_line),
           "Process pressure: %d collected | top %s",
           snapshot->processes.count,
           snapshot->processes.count > 0 ? snapshot->processes.processes[0].command
                                        : "unavailable");

  if (snapshot->network.route.has_default && snapshot->network.dns.count > 0 &&
      snapshot->network.has_active_interface) {
    snprintf(network_line, sizeof(network_line),
             "Network: route, DNS, and active interface present");
  } else {
    snprintf(network_line, sizeof(network_line),
             "Network: route %s | DNS %s | active %s",
             snapshot->network.route.has_default ? "ok" : "missing",
             snapshot->network.dns.count > 0 ? "ok" : "missing",
             snapshot->network.has_active_interface ? "ok" : "missing");
  }

  trfx_print_clipped(win, (*row)++, 2, network_line);
  trfx_print_clipped(win, (*row)++, 2, cpu_line);
  trfx_print_clipped(win, (*row)++, 2, memory_line);
  trfx_print_clipped(win, (*row)++, 2, disk_line);
  trfx_print_clipped(win, (*row)++, 2, process_line);

  if (panel_has_room(*row, max_lines))
    trfx_print_clipped(win, (*row)++, 2, "Recent logs:");
  render_support_log_lines(win, &snapshot->logs, row, max_lines);
}

static void render_support_bandwidth_view(
    WINDOW *win, const TrfxNetworkSampleBuffer *samples,
    const TrfxBandwidthReport *report, const TrfxBandwidthTrend *trend,
    const char *bandwidth_error, const char *trend_error, int *row,
    int max_lines) {
  char line[256];
  char label[32];
  char local[48];
  char remote[48];
  char rx[32];
  char tx[32];

  if (!win || !samples || !report || !trend || !row)
    return;

  snprintf(line, sizeof(line), "Mode: %s | Source: %s",
           trfx_bandwidth_mode_name(report->mode),
           report->source[0] ? report->source : "unknown");
  trfx_print_clipped(win, (*row)++, 2, line);

  if (report->interface_count > 0) {
    double total_rx = 0.0;
    double total_tx = 0.0;
    for (int i = 0; i < report->interface_count; i++) {
      total_rx += report->interface_rates[i].rx_bytes_per_sec;
      total_tx += report->interface_rates[i].tx_bytes_per_sec;
    }
    trfx_format_net_bytes(total_rx, rx, sizeof(rx));
    trfx_format_net_bytes(total_tx, tx, sizeof(tx));
    snprintf(line, sizeof(line), "Totals: rx %s/s | tx %s/s", rx, tx);
    trfx_print_clipped(win, (*row)++, 2, line);
  } else {
    trfx_print_clipped(win, (*row)++, 2, "Totals: unavailable");
  }

  if (report->flow_count == 0) {
    trfx_print_clipped(win, (*row)++, 2, "No measured bandwidth flows");
  } else {
    int visible = report->flow_count < 3 ? report->flow_count : 3;
    for (int i = 0; i < visible && panel_has_room(*row, max_lines); i++) {
      const TrfxBandwidthFlow *flow = &report->flows[i];
      trfx_clip_text(flow->label[0] ? flow->label : "flow", label,
                     sizeof(label), (int)sizeof(label) - 1);
      trfx_clip_text(flow->local[0] ? flow->local : "-", local,
                     sizeof(local), (int)sizeof(local) - 1);
      trfx_clip_text(flow->remote[0] ? flow->remote : "-", remote,
                     sizeof(remote), (int)sizeof(remote) - 1);
      trfx_format_net_bytes(flow->rx_bytes_per_sec, rx, sizeof(rx));
      trfx_format_net_bytes(flow->tx_bytes_per_sec, tx, sizeof(tx));
      snprintf(line, sizeof(line), "%d. %s | %s -> %s | rx %s/s | tx %s/s",
               i + 1, label, local, remote, rx, tx);
      trfx_print_clipped(win, (*row)++, 2, line);
    }
  }

  if (trend->point_count > 0) {
    int latest = trend->point_count - 1;
    trfx_format_net_bytes(trend->rx_bytes_per_sec[latest], rx, sizeof(rx));
    trfx_format_net_bytes(trend->tx_bytes_per_sec[latest], tx, sizeof(tx));
    snprintf(line, sizeof(line), "Trend: %d points | latest rx %s/s | tx %s/s",
             trend->point_count, rx, tx);
    trfx_print_clipped(win, (*row)++, 2, line);
  } else {
    trfx_print_clipped(win, (*row)++, 2, "Trend: unavailable");
  }

  if (bandwidth_error[0] != '\0')
    trfx_print_clipped(win, (*row)++, 2, bandwidth_error);
  if (trend_error[0] != '\0')
    trfx_print_clipped(win, (*row)++, 2, trend_error);

  if (trfx_network_sample_buffer_count(samples) > 0) {
    const TrfxNetworkSample *latest =
        trfx_network_sample_buffer_at(samples,
                                      trfx_network_sample_buffer_count(samples) - 1);
    if (latest) {
      snprintf(line, sizeof(line),
               "Latest sample: connections %d | owners %d | interfaces %d",
               latest->snapshot.connection_count,
               latest->snapshot.socket_owner_count,
               latest->snapshot.interfaces.count);
      trfx_print_clipped(win, (*row)++, 2, line);
    }
  }
}

static void render_support_connection_view(
    WINDOW *win, const TrfxConnectionSummaryResult *connections,
    const TrfxBandwidthReport *bandwidth_report, int focus_index, int *row,
    int max_lines) {
  const TrfxConnectionSummary *connection;
  const TrfxBandwidthFlow *flow = NULL;
  char local[64];
  char remote[64];
  char line[256];
  char rx[32];
  char tx[32];

  if (!win || !connections || !row)
    return;

  (void)max_lines;

  if (connections->count <= 0) {
    trfx_print_empty_state(win, "No visible connections");
    return;
  }

  if (focus_index < 0)
    focus_index = 0;
  if (focus_index >= connections->count)
    focus_index = connections->count - 1;

  connection = &connections->rows[focus_index];
  if (bandwidth_report) {
    for (int i = 0; i < bandwidth_report->flow_count; i++) {
      const TrfxBandwidthFlow *current = &bandwidth_report->flows[i];

      if ((current->proto[0] != '\0' &&
           strcmp(current->proto, connection->protocol) == 0 &&
           strcmp(current->local, connection->local_endpoint) == 0 &&
           strcmp(current->remote, connection->remote_endpoint) == 0) ||
          (current->pid[0] != '\0' &&
           strcmp(current->pid, connection->pid) == 0 &&
           current->process[0] != '\0' &&
           strcmp(current->process, connection->process) == 0)) {
        flow = current;
        break;
      }
    }
  }
  trfx_format_endpoint_for_tui(connection->local_endpoint, local,
                               sizeof(local));
  trfx_format_endpoint_for_tui(connection->remote_endpoint, remote,
                               sizeof(remote));

  snprintf(line, sizeof(line), "Selection: %d/%d", focus_index + 1,
           connections->count);
  trfx_print_clipped(win, (*row)++, 2, line);
  snprintf(line, sizeof(line), "Proto: %s | State: %s",
           connection->protocol[0] ? connection->protocol : "-",
           connection->state[0] ? connection->state : "-");
  trfx_print_clipped(win, (*row)++, 2, line);
  snprintf(line, sizeof(line), "Local: %s", local);
  trfx_print_clipped(win, (*row)++, 2, line);
  snprintf(line, sizeof(line), "Remote: %s", remote);
  trfx_print_clipped(win, (*row)++, 2, line);

  if (connection->has_owner) {
    snprintf(line, sizeof(line), "Owner: UID %s | PID %s | %s",
             connection->uid[0] ? connection->uid : "-",
             connection->pid[0] ? connection->pid : "-",
             connection->process[0] ? connection->process : "-");
  } else {
    snprintf(line, sizeof(line), "Owner: unavailable for this connection");
  }
  trfx_print_clipped(win, (*row)++, 2, line);

  if (flow) {
    trfx_format_net_bytes(flow->rx_bytes_per_sec, rx, sizeof(rx));
    trfx_format_net_bytes(flow->tx_bytes_per_sec, tx, sizeof(tx));
    snprintf(line, sizeof(line), "Activity: top flow | rx %s/s | tx %s/s",
             rx, tx);
  } else {
    snprintf(line, sizeof(line), "Activity: not among measured top flows");
  }
  trfx_print_clipped(win, (*row)++, 2, line);
}

static void render_support_action_audit_view(WINDOW *win, int *row,
                                             int max_lines) {
  char line[384];

  if (!win || !row)
    return;

  if (trfx_action_audit_count() == 0) {
    trfx_print_clipped(win, (*row)++, 2, "No recorded actions yet.");
    return;
  }

  for (size_t i = 0; i < trfx_action_audit_count() &&
                     panel_has_room(*row, max_lines);
       i++) {
    const TrfxActionAuditEntry *entry = trfx_action_audit_at(i);
    char time_text[32];

    if (!entry)
      continue;

    format_support_time(entry->when, time_text, sizeof(time_text));
    snprintf(line, sizeof(line), "%s | %s | %s | %s", time_text,
             trfx_action_kind_name(entry->request.kind),
             trfx_action_target_kind_name(entry->request.target.kind),
             entry->message);
    trfx_print_clipped(win, (*row)++, 2, line);
  }
}

static void format_connection_summary_row(const TrfxConnectionSummary *connection,
                                          int panel_width, char *line,
                                          size_t line_size) {
  if (!connection || !line || line_size == 0)
    return;

  char local[64];
  char remote[64];
  int inner_width = panel_width > 4 ? panel_width - 4 : panel_width;
  int endpoint_width = inner_width >= 160 ? 36 : inner_width >= 140 ? 32
                       : inner_width >= 120 ? 28
                       : inner_width >= 100 ? 24
                       : inner_width >= 80  ? 18
                                            : 14;
  int user_width = inner_width >= 120 ? 8 : inner_width >= 100 ? 7 : 6;
  int process_width = inner_width >= 120 ? 14 : inner_width >= 100 ? 12 : 10;

  trfx_format_endpoint_for_tui(connection->local_endpoint, local,
                               sizeof(local));
  trfx_format_endpoint_for_tui(connection->remote_endpoint, remote,
                               sizeof(remote));

  snprintf(line, line_size,
           "%-6.6s %-*s %-*s %-13.13s %-7.7s %-*.*s %-7.7s %-*.*s",
           connection->protocol, endpoint_width, local, endpoint_width, remote,
           connection->state, connection->uid, user_width, user_width,
           connection->user, connection->pid,
           process_width, process_width, connection->process);
}

static void render_connection_group_summary(
    WINDOW *win, const TrfxConnectionSummaryResult *connections, int *row,
    int line, int max_lines) {
  int tcp_count = 0;
  int udp_count = 0;
  int established_count = 0;
  int listen_count = 0;
  int other_count = 0;
  char summary[256];

  if (!win || !connections || !row)
    return;

  for (int i = 0; i < connections->count; i++) {
    const TrfxConnectionSummary *current = &connections->rows[i];

    if (strcmp(current->protocol, "TCP") == 0)
      tcp_count++;
    else if (strcmp(current->protocol, "UDP") == 0)
      udp_count++;

    if (strcmp(current->state, "ESTABLISHED") == 0)
      established_count++;
    else if (strcmp(current->state, "LISTEN") == 0 ||
             strcmp(current->state, "UNCONN") == 0)
      listen_count++;
    else
      other_count++;
  }

  snprintf(summary, sizeof(summary),
           "Groups: ESTABLISHED %d | LISTEN/UNCONN %d | Other %d | TCP %d | UDP %d",
           established_count, listen_count, other_count, tcp_count, udp_count);

  if (panel_has_room(*row, max_lines))
    trfx_print_clipped(win, (*row)++, line, summary);
}

static const TrfxBandwidthFlow *
connection_find_hot_flow(const TrfxConnectionSummary *connection,
                         const TrfxBandwidthReport *report) {
  if (!connection || !report)
    return NULL;

  for (int i = 0; i < report->flow_count; i++) {
    const TrfxBandwidthFlow *flow = &report->flows[i];

    if ((flow->proto[0] != '\0' &&
         strcmp(flow->proto, connection->protocol) == 0 &&
         strcmp(flow->local, connection->local_endpoint) == 0 &&
         strcmp(flow->remote, connection->remote_endpoint) == 0) ||
        (flow->pid[0] != '\0' && strcmp(flow->pid, connection->pid) == 0 &&
         flow->process[0] != '\0' &&
         strcmp(flow->process, connection->process) == 0)) {
      return flow;
    }
  }

  return NULL;
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
    int shown = snapshot->dns.count < 3 ? snapshot->dns.count : 3;

    dns[0] = '\0';
    for (int i = 0; i < shown; i++) {
      if (i > 0)
        strncat(dns, ", ", sizeof(dns) - strlen(dns) - 1);
      strncat(dns, snapshot->dns.servers[i],
              sizeof(dns) - strlen(dns) - 1);
    }
    if (snapshot->dns.count > shown)
      strncat(dns, ", ...", sizeof(dns) - strlen(dns) - 1);
    snprintf(line, line_size, "DNS: %d server%s detected | %s",
             snapshot->dns.count,
             snapshot->dns.count == 1 ? "" : "s", dns);
    return;
  }

  if (snapshot->dns_status == TRFX_COLLECTOR_OK) {
    snprintf(line, line_size, "DNS: 0 servers configured");
  } else {
    snprintf(line, line_size, "DNS: unavailable");
  }
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

  if (snapshot->vpn_ip[0] == '\0') {
    snprintf(line, line_size, "VPN: %s detected | IP unavailable",
             snapshot->vpn_interface);
    return;
  }

  snprintf(line, line_size, "VPN: %s detected | IP %s", snapshot->vpn_interface,
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

static void render_route_consistency_summary(WINDOW *win,
                                             const TrfxNetworkSnapshot *snapshot,
                                             int *row, int line,
                                             int max_lines) {
  char summary[256];

  if (!win || !snapshot || !row)
    return;

  trfx_format_route_consistency_summary(snapshot, summary, sizeof(summary));

  if (panel_has_room(*row, max_lines))
    trfx_print_clipped(win, (*row)++, line, summary);
}

static void render_network_health_summary(WINDOW *win,
                                          const TrfxNetworkSnapshot *snapshot,
                                          int *row, int line, int max_lines) {
  char summary[256];

  if (!win || !snapshot || !row)
    return;

  trfx_format_network_health_line(snapshot, summary, sizeof(summary));

  if (panel_has_room(*row, max_lines))
    trfx_print_clipped(win, (*row)++, line, summary);
}

static void render_overview_section_title(WINDOW *win, int *row, int line,
                                          int max_lines, const char *title) {
  if (!win || !row || !title)
    return;

  if (!panel_has_room(*row, max_lines))
    return;

  wattron(win, A_BOLD);
  trfx_print_clipped(win, (*row)++, line, title);
  wattroff(win, A_BOLD);
}

static int interface_status_sort_rank(const TrfxNetworkSnapshot *snapshot,
                                      const TrfxInterfaceStatus *status) {
  if (!snapshot || !status)
    return 2;

  if (snapshot->has_active_interface &&
      strcmp(snapshot->active_interface, status->name) == 0)
    return 0;

  if (status->is_up)
    return 1;

  return 2;
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

    snprintf(linebuf, sizeof(linebuf),
             "%c %.15s %.16s [%.24s] %.24s -> %.24s | rx %.10s/s tx %.10s/s",
             i == selected_index ? '>' : ' ', flow->pid[0] ? flow->pid : "-",
             flow->process[0] ? flow->process : "-",
             flow->detail[0] ? flow->detail : flow->label,
             flow->local[0] ? flow->local : "-",
             flow->remote[0] ? flow->remote : "-", rx, tx);
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

static const TrfxInterfaceRate *find_interface_rate(
    const TrfxBandwidthReport *report, const char *name) {
  if (!report || !name || name[0] == '\0')
    return NULL;

  for (int i = 0; i < report->interface_count; i++) {
    if (strcmp(report->interface_rates[i].name, name) == 0)
      return &report->interface_rates[i];
  }

  return NULL;
}

static void format_interface_address(const TrfxInterfaceStatus *status,
                                     char *line, size_t line_size) {
  if (!status || !line || line_size == 0)
    return;

  if (status->has_ipv4 && status->has_ipv6 &&
      strcmp(status->ipv4, status->ipv6) != 0) {
    snprintf(line, line_size, "%s / %s", status->ipv4, status->ipv6);
    return;
  }

  if (status->has_ipv4) {
    snprintf(line, line_size, "%s", status->ipv4);
    return;
  }

  if (status->has_ipv6) {
    snprintf(line, line_size, "%s", status->ipv6);
    return;
  }

  snprintf(line, line_size, "N/A");
}

static void format_rate_or_na(const TrfxInterfaceRate *rate, int is_rx,
                              char *line, size_t line_size) {
  if (!line || line_size == 0)
    return;

  if (!rate || rate->name[0] == '\0') {
    snprintf(line, line_size, "N/A");
    return;
  }

  trfx_format_net_bytes(is_rx ? rate->rx_bytes_per_sec
                              : rate->tx_bytes_per_sec,
                        line, line_size);
}

static void format_interface_status_row(const TrfxInterfaceStatus *status,
                                        const TrfxInterfaceRate *rate,
                                        char *line, size_t line_size) {
  char address[160];
  char rx[32];
  char tx[32];

  if (!status || !line || line_size == 0)
    return;

  format_interface_address(status, address, sizeof(address));
  format_rate_or_na(rate, 1, rx, sizeof(rx));
  format_rate_or_na(rate, 0, tx, sizeof(tx));

  snprintf(line, line_size, "%-12.12s %-8.8s %-8.8s %-36.36s %10.10s %10.10s",
           status->name[0] ? status->name : "N/A",
           status->operstate[0] ? status->operstate : "unknown",
           status->carrier[0] ? status->carrier : "unknown", address, rx, tx);
}

static void render_interface_status_table(
    WINDOW *win, const TrfxNetworkSnapshot *snapshot,
    const TrfxBandwidthReport *report, int *row, int line, int max_lines) {
  char header[256];
  TrfxInterfaceStatus sorted[TRFX_MAX_INTERFACES];
  int sorted_count = 0;

  if (!win || !snapshot || !row)
    return;

  render_overview_section_title(win, row, line, max_lines, "Interfaces");

  if (!panel_has_room(*row, max_lines))
    return;

  snprintf(header, sizeof(header),
           "%-12s %-8s %-8s %-36s %10s %10s", "Interface", "State",
           "Carrier", "Address", "Rx/s", "Tx/s");
  trfx_print_clipped(win, (*row)++, line, header);

  if (snapshot->interface_statuses.count <= 0) {
    trfx_print_empty_state(win, "No interface status data available");
    return;
  }

  sorted_count = snapshot->interface_statuses.count;
  if (sorted_count > TRFX_MAX_INTERFACES)
    sorted_count = TRFX_MAX_INTERFACES;

  for (int i = 0; i < sorted_count; i++)
    sorted[i] = snapshot->interface_statuses.items[i];

  for (int i = 0; i < sorted_count - 1; i++) {
    for (int j = i + 1; j < sorted_count; j++) {
      int left_rank = interface_status_sort_rank(snapshot, &sorted[i]);
      int right_rank = interface_status_sort_rank(snapshot, &sorted[j]);

      if (left_rank > right_rank ||
          (left_rank == right_rank &&
           strcmp(sorted[i].name, sorted[j].name) > 0)) {
        TrfxInterfaceStatus tmp = sorted[i];
        sorted[i] = sorted[j];
        sorted[j] = tmp;
      }
    }
  }

  for (int i = 0; i < sorted_count && panel_has_room(*row, max_lines); i++) {
    char linebuf[256];
    const TrfxInterfaceRate *rate =
        find_interface_rate(report, sorted[i].name);

    if (snapshot->has_active_interface &&
        strcmp(snapshot->active_interface, sorted[i].name) == 0)
      wattron(win, A_BOLD);
    format_interface_status_row(&sorted[i], rate, linebuf, sizeof(linebuf));
    trfx_print_clipped(win, (*row)++, line, linebuf);
    if (snapshot->has_active_interface &&
        strcmp(snapshot->active_interface, sorted[i].name) == 0)
      wattroff(win, A_BOLD);
  }
}

static void render_bandwidth_totals_summary(WINDOW *win,
                                            const TrfxBandwidthReport *report,
                                            int *row, int line,
                                            int max_lines) {
  double total_rx = 0.0;
  double total_tx = 0.0;
  char rx[32];
  char tx[32];
  char summary[256];
  const char *balance = "balanced";

  if (!win || !report || !row)
    return;

  if (report->interface_count <= 0) {
    if (panel_has_room(*row, max_lines))
      trfx_print_clipped(win, (*row)++, line, "Traffic: unavailable");
    return;
  }

  for (int i = 0; i < report->interface_count; i++) {
    total_rx += report->interface_rates[i].rx_bytes_per_sec;
    total_tx += report->interface_rates[i].tx_bytes_per_sec;
  }

  if (total_rx > total_tx * 1.10)
    balance = "receive dominant";
  else if (total_tx > total_rx * 1.10)
    balance = "transmit dominant";

  trfx_format_net_bytes(total_rx, rx, sizeof(rx));
  trfx_format_net_bytes(total_tx, tx, sizeof(tx));
  snprintf(summary, sizeof(summary),
           "Traffic: rx %s/s | tx %s/s | %s", rx, tx, balance);

  if (panel_has_room(*row, max_lines))
    trfx_print_clipped(win, (*row)++, line, summary);
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

    trfx_clear_window_content(win);

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

    trfx_clear_window_content(win);

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

    trfx_clear_window_content(win);

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

    trfx_clear_window_content(win);

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
  WINDOW *win = thread_arg->window;
  volatile int *local_stop = thread_arg->stop_requested;

  free(arg);
  wait_until_ready();
  trfx_connection_state_init();

  while (!trfx_thread_should_stop(local_stop)) {

    if (trfx_runtime_is_paused()) {
      trfx_dynamic_thread_sleep_ms(local_stop, TUI_PAUSE_INTERVAL_MS);
      continue;
    }

    pthread_mutex_lock(&ncurses_mutex);
    int h, w;
    getmaxyx(win, h, w);
    (void)w;
    trfx_clear_window_content(win);

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
    mvwprintw(win, row++, 2, " Processes ");
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

    {
      int elapsed = 0;
      const int step_ms = 25;
      while (!trfx_thread_should_stop(local_stop) &&
             elapsed < TUI_REFRESH_INTERVAL_MS) {
        if (trfx_support_view_consume_refresh_request())
          break;
        usleep((useconds_t)step_ms * 1000);
        elapsed += step_ms;
      }
    }
  }
  return NULL;
}

void *process_compact_info_thread(void *arg) {
  ThreadArg *thread_arg = (ThreadArg *)arg;
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
    trfx_clear_window_content(win);

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
    mvwprintw(win, row++, 2, " Processes ");
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
    TrfxConnectionSummaryResult connections;
    TrfxBandwidthReport bandwidth_report;
    char snapshot_error[128];
    char connection_error[128];
    trfx_init_network_snapshot(&snapshot);
    TrfxCollectorStatus snapshot_status = trfx_collect_network_snapshot(
        &snapshot, snapshot_error, sizeof(snapshot_error));
    trfx_init_connection_summary_result(&connections);
    trfx_collect_connection_summary(snapshot.connections, snapshot.connection_count,
                                    &connections, connection_error,
                                    sizeof(connection_error));
    trfx_connection_state_update(&connections);
    trfx_init_bandwidth_report(&bandwidth_report);
    trfx_bandwidth_state_copy(NULL, &bandwidth_report, NULL);

    pthread_mutex_lock(&ncurses_mutex);

    trfx_clear_window_content(win);

    wattron(win, trfx_color_attr(COLOR_BORDER));
    box(win, 0, 0);
    wattroff(win, trfx_color_attr(COLOR_BORDER));

    wattron(win, A_BOLD);
    mvwprintw(win, 0, 2, " Connections ");
    wattroff(win, A_BOLD);

    int y = 1;
    char header[256];
    int panel_width = getmaxx(win);
    int inner_width = panel_width > 4 ? panel_width - 4 : panel_width;
    int endpoint_width = inner_width >= 160 ? 36 : inner_width >= 140 ? 32
                         : inner_width >= 120 ? 28
                         : inner_width >= 100 ? 24
                         : inner_width >= 80  ? 18
                                              : 14;
    int user_width = inner_width >= 120 ? 8 : inner_width >= 100 ? 7 : 6;
    int process_width = inner_width >= 120 ? 14 : inner_width >= 100 ? 12 : 10;
    char summary[256];
    format_connection_summary(&snapshot, summary, sizeof(summary));
    trfx_print_clipped(win, y++, 2, summary);
    render_connection_group_summary(win, &connections, &y, 2,
                                    getmaxy(win) - 1);
    if (panel_has_room(y, getmaxy(win) - 1))
      trfx_print_clipped(win, y++, 2,
                         "Order: ESTABLISHED first, LISTEN/UNCONN next, other states last");
    if (panel_has_room(y, getmaxy(win) - 1))
      trfx_print_clipped(win, y++, 2,
                         "Marked rows: * measured top flow | ! unowned");
    snprintf(header, sizeof(header), "%-6s %-*s %-*s %-13s %-7s %-*s %-7s %-*s",
             "Proto", endpoint_width, "Local", endpoint_width, "Remote",
             "State", "UID", user_width, "User", "PID", process_width,
             "Process");
    wattron(win, trfx_color_attr(COLOR_HEADER) | A_BOLD);
    trfx_print_clipped(win, y++, 2, header);
    wattroff(win, trfx_color_attr(COLOR_HEADER) | A_BOLD);

    if (snapshot_status != TRFX_COLLECTOR_OK && snapshot_error[0] != '\0' &&
        panel_has_room(y, getmaxy(win) - 1)) {
      mvwprintw(win, y++, 2, "Connection snapshot: %s", snapshot_error);
    }

    if (connections.count == 0) {
      trfx_print_empty_state(win,
                             connection_error[0] ? connection_error
                                                 : "No connection rows available");
    }

    int selected_index = 0;
    if (!trfx_connection_state_copy(NULL, &selected_index))
      selected_index = 0;
    for (int i = 0; i < connections.count && y < getmaxy(win) - 1; ++i) {
      char line[256];
      char marker = connections.rows[i].has_owner ? ' ' : '!';
      if (connection_find_hot_flow(&connections.rows[i], &bandwidth_report))
        marker = '*';
      format_connection_summary_row(&connections.rows[i], panel_width, line,
                                    sizeof(line));
      {
        char marked_line[320];
        snprintf(marked_line, sizeof(marked_line), "%c %s", marker, line);
        trfx_print_clipped(win, y++, 2, marked_line);
      }
    }

    wrefresh(win);
    pthread_mutex_unlock(&ncurses_mutex);
    trfx_dynamic_thread_sleep_ms(local_stop, TUI_REFRESH_INTERVAL_MS);
  }
  return NULL;
}

void *socket_owner_info_thread(void *arg) {
  ThreadArg *thread_arg = (ThreadArg *)arg;
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

    trfx_clear_window_content(win);

    wattron(win, trfx_color_attr(COLOR_BORDER));
    box(win, 0, 0);
    wattroff(win, trfx_color_attr(COLOR_BORDER));

    wattron(win, A_BOLD);
    mvwprintw(win, 0, 2, " Socket Inventory ");
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

void *support_info_thread(void *arg) {
  ThreadArg *thread_arg = (ThreadArg *)arg;
  WINDOW *win = thread_arg->window;
  volatile int *local_stop = thread_arg->stop_requested;
  TrfxNetworkSampleBuffer bandwidth_samples;

  free(arg);
  wait_until_ready();
  trfx_bandwidth_state_init();
  trfx_connection_state_init();
  trfx_init_network_sample_buffer(&bandwidth_samples);

  while (!trfx_thread_should_stop(local_stop)) {
    if (trfx_runtime_is_paused()) {
      trfx_dynamic_thread_sleep_ms(local_stop, TUI_PAUSE_INTERVAL_MS);
      continue;
    }

    TrfxDiagnosticsSnapshot snapshot;
    TrfxAlertSummary alerts;
    TrfxBandwidthReport bandwidth_report;
    TrfxBandwidthTrend bandwidth_trend;
    TrfxConnectionSummaryResult connections;
    char error[256];
    char bandwidth_error[128];
    char trend_error[128];
    int row = 0;
    int max_rows, max_cols;
    int bandwidth_focus_index = 0;
    int connection_focus_index = 0;
    size_t selected_index;
    TrfxSupportViewId selected_view_id;

    trfx_init_diagnostics_snapshot(&snapshot);
    trfx_init_alert_summary(&alerts);
    TrfxCollectorStatus status =
        trfx_collect_diagnostics_snapshot(&snapshot, error, sizeof(error));
    trfx_collect_diagnostics_alerts(&snapshot, &alerts);
    trfx_network_sample_buffer_push(&bandwidth_samples, &snapshot.network,
                                     time(NULL));
    trfx_init_bandwidth_report(&bandwidth_report);
    trfx_collect_bandwidth_report(&bandwidth_samples, &bandwidth_report,
                                  bandwidth_error, sizeof(bandwidth_error));
    trfx_init_bandwidth_trend(&bandwidth_trend);
    trfx_collect_bandwidth_trend(&bandwidth_samples, &bandwidth_trend,
                                 trend_error, sizeof(trend_error));
    trfx_init_connection_summary_result(&connections);
    trfx_connection_state_copy(&connections, &connection_focus_index);
    trfx_bandwidth_state_copy(NULL, &bandwidth_report, &bandwidth_focus_index);
    selected_index = trfx_support_view_selected_index();
    selected_view_id = trfx_support_view_id_at(selected_index);

    pthread_mutex_lock(&ncurses_mutex);
    getmaxyx(win, max_rows, max_cols);
    (void)max_cols;
    trfx_clear_window_content(win);
    wattron(win, trfx_color_attr(COLOR_BORDER));
    box(win, 0, 0);
    wattroff(win, trfx_color_attr(COLOR_BORDER));
    wattron(win, A_BOLD);
    mvwprintw(win, row++, 2, " Support Panel ");
    wattroff(win, A_BOLD);
    if (panel_has_room(row, max_rows)) {
      row++;
      render_support_view_header(win, &row, max_rows);
    }

    if (panel_has_room(row, max_rows))
      row++;

    switch (selected_view_id) {
    case TRFX_SUPPORT_VIEW_LOGS:
      render_support_logs_view(win, &snapshot, error, &row, max_rows);
      break;
    case TRFX_SUPPORT_VIEW_DIAGNOSTICS:
      render_support_diagnostics_view(win, &snapshot, &alerts, error, &row,
                                      max_rows);
      break;
    case TRFX_SUPPORT_VIEW_ROUTE_DNS:
      render_support_route_dns_view(win, &snapshot, error, &row, max_rows);
      break;
    case TRFX_SUPPORT_VIEW_NETWORK_HEALTH:
      render_support_network_health_view(win, &snapshot, &row, max_rows);
      break;
    case TRFX_SUPPORT_VIEW_BANDWIDTH:
      render_support_bandwidth_view(win, &bandwidth_samples, &bandwidth_report,
                                    &bandwidth_trend, bandwidth_error,
                                    trend_error, &row, max_rows);
      break;
    case TRFX_SUPPORT_VIEW_CONNECTION_DETAIL:
      render_support_connection_view(win, &connections, &bandwidth_report,
                                     connection_focus_index, &row, max_rows);
      break;
    case TRFX_SUPPORT_VIEW_ACTION_AUDIT:
      render_support_action_audit_view(win, &row, max_rows);
      break;
    case TRFX_SUPPORT_VIEW_OVERVIEW:
    default:
      render_support_overview_view(win, &snapshot, &alerts, status, error,
                                   &row, max_rows);
      break;
    }

    wrefresh(win);
    pthread_mutex_unlock(&ncurses_mutex);

    {
      int elapsed = 0;
      const int step_ms = 25;
      while (!trfx_thread_should_stop(local_stop) &&
             elapsed < TUI_REFRESH_INTERVAL_MS) {
        if (trfx_support_view_consume_refresh_request())
          break;
        usleep((useconds_t)step_ms * 1000);
        elapsed += step_ms;
      }
    }
  }

  return NULL;
}

void *network_info_thread(void *arg) {
  ThreadArg *thread_arg = (ThreadArg *)arg;
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

    // Lock only for ncurses rendering
    pthread_mutex_lock(&ncurses_mutex);

    getmaxyx(win, max_rows, max_cols);
    (void)max_cols;
    int row = 0;
    int line = 4;
    int max_lines = max_rows - 1;

    trfx_clear_window_content(win);
    wattron(win, trfx_color_attr(COLOR_BORDER));
    box(win, 0, 0);
    wattroff(win, trfx_color_attr(COLOR_BORDER));

    wattron(win, A_BOLD);
    mvwprintw(win, row++, 2, " Network Overview ");
    wattroff(win, A_BOLD);

    render_overview_section_title(win, &row, line, max_lines, "Path");
    render_network_summary(win, &snapshot, &row, line, max_lines);

    if (panel_has_room(row, max_lines))
      row++;

    render_overview_section_title(win, &row, line, max_lines, "Health");
    render_route_consistency_summary(win, &snapshot, &row, line, max_lines);
    render_network_health_summary(win, &snapshot, &row, line, max_lines);

    if (panel_has_room(row, max_lines))
      row++;
    render_interface_status_table(win, &snapshot, &bandwidth_report, &row,
                                  line, max_lines);

    if (panel_has_room(row, max_lines))
      row++;
    render_overview_section_title(win, &row, line, max_lines, "Traffic");
    render_bandwidth_totals_summary(win, &bandwidth_report, &row, line,
                                    max_lines);

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

    wrefresh(win);
    pthread_mutex_unlock(&ncurses_mutex);
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

    trfx_clear_window_content(win);

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
