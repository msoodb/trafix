/*
 * Copyright (C) 2025 Masoud Bolhassani <masoud.bolhassani@gmail.com>
 *
 * This file is part of Trafix.
 *
 * Trafix is released under the GNU General Public License v3 (GPL-3.0).
 * See LICENSE file for details.
 */

#include "trfx_config.h"
#include "trfx_cli.h"
#include "trfx_connections.h"
#include "trfx_netinfo.h"
#include "trfx_utils.h"
#include "trfx_version.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#ifndef TRFX_VERSION
#define TRFX_VERSION "unknown"
#endif

#define ASSERT_STR_EQ(actual, expected)                                        \
  do {                                                                        \
    if (strcmp((actual), (expected)) != 0) {                                   \
      fprintf(stderr, "%s:%d: expected \"%s\", got \"%s\"\n", __FILE__,       \
              __LINE__, (expected), (actual));                                \
      return 1;                                                               \
    }                                                                         \
  } while (0)

#define ASSERT_INT_EQ(actual, expected)                                        \
  do {                                                                        \
    if ((actual) != (expected)) {                                             \
      fprintf(stderr, "%s:%d: expected %d, got %d\n", __FILE__, __LINE__,     \
              (expected), (actual));                                          \
      return 1;                                                               \
    }                                                                         \
  } while (0)

#define ASSERT_MODE_EQ(actual, expected)                                       \
  do {                                                                        \
    if ((actual) != (expected)) {                                             \
      fprintf(stderr, "%s:%d: expected CLI mode %d, got %d\n", __FILE__,     \
              __LINE__, (expected), (actual));                                \
      return 1;                                                               \
    }                                                                         \
  } while (0)

static int test_format_bytes(void) {
  char buf[16];

  format_bytes(512.0, buf, sizeof(buf));
  ASSERT_STR_EQ(buf, "512M");

  format_bytes(1536.0, buf, sizeof(buf));
  ASSERT_STR_EQ(buf, "1.5G");

  return 0;
}

static int test_read_config(void) {
  char path[] = "/tmp/trafix-test-config-XXXXXX";
  int fd = mkstemp(path);
  if (fd == -1) {
    perror("mkstemp");
    return 1;
  }

  FILE *file = fdopen(fd, "w");
  if (!file) {
    perror("fdopen");
    close(fd);
    unlink(path);
    return 1;
  }

  fputs("# test config\n", file);
  fputs("TEMP_WARN_YELLOW = 42\n", file);
  fputs("TEMP_WARN_RED = 84\n", file);
  fputs("ROW2_MODULES = 2\n", file);
  fclose(file);

  TEMP_WARN_YELLOW = 50;
  TEMP_WARN_RED = 75;
  ROW2_MODULES = 3;

  read_config(path);
  unlink(path);

  ASSERT_INT_EQ(TEMP_WARN_YELLOW, 42);
  ASSERT_INT_EQ(TEMP_WARN_RED, 84);
  ASSERT_INT_EQ(ROW2_MODULES, 2);

  return 0;
}

static int test_parse_cli(void) {
  char *default_argv[] = {"trafix"};
  TrfxCliOptions options = trfx_parse_cli(1, default_argv);
  ASSERT_MODE_EQ(options.mode, TRFX_CLI_MODE_TUI);
  ASSERT_STR_EQ(options.error, "");

  char *help_long_argv[] = {"trafix", "--help"};
  options = trfx_parse_cli(2, help_long_argv);
  ASSERT_MODE_EQ(options.mode, TRFX_CLI_MODE_HELP);
  ASSERT_STR_EQ(options.error, "");

  char *help_short_argv[] = {"trafix", "-h"};
  options = trfx_parse_cli(2, help_short_argv);
  ASSERT_MODE_EQ(options.mode, TRFX_CLI_MODE_HELP);
  ASSERT_STR_EQ(options.error, "");

  char *version_long_argv[] = {"trafix", "--version"};
  options = trfx_parse_cli(2, version_long_argv);
  ASSERT_MODE_EQ(options.mode, TRFX_CLI_MODE_VERSION);
  ASSERT_STR_EQ(options.error, "");

  char *version_short_argv[] = {"trafix", "-v"};
  options = trfx_parse_cli(2, version_short_argv);
  ASSERT_MODE_EQ(options.mode, TRFX_CLI_MODE_VERSION);
  ASSERT_STR_EQ(options.error, "");

  char *bad_argv[] = {"trafix", "--bad-option"};
  options = trfx_parse_cli(2, bad_argv);
  ASSERT_MODE_EQ(options.mode, TRFX_CLI_MODE_INVALID);
  ASSERT_STR_EQ(options.error, "unknown argument: --bad-option");

  char *unknown_command_argv[] = {"trafix", "connections"};
  options = trfx_parse_cli(2, unknown_command_argv);
  ASSERT_MODE_EQ(options.mode, TRFX_CLI_MODE_INVALID);
  ASSERT_STR_EQ(options.error, "unknown argument: connections");

  char *too_many_argv[] = {"trafix", "--help", "--version"};
  options = trfx_parse_cli(3, too_many_argv);
  ASSERT_MODE_EQ(options.mode, TRFX_CLI_MODE_INVALID);
  ASSERT_STR_EQ(options.error, "unknown argument: --help");

  char *default_plus_extra_argv[] = {"trafix", "tui", "--help"};
  options = trfx_parse_cli(3, default_plus_extra_argv);
  ASSERT_MODE_EQ(options.mode, TRFX_CLI_MODE_INVALID);
  ASSERT_STR_EQ(options.error, "unknown argument: tui");

  char *version_plus_extra_argv[] = {"trafix", "--version", "extra"};
  options = trfx_parse_cli(3, version_plus_extra_argv);
  ASSERT_MODE_EQ(options.mode, TRFX_CLI_MODE_INVALID);
  ASSERT_STR_EQ(options.error, "unknown argument: --version");

  return 0;
}

