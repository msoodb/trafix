/*
 * Copyright (C) 2025 Masoud Bolhassani <masoud.bolhassani@gmail.com>
 *
 * This file is part of Trafix.
 *
 * Trafix is released under the GNU General Public License v3 (GPL-3.0).
 * See LICENSE file for details.
 */

#include "test_common.h"
#include "trfx_cli_output.h"

#include <stdio.h>

static int read_tmpfile(FILE *file, char *buf, size_t bufsize) {
  size_t nread;

  if (fflush(file) != 0)
    return 1;
  if (fseek(file, 0, SEEK_SET) != 0)
    return 1;

  nread = fread(buf, 1, bufsize - 1, file);
  if (ferror(file))
    return 1;

  buf[nread] = '\0';
  return 0;
}

static int test_interfaces_text_snapshot(void) {
  char output[512];
  TrfxInterfaceStatsResult result =
      trfx_collect_interface_stats_path("tests/fixtures/proc_net_dev");
  FILE *file = tmpfile();
  if (!file) {
    perror("tmpfile");
    return 1;
  }

  trfx_print_interfaces_text(file, &result);
  if (read_tmpfile(file, output, sizeof(output)) != 0) {
    fclose(file);
    return 1;
  }
  fclose(file);

  ASSERT_STR_EQ(output,
                "INTERFACE           RX_BYTES     TX_BYTES\n"
                "lo                      4096         8192\n"
                "eth0                 1048576      2097152\n");

  return 0;
}

static int test_interfaces_json_snapshot(void) {
  char output[512];
  TrfxInterfaceStatsResult result =
      trfx_collect_interface_stats_path("tests/fixtures/proc_net_dev");
  FILE *file = tmpfile();
  if (!file) {
    perror("tmpfile");
    return 1;
  }

  trfx_print_interfaces_json(file, &result);
  if (read_tmpfile(file, output, sizeof(output)) != 0) {
    fclose(file);
    return 1;
  }
  fclose(file);

  ASSERT_STR_EQ(output,
                "{\"interfaces\":[{\"interface\":\"lo\",\"rx_bytes\":4096,"
                "\"tx_bytes\":8192},{\"interface\":\"eth0\","
                "\"rx_bytes\":1048576,\"tx_bytes\":2097152}]}\n");

  return 0;
}

static int test_listeners_text_snapshot(void) {
  char output[1024];
  ConnectionInfo connections[] = {
      {"TCP", "0.0.0.0:22", "0.0.0.0:0", "LISTEN", 100, 0, "root", "42",
       "sshd"},
      {"TCP", "127.0.0.1:8080", "127.0.0.1:50000", "ESTABLISHED", 101, 1000,
       "user", "99", "curl"},
      {"UDP", "0.0.0.0:53", "0.0.0.0:0", "UNCONN", 102, 0, "root", "-",
       "-"},
  };
  FILE *file = tmpfile();
  if (!file) {
    perror("tmpfile");
    return 1;
  }

  trfx_print_listeners_text(file, connections, 3);
  if (read_tmpfile(file, output, sizeof(output)) != 0) {
    fclose(file);
    return 1;
  }
  fclose(file);

  ASSERT_STR_EQ(output,
                "PROTO  LOCAL                  UID      USER             PID     PROCESS         \n"
                "TCP    0.0.0.0:22             0        root             42      sshd            \n"
                "UDP    0.0.0.0:53             0        root             -       -               \n");

  return 0;
}

static int test_listeners_json_snapshot(void) {
  char output[1024];
  ConnectionInfo connections[] = {
      {"TCP", "0.0.0.0:22", "0.0.0.0:0", "LISTEN", 100, 0, "root", "42",
       "sshd"},
      {"TCP", "127.0.0.1:8080", "127.0.0.1:50000", "ESTABLISHED", 101, 1000,
       "user", "99", "curl"},
  };
  FILE *file = tmpfile();
  if (!file) {
    perror("tmpfile");
    return 1;
  }

  trfx_print_listeners_json(file, connections, 2);
  if (read_tmpfile(file, output, sizeof(output)) != 0) {
    fclose(file);
    return 1;
  }
  fclose(file);

  ASSERT_STR_EQ(output,
                "{\"listeners\":[{\"proto\":\"TCP\",\"local\":\"0.0.0.0:22\","
                "\"uid\":0,\"user\":\"root\",\"pid\":\"42\","
                "\"process\":\"sshd\"}]}\n");

  return 0;
}

