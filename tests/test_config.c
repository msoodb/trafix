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
  fputs("ALERT_MEMORY_WARN_PERCENT = 88\n", file);
  fputs("ALERT_DISK_WARN_PERCENT = 89\n", file);
  fputs("ALERT_REQUIRE_DEFAULT_ROUTE = FALSE\n", file);
  fputs("ALERT_REQUIRE_DNS = 0\n", file);
  fputs("ROW2_MODULES = 2\n", file);
  fputs("TUI_REFRESH_INTERVAL_MS = 750\n", file);
  fputs("TUI_PAUSE_INTERVAL_MS = 75\n", file);
  fputs("TUI_READY_CHECK_INTERVAL_MS = 15\n", file);
  fputs("TUI_SMALL_PANEL_REFRESH_MS = 1500\n", file);
  fclose(file);

  TEMP_WARN_YELLOW = 50;
  TEMP_WARN_RED = 75;
  ALERT_MEMORY_WARN_PERCENT = 90;
  ALERT_DISK_WARN_PERCENT = 90;
  ALERT_REQUIRE_DEFAULT_ROUTE = 1;
  ALERT_REQUIRE_DNS = 1;
  ROW2_MODULES = 3;
  TUI_REFRESH_INTERVAL_MS = 1000;
  TUI_PAUSE_INTERVAL_MS = 100;
  TUI_READY_CHECK_INTERVAL_MS = 10;
  TUI_SMALL_PANEL_REFRESH_MS = 2000;

  read_config(path);
  unlink(path);

  ASSERT_INT_EQ(TEMP_WARN_YELLOW, 42);
  ASSERT_INT_EQ(TEMP_WARN_RED, 84);
  ASSERT_INT_EQ(ALERT_MEMORY_WARN_PERCENT, 88);
  ASSERT_INT_EQ(ALERT_DISK_WARN_PERCENT, 89);
  ASSERT_INT_EQ(ALERT_REQUIRE_DEFAULT_ROUTE, 0);
  ASSERT_INT_EQ(ALERT_REQUIRE_DNS, 0);
  ASSERT_INT_EQ(ROW2_MODULES, 2);
  ASSERT_INT_EQ(TUI_REFRESH_INTERVAL_MS, 750);
  ASSERT_INT_EQ(TUI_PAUSE_INTERVAL_MS, 75);
  ASSERT_INT_EQ(TUI_READY_CHECK_INTERVAL_MS, 15);
  ASSERT_INT_EQ(TUI_SMALL_PANEL_REFRESH_MS, 1500);

  return 0;
}

int main(void) {
  if (test_read_config() != 0)
    return 1;

  return 0;
}
