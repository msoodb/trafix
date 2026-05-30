/*
 * Copyright (C) 2025 Masoud Bolhassani <masoud.bolhassani@gmail.com>
 *
 * This file is part of Trafix.
 *
 * Trafix is released under the GNU General Public License v3 (GPL-3.0).
 * See LICENSE file for details.
 */

#ifndef TRFX_CLI_OUTPUT_H
#define TRFX_CLI_OUTPUT_H

#include <stdio.h>

#include "trfx_cli.h"
#include "trfx_connections.h"
#include "trfx_diagnostics.h"
#include "trfx_netinfo.h"
#include "trfx_sysinfo.h"

void trfx_print_interfaces_text(FILE *out,
                                const TrfxInterfaceStatsResult *result);
void trfx_print_interfaces_json(FILE *out,
                                const TrfxInterfaceStatsResult *result);
void trfx_print_connections_text(FILE *out, const ConnectionInfo connections[],
                                 int count, const TrfxCliOptions *options);
void trfx_print_connections_json(FILE *out, const ConnectionInfo connections[],
                                 int count, const TrfxCliOptions *options);
void trfx_print_listeners_text(FILE *out, const ConnectionInfo connections[],
                               int count);
void trfx_print_listeners_json(FILE *out, const ConnectionInfo connections[],
                               int count);
void trfx_print_system_text(FILE *out, const SystemOverview *overview);
void trfx_print_system_json(FILE *out, const SystemOverview *overview);
void trfx_print_diagnostics_text(FILE *out,
                                 const TrfxDiagnosticsSnapshot *snapshot);

#endif // TRFX_CLI_OUTPUT_H
