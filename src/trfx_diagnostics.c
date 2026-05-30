/*
 * Copyright (C) 2025 Masoud Bolhassani <masoud.bolhassani@gmail.com>
 *
 * This file is part of Trafix.
 *
 * Trafix is released under the GNU General Public License v3 (GPL-3.0).
 * See LICENSE file for details.
 */

#include "trfx_diagnostics.h"

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void diagnostics_log_snapshot_reset(TrfxDiagnosticsLogSnapshot *snapshot) {
  if (!snapshot)
    return;

  memset(snapshot, 0, sizeof(*snapshot));
  snapshot->status = TRFX_COLLECTOR_PARSE_FAILED;
}

static void diagnostics_log_snapshot_set_error(TrfxDiagnosticsLogSnapshot *snapshot,
                                               const char *message) {
  if (!snapshot)
    return;

  if (!message || message[0] == '\0')
    message = "logs unavailable";
  snprintf(snapshot->error, sizeof(snapshot->error), "%s", message);
}

static void diagnostics_log_snapshot_add(TrfxDiagnosticsLogSnapshot *snapshot,
                                         const char *source_name,
                                         const char *line) {
  int slot;

  if (!snapshot || !line || line[0] == '\0')
    return;

  if (snapshot->count < TRFX_DIAGNOSTICS_MAX_LOG_LINES) {
    slot = snapshot->count;
    snapshot->count++;
  } else {
    memmove(&snapshot->lines[0], &snapshot->lines[1],
            sizeof(snapshot->lines[0]) * (TRFX_DIAGNOSTICS_MAX_LOG_LINES - 1));
    slot = TRFX_DIAGNOSTICS_MAX_LOG_LINES - 1;
  }

  snprintf(snapshot->lines[slot].source, sizeof(snapshot->lines[slot].source),
           "%s", source_name && source_name[0] ? source_name : "logs");
  snprintf(snapshot->lines[slot].text, sizeof(snapshot->lines[slot].text),
           "%.255s", line);
}

static TrfxCollectorStatus diagnostics_collect_log_file(FILE *fp,
                                                       const char *source_name,
                                                       TrfxDiagnosticsLogSnapshot *snapshot) {
  char line[512];
  int line_count = 0;

  if (!fp || !snapshot)
    return TRFX_COLLECTOR_INVALID_ARGUMENT;

  while (fgets(line, sizeof(line), fp)) {
    line[strcspn(line, "\n")] = '\0';
    if (line[0] == '\0')
      continue;
    diagnostics_log_snapshot_add(snapshot, source_name, line);
    line_count++;
  }

  if (line_count == 0)
    return TRFX_COLLECTOR_PARSE_FAILED;

  snapshot->status = TRFX_COLLECTOR_OK;
  snapshot->error[0] = '\0';
  return TRFX_COLLECTOR_OK;
}

static TrfxCollectorStatus diagnostics_collect_log_path(const char *path,
                                                        const char *source_name,
                                                        TrfxDiagnosticsLogSnapshot *snapshot,
                                                        char *error,
                                                        size_t error_size) {
  FILE *fp;
  TrfxCollectorStatus status;

  if (error && error_size > 0)
    error[0] = '\0';

  if (!path || path[0] == '\0' || !snapshot) {
    if (error && error_size > 0)
      snprintf(error, error_size, "invalid argument");
    return TRFX_COLLECTOR_INVALID_ARGUMENT;
  }

  fp = fopen(path, "r");
  if (!fp) {
    if (error && error_size > 0)
      snprintf(error, error_size, "open failed: %s", strerror(errno));
    return TRFX_COLLECTOR_OPEN_FAILED;
  }

  status = diagnostics_collect_log_file(fp, source_name, snapshot);
  fclose(fp);

  if (status == TRFX_COLLECTOR_PARSE_FAILED && error && error_size > 0)
    snprintf(error, error_size, "no readable log lines in %s", path);

  return status;
}

static TrfxCollectorStatus diagnostics_collect_log_command(const char *command,
                                                           const char *source_name,
                                                           TrfxDiagnosticsLogSnapshot *snapshot,
                                                           char *error,
                                                           size_t error_size) {
  FILE *fp;
  TrfxCollectorStatus status;

  if (error && error_size > 0)
    error[0] = '\0';

  if (!command || command[0] == '\0' || !snapshot) {
    if (error && error_size > 0)
      snprintf(error, error_size, "invalid argument");
    return TRFX_COLLECTOR_INVALID_ARGUMENT;
  }

  fp = popen(command, "r");
  if (!fp) {
    if (error && error_size > 0)
      snprintf(error, error_size, "open failed: %s", strerror(errno));
    return TRFX_COLLECTOR_OPEN_FAILED;
  }

  status = diagnostics_collect_log_file(fp, source_name, snapshot);
  pclose(fp);

  if (status == TRFX_COLLECTOR_PARSE_FAILED && error && error_size > 0)
    snprintf(error, error_size, "no readable log lines from %s", source_name);

  return status;
}

void trfx_init_diagnostics_log_snapshot(TrfxDiagnosticsLogSnapshot *snapshot) {
  diagnostics_log_snapshot_reset(snapshot);
}

