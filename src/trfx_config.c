/*
 * Copyright (C) 2025 Masoud Bolhassani <masoud.bolhassani@gmail.com>
 *
 * This file is part of Trafix.
 *
 * Trafix is released under the GNU General Public License v3 (GPL-3.0).
 * See LICENSE file for details.
 */

#include <stdio.h>
#include <string.h>
#include "trfx_config.h"

int temp_warn_yellow = 50;
int temp_warn_red = 75;

// Global configuration variables
void read_config(const char *config_file) {
  FILE *file = fopen(config_file, "r");
  if (!file) {
    return;
  }

  char line[256];
  while (fgets(line, sizeof(line), file)) {
    line[strcspn(line, "\n")] = 0;
    if (line[0] == '#' || line[0] == '\0')
      continue;
    if (strncmp(line, "temp_warn_yellow", 16) == 0) {
      sscanf(line, "temp_warn_yellow = %d", &temp_warn_yellow);
    } else if (strncmp(line, "temp_warn_red", 13) == 0) {
      sscanf(line, "temp_warn_red = %d", &temp_warn_red);
    }
  }
  fclose(file);
}
