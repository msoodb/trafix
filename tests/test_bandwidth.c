/*
 * Copyright (C) 2025 Masoud Bolhassani <masoud.bolhassani@gmail.com>
 *
 * This file is part of Trafix.
 *
 * Trafix is released under the GNU General Public License v3 (GPL-3.0).
 * See LICENSE file for details.
 */

#include "test_common.h"

#include "trfx_bandwidth.h"

static void set_interface_stat(TrfxNetworkSnapshot *snapshot,
                               const char *name, unsigned long rx,
                               unsigned long tx) {
  snapshot->interfaces.count = 1;
  snprintf(snapshot->interfaces.stats[0].name,
           sizeof(snapshot->interfaces.stats[0].name), "%s", name);
  snapshot->interfaces.stats[0].rx_bytes = rx;
  snapshot->interfaces.stats[0].tx_bytes = tx;
}

static int test_bandwidth_report_unsupported(void) {
  TrfxNetworkSampleBuffer buffer;
  TrfxBandwidthReport report;
  TrfxNetworkSnapshot snapshot;

  trfx_init_network_sample_buffer(&buffer);
  trfx_init_network_snapshot(&snapshot);
  set_interface_stat(&snapshot, "eth0", 100, 200);
  trfx_network_sample_buffer_push(&buffer, &snapshot, 0);

  trfx_init_bandwidth_report(&report);
  ASSERT_INT_EQ(trfx_collect_bandwidth_report(&buffer, &report, NULL, 0),
                TRFX_COLLECTOR_OK);
  ASSERT_INT_EQ(report.mode, TRFX_BW_MODE_UNSUPPORTED);
  if (strstr(report.source, "need at least two samples") == NULL) {
    fprintf(stderr, "%s:%d: missing unsupported message\n", __FILE__,
            __LINE__);
    return 1;
  }
  ASSERT_INT_EQ(report.flow_count, 0);

  return 0;
}

static int test_bandwidth_report_socket_estimate(void) {
  TrfxNetworkSampleBuffer buffer;
  TrfxBandwidthReport report;
  TrfxNetworkSnapshot snapshot;

  trfx_init_network_sample_buffer(&buffer);
  trfx_init_network_snapshot(&snapshot);

  set_interface_stat(&snapshot, "eth0", 100, 200);
  trfx_network_sample_buffer_push(&buffer, &snapshot, 0);

  set_interface_stat(&snapshot, "eth0", 300, 500);
  snapshot.connection_count = 2;
  snprintf(snapshot.connections[0].pid, sizeof(snapshot.connections[0].pid),
           "101");
  snprintf(snapshot.connections[0].process,
           sizeof(snapshot.connections[0].process), "alpha");
  snprintf(snapshot.connections[0].protocol,
           sizeof(snapshot.connections[0].protocol), "TCP");
  snprintf(snapshot.connections[0].local_addr,
           sizeof(snapshot.connections[0].local_addr), "127.0.0.1:1000");
  snprintf(snapshot.connections[0].remote_addr,
           sizeof(snapshot.connections[0].remote_addr), "10.0.0.2:443");
  snprintf(snapshot.connections[1].pid, sizeof(snapshot.connections[1].pid),
           "202");
  snprintf(snapshot.connections[1].process,
           sizeof(snapshot.connections[1].process), "beta");
  snprintf(snapshot.connections[1].protocol,
           sizeof(snapshot.connections[1].protocol), "UDP");
  snprintf(snapshot.connections[1].local_addr,
           sizeof(snapshot.connections[1].local_addr), "127.0.0.1:2000");
  snprintf(snapshot.connections[1].remote_addr,
           sizeof(snapshot.connections[1].remote_addr), "10.0.0.3:53");
  trfx_network_sample_buffer_push(&buffer, &snapshot, 2);

  trfx_init_bandwidth_report(&report);
  ASSERT_INT_EQ(trfx_collect_bandwidth_report(&buffer, &report, NULL, 0),
                TRFX_COLLECTOR_OK);
  ASSERT_INT_EQ(report.mode, TRFX_BW_MODE_SOCKET_ESTIMATED);
  ASSERT_INT_EQ(report.interface_count, 1);
  ASSERT_INT_EQ(report.flow_count, 2);
  ASSERT_INT_EQ((int)report.interface_rates[0].rx_bytes_per_sec, 100);
  ASSERT_INT_EQ((int)report.interface_rates[0].tx_bytes_per_sec, 150);

  return 0;
}

static int test_bandwidth_report_process_estimate(void) {
  TrfxNetworkSampleBuffer buffer;
  TrfxBandwidthReport report;
  TrfxNetworkSnapshot snapshot;

  trfx_init_network_sample_buffer(&buffer);
  trfx_init_network_snapshot(&snapshot);

  set_interface_stat(&snapshot, "eth0", 100, 100);
  trfx_network_sample_buffer_push(&buffer, &snapshot, 0);

  set_interface_stat(&snapshot, "eth0", 260, 340);
  snapshot.connection_count = 0;
  snapshot.socket_owner_count = 3;
  snprintf(snapshot.socket_owners[0].pid,
           sizeof(snapshot.socket_owners[0].pid), "101");
  snprintf(snapshot.socket_owners[0].process,
           sizeof(snapshot.socket_owners[0].process), "alpha");
  snprintf(snapshot.socket_owners[1].pid,
           sizeof(snapshot.socket_owners[1].pid), "101");
  snprintf(snapshot.socket_owners[1].process,
           sizeof(snapshot.socket_owners[1].process), "alpha");
  snprintf(snapshot.socket_owners[2].pid,
           sizeof(snapshot.socket_owners[2].pid), "202");
  snprintf(snapshot.socket_owners[2].process,
           sizeof(snapshot.socket_owners[2].process), "beta");
  trfx_network_sample_buffer_push(&buffer, &snapshot, 2);

  trfx_init_bandwidth_report(&report);
  ASSERT_INT_EQ(trfx_collect_bandwidth_report(&buffer, &report, NULL, 0),
                TRFX_COLLECTOR_OK);
  ASSERT_INT_EQ(report.mode, TRFX_BW_MODE_PROCESS_ESTIMATED);
  ASSERT_INT_EQ(report.flow_count, 2);
  ASSERT_STR_EQ(report.flows[0].pid, "101");
  ASSERT_STR_EQ(report.flows[0].detail, "2 sockets");
  ASSERT_STR_EQ(report.flows[1].pid, "202");
  ASSERT_INT_EQ((int)report.flows[0].rx_bytes_per_sec, 53);
  ASSERT_INT_EQ((int)report.flows[0].tx_bytes_per_sec, 80);

  return 0;
}

int main(void) {
  if (test_bandwidth_report_unsupported() != 0)
    return 1;

  if (test_bandwidth_report_socket_estimate() != 0)
    return 1;

  if (test_bandwidth_report_process_estimate() != 0)
    return 1;

  return 0;
}
