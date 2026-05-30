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

static int is_command(const char *arg, TrfxCliMode *mode) {
  if (strcmp(arg, "interfaces") == 0) {
    *mode = TRFX_CLI_MODE_INTERFACES;
    return 1;
  }

  if (strcmp(arg, "connections") == 0) {
    *mode = TRFX_CLI_MODE_CONNECTIONS;
    return 1;
  }

  if (strcmp(arg, "listeners") == 0) {
    *mode = TRFX_CLI_MODE_LISTENERS;
    return 1;
  }

  if (strcmp(arg, "system") == 0) {
    *mode = TRFX_CLI_MODE_SYSTEM;
    return 1;
  }

  if (strcmp(arg, "diagnostics") == 0) {
    *mode = TRFX_CLI_MODE_DIAGNOSTICS;
    return 1;
  }

  if (strcmp(arg, "drop") == 0) {
    *mode = TRFX_CLI_MODE_DROP;
    return 1;
  }

  if (strcmp(arg, "kill") == 0) {
    *mode = TRFX_CLI_MODE_KILL;
    return 1;
  }

  return 0;
}

static int is_supported_state_filter(const char *state) {
  const char *states[] = {"ESTABLISHED", "SYN_SENT", "SYN_RECV", "FIN_WAIT1",
                          "FIN_WAIT2",   "TIME_WAIT", "CLOSE",    "CLOSE_WAIT",
                          "LAST_ACK",    "LISTEN",    "CLOSING",  "NEW_SYN_RECV",
                          "UNCONN",      "UNKNOWN",   NULL};

  for (int i = 0; states[i]; i++) {
    if (strcmp(state, states[i]) == 0)
      return 1;
  }

  return 0;
}

static void set_unknown_argument(TrfxCliOptions *options, const char *arg) {
  options->mode = TRFX_CLI_MODE_INVALID;
  snprintf(options->error, sizeof(options->error), "unknown argument: %s",
           arg ? arg : "");
}

TrfxCliOptions trfx_parse_cli(int argc, char **argv) {
  TrfxCliOptions options = {
      .mode = TRFX_CLI_MODE_TUI,
      .output_format = TRFX_CLI_OUTPUT_TEXT,
      .has_proto_filter = 0,
      .has_state_filter = 0,
      .has_target_pid = 0,
      .has_drop_target = 0,
      .confirmed = 0,
  };

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

    if (strcmp(arg, "kill") == 0) {
      options.mode = TRFX_CLI_MODE_INVALID;
      snprintf(options.error, sizeof(options.error), "kill requires a PID");
      return options;
    }

    if (strcmp(arg, "drop") == 0) {
      options.mode = TRFX_CLI_MODE_INVALID;
      snprintf(options.error, sizeof(options.error),
               "drop requires a target type and endpoints");
      return options;
    }

    if (strcmp(arg, "diagnostics") == 0) {
      options.mode = TRFX_CLI_MODE_DIAGNOSTICS;
      return options;
    }

    if (is_command(arg, &options.mode)) {
      return options;
    }
  }

  if (argc >= 3 && strcmp(argv[1], "kill") == 0) {
    options.mode = TRFX_CLI_MODE_KILL;
    snprintf(options.target_pid, sizeof(options.target_pid), "%s", argv[2]);
    options.has_target_pid = 1;

    for (int i = 3; i < argc; i++) {
      if (strcmp(argv[i], "--yes") == 0 || strcmp(argv[i], "-y") == 0) {
        options.confirmed = 1;
      } else {
        set_unknown_argument(&options, argv[i]);
        return options;
      }
    }

    return options;
  }

  if (argc >= 6 && strcmp(argv[1], "drop") == 0) {
    options.mode = TRFX_CLI_MODE_DROP;
    snprintf(options.drop_kind, sizeof(options.drop_kind), "%s", argv[2]);
    if (strcmp(argv[3], "tcp") == 0 || strcmp(argv[3], "TCP") == 0) {
      snprintf(options.drop_proto, sizeof(options.drop_proto), "TCP");
    } else if (strcmp(argv[3], "udp") == 0 || strcmp(argv[3], "UDP") == 0) {
      snprintf(options.drop_proto, sizeof(options.drop_proto), "UDP");
    } else {
      set_unknown_argument(&options, argv[3]);
      return options;
    }
    snprintf(options.drop_local, sizeof(options.drop_local), "%s", argv[4]);
    snprintf(options.drop_remote, sizeof(options.drop_remote), "%s", argv[5]);
    options.has_drop_target = 1;

    if (strcmp(options.drop_kind, "connection") != 0 &&
        strcmp(options.drop_kind, "socket") != 0) {
      set_unknown_argument(&options, options.drop_kind);
      return options;
    }

    for (int i = 6; i < argc; i++) {
      if (strcmp(argv[i], "--yes") == 0 || strcmp(argv[i], "-y") == 0) {
        options.confirmed = 1;
      } else {
        set_unknown_argument(&options, argv[i]);
        return options;
      }
    }

    return options;
  }

  if (argc >= 3 && is_command(argv[1], &options.mode)) {
    int i = 2;
    while (i < argc) {
      const char *arg = argv[i];

      if (strcmp(arg, "--json") == 0) {
        if (options.output_format == TRFX_CLI_OUTPUT_JSON) {
          set_unknown_argument(&options, arg);
          return options;
        }
        options.output_format = TRFX_CLI_OUTPUT_JSON;
        i++;
        continue;
      }

      if (strcmp(arg, "--proto") == 0) {
        if (options.mode != TRFX_CLI_MODE_CONNECTIONS || i + 1 >= argc ||
            options.has_proto_filter) {
          set_unknown_argument(&options, arg);
          return options;
        }

        const char *value = argv[i + 1];
        if (strcmp(value, "tcp") == 0 || strcmp(value, "TCP") == 0) {
          snprintf(options.proto_filter, sizeof(options.proto_filter), "TCP");
        } else if (strcmp(value, "udp") == 0 || strcmp(value, "UDP") == 0) {
          snprintf(options.proto_filter, sizeof(options.proto_filter), "UDP");
        } else {
          set_unknown_argument(&options, value);
          return options;
        }

        options.has_proto_filter = 1;
        i += 2;
        continue;
      }

      if (strcmp(arg, "--state") == 0) {
        if (options.mode != TRFX_CLI_MODE_CONNECTIONS || i + 1 >= argc ||
            options.has_state_filter) {
          set_unknown_argument(&options, arg);
          return options;
        }

        const char *value = argv[i + 1];
        if (!is_supported_state_filter(value)) {
          set_unknown_argument(&options, value);
          return options;
        }

        snprintf(options.state_filter, sizeof(options.state_filter), "%s",
                 value);
        options.has_state_filter = 1;
        i += 2;
        continue;
      }

      set_unknown_argument(&options, arg);
      return options;
    }

    return options;
  }

  options.mode = TRFX_CLI_MODE_INVALID;
  if (argc > 1 && argv[1]) {
    set_unknown_argument(&options, argv[1]);
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
  printf("\n");
  printf("Commands:\n");
  printf("  interfaces       Print interface counters\n");
  printf("  connections      Print TCP/UDP connections\n");
  printf("  listeners        Print listening sockets\n");
  printf("  system           Print system overview\n");
  printf("  diagnostics      Print a troubleshooting snapshot\n");
  printf("  kill PID         Request a controlled process kill\n");
  printf("  drop TYPE ...    Request a controlled socket drop\n");
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
