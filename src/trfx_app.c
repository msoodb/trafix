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
#include <time.h>

#include "trfx_cli_output.h"
#include "trfx_config.h"
#include "trfx_connections.h"
#include "trfx_dashboard.h"
#include "trfx_netinfo.h"
#include "trfx_sysinfo.h"

int trfx_run_tui(void) {
  srand(time(NULL));
  read_config(CONFIG_FILE);
  start_dashboard();
  return TRFX_EXIT_OK;
}

int trfx_run_interfaces_command(TrfxCliOutputFormat output_format) {
  TrfxInterfaceStatsResult result =
      trfx_collect_interface_stats_path("/proc/net/dev");

  if (result.status != TRFX_COLLECTOR_OK) {
    fprintf(stderr, "trafix: failed to collect interfaces: %s\n",
            result.error[0] ? result.error : "unknown error");
    return TRFX_EXIT_DATA_UNAVAILABLE;
  }

  if (output_format == TRFX_CLI_OUTPUT_JSON) {
    trfx_print_interfaces_json(stdout, &result);
    return TRFX_EXIT_OK;
  }

  trfx_print_interfaces_text(stdout, &result);

  return TRFX_EXIT_OK;
}

int trfx_run_connections_command(const TrfxCliOptions *options) {
  ConnectionInfo connections[MAX_CONNECTIONS];
  int count = get_connection_info(connections, MAX_CONNECTIONS);
  TrfxCliOutputFormat output_format =
      options ? options->output_format : TRFX_CLI_OUTPUT_TEXT;

  if (output_format == TRFX_CLI_OUTPUT_JSON) {
    trfx_print_connections_json(stdout, connections, count, options);
    return TRFX_EXIT_OK;
  }

  trfx_print_connections_text(stdout, connections, count, options);

  return TRFX_EXIT_OK;
}

int trfx_run_listeners_command(TrfxCliOutputFormat output_format) {
  ConnectionInfo connections[MAX_CONNECTIONS];
  int count = get_connection_info(connections, MAX_CONNECTIONS);

  if (output_format == TRFX_CLI_OUTPUT_JSON) {
    trfx_print_listeners_json(stdout, connections, count);
    return TRFX_EXIT_OK;
  }

  trfx_print_listeners_text(stdout, connections, count);
  return TRFX_EXIT_OK;
}

int trfx_run_system_command(TrfxCliOutputFormat output_format) {
  SystemOverview overview = get_system_overview();

  if (output_format == TRFX_CLI_OUTPUT_JSON) {
    trfx_print_system_json(stdout, &overview);
    return TRFX_EXIT_OK;
  }

  trfx_print_system_text(stdout, &overview);

  return TRFX_EXIT_OK;
}
