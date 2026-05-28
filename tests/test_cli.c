/*
 * Copyright (C) 2025 Masoud Bolhassani <masoud.bolhassani@gmail.com>
 *
 * This file is part of Trafix.
 *
 * Trafix is released under the GNU General Public License v3 (GPL-3.0).
 * See LICENSE file for details.
 */

#include "test_common.h"
#include "trfx_cli.h"

static int test_parse_cli(void) {
  char *default_argv[] = {"trafix"};
  TrfxCliOptions options = trfx_parse_cli(1, default_argv);
  ASSERT_MODE_EQ(options.mode, TRFX_CLI_MODE_TUI);
  ASSERT_INT_EQ(options.output_format, TRFX_CLI_OUTPUT_TEXT);
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

  char *interfaces_argv[] = {"trafix", "interfaces"};
  options = trfx_parse_cli(2, interfaces_argv);
  ASSERT_MODE_EQ(options.mode, TRFX_CLI_MODE_INTERFACES);
  ASSERT_INT_EQ(options.output_format, TRFX_CLI_OUTPUT_TEXT);
  ASSERT_STR_EQ(options.error, "");

  char *connections_argv[] = {"trafix", "connections"};
  options = trfx_parse_cli(2, connections_argv);
  ASSERT_MODE_EQ(options.mode, TRFX_CLI_MODE_CONNECTIONS);
  ASSERT_INT_EQ(options.output_format, TRFX_CLI_OUTPUT_TEXT);
  ASSERT_INT_EQ(options.has_proto_filter, 0);
  ASSERT_INT_EQ(options.has_state_filter, 0);
  ASSERT_STR_EQ(options.error, "");

  char *system_argv[] = {"trafix", "system"};
  options = trfx_parse_cli(2, system_argv);
  ASSERT_MODE_EQ(options.mode, TRFX_CLI_MODE_SYSTEM);
  ASSERT_INT_EQ(options.output_format, TRFX_CLI_OUTPUT_TEXT);
  ASSERT_STR_EQ(options.error, "");

  char *interfaces_json_argv[] = {"trafix", "interfaces", "--json"};
  options = trfx_parse_cli(3, interfaces_json_argv);
  ASSERT_MODE_EQ(options.mode, TRFX_CLI_MODE_INTERFACES);
  ASSERT_INT_EQ(options.output_format, TRFX_CLI_OUTPUT_JSON);
  ASSERT_STR_EQ(options.error, "");

  char *connections_json_argv[] = {"trafix", "connections", "--json"};
  options = trfx_parse_cli(3, connections_json_argv);
  ASSERT_MODE_EQ(options.mode, TRFX_CLI_MODE_CONNECTIONS);
  ASSERT_INT_EQ(options.output_format, TRFX_CLI_OUTPUT_JSON);
  ASSERT_STR_EQ(options.error, "");

  char *connections_proto_tcp_argv[] = {"trafix", "connections", "--proto",
                                        "tcp"};
  options = trfx_parse_cli(4, connections_proto_tcp_argv);
  ASSERT_MODE_EQ(options.mode, TRFX_CLI_MODE_CONNECTIONS);
  ASSERT_INT_EQ(options.output_format, TRFX_CLI_OUTPUT_TEXT);
  ASSERT_INT_EQ(options.has_proto_filter, 1);
  ASSERT_STR_EQ(options.proto_filter, "TCP");
  ASSERT_STR_EQ(options.error, "");

  char *connections_proto_udp_argv[] = {"trafix", "connections", "--proto",
                                        "udp"};
  options = trfx_parse_cli(4, connections_proto_udp_argv);
  ASSERT_MODE_EQ(options.mode, TRFX_CLI_MODE_CONNECTIONS);
  ASSERT_INT_EQ(options.has_proto_filter, 1);
  ASSERT_STR_EQ(options.proto_filter, "UDP");
  ASSERT_STR_EQ(options.error, "");

  char *connections_state_argv[] = {"trafix", "connections", "--state",
                                    "ESTABLISHED"};
  options = trfx_parse_cli(4, connections_state_argv);
  ASSERT_MODE_EQ(options.mode, TRFX_CLI_MODE_CONNECTIONS);
  ASSERT_INT_EQ(options.has_state_filter, 1);
  ASSERT_STR_EQ(options.state_filter, "ESTABLISHED");
  ASSERT_STR_EQ(options.error, "");

  char *connections_json_proto_argv[] = {"trafix", "connections", "--json",
                                         "--proto", "tcp"};
  options = trfx_parse_cli(5, connections_json_proto_argv);
  ASSERT_MODE_EQ(options.mode, TRFX_CLI_MODE_CONNECTIONS);
  ASSERT_INT_EQ(options.output_format, TRFX_CLI_OUTPUT_JSON);
  ASSERT_INT_EQ(options.has_proto_filter, 1);
  ASSERT_STR_EQ(options.proto_filter, "TCP");
  ASSERT_STR_EQ(options.error, "");

  char *connections_proto_state_argv[] = {"trafix", "connections", "--proto",
                                          "tcp", "--state", "LISTEN"};
  options = trfx_parse_cli(6, connections_proto_state_argv);
  ASSERT_MODE_EQ(options.mode, TRFX_CLI_MODE_CONNECTIONS);
  ASSERT_INT_EQ(options.has_proto_filter, 1);
  ASSERT_INT_EQ(options.has_state_filter, 1);
  ASSERT_STR_EQ(options.proto_filter, "TCP");
  ASSERT_STR_EQ(options.state_filter, "LISTEN");
  ASSERT_STR_EQ(options.error, "");

  char *system_json_argv[] = {"trafix", "system", "--json"};
  options = trfx_parse_cli(3, system_json_argv);
  ASSERT_MODE_EQ(options.mode, TRFX_CLI_MODE_SYSTEM);
  ASSERT_INT_EQ(options.output_format, TRFX_CLI_OUTPUT_JSON);
  ASSERT_STR_EQ(options.error, "");

  char *unknown_command_argv[] = {"trafix", "listeners"};
  options = trfx_parse_cli(2, unknown_command_argv);
  ASSERT_MODE_EQ(options.mode, TRFX_CLI_MODE_INVALID);
  ASSERT_STR_EQ(options.error, "unknown argument: listeners");

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

  char *json_before_command_argv[] = {"trafix", "--json", "interfaces"};
  options = trfx_parse_cli(3, json_before_command_argv);
  ASSERT_MODE_EQ(options.mode, TRFX_CLI_MODE_INVALID);
  ASSERT_STR_EQ(options.error, "unknown argument: --json");

  char *interfaces_plus_extra_argv[] = {"trafix", "interfaces", "--bad"};
  options = trfx_parse_cli(3, interfaces_plus_extra_argv);
  ASSERT_MODE_EQ(options.mode, TRFX_CLI_MODE_INVALID);
  ASSERT_STR_EQ(options.error, "unknown argument: --bad");

  char *interfaces_proto_argv[] = {"trafix", "interfaces", "--proto", "tcp"};
  options = trfx_parse_cli(4, interfaces_proto_argv);
  ASSERT_MODE_EQ(options.mode, TRFX_CLI_MODE_INVALID);
  ASSERT_STR_EQ(options.error, "unknown argument: --proto");

  char *bad_proto_argv[] = {"trafix", "connections", "--proto", "icmp"};
  options = trfx_parse_cli(4, bad_proto_argv);
  ASSERT_MODE_EQ(options.mode, TRFX_CLI_MODE_INVALID);
  ASSERT_STR_EQ(options.error, "unknown argument: icmp");

  char *bad_state_argv[] = {"trafix", "connections", "--state", "BOGUS"};
  options = trfx_parse_cli(4, bad_state_argv);
  ASSERT_MODE_EQ(options.mode, TRFX_CLI_MODE_INVALID);
  ASSERT_STR_EQ(options.error, "unknown argument: BOGUS");

  return 0;
}

int main(void) {
  if (test_parse_cli() != 0)
    return 1;

  return 0;
}
