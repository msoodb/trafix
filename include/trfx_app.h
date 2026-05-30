/*
 * Copyright (C) 2025 Masoud Bolhassani <masoud.bolhassani@gmail.com>
 *
 * This file is part of Trafix.
 *
 * Trafix is released under the GNU General Public License v3 (GPL-3.0).
 * See LICENSE file for details.
 */

#ifndef TRFX_APP_H
#define TRFX_APP_H

#include "trfx_cli.h"

int trfx_run_tui(void);
int trfx_run_interfaces_command(TrfxCliOutputFormat output_format);
int trfx_run_connections_command(const TrfxCliOptions *options);
int trfx_run_listeners_command(TrfxCliOutputFormat output_format);
int trfx_run_system_command(TrfxCliOutputFormat output_format);
int trfx_run_diagnostics_command(void);
int trfx_run_kill_command(const TrfxCliOptions *options);
int trfx_run_drop_command(const TrfxCliOptions *options);

#endif // TRFX_APP_H
