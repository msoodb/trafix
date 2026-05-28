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

#include "trfx_version.h"

TrfxCliOptions trfx_parse_cli(int argc, char **argv) {
  TrfxCliOptions options = {TRFX_CLI_MODE_TUI, TRFX_CLI_OUTPUT_TEXT, {0}};

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

    if (strcmp(arg, "interfaces") == 0) {
      options.mode = TRFX_CLI_MODE_INTERFACES;
      return options;
    }

    if (strcmp(arg, "connections") == 0) {
      options.mode = TRFX_CLI_MODE_CONNECTIONS;
      return options;
    }

    if (strcmp(arg, "system") == 0) {
      options.mode = TRFX_CLI_MODE_SYSTEM;
      return options;
    }
  }

  if (argc == 3) {
    const char *command = argv[1];
    const char *flag = argv[2];

    if (strcmp(flag, "--json") == 0) {
      if (strcmp(command, "interfaces") == 0) {
        options.mode = TRFX_CLI_MODE_INTERFACES;
        options.output_format = TRFX_CLI_OUTPUT_JSON;
        return options;
      }

      if (strcmp(command, "connections") == 0) {
        options.mode = TRFX_CLI_MODE_CONNECTIONS;
        options.output_format = TRFX_CLI_OUTPUT_JSON;
        return options;
      }

      if (strcmp(command, "system") == 0) {
        options.mode = TRFX_CLI_MODE_SYSTEM;
        options.output_format = TRFX_CLI_OUTPUT_JSON;
        return options;
      }
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

void trfx_print_cli_help(void) {
  printf("Usage: trafix [OPTION]\n");
  printf("\n");
  printf("Launch Trafix Linux monitoring TUI.\n");
  printf("\n");
  printf("Options:\n");
  printf("  -h, --help       Show this help message\n");
  printf("  -v, --version    Show version information\n");
}

void trfx_print_cli_version(void) {
  printf("trafix %s\n", trfx_get_version());
}

void trfx_print_cli_error(const TrfxCliOptions *options) {
  const char *message = "invalid arguments";

  if (options && options->error[0] != '\0') {
    message = options->error;
  }

  fprintf(stderr, "trafix: %s\n", message);
  fprintf(stderr, "Try 'trafix --help' for usage.\n");
}

void trfx_print_cli_not_implemented(const char *command) {
  fprintf(stderr, "trafix: command not implemented yet: %s\n", command);
}
