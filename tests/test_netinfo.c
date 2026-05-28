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
  ASSERT_STR_EQ(line, " eth0            |    2.00 KB |    1.00 MB");

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

  if (test_parse_default_route_line() != 0)
    return 1;

  if (test_interface_name_validation() != 0)
    return 1;

  return 0;
}
