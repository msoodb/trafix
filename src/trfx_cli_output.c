/*
 * Copyright (C) 2025 Masoud Bolhassani <masoud.bolhassani@gmail.com>
 *
 * This file is part of Trafix.
 *
 * Trafix is released under the GNU General Public License v3 (GPL-3.0).
 * See LICENSE file for details.
 */

#include "trfx_cli_output.h"

#include <string.h>

static void print_json_string(FILE *out, const char *value) {
  fputc('"', out);

  for (const unsigned char *p = (const unsigned char *)value; p && *p; p++) {
    switch (*p) {
    case '"':
      fputs("\\\"", out);
      break;
    case '\\':
      fputs("\\\\", out);
      break;
    case '\b':
      fputs("\\b", out);
      break;
    case '\f':
      fputs("\\f", out);
      break;
    case '\n':
      fputs("\\n", out);
      break;
    case '\r':
      fputs("\\r", out);
      break;
    case '\t':
      fputs("\\t", out);
      break;
    default:
      if (*p < 0x20) {
        fprintf(out, "\\u%04x", *p);
      } else {
        fputc(*p, out);
      }
      break;
    }
  }

  fputc('"', out);
}

static int connection_matches_filters(const ConnectionInfo *connection,
                                      const TrfxCliOptions *options) {
  if (!connection || !options)
    return 0;

  if (options->has_proto_filter &&
      strcmp(connection->protocol, options->proto_filter) != 0) {
    return 0;
  }

  if (options->has_state_filter &&
      strcmp(connection->state, options->state_filter) != 0) {
    return 0;
  }

  return 1;
}

static int connection_is_listener(const ConnectionInfo *connection) {
  if (!connection)
    return 0;

  if (strcmp(connection->state, "LISTEN") == 0)
    return 1;

  return strcmp(connection->protocol, "UDP") == 0 &&
         strcmp(connection->state, "UNCONN") == 0;
}

void trfx_print_interfaces_text(FILE *out,
                                const TrfxInterfaceStatsResult *result) {
  fprintf(out, "%-15s %12s %12s\n", "INTERFACE", "RX_BYTES", "TX_BYTES");
  for (int i = 0; result && i < result->count; i++) {
    fprintf(out, "%-15s %12lu %12lu\n", result->stats[i].name,
            result->stats[i].rx_bytes, result->stats[i].tx_bytes);
  }
}

void trfx_print_interfaces_json(FILE *out,
                                const TrfxInterfaceStatsResult *result) {
  fprintf(out, "{\"interfaces\":[");
  for (int i = 0; result && i < result->count; i++) {
    if (i > 0)
      fputc(',', out);
    fprintf(out, "{\"interface\":");
    print_json_string(out, result->stats[i].name);
    fprintf(out, ",\"rx_bytes\":%lu,\"tx_bytes\":%lu}",
            result->stats[i].rx_bytes, result->stats[i].tx_bytes);
  }
  fprintf(out, "]}\n");
}

void trfx_print_connections_text(FILE *out, const ConnectionInfo connections[],
                                 int count, const TrfxCliOptions *options) {
  fprintf(out, "%-6s %-22s %-22s %-15s %-8s %-16s %-7s %-16s\n", "PROTO",
          "LOCAL", "REMOTE", "STATE", "UID", "USER", "PID", "PROCESS");
  for (int i = 0; connections && i < count; i++) {
    if (!connection_matches_filters(&connections[i], options))
      continue;

    fprintf(out, "%-6s %-22s %-22s %-15s %-8u %-16.16s %-7s %-16.16s\n",
            connections[i].protocol,
            connections[i].local_addr, connections[i].remote_addr,
            connections[i].state, connections[i].uid, connections[i].user,
            connections[i].pid, connections[i].process);
  }
}

void trfx_print_connections_json(FILE *out, const ConnectionInfo connections[],
                                 int count, const TrfxCliOptions *options) {
  int written = 0;
  fprintf(out, "{\"connections\":[");
  for (int i = 0; connections && i < count; i++) {
    if (!connection_matches_filters(&connections[i], options))
      continue;

    if (written > 0)
      fputc(',', out);
    fprintf(out, "{\"proto\":");
    print_json_string(out, connections[i].protocol);
    fprintf(out, ",\"local\":");
    print_json_string(out, connections[i].local_addr);
    fprintf(out, ",\"remote\":");
    print_json_string(out, connections[i].remote_addr);
    fprintf(out, ",\"state\":");
    print_json_string(out, connections[i].state);
    fprintf(out, ",\"uid\":%u", connections[i].uid);
    fprintf(out, ",\"user\":");
    print_json_string(out, connections[i].user);
    fprintf(out, ",\"pid\":");
    print_json_string(out, connections[i].pid);
    fprintf(out, ",\"process\":");
    print_json_string(out, connections[i].process);
    fputc('}', out);
    written++;
  }
  fprintf(out, "]}\n");
}

void trfx_print_listeners_text(FILE *out, const ConnectionInfo connections[],
                               int count) {
  fprintf(out, "%-6s %-22s %-8s %-16s %-7s %-16s\n", "PROTO", "LOCAL",
          "UID", "USER", "PID", "PROCESS");
  for (int i = 0; connections && i < count; i++) {
    if (!connection_is_listener(&connections[i]))
      continue;

    fprintf(out, "%-6s %-22s %-8u %-16.16s %-7s %-16.16s\n",
            connections[i].protocol, connections[i].local_addr,
            connections[i].uid, connections[i].user, connections[i].pid,
            connections[i].process);
  }
}

