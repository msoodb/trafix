/*
 * Copyright (C) 2025 Masoud Bolhassani <masoud.bolhassani@gmail.com>
 *
 * This file is part of Trafix.
 *
 * Trafix is released under the GNU General Public License v3 (GPL-3.0).
 * See LICENSE file for details.
 */

#include "test_common.h"
#include "trfx_netinfo.h"
#include "trfx_threads.h"

static int test_parse_interface_stats_fixture(void) {
  TrfxInterfaceStat stats[TRFX_MAX_INTERFACES];
  int count = trfx_parse_interface_stats_path("tests/fixtures/proc_net_dev",
                                              stats, TRFX_MAX_INTERFACES);

  ASSERT_INT_EQ(count, 2);
  ASSERT_STR_EQ(stats[0].name, "lo");
  ASSERT_INT_EQ((int)stats[0].rx_bytes, 4096);
  ASSERT_INT_EQ((int)stats[0].tx_bytes, 8192);
  ASSERT_STR_EQ(stats[1].name, "eth0");
  ASSERT_INT_EQ((int)stats[1].rx_bytes, 1048576);
  ASSERT_INT_EQ((int)stats[1].tx_bytes, 2097152);

  return 0;
}

static int test_collect_interface_stats_result(void) {
  TrfxInterfaceStatsResult result =
      trfx_collect_interface_stats_path("tests/fixtures/proc_net_dev");

  ASSERT_INT_EQ(result.status, TRFX_COLLECTOR_OK);
  ASSERT_INT_EQ(result.count, 2);
  ASSERT_STR_EQ(result.error, "");
  ASSERT_STR_EQ(result.stats[0].name, "lo");
  ASSERT_STR_EQ(result.stats[1].name, "eth0");

  result = trfx_collect_interface_stats_path("tests/fixtures/no_such_file");
  ASSERT_INT_EQ(result.status, TRFX_COLLECTOR_OPEN_FAILED);
  ASSERT_INT_EQ(result.count, 0);

  result = trfx_collect_interface_stats_path(NULL);
  ASSERT_INT_EQ(result.status, TRFX_COLLECTOR_INVALID_ARGUMENT);
  ASSERT_INT_EQ(result.count, 0);

  return 0;
}

static int test_format_interface_usage_line(void) {
  char line[128];

  trfx_format_interface_usage_line("eth0", 2048.0, 1048576.0, line,
                                   sizeof(line));
  ASSERT_STR_EQ(line, " eth0            |  2.00 KB/s |  1.00 MB/s");

  return 0;
}

static int test_calculate_interface_rates(void) {
  TrfxInterfaceStat previous[] = {
      {"eth0", 1000, 2000},
      {"lo", 500, 800},
  };
  TrfxInterfaceStat current[] = {
      {"eth0", 3000, 2600},
      {"lo", 1500, 1800},
  };
  TrfxInterfaceRate rates[2];

  int count = trfx_calculate_interface_rates(previous, 2, current, 2, 2.0,
                                             rates, 2);
  ASSERT_INT_EQ(count, 2);
  ASSERT_STR_EQ(rates[0].name, "eth0");
  ASSERT_INT_EQ((int)rates[0].rx_bytes_per_sec, 1000);
  ASSERT_INT_EQ((int)rates[0].tx_bytes_per_sec, 300);
  ASSERT_STR_EQ(rates[1].name, "lo");
  ASSERT_INT_EQ((int)rates[1].rx_bytes_per_sec, 500);
  ASSERT_INT_EQ((int)rates[1].tx_bytes_per_sec, 500);

  return 0;
}

static int test_calculate_interface_rates_counter_reset(void) {
  TrfxInterfaceStat previous[] = {
      {"eth0", 9000, 9000},
  };
  TrfxInterfaceStat current[] = {
      {"eth0", 100, 200},
  };
  TrfxInterfaceRate rates[1];

  int count = trfx_calculate_interface_rates(previous, 1, current, 1, 1.0,
                                             rates, 1);
  ASSERT_INT_EQ(count, 1);
  ASSERT_STR_EQ(rates[0].name, "eth0");
  ASSERT_INT_EQ((int)rates[0].rx_bytes_per_sec, 0);
  ASSERT_INT_EQ((int)rates[0].tx_bytes_per_sec, 0);

  return 0;
}

