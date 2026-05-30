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
#include <sys/stat.h>
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

static int test_runtime_profile_load(void) {
  char base_dir[] = "/tmp/trafix-config-XXXXXX";
  char profile_dir[512];
  char base_path[512];
  char profile_path[512];
  char error[128];
  FILE *file;

  if (!mkdtemp(base_dir)) {
    perror("mkdtemp");
    return 1;
  }

  snprintf(profile_dir, sizeof(profile_dir), "%s/profiles", base_dir);
  if (mkdir(profile_dir, 0700) != 0) {
    perror("mkdir");
    rmdir(base_dir);
    return 1;
  }

  snprintf(base_path, sizeof(base_path), "%s/base.cfg", base_dir);
  file = fopen(base_path, "w");
  if (!file) {
    perror("fopen");
    rmdir(profile_dir);
    rmdir(base_dir);
    return 1;
  }
  fputs("TEMP_WARN_RED = 81\n", file);
  fputs("SHOW_TOP_PANELS = TRUE\n", file);
  fclose(file);

  if (strlen(profile_dir) + strlen("/work.cfg") + 1 > sizeof(profile_path)) {
    rmdir(profile_dir);
    rmdir(base_dir);
    return 1;
  }
  strcpy(profile_path, profile_dir);
  strcat(profile_path, "/work.cfg");
  file = fopen(profile_path, "w");
  if (!file) {
    perror("fopen");
    unlink(base_path);
    rmdir(profile_dir);
    rmdir(base_dir);
    return 1;
  }
  fputs("ALERT_MEMORY_WARN_PERCENT = 77\n", file);
  fputs("ALERT_REQUIRE_DNS = FALSE\n", file);
  fclose(file);

  setenv("TRAFX_CONFIG_FILE", base_path, 1);
  setenv("TRAFX_PROFILE_DIR", profile_dir, 1);

  TEMP_WARN_RED = 75;
  SHOW_TOP_PANELS = 0;
  ALERT_MEMORY_WARN_PERCENT = 90;
  ALERT_REQUIRE_DNS = 1;

  ASSERT_INT_EQ(trfx_load_runtime_config("work", error, sizeof(error)), 1);
  ASSERT_INT_EQ(TEMP_WARN_RED, 81);
  ASSERT_INT_EQ(SHOW_TOP_PANELS, 1);
  ASSERT_INT_EQ(ALERT_MEMORY_WARN_PERCENT, 77);
  ASSERT_INT_EQ(ALERT_REQUIRE_DNS, 0);

  ASSERT_INT_EQ(trfx_load_runtime_config("missing", error, sizeof(error)), 0);
  ASSERT_INT_EQ(strstr(error, "profile not found") != NULL, 1);

  unsetenv("TRAFX_CONFIG_FILE");
  unsetenv("TRAFX_PROFILE_DIR");
  unlink(profile_path);
  unlink(base_path);
  rmdir(profile_dir);
  rmdir(base_dir);

  return 0;
}

int main(void) {
  if (test_read_config() != 0)
    return 1;

  if (test_runtime_profile_load() != 0)
    return 1;

  return 0;
}
