/*
 * Copyright (C) 2025 Masoud Bolhassani <masoud.bolhassani@gmail.com>
 *
 * This file is part of Trafix.
 *
 * Trafix is released under the GNU General Public License v3 (GPL-3.0).
 * See LICENSE file for details.
 */

#include "trfx_cli.h"

#include <stdio.h>
#include <string.h>

TrfxCliOptions trfx_parse_cli(int argc, char **argv) {
  TrfxCliOptions options = {TRFX_CLI_MODE_TUI, {0}};

  if (argc <= 1) {
    return options;
  }

  if (argc == 2) {
    const char *arg = argv[1];

    if (strcmp(arg, "--help") == 0 || strcmp(arg, "-h") == 0) {
      options.mode = TRFX_CLI_MODE_HELP;
      return options;
    }

    if (strcmp(arg, "--version") == 0 || strcmp(arg, "-v") == 0) {
      options.mode = TRFX_CLI_MODE_VERSION;
      return options;
    }
  }

  options.mode = TRFX_CLI_MODE_INVALID;
  if (argc > 1 && argv[1]) {
    snprintf(options.error, sizeof(options.error), "unknown argument: %s",
             argv[1]);
  } else {
    snprintf(options.error, sizeof(options.error), "invalid arguments");
  }

  return options;
}
