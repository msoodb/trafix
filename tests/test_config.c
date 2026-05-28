/*
 * Copyright (C) 2025 Masoud Bolhassani <masoud.bolhassani@gmail.com>
 *
 * This file is part of Trafix.
 *
 * Trafix is released under the GNU General Public License v3 (GPL-3.0).
 * See LICENSE file for details.
 */

#include "test_common.h"
#include "trfx_config.h"

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

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
  if (test_read_config() != 0)
    return 1;

  return 0;
}
