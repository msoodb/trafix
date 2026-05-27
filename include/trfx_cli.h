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

typedef enum {
  TRFX_CLI_MODE_TUI = 0,
  TRFX_CLI_MODE_HELP,
  TRFX_CLI_MODE_VERSION,
  TRFX_CLI_MODE_INVALID
} TrfxCliMode;

typedef struct {
  TrfxCliMode mode;
  char error[TRFX_CLI_ERROR_MAX];
} TrfxCliOptions;

TrfxCliOptions trfx_parse_cli(int argc, char **argv);

#endif // TRFX_CLI_H
