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

int main(void) {
  if (test_interfaces_text_snapshot() != 0)
    return 1;

  if (test_interfaces_json_snapshot() != 0)
    return 1;

  return 0;
}
