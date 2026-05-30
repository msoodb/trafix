/*
 * Copyright (C) 2025 Masoud Bolhassani <masoud.bolhassani@gmail.com>
 *
 * This file is part of Trafix.
 *
 * Trafix is released under the GNU General Public License v3 (GPL-3.0).
 * See LICENSE file for details.
 */

#ifndef TRFX_CLI_H
#define TRFX_CLI_H

#define TRFX_CLI_ERROR_MAX 128
#define TRFX_CLI_PROTO_FILTER_MAX 8
#define TRFX_CLI_STATE_FILTER_MAX 32
#define TRFX_CLI_TARGET_PID_MAX 16
#define TRFX_CLI_DROP_KIND_MAX 16
#define TRFX_CLI_ENDPOINT_MAX 64

typedef enum {
  TRFX_CLI_MODE_TUI = 0,
  TRFX_CLI_MODE_HELP,
  TRFX_CLI_MODE_VERSION,
  TRFX_CLI_MODE_INTERFACES,
  TRFX_CLI_MODE_CONNECTIONS,
  TRFX_CLI_MODE_LISTENERS,
  TRFX_CLI_MODE_SYSTEM,
  TRFX_CLI_MODE_DIAGNOSTICS,
  TRFX_CLI_MODE_DROP,
  TRFX_CLI_MODE_KILL,
  TRFX_CLI_MODE_INVALID
} TrfxCliMode;

typedef enum {
  TRFX_CLI_OUTPUT_TEXT = 0,
  TRFX_CLI_OUTPUT_JSON
} TrfxCliOutputFormat;

typedef enum {
  TRFX_EXIT_OK = 0,
  TRFX_EXIT_ERROR = 1,
  TRFX_EXIT_DATA_UNAVAILABLE = 2
} TrfxExitCode;

typedef struct {
  TrfxCliMode mode;
  TrfxCliOutputFormat output_format;
  int has_proto_filter;
  char proto_filter[TRFX_CLI_PROTO_FILTER_MAX];
  int has_state_filter;
  char state_filter[TRFX_CLI_STATE_FILTER_MAX];
  int has_target_pid;
  char target_pid[TRFX_CLI_TARGET_PID_MAX];
  int has_drop_target;
  char drop_kind[TRFX_CLI_DROP_KIND_MAX];
  char drop_proto[TRFX_CLI_PROTO_FILTER_MAX];
  char drop_local[TRFX_CLI_ENDPOINT_MAX];
  char drop_remote[TRFX_CLI_ENDPOINT_MAX];
  int confirmed;
  char error[TRFX_CLI_ERROR_MAX];
} TrfxCliOptions;

TrfxCliOptions trfx_parse_cli(int argc, char **argv);
void trfx_print_cli_help(void);
void trfx_print_cli_version(void);
void trfx_print_cli_error(const TrfxCliOptions *options);
void trfx_print_cli_not_implemented(const char *command);

#endif // TRFX_CLI_H