void trfx_init_diagnostics_snapshot(TrfxDiagnosticsSnapshot *snapshot) {
  if (!snapshot)
    return;

  memset(snapshot, 0, sizeof(*snapshot));
  trfx_init_diagnostics_log_snapshot(&snapshot->logs);
  trfx_init_network_snapshot(&snapshot->network);
  snapshot->status = TRFX_COLLECTOR_PARSE_FAILED;
}

const TrfxDiagnosticsLogLine *trfx_diagnostics_log_at(
    const TrfxDiagnosticsLogSnapshot *snapshot, size_t index) {
  if (!snapshot || index >= (size_t)snapshot->count)
    return NULL;

  return &snapshot->lines[(size_t)snapshot->count - 1 - index];
}

size_t trfx_diagnostics_log_count(const TrfxDiagnosticsLogSnapshot *snapshot) {
  if (!snapshot)
    return 0;

  return (size_t)snapshot->count;
}

TrfxCollectorStatus trfx_collect_diagnostics_log_path(
    const char *path, const char *source_name,
    TrfxDiagnosticsLogSnapshot *snapshot, char *error, size_t error_size) {
  TrfxCollectorStatus status;

  if (!snapshot) {
    if (error && error_size > 0)
      snprintf(error, error_size, "invalid argument");
    return TRFX_COLLECTOR_INVALID_ARGUMENT;
  }

  trfx_init_diagnostics_log_snapshot(snapshot);
  status = diagnostics_collect_log_path(path, source_name, snapshot, error,
                                        error_size);
  snapshot->status = status;
  if (status != TRFX_COLLECTOR_OK)
    diagnostics_log_snapshot_set_error(snapshot, error);
  return status;
}

TrfxCollectorStatus trfx_collect_diagnostics_logs(TrfxDiagnosticsLogSnapshot *snapshot,
                                                  char *error,
                                                  size_t error_size) {
  static const struct {
    const char *path;
    const char *source_name;
  } file_sources[] = {
      {"/var/log/syslog", "syslog"},
      {"/var/log/messages", "messages"},
      {"/var/log/kern.log", "kern.log"},
  };
  const char *journal_command = "journalctl -k -n 12 --no-pager -o cat 2>/dev/null";
  const char *dmesg_command = "dmesg --ctime --color=never 2>/dev/null";
  TrfxCollectorStatus status = TRFX_COLLECTOR_OPEN_FAILED;
  char local_error[128];

  if (!snapshot) {
    if (error && error_size > 0)
      snprintf(error, error_size, "invalid argument");
    return TRFX_COLLECTOR_INVALID_ARGUMENT;
  }

  trfx_init_diagnostics_log_snapshot(snapshot);
  if (error && error_size > 0)
    error[0] = '\0';

  for (size_t i = 0; i < sizeof(file_sources) / sizeof(file_sources[0]); i++) {
    status = diagnostics_collect_log_path(file_sources[i].path,
                                          file_sources[i].source_name,
                                          snapshot, local_error,
                                          sizeof(local_error));
    if (status == TRFX_COLLECTOR_OK)
      break;
  }

  if (status != TRFX_COLLECTOR_OK) {
    diagnostics_log_snapshot_reset(snapshot);
    status = diagnostics_collect_log_command(journal_command, "journalctl",
                                             snapshot, local_error,
                                             sizeof(local_error));
  }

  if (status != TRFX_COLLECTOR_OK) {
    diagnostics_log_snapshot_reset(snapshot);
    status = diagnostics_collect_log_command(dmesg_command, "dmesg", snapshot,
                                             local_error, sizeof(local_error));
  }

  snapshot->status = status;
  if (status != TRFX_COLLECTOR_OK) {
    diagnostics_log_snapshot_set_error(snapshot, local_error);
    if (error && error_size > 0)
      snprintf(error, error_size, "%s", snapshot->error);
  }

  return status;
}

TrfxCollectorStatus trfx_collect_diagnostics_snapshot(
    TrfxDiagnosticsSnapshot *snapshot, char *error, size_t error_size) {
  TrfxCollectorStatus network_status;
  TrfxCollectorStatus log_status;

  if (!snapshot) {
    if (error && error_size > 0)
      snprintf(error, error_size, "invalid argument");
    return TRFX_COLLECTOR_INVALID_ARGUMENT;
  }

  if (error && error_size > 0)
    error[0] = '\0';

  trfx_init_diagnostics_snapshot(snapshot);
  snapshot->system = get_system_overview();

  network_status = trfx_collect_network_snapshot(&snapshot->network, error,
                                                 error_size);
  snapshot->status = network_status;

  log_status = trfx_collect_diagnostics_logs(&snapshot->logs, error,
                                             error_size);
  if (snapshot->status == TRFX_COLLECTOR_OK &&
      log_status != TRFX_COLLECTOR_OK &&
      log_status != TRFX_COLLECTOR_OPEN_FAILED) {
    snapshot->status = log_status;
  }

  if (snapshot->status == TRFX_COLLECTOR_OK && error && error_size > 0)
    error[0] = '\0';

  return snapshot->status;
}
