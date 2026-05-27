/*
 * Copyright (C) 2025 Masoud Bolhassani <masoud.bolhassani@gmail.com>
 *
 * This file is part of Trafix.
 *
 * Trafix is released under the GNU General Public License v3 (GPL-3.0).
 * See LICENSE file for details.
 */

#include "trfx_config.h"
#include "trfx_utils.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

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

int main(void) {
  if (test_format_bytes() != 0)
    return 1;

  if (test_read_config() != 0)
    return 1;

  return 0;
}