static int test_get_version(void) {
  ASSERT_STR_EQ(trfx_get_version(), TRFX_VERSION);
  return 0;
}

static int test_parse_connection_fixtures(void) {
  ConnectionInfo connections[MAX_CONNECTIONS];

  int count = trfx_parse_connection_path("tests/fixtures/proc_net_tcp", "TCP",
                                         connections, 0, MAX_CONNECTIONS);
  ASSERT_INT_EQ(count, 2);
  ASSERT_STR_EQ(connections[0].protocol, "TCP");
  ASSERT_STR_EQ(connections[0].local_addr, "127.0.0.1:8080");
  ASSERT_STR_EQ(connections[0].remote_addr, "127.0.0.2:443");
  ASSERT_STR_EQ(connections[0].state, "ESTABLISHED");
  ASSERT_STR_EQ(connections[1].protocol, "TCP");
  ASSERT_STR_EQ(connections[1].local_addr, "0.0.0.0:22");
  ASSERT_STR_EQ(connections[1].remote_addr, "0.0.0.0:0");
  ASSERT_STR_EQ(connections[1].state, "LISTEN");

  count = trfx_parse_connection_path("tests/fixtures/proc_net_udp", "UDP",
                                     connections, count, MAX_CONNECTIONS);
  ASSERT_INT_EQ(count, 4);
  ASSERT_STR_EQ(connections[2].protocol, "UDP");
  ASSERT_STR_EQ(connections[2].local_addr, "127.0.0.1:53");
  ASSERT_STR_EQ(connections[2].state, "UNCONN");
  ASSERT_STR_EQ(connections[3].protocol, "UDP");
  ASSERT_STR_EQ(connections[3].local_addr, "0.0.0.0:68");
  ASSERT_STR_EQ(connections[3].state, "UNCONN");

  return 0;
}

static int test_tcp_state_names(void) {
  ASSERT_STR_EQ(trfx_tcp_state_name(1), "ESTABLISHED");
  ASSERT_STR_EQ(trfx_tcp_state_name(2), "SYN_SENT");
  ASSERT_STR_EQ(trfx_tcp_state_name(3), "SYN_RECV");
  ASSERT_STR_EQ(trfx_tcp_state_name(4), "FIN_WAIT1");
  ASSERT_STR_EQ(trfx_tcp_state_name(5), "FIN_WAIT2");
  ASSERT_STR_EQ(trfx_tcp_state_name(6), "TIME_WAIT");
  ASSERT_STR_EQ(trfx_tcp_state_name(7), "CLOSE");
  ASSERT_STR_EQ(trfx_tcp_state_name(8), "CLOSE_WAIT");
  ASSERT_STR_EQ(trfx_tcp_state_name(9), "LAST_ACK");
  ASSERT_STR_EQ(trfx_tcp_state_name(10), "LISTEN");
  ASSERT_STR_EQ(trfx_tcp_state_name(11), "CLOSING");
  ASSERT_STR_EQ(trfx_tcp_state_name(12), "NEW_SYN_RECV");
  ASSERT_STR_EQ(trfx_tcp_state_name(99), "UNKNOWN");

  return 0;
}

static int test_udp_state_names(void) {
  ASSERT_STR_EQ(trfx_udp_state_name(1), "ESTABLISHED");
  ASSERT_STR_EQ(trfx_udp_state_name(7), "UNCONN");
  ASSERT_STR_EQ(trfx_udp_state_name(99), "UNKNOWN");

  return 0;
}

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
  if (test_format_bytes() != 0)
    return 1;

  if (test_read_config() != 0)
    return 1;

  if (test_parse_cli() != 0)
    return 1;

  if (test_get_version() != 0)
    return 1;

  if (test_parse_connection_fixtures() != 0)
    return 1;

  if (test_tcp_state_names() != 0)
    return 1;

  if (test_udp_state_names() != 0)
    return 1;

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
