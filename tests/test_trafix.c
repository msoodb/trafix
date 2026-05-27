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

int main(void) {
  if (test_format_bytes() != 0)
    return 1;

  if (test_read_config() != 0)
    return 1;

  if (test_parse_cli() != 0)
    return 1;

  if (test_get_version() != 0)
    return 1;

  return 0;
}