void trfx_print_listeners_json(FILE *out, const ConnectionInfo connections[],
                               int count) {
  int written = 0;
  fprintf(out, "{\"listeners\":[");
  for (int i = 0; connections && i < count; i++) {
    if (!connection_is_listener(&connections[i]))
      continue;

    if (written > 0)
      fputc(',', out);
    fprintf(out, "{\"proto\":");
    print_json_string(out, connections[i].protocol);
    fprintf(out, ",\"local\":");
    print_json_string(out, connections[i].local_addr);
    fprintf(out, ",\"uid\":%u", connections[i].uid);
    fprintf(out, ",\"user\":");
    print_json_string(out, connections[i].user);
    fprintf(out, ",\"pid\":");
    print_json_string(out, connections[i].pid);
    fprintf(out, ",\"process\":");
    print_json_string(out, connections[i].process);
    fputc('}', out);
    written++;
  }
  fprintf(out, "]}\n");
}

void trfx_print_system_text(FILE *out, const SystemOverview *overview) {
  fprintf(out, "%-16s %s\n", "HOSTNAME", overview->hostname);
  fprintf(out, "%-16s %s\n", "OS", overview->os_version);
  fprintf(out, "%-16s %s\n", "KERNEL", overview->kernel_version);
  fprintf(out, "%-16s %s\n", "UPTIME", overview->uptime);
  fprintf(out, "%-16s %s\n", "LOAD_AVG", overview->load_avg);
  fprintf(out, "%-16s %s\n", "LOGGED_IN_USERS", overview->logged_in_users);
}

void trfx_print_system_json(FILE *out, const SystemOverview *overview) {
  fprintf(out, "{\"hostname\":");
  print_json_string(out, overview->hostname);
  fprintf(out, ",\"os\":");
  print_json_string(out, overview->os_version);
  fprintf(out, ",\"kernel\":");
  print_json_string(out, overview->kernel_version);
  fprintf(out, ",\"uptime\":");
  print_json_string(out, overview->uptime);
  fprintf(out, ",\"load_avg\":");
  print_json_string(out, overview->load_avg);
  fprintf(out, ",\"logged_in_users\":");
  print_json_string(out, overview->logged_in_users);
  fprintf(out, "}\n");
}

void trfx_print_diagnostics_text(FILE *out,
                                 const TrfxDiagnosticsSnapshot *snapshot) {
  size_t log_count;

  if (!out || !snapshot)
    return;

  fprintf(out, "SYSTEM\n");
  fprintf(out, "HOSTNAME          %s\n", snapshot->system.hostname);
  fprintf(out, "OS                %s\n", snapshot->system.os_version);
  fprintf(out, "KERNEL            %s\n", snapshot->system.kernel_version);
  fprintf(out, "UPTIME            %s\n", snapshot->system.uptime);
  fprintf(out, "LOAD_AVG          %s\n", snapshot->system.load_avg);
  fprintf(out, "LOGGED_IN_USERS   %s\n", snapshot->system.logged_in_users);
  fprintf(out, "\n");

  fprintf(out, "NETWORK\n");
  if (snapshot->network.route.has_default) {
    fprintf(out, "ROUTE             default via %s dev %s metric %s\n",
            snapshot->network.route.gateway, snapshot->network.route.interface,
            snapshot->network.route.metric);
  } else {
    fprintf(out, "ROUTE             unavailable\n");
  }
  fprintf(out, "DNS               ");
  if (snapshot->network.dns.count > 0) {
    for (int i = 0; i < snapshot->network.dns.count; i++) {
      if (i > 0)
        fputs(", ", out);
      fputs(snapshot->network.dns.servers[i], out);
    }
    fputc('\n', out);
  } else {
    fprintf(out, "unavailable\n");
  }
  fprintf(out, "ACTIVE_IFACE      %s (%s) %s %s\n",
          snapshot->network.has_active_interface ? snapshot->network.active_interface
                                                 : "unavailable",
          snapshot->network.active_type,
          snapshot->network.active_ip[0] ? snapshot->network.active_ip : "N/A",
          snapshot->network.active_mac[0] ? snapshot->network.active_mac : "N/A");
  fprintf(out, "VPN               %s\n",
          snapshot->network.has_vpn_interface ? snapshot->network.vpn_interface
                                              : "unavailable");
  fprintf(out, "\n");

  fprintf(out, "PRESSURE\n");
  fprintf(out, "CPU               avg %.1f%% | temp %.1fC | cores %d\n",
          snapshot->cpu.avg_usage, snapshot->cpu.temperature,
          snapshot->cpu.num_cores);
  fprintf(out, "MEMORY            %.1f%% | RAM %ld/%ld | SWAP %ld/%ld\n",
          snapshot->memory.mem_percent, snapshot->memory.used_ram,
          snapshot->memory.total_ram, snapshot->memory.used_swap,
          snapshot->memory.total_swap);
  fprintf(out, "DISK              %d mounts | %.1f/%.1f MB used\n",
          snapshot->disk_count, snapshot->disk_total_used_mb,
          snapshot->disk_total_mb);
  fprintf(out, "PROCESSES         %d captured | top %s\n",
          snapshot->processes.count,
          snapshot->processes.count > 0 ? snapshot->processes.processes[0].command
                                        : "unavailable");
  fprintf(out, "\n");

  fprintf(out, "LOGS\n");
  log_count = trfx_diagnostics_log_count(&snapshot->logs);
  if (log_count == 0) {
    fprintf(out, "  unavailable\n");
    return;
  }

  for (size_t i = 0; i < log_count; i++) {
    const TrfxDiagnosticsLogLine *line = trfx_diagnostics_log_at(&snapshot->logs, i);
    if (!line)
      continue;
    fprintf(out, "  [%s] %s\n", line->source, line->text);
  }
}