static int test_parse_default_route_line(void) {
  char gateway[64];
  char metric[64];
  int found = trfx_parse_default_route_line(
      "default via 192.168.1.1 dev eth0 proto dhcp metric 100\n", gateway,
      sizeof(gateway), metric, sizeof(metric));

  ASSERT_INT_EQ(found, 1);
  ASSERT_STR_EQ(gateway, "192.168.1.1");
  ASSERT_STR_EQ(metric, "100");

  found = trfx_parse_default_route_line("10.0.0.0/24 dev wg0\n", gateway,
                                        sizeof(gateway), metric,
                                        sizeof(metric));
  ASSERT_INT_EQ(found, 0);

  found = trfx_parse_default_route_line("default dev wlan0 proto dhcp\n",
                                        gateway, sizeof(gateway), metric,
                                        sizeof(metric));
  ASSERT_INT_EQ(found, 1);
  ASSERT_STR_EQ(gateway, "N/A");
  ASSERT_STR_EQ(metric, "N/A");

  FILE *fp = fopen("tests/fixtures/ip_route", "r");
  if (!fp) {
    perror("fopen");
    return 1;
  }

  char line[256];
  found = 0;
  while (fgets(line, sizeof(line), fp)) {
    if (trfx_parse_default_route_line(line, gateway, sizeof(gateway), metric,
                                      sizeof(metric))) {
      found = 1;
      break;
    }
  }
  fclose(fp);

  ASSERT_INT_EQ(found, 1);
  ASSERT_STR_EQ(gateway, "192.168.1.1");
  ASSERT_STR_EQ(metric, "100");

  return 0;
}

static int test_route_summary_collector(void) {
  TrfxRouteSummary summary;
  char error[128];
  TrfxCollectorStatus status = trfx_collect_route_summary_path(
      "tests/fixtures/ip_route", &summary, error, sizeof(error));

  ASSERT_INT_EQ(status, TRFX_COLLECTOR_OK);
  ASSERT_INT_EQ(summary.has_default, 1);
  ASSERT_STR_EQ(summary.destination, "default");
  ASSERT_STR_EQ(summary.gateway, "192.168.1.1");
  ASSERT_STR_EQ(summary.interface, "eth0");
  ASSERT_STR_EQ(summary.metric, "100");
  ASSERT_STR_EQ(error, "");

  status = trfx_collect_route_summary_path("tests/fixtures/no_such_route",
                                           &summary, error, sizeof(error));
  ASSERT_INT_EQ(status, TRFX_COLLECTOR_OPEN_FAILED);

  return 0;
}

static int test_parse_route_summary_without_gateway_or_metric(void) {
  TrfxRouteSummary summary;

  ASSERT_INT_EQ(trfx_parse_route_summary_line("default dev wlan0 proto dhcp\n",
                                              &summary),
                1);
  ASSERT_INT_EQ(summary.has_default, 1);
  ASSERT_STR_EQ(summary.destination, "default");
  ASSERT_STR_EQ(summary.gateway, "N/A");
  ASSERT_STR_EQ(summary.interface, "wlan0");
  ASSERT_STR_EQ(summary.metric, "N/A");

  return 0;
}

static int test_dns_summary_collector(void) {
  TrfxDnsSummary summary;
  char error[128];
  TrfxCollectorStatus status = trfx_collect_dns_summary_path(
      "tests/fixtures/resolv_conf", &summary, error, sizeof(error));

  ASSERT_INT_EQ(status, TRFX_COLLECTOR_OK);
  ASSERT_INT_EQ(summary.count, 3);
  ASSERT_STR_EQ(summary.servers[0], "1.1.1.1");
  ASSERT_STR_EQ(summary.servers[1], "8.8.8.8");
  ASSERT_STR_EQ(summary.servers[2], "2001:4860:4860::8888");
  ASSERT_STR_EQ(error, "");

  return 0;
}

