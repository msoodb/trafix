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
    } else {
      fprintf(stderr, "Warning: Unknown config key '%s' at line %d\n", key,
              line_num);
    }
  }

  fclose(file);
}
