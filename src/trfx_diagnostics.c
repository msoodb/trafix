/*
 * Copyright (C) 2025 Masoud Bolhassani <masoud.bolhassani@gmail.com>
 *
 * This file is part of Trafix.
 *
 * Trafix is released under the GNU General Public License v3 (GPL-3.0).
 * See LICENSE file for details.
 */

#include "trfx_diagnostics.h"
#include "trfx_config.h"

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

static void diagnostics_alert_summary_reset(TrfxAlertSummary *summary) {
  if (!summary)
    return;

  memset(summary, 0, sizeof(*summary));
}

static void diagnostics_alert_summary_add(TrfxAlertSummary *summary,
                                          const char *line) {
  if (!summary || !line || line[0] == '\0')
    return;

  if (summary->count >= TRFX_DIAGNOSTICS_MAX_ALERTS)
    return;

  snprintf(summary->lines[summary->count], sizeof(summary->lines[summary->count]),
           "%.127s", line);
  summary->count++;
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
  snapshot->disk_count = 0;
  snapshot->disk_total_used_mb = 0.0;
  snapshot->disk_total_mb = 0.0;
  trfx_init_diagnostics_log_snapshot(&snapshot->logs);
  trfx_init_network_snapshot(&snapshot->network);
  snapshot->status = TRFX_COLLECTOR_PARSE_FAILED;
}

void trfx_init_alert_summary(TrfxAlertSummary *summary) {
  diagnostics_alert_summary_reset(summary);
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

const char *trfx_diagnostics_alert_at(const TrfxAlertSummary *summary,
                                      size_t index) {
  if (!summary || index >= (size_t)summary->count)
    return NULL;

  return summary->lines[index];
}

size_t trfx_diagnostics_alert_count(const TrfxAlertSummary *summary) {
  if (!summary)
    return 0;

  return (size_t)summary->count;
}

void trfx_collect_diagnostics_alerts(const TrfxDiagnosticsSnapshot *snapshot,
                                     TrfxAlertSummary *summary) {
  double disk_usage_percent;

  if (!snapshot || !summary)
    return;

  diagnostics_alert_summary_reset(summary);

  if (snapshot->cpu.temperature >= 0.0f &&
      snapshot->cpu.temperature >= (float)TEMP_WARN_RED) {
    char line[128];
    snprintf(line, sizeof(line), "CPU temperature high: %.1fC >= %dC",
             snapshot->cpu.temperature, TEMP_WARN_RED);
    diagnostics_alert_summary_add(summary, line);
  }

  if (snapshot->memory.total_ram > 0 &&
      snapshot->memory.mem_percent >= (float)ALERT_MEMORY_WARN_PERCENT) {
    char line[128];
    snprintf(line, sizeof(line), "Memory pressure high: %.1f%% >= %d%%",
             snapshot->memory.mem_percent, ALERT_MEMORY_WARN_PERCENT);
    diagnostics_alert_summary_add(summary, line);
  }

  if (snapshot->disk_count > 0 && snapshot->disk_total_mb > 0.0 &&
      ALERT_DISK_WARN_PERCENT > 0) {
    disk_usage_percent =
        (snapshot->disk_total_used_mb / snapshot->disk_total_mb) * 100.0;
    if (disk_usage_percent >= (double)ALERT_DISK_WARN_PERCENT) {
      char line[128];
      snprintf(line, sizeof(line), "Disk pressure high: %.1f%% >= %d%%",
               disk_usage_percent, ALERT_DISK_WARN_PERCENT);
      diagnostics_alert_summary_add(summary, line);
    }
  }

  if (ALERT_REQUIRE_DEFAULT_ROUTE &&
      snapshot->network.route_status == TRFX_COLLECTOR_OK &&
      !snapshot->network.route.has_default) {
    diagnostics_alert_summary_add(summary, "Default route missing");
  }

  if (ALERT_REQUIRE_DNS && snapshot->network.dns_status == TRFX_COLLECTOR_OK &&
      snapshot->network.dns.count == 0) {
    diagnostics_alert_summary_add(summary, "DNS servers missing");
  }
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
  snapshot->cpu = get_cpu_info();
  snapshot->memory = get_memory_info();
  snapshot->processes = trfx_collect_processes(SORT_BY_MEM);
  snapshot->disk_count = get_disk_info(snapshot->disks, MAX_DISKS,
                                       &snapshot->disk_total_used_mb,
                                       &snapshot->disk_total_mb);

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