static int test_diagnostics_text_snapshot(void) {
  char output[2048];
  TrfxDiagnosticsSnapshot snapshot;
  FILE *file = tmpfile();
  if (!file) {
    perror("tmpfile");
    return 1;
  }

  trfx_init_diagnostics_snapshot(&snapshot);
  snprintf(snapshot.system.hostname, sizeof(snapshot.system.hostname),
           "trafix-test");
  snprintf(snapshot.system.os_version, sizeof(snapshot.system.os_version),
           "TestOS");
  snprintf(snapshot.system.kernel_version,
           sizeof(snapshot.system.kernel_version), "1.0.0");
  snprintf(snapshot.system.uptime, sizeof(snapshot.system.uptime), "1d 2h 3m");
  snprintf(snapshot.system.load_avg, sizeof(snapshot.system.load_avg),
           "0.10 0.20 0.30");
  snprintf(snapshot.system.logged_in_users,
           sizeof(snapshot.system.logged_in_users), "alice bob");

  snapshot.cpu.avg_usage = 12.5f;
  snapshot.cpu.temperature = 61.0f;
  snapshot.cpu.num_cores = 4;

  snapshot.memory.mem_percent = 42.0f;
  snapshot.memory.used_ram = 4096;
  snapshot.memory.total_ram = 8192;
  snapshot.memory.used_swap = 128;
  snapshot.memory.total_swap = 1024;

  snapshot.disk_count = 2;
  snapshot.disk_total_used_mb = 2048.0;
  snapshot.disk_total_mb = 4096.0;

  snapshot.processes.count = 2;
  snprintf(snapshot.processes.processes[0].command,
           sizeof(snapshot.processes.processes[0].command), "nginx");

  snapshot.network.route.has_default = 1;
  snprintf(snapshot.network.route.gateway, sizeof(snapshot.network.route.gateway),
           "192.0.2.1");
  snprintf(snapshot.network.route.interface,
           sizeof(snapshot.network.route.interface), "eth0");
  snprintf(snapshot.network.route.metric, sizeof(snapshot.network.route.metric),
           "100");
  snapshot.network.dns.count = 2;
  snprintf(snapshot.network.dns.servers[0],
           sizeof(snapshot.network.dns.servers[0]), "1.1.1.1");
  snprintf(snapshot.network.dns.servers[1],
           sizeof(snapshot.network.dns.servers[1]), "8.8.8.8");
  snapshot.network.has_active_interface = 1;
  snprintf(snapshot.network.active_interface,
           sizeof(snapshot.network.active_interface), "eth0");
  snprintf(snapshot.network.active_type, sizeof(snapshot.network.active_type),
           "Ethernet");
  snprintf(snapshot.network.active_ip, sizeof(snapshot.network.active_ip),
           "192.0.2.10");
  snprintf(snapshot.network.active_mac, sizeof(snapshot.network.active_mac),
           "de:ad:be:ef:00:01");

  snapshot.logs.count = 1;
  snprintf(snapshot.logs.lines[0].source, sizeof(snapshot.logs.lines[0].source),
           "journalctl");
  snprintf(snapshot.logs.lines[0].text, sizeof(snapshot.logs.lines[0].text),
           "link up");
  snapshot.logs.status = TRFX_COLLECTOR_OK;

  trfx_print_diagnostics_text(file, &snapshot);
  if (read_tmpfile(file, output, sizeof(output)) != 0) {
    fclose(file);
    return 1;
  }
  fclose(file);

  ASSERT_INT_EQ(strstr(output, "SYSTEM") != NULL, 1);
  ASSERT_INT_EQ(strstr(output, "NETWORK") != NULL, 1);
  ASSERT_INT_EQ(strstr(output, "PRESSURE") != NULL, 1);
  ASSERT_INT_EQ(strstr(output, "LOGS") != NULL, 1);
  ASSERT_INT_EQ(strstr(output, "trafix-test") != NULL, 1);
  ASSERT_INT_EQ(strstr(output, "1.1.1.1, 8.8.8.8") != NULL, 1);

  return 0;
}

int main(void) {
  if (test_interfaces_text_snapshot() != 0)
    return 1;

  if (test_interfaces_json_snapshot() != 0)
    return 1;

  if (test_listeners_text_snapshot() != 0)
    return 1;

  if (test_listeners_json_snapshot() != 0)
    return 1;

  if (test_diagnostics_text_snapshot() != 0)
    return 1;

  return 0;
}
