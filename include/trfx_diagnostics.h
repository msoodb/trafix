/*
 * Copyright (C) 2025 Masoud Bolhassani <masoud.bolhassani@gmail.com>
 *
 * This file is part of Trafix.
 *
 * Trafix is released under the GNU General Public License v3 (GPL-3.0).
 * See LICENSE file for details.
 */

#ifndef TRFX_DIAGNOSTICS_H
#define TRFX_DIAGNOSTICS_H

#include <stddef.h>

#include "trfx_cpu.h"
#include "trfx_disk.h"
#include "trfx_meminfo.h"
#include "trfx_procinfo.h"
#include "trfx_netinfo.h"
#include "trfx_sysinfo.h"

#define TRFX_DIAGNOSTICS_MAX_LOG_LINES 12

typedef struct {
  char source[32];
  char text[256];
} TrfxDiagnosticsLogLine;

typedef struct {
  TrfxCollectorStatus status;
  int count;
  char error[128];
  TrfxDiagnosticsLogLine lines[TRFX_DIAGNOSTICS_MAX_LOG_LINES];
} TrfxDiagnosticsLogSnapshot;

typedef struct {
  SystemOverview system;
  CPUInfo cpu;
  MemoryInfo memory;
  DiskInfo disks[MAX_DISKS];
  int disk_count;
  double disk_total_used_mb;
  double disk_total_mb;
  TrfxProcessResult processes;
  TrfxNetworkSnapshot network;
  TrfxDiagnosticsLogSnapshot logs;
  TrfxCollectorStatus status;
} TrfxDiagnosticsSnapshot;

void trfx_init_diagnostics_log_snapshot(TrfxDiagnosticsLogSnapshot *snapshot);
void trfx_init_diagnostics_snapshot(TrfxDiagnosticsSnapshot *snapshot);
const TrfxDiagnosticsLogLine *trfx_diagnostics_log_at(
    const TrfxDiagnosticsLogSnapshot *snapshot, size_t index);
size_t trfx_diagnostics_log_count(const TrfxDiagnosticsLogSnapshot *snapshot);
TrfxCollectorStatus trfx_collect_diagnostics_log_path(
    const char *path, const char *source_name,
    TrfxDiagnosticsLogSnapshot *snapshot, char *error, size_t error_size);
TrfxCollectorStatus trfx_collect_diagnostics_logs(TrfxDiagnosticsLogSnapshot *snapshot,
                                                  char *error,
                                                  size_t error_size);
TrfxCollectorStatus trfx_collect_diagnostics_snapshot(
    TrfxDiagnosticsSnapshot *snapshot, char *error, size_t error_size);

#endif // TRFX_DIAGNOSTICS_H
