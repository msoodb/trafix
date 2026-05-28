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
  ASSERT_STR_EQ(options.error, "");

  char *connections_argv[] = {"trafix", "connections"};
  options = trfx_parse_cli(2, connections_argv);
  ASSERT_MODE_EQ(options.mode, TRFX_CLI_MODE_CONNECTIONS);
  ASSERT_STR_EQ(options.error, "");

  char *system_argv[] = {"trafix", "system"};
  options = trfx_parse_cli(2, system_argv);
  ASSERT_MODE_EQ(options.mode, TRFX_CLI_MODE_SYSTEM);
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

  char *interfaces_plus_extra_argv[] = {"trafix", "interfaces", "--json"};
  options = trfx_parse_cli(3, interfaces_plus_extra_argv);
  ASSERT_MODE_EQ(options.mode, TRFX_CLI_MODE_INVALID);
  ASSERT_STR_EQ(options.error, "unknown argument: interfaces");

  return 0;
}

int main(void) {
  if (test_parse_cli() != 0)
    return 1;

  return 0;
}
