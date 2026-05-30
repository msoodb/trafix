/*
 * Copyright (C) 2025 Masoud Bolhassani <masoud.bolhassani@gmail.com>
 *
 * This file is part of Trafix.
 *
 * Trafix is released under the GNU General Public License v3 (GPL-3.0).
 * See LICENSE file for details.
 */

#include "test_common.h"

#include <time.h>

#include "trfx_bandwidth.h"

static void set_interface_stat(TrfxNetworkSnapshot *snapshot,
                               const char *name, unsigned long rx,
                               unsigned long tx) {
  int slot = snapshot->interfaces.count;

  if (slot >= TRFX_MAX_INTERFACES)
    return;

  snprintf(snapshot->interfaces.stats[slot].name,
           sizeof(snapshot->interfaces.stats[slot].name), "%s", name);
  snapshot->interfaces.stats[slot].rx_bytes = rx;
  snapshot->interfaces.stats[slot].tx_bytes = tx;
  snapshot->interfaces.count++;
}

static void populate_sample(TrfxNetworkSnapshot *snapshot, unsigned long base_rx,
                            unsigned long base_tx) {
  int i;

  trfx_init_network_snapshot(snapshot);
  snapshot->interfaces.count = 0;
  for (i = 0; i < 8; i++) {
    char name[16];
    snprintf(name, sizeof(name), "eth%d", i);
    set_interface_stat(snapshot, name, base_rx + (unsigned long)(i * 100),
                       base_tx + (unsigned long)(i * 50));
  }

  snapshot->connection_count = 16;
  for (i = 0; i < snapshot->connection_count; i++) {
    snprintf(snapshot->connections[i].pid,
             sizeof(snapshot->connections[i].pid), "%d", 1000 + i / 2);
    snprintf(snapshot->connections[i].process,
             sizeof(snapshot->connections[i].process), "proc-%d", i / 2);
    snprintf(snapshot->connections[i].protocol,
             sizeof(snapshot->connections[i].protocol),
             (i % 2 == 0) ? "TCP" : "UDP");
    snprintf(snapshot->connections[i].local_addr,
             sizeof(snapshot->connections[i].local_addr),
             "127.0.0.1:%d", 1000 + i);
    snprintf(snapshot->connections[i].remote_addr,
             sizeof(snapshot->connections[i].remote_addr),
             "10.0.0.%d:%d", i + 1, 4000 + i);
  }

}

static int test_bandwidth_refresh_guardrail(void) {
  TrfxNetworkSampleBuffer buffer;
  TrfxBandwidthReport report;
  TrfxBandwidthTrend trend;
  TrfxNetworkSnapshot snapshot;
  struct timespec start;
  struct timespec end;
  long elapsed_ms;
  int i;

  trfx_init_network_sample_buffer(&buffer);
  populate_sample(&snapshot, 1000, 2000);
  trfx_network_sample_buffer_push(&buffer, &snapshot, 0);
  populate_sample(&snapshot, 2500, 5000);
  trfx_network_sample_buffer_push(&buffer, &snapshot, 2);

  trfx_init_bandwidth_report(&report);
  trfx_init_bandwidth_trend(&trend);

  if (clock_gettime(CLOCK_MONOTONIC, &start) != 0) {
    perror("clock_gettime");
    return 1;
  }

  for (i = 0; i < 250; i++) {
    ASSERT_INT_EQ(trfx_collect_bandwidth_report(&buffer, &report, NULL, 0),
                  TRFX_COLLECTOR_OK);
    ASSERT_INT_EQ(trfx_collect_bandwidth_trend(&buffer, &trend, NULL, 0),
                  TRFX_COLLECTOR_OK);
  }

  if (clock_gettime(CLOCK_MONOTONIC, &end) != 0) {
    perror("clock_gettime");
    return 1;
  }

  elapsed_ms = (end.tv_sec - start.tv_sec) * 1000L +
               (end.tv_nsec - start.tv_nsec) / 1000000L;
  ASSERT_INT_EQ(elapsed_ms < 1000, 1);

  return 0;
}

int main(void) {
  if (test_bandwidth_refresh_guardrail() != 0)
    return 1;

  return 0;
}
