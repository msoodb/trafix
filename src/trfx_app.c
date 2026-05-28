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

#include "trfx_config.h"
#include "trfx_connections.h"
#include "trfx_dashboard.h"
#include "trfx_netinfo.h"

int trfx_run_tui(void) {
  srand(time(NULL));
  read_config(CONFIG_FILE);
  start_dashboard();
  return 0;
}

int trfx_run_interfaces_command(void) {
  TrfxInterfaceStatsResult result =
      trfx_collect_interface_stats_path("/proc/net/dev");

  if (result.status != TRFX_COLLECTOR_OK) {
    fprintf(stderr, "trafix: failed to collect interfaces: %s\n",
            result.error[0] ? result.error : "unknown error");
    return 1;
  }

  printf("%-15s %12s %12s\n", "INTERFACE", "RX_BYTES", "TX_BYTES");
  for (int i = 0; i < result.count; i++) {
    printf("%-15s %12lu %12lu\n", result.stats[i].name,
           result.stats[i].rx_bytes, result.stats[i].tx_bytes);
  }

  return 0;
}

int trfx_run_connections_command(void) {
  ConnectionInfo connections[MAX_CONNECTIONS];
  int count = get_connection_info(connections, MAX_CONNECTIONS);

  printf("%-6s %-22s %-22s %-15s\n", "PROTO", "LOCAL", "REMOTE", "STATE");
  for (int i = 0; i < count; i++) {
    printf("%-6s %-22s %-22s %-15s\n", connections[i].protocol,
           connections[i].local_addr, connections[i].remote_addr,
           connections[i].state);
  }

  return 0;
}