static int test_dns_summary_empty_file(void) {
  TrfxDnsSummary summary;
  char error[128];
  TrfxCollectorStatus status = trfx_collect_dns_summary_path(
      "tests/fixtures/resolv_conf_empty", &summary, error, sizeof(error));

  ASSERT_INT_EQ(status, TRFX_COLLECTOR_OK);
  ASSERT_INT_EQ(summary.count, 0);
  ASSERT_STR_EQ(error, "");

  return 0;
}

static int test_network_snapshot_init(void) {
  TrfxNetworkSnapshot snapshot;

  trfx_init_network_snapshot(&snapshot);

  ASSERT_INT_EQ(snapshot.interfaces.status, TRFX_COLLECTOR_PARSE_FAILED);
  ASSERT_INT_EQ(snapshot.interface_statuses.status, TRFX_COLLECTOR_PARSE_FAILED);
  ASSERT_INT_EQ(snapshot.interface_statuses.count, 0);
  ASSERT_INT_EQ(snapshot.route_status, TRFX_COLLECTOR_PARSE_FAILED);
  ASSERT_INT_EQ(snapshot.dns_status, TRFX_COLLECTOR_PARSE_FAILED);
  ASSERT_INT_EQ(snapshot.has_active_interface, 0);
  ASSERT_STR_EQ(snapshot.route.destination, "N/A");
  ASSERT_STR_EQ(snapshot.route.gateway, "N/A");
  ASSERT_STR_EQ(snapshot.route.interface, "N/A");
  ASSERT_STR_EQ(snapshot.route.metric, "N/A");
  ASSERT_STR_EQ(snapshot.active_type, "N/A");
  ASSERT_STR_EQ(snapshot.vpn_interface, "");

  return 0;
}

static int test_interface_status_collection(void) {
  TrfxInterfaceStatsResult interfaces =
      trfx_collect_interface_stats_path("tests/fixtures/proc_net_dev");
  TrfxInterfaceStatusResult statuses;
  char error[128];
  int i;

  ASSERT_INT_EQ(interfaces.status, TRFX_COLLECTOR_OK);

  trfx_init_interface_statuses(&statuses);
  ASSERT_INT_EQ(statuses.status, TRFX_COLLECTOR_PARSE_FAILED);
  ASSERT_INT_EQ(statuses.count, 0);

  ASSERT_INT_EQ(trfx_collect_interface_statuses(&interfaces, &statuses, error,
                                                sizeof(error)),
                TRFX_COLLECTOR_OK);
  ASSERT_STR_EQ(error, "");
  ASSERT_INT_EQ(statuses.count, interfaces.count);
  ASSERT_STR_EQ(statuses.items[0].name, "lo");
  ASSERT_INT_EQ((int)statuses.items[0].rx_bytes, 4096);
  ASSERT_INT_EQ((int)statuses.items[0].tx_bytes, 8192);
  ASSERT_STR_EQ(statuses.items[1].name, "eth0");
  ASSERT_INT_EQ((int)statuses.items[1].rx_bytes, 1048576);
  ASSERT_INT_EQ((int)statuses.items[1].tx_bytes, 2097152);

  for (i = 0; i < statuses.count; i++) {
    ASSERT_INT_EQ(statuses.items[i].operstate[0] != '\0', 1);
    ASSERT_INT_EQ(statuses.items[i].carrier[0] != '\0', 1);
    ASSERT_INT_EQ(statuses.items[i].type[0] != '\0', 1);
  }

  return 0;
}

