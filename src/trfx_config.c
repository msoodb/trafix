/*
 * Copyright (C) 2025 Masoud Bolhassani <masoud.bolhassani@gmail.com>
 *
 * This file is part of Trafix.
 *
 * Trafix is released under the GNU General Public License v3 (GPL-3.0).
 * See LICENSE file for details.
 */

#include "trfx_config.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

int TEMP_WARN_YELLOW = 50;
int TEMP_WARN_RED = 75;
int ROW2_MODULES = 3;
int SHOW_TOP_PANELS = 1;
int TUI_REFRESH_INTERVAL_MS = 1000;
int TUI_PAUSE_INTERVAL_MS = 100;
int TUI_READY_CHECK_INTERVAL_MS = 10;
int TUI_SMALL_PANEL_REFRESH_MS = 2000;

static int parse_bounded_int(const char *value, int default_value, int min_value,
                             int max_value, const char *name, int line_num) {
  char *end = NULL;
  long parsed = strtol(value, &end, 10);

  while (end && isspace((unsigned char)*end))
    end++;

  if (!value[0] || !end || *end != '\0') {
    fprintf(stderr,
            "Warning: Invalid integer for %s at line %d. Defaulting to %d.\n",
            name, line_num, default_value);
    return default_value;
  }

  if (parsed < min_value || parsed > max_value) {
    fprintf(stderr,
            "Warning: %s out of range (%d-%d), got %ld. Defaulting to %d.\n",
            name, min_value, max_value, parsed, default_value);
    return default_value;
  }

  return (int)parsed;
}

static void trim_whitespace(char *str) {
  char *end;

  // Trim leading space
  while (isspace((unsigned char)*str))
    str++;

  // All spaces?
  if (*str == 0)
    return;

  // Trim trailing space
  end = str + strlen(str) - 1;
  while (end > str && isspace((unsigned char)*end))
    end--;

  // Write new null terminator
  *(end + 1) = 0;
}

void read_config(const char *config_file) {
  FILE *file = fopen(config_file, "r");
  if (!file) {
    fprintf(stderr, "Warning: Could not open config file: %s\n", config_file);
    return;
  }

  char line[256];
  int line_num = 0;

  while (fgets(line, sizeof(line), file)) {
    line_num++;

    // Remove newline
    line[strcspn(line, "\n")] = 0;

    // Skip empty lines and comments
    if (line[0] == '#' || line[0] == '\0')
      continue;

    // Split line into key and value
    char *key = strtok(line, "=");
    char *value = strtok(NULL, "=");

    if (!key || !value) {
      fprintf(stderr, "Warning: Invalid config entry at line %d\n", line_num);
      continue;
    }

    trim_whitespace(key);
    trim_whitespace(value);

    if (strcmp(key, "TEMP_WARN_YELLOW") == 0) {
      TEMP_WARN_YELLOW = atoi(value);
    } else if (strcmp(key, "TEMP_WARN_RED") == 0) {
      TEMP_WARN_RED = atoi(value);
    } else if (strcmp(key, "ROW2_MODULES") == 0) {
      ROW2_MODULES = atoi(value);
      if (ROW2_MODULES < 1 || ROW2_MODULES > 3) {
        fprintf(stderr,
                "Warning: ROW2_MODULES out of range (1–3), got %d. Defaulting "
                "to 3.\n",
                ROW2_MODULES);
        ROW2_MODULES = 3;
      }
    } else if (strcmp(key, "SHOW_TOP_PANELS") == 0) {
      SHOW_TOP_PANELS = parse_bounded_int(value, 1, 0, 1, key, line_num);
    } else if (strcmp(key, "TUI_REFRESH_INTERVAL_MS") == 0) {
      TUI_REFRESH_INTERVAL_MS =
          parse_bounded_int(value, 1000, 250, 10000, key, line_num);
    } else if (strcmp(key, "TUI_PAUSE_INTERVAL_MS") == 0) {
      TUI_PAUSE_INTERVAL_MS =
          parse_bounded_int(value, 100, 25, 1000, key, line_num);
    } else if (strcmp(key, "TUI_READY_CHECK_INTERVAL_MS") == 0) {
      TUI_READY_CHECK_INTERVAL_MS =
          parse_bounded_int(value, 10, 1, 250, key, line_num);
    } else if (strcmp(key, "TUI_SMALL_PANEL_REFRESH_MS") == 0) {
      TUI_SMALL_PANEL_REFRESH_MS =
          parse_bounded_int(value, 2000, 250, 10000, key, line_num);
    } else {
      fprintf(stderr, "Warning: Unknown config key '%s' at line %d\n", key,
              line_num);
    }
  }

  fclose(file);
}
