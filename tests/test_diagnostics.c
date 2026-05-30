/*
 * Copyright (C) 2025 Masoud Bolhassani <masoud.bolhassani@gmail.com>
 *
 * This file is part of Trafix.
 *
 * Trafix is released under the GNU General Public License v3 (GPL-3.0).
 * See LICENSE file for details.
 */

#include "test_common.h"

#include <stdio.h>
#include <unistd.h>

#include "trfx_diagnostics.h"

static int test_diagnostics_log_path(void) {
  const char *path = "tests/fixtures/diagnostics_logs.txt";
  TrfxDiagnosticsLogSnapshot logs;
  char error[128];
  const TrfxDiagnosticsLogLine *line;

  ASSERT_INT_EQ(trfx_collect_diagnostics_log_path(path, "fixture", &logs,
                                                  error, sizeof(error)),
                TRFX_COLLECTOR_OK);
  ASSERT_INT_EQ((int)trfx_diagnostics_log_count(&logs), 2);

  line = trfx_diagnostics_log_at(&logs, 0);
  ASSERT_INT_EQ(line != NULL, 1);
  ASSERT_STR_EQ(line->source, "fixture");
  ASSERT_STR_EQ(line->text, "network: dns fallback");

  line = trfx_diagnostics_log_at(&logs, 1);
  ASSERT_INT_EQ(line != NULL, 1);
  ASSERT_STR_EQ(line->text, "kernel: link up");
  return 0;
}

static int test_diagnostics_snapshot_init(void) {
  TrfxDiagnosticsSnapshot snapshot;

  trfx_init_diagnostics_snapshot(&snapshot);
  ASSERT_INT_EQ(snapshot.status, TRFX_COLLECTOR_PARSE_FAILED);
  ASSERT_INT_EQ(snapshot.logs.status, TRFX_COLLECTOR_PARSE_FAILED);
  ASSERT_INT_EQ(snapshot.network.route_status, TRFX_COLLECTOR_PARSE_FAILED);
  ASSERT_STR_EQ(snapshot.system.hostname, "");

  return 0;
}

static int test_diagnostics_snapshot_collect(void) {
  TrfxDiagnosticsSnapshot snapshot;
  char error[128];
  TrfxCollectorStatus status;

  status = trfx_collect_diagnostics_snapshot(&snapshot, error, sizeof(error));
  ASSERT_INT_EQ(status == TRFX_COLLECTOR_OK ||
                status == TRFX_COLLECTOR_PARSE_FAILED ||
                status == TRFX_COLLECTOR_OPEN_FAILED,
                1);
  ASSERT_INT_EQ(snapshot.system.hostname[0] != '\0', 1);
  ASSERT_INT_EQ(snapshot.cpu.num_cores >= 0, 1);
  ASSERT_INT_EQ(snapshot.memory.total_ram >= 0, 1);
  ASSERT_INT_EQ(snapshot.disk_count >= 0, 1);
  ASSERT_INT_EQ(snapshot.processes.count >= 0, 1);
  ASSERT_INT_EQ(snapshot.network.route_status == TRFX_COLLECTOR_OK ||
                snapshot.network.route_status == TRFX_COLLECTOR_OPEN_FAILED ||
                snapshot.network.route_status == TRFX_COLLECTOR_PARSE_FAILED,
                1);
  ASSERT_INT_EQ(snapshot.logs.status == TRFX_COLLECTOR_OK ||
                snapshot.logs.status == TRFX_COLLECTOR_OPEN_FAILED ||
                snapshot.logs.status == TRFX_COLLECTOR_PARSE_FAILED,
                1);

  return 0;
}

static int test_diagnostics_alerts(void) {
  TrfxDiagnosticsSnapshot snapshot;
  TrfxAlertSummary alerts;

  trfx_init_diagnostics_snapshot(&snapshot);
  snapshot.cpu.temperature = 82.0f;
  snapshot.memory.total_ram = 8192;
  snapshot.memory.mem_percent = 91.0f;
  snapshot.disk_count = 1;
  snapshot.disk_total_used_mb = 950.0;
  snapshot.disk_total_mb = 1000.0;
  snapshot.network.route_status = TRFX_COLLECTOR_OK;
  snapshot.network.dns_status = TRFX_COLLECTOR_OK;
  snapshot.network.route.has_default = 0;
  snapshot.network.dns.count = 0;

  trfx_init_alert_summary(&alerts);
  trfx_collect_diagnostics_alerts(&snapshot, &alerts);

  ASSERT_INT_EQ(trfx_diagnostics_alert_count(&alerts) >= 4, 1);
  ASSERT_INT_EQ(strstr(trfx_diagnostics_alert_at(&alerts, 0),
                       "CPU temperature high") != NULL,
                1);
  ASSERT_INT_EQ(strstr(trfx_diagnostics_alert_at(&alerts, 1),
                       "Memory pressure high") != NULL,
                1);
  ASSERT_INT_EQ(strstr(trfx_diagnostics_alert_at(&alerts, 2),
                       "Disk pressure high") != NULL,
                1);
  ASSERT_INT_EQ(strstr(trfx_diagnostics_alert_at(&alerts, 3),
                       "Default route missing") != NULL,
                1);

  return 0;
}

int main(void) {
  if (test_diagnostics_log_path() != 0)
    return 1;

  if (test_diagnostics_snapshot_init() != 0)
    return 1;

  if (test_diagnostics_snapshot_collect() != 0)
    return 1;

  if (test_diagnostics_alerts() != 0)
    return 1;

  return 0;
}
