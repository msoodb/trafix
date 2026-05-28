/*
 * Copyright (C) 2025 Masoud Bolhassani <masoud.bolhassani@gmail.com>
 *
 * This file is part of Trafix.
 *
 * Trafix is released under the GNU General Public License v3 (GPL-3.0).
 * See LICENSE file for details.
 */

#include "trfx_app.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "trfx_config.h"
#include "trfx_connections.h"
#include "trfx_dashboard.h"
#include "trfx_netinfo.h"
#include "trfx_sysinfo.h"

static void print_json_string(const char *value) {
  putchar('"');

  for (const unsigned char *p = (const unsigned char *)value; p && *p; p++) {
    switch (*p) {
    case '"':
      fputs("\\\"", stdout);
      break;
    case '\\':
      fputs("\\\\", stdout);
      break;
    case '\b':
      fputs("\\b", stdout);
      break;
    case '\f':
      fputs("\\f", stdout);
      break;
    case '\n':
      fputs("\\n", stdout);
      break;
    case '\r':
      fputs("\\r", stdout);
      break;
    case '\t':
      fputs("\\t", stdout);
      break;
    default:
      if (*p < 0x20) {
        printf("\\u%04x", *p);
      } else {
        putchar(*p);
      }
      break;
    }
  }

  putchar('"');
}

int trfx_run_tui(void) {
  srand(time(NULL));
  read_config(CONFIG_FILE);
  start_dashboard();
  return 0;
}

int trfx_run_interfaces_command(TrfxCliOutputFormat output_format) {
  TrfxInterfaceStatsResult result =
      trfx_collect_interface_stats_path("/proc/net/dev");

  if (result.status != TRFX_COLLECTOR_OK) {
    fprintf(stderr, "trafix: failed to collect interfaces: %s\n",
            result.error[0] ? result.error : "unknown error");
    return 1;
  }

  if (output_format == TRFX_CLI_OUTPUT_JSON) {
    printf("{\"interfaces\":[");
    for (int i = 0; i < result.count; i++) {
      if (i > 0)
        putchar(',');
      printf("{\"interface\":");
      print_json_string(result.stats[i].name);
      printf(",\"rx_bytes\":%lu,\"tx_bytes\":%lu}", result.stats[i].rx_bytes,
             result.stats[i].tx_bytes);
    }
    printf("]}\n");
    return 0;
  }

  printf("%-15s %12s %12s\n", "INTERFACE", "RX_BYTES", "TX_BYTES");
  for (int i = 0; i < result.count; i++) {
    printf("%-15s %12lu %12lu\n", result.stats[i].name,
           result.stats[i].rx_bytes, result.stats[i].tx_bytes);
  }

  return 0;
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

int trfx_run_connections_command(const TrfxCliOptions *options) {
  ConnectionInfo connections[MAX_CONNECTIONS];
  int count = get_connection_info(connections, MAX_CONNECTIONS);
  TrfxCliOutputFormat output_format =
      options ? options->output_format : TRFX_CLI_OUTPUT_TEXT;

  if (output_format == TRFX_CLI_OUTPUT_JSON) {
    int written = 0;
    printf("{\"connections\":[");
    for (int i = 0; i < count; i++) {
      if (!connection_matches_filters(&connections[i], options))
        continue;

      if (written > 0)
        putchar(',');
      printf("{\"proto\":");
      print_json_string(connections[i].protocol);
      printf(",\"local\":");
      print_json_string(connections[i].local_addr);
      printf(",\"remote\":");
      print_json_string(connections[i].remote_addr);
      printf(",\"state\":");
      print_json_string(connections[i].state);
      putchar('}');
      written++;
    }
    printf("]}\n");
    return 0;
  }

  printf("%-6s %-22s %-22s %-15s\n", "PROTO", "LOCAL", "REMOTE", "STATE");
  for (int i = 0; i < count; i++) {
    if (!connection_matches_filters(&connections[i], options))
      continue;

    printf("%-6s %-22s %-22s %-15s\n", connections[i].protocol,
           connections[i].local_addr, connections[i].remote_addr,
           connections[i].state);
  }

  return 0;
}

int trfx_run_system_command(TrfxCliOutputFormat output_format) {
  SystemOverview overview = get_system_overview();

  if (output_format == TRFX_CLI_OUTPUT_JSON) {
    printf("{\"hostname\":");
    print_json_string(overview.hostname);
    printf(",\"os\":");
    print_json_string(overview.os_version);
    printf(",\"kernel\":");
    print_json_string(overview.kernel_version);
    printf(",\"uptime\":");
    print_json_string(overview.uptime);
    printf(",\"load_avg\":");
    print_json_string(overview.load_avg);
    printf(",\"logged_in_users\":");
    print_json_string(overview.logged_in_users);
    printf("}\n");
    return 0;
  }

  printf("%-16s %s\n", "HOSTNAME", overview.hostname);
  printf("%-16s %s\n", "OS", overview.os_version);
  printf("%-16s %s\n", "KERNEL", overview.kernel_version);
  printf("%-16s %s\n", "UPTIME", overview.uptime);
  printf("%-16s %s\n", "LOAD_AVG", overview.load_avg);
  printf("%-16s %s\n", "LOGGED_IN_USERS", overview.logged_in_users);

  return 0;
}