static int test_network_sample_buffer(void) {
  TrfxNetworkSampleBuffer buffer;
  TrfxNetworkSnapshot snapshot;

  trfx_init_network_sample_buffer(&buffer);
  ASSERT_INT_EQ((int)trfx_network_sample_buffer_count(&buffer), 0);

  trfx_init_network_snapshot(&snapshot);
  snapshot.connection_count = 1;
  snprintf(snapshot.connections[0].protocol,
           sizeof(snapshot.connections[0].protocol), "TCP");

  for (int i = 0; i < TRFX_NETWORK_SAMPLE_HISTORY + 2; i++) {
    snapshot.connection_count = i + 1;
    trfx_network_sample_buffer_push(&buffer, &snapshot, (time_t)i);
  }

  ASSERT_INT_EQ((int)trfx_network_sample_buffer_count(&buffer),
                TRFX_NETWORK_SAMPLE_HISTORY);

  const TrfxNetworkSample *first = trfx_network_sample_buffer_at(&buffer, 0);
  const TrfxNetworkSample *last =
      trfx_network_sample_buffer_at(&buffer,
                                     trfx_network_sample_buffer_count(&buffer) - 1);
  if (!first || !last) {
    fprintf(stderr, "%s:%d: expected populated samples\n", __FILE__,
            __LINE__);
    return 1;
  }
  ASSERT_INT_EQ((int)first->captured_at, 2);
  ASSERT_INT_EQ((int)last->captured_at, TRFX_NETWORK_SAMPLE_HISTORY + 1);
  ASSERT_INT_EQ(last->snapshot.connection_count,
                TRFX_NETWORK_SAMPLE_HISTORY + 2);

  if (trfx_network_sample_buffer_at(&buffer, 99) != NULL) {
    fprintf(stderr, "%s:%d: expected NULL for out-of-range sample\n",
            __FILE__, __LINE__);
    return 1;
  }

  return 0;
}

static int test_route_consistency_formatter(void) {
  TrfxNetworkSnapshot snapshot;
  char line[256];

  trfx_init_network_snapshot(&snapshot);
  snapshot.route_status = TRFX_COLLECTOR_OK;
  snapshot.route.has_default = 1;
  snprintf(snapshot.route.interface, sizeof(snapshot.route.interface), "eth0");
  snapshot.has_active_interface = 1;
  snprintf(snapshot.active_interface, sizeof(snapshot.active_interface), "eth0");
  snprintf(snapshot.active_ip, sizeof(snapshot.active_ip), "192.168.1.10");

  trfx_format_route_consistency_summary(&snapshot, line, sizeof(line));
  ASSERT_STR_EQ(line, "Route check: route eth0 | active eth0 | IP 192.168.1.10 | ok");

  snprintf(snapshot.active_interface, sizeof(snapshot.active_interface), "wlan0");
  trfx_format_route_consistency_summary(&snapshot, line, sizeof(line));
  ASSERT_STR_EQ(line,
                "Route check: route eth0 | active wlan0 | IP 192.168.1.10 | mismatch");

  trfx_init_network_snapshot(&snapshot);
  trfx_format_route_consistency_summary(&snapshot, line, sizeof(line));
  ASSERT_STR_EQ(line, "Route check: unavailable");

  return 0;
}

static int test_interface_name_validation(void) {
  ASSERT_INT_EQ(trfx_is_valid_interface_name("eth0"), 1);
  ASSERT_INT_EQ(trfx_is_valid_interface_name("wlp2s0"), 1);
  ASSERT_INT_EQ(trfx_is_valid_interface_name("wg-test.1"), 1);
  ASSERT_INT_EQ(trfx_is_valid_interface_name("bad;ifname"), 0);
  ASSERT_INT_EQ(trfx_is_valid_interface_name("bad name"), 0);

  return 0;
}

int main(void) {
  if (test_parse_interface_stats_fixture() != 0)
    return 1;

  if (test_collect_interface_stats_result() != 0)
    return 1;

  if (test_format_interface_usage_line() != 0)
    return 1;

  if (test_calculate_interface_rates() != 0)
    return 1;

  if (test_calculate_interface_rates_counter_reset() != 0)
    return 1;

  if (test_parse_default_route_line() != 0)
    return 1;

  if (test_route_summary_collector() != 0)
    return 1;

  if (test_parse_route_summary_without_gateway_or_metric() != 0)
    return 1;

  if (test_dns_summary_collector() != 0)
    return 1;

  if (test_dns_summary_empty_file() != 0)
    return 1;

  if (test_network_snapshot_init() != 0)
    return 1;

  if (test_interface_status_collection() != 0)
    return 1;

  if (test_network_sample_buffer() != 0)
    return 1;

  if (test_route_consistency_formatter() != 0)
    return 1;

  if (test_interface_name_validation() != 0)
    return 1;

  return 0;
}
