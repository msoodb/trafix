/*
 * Copyright (C) 2025 Masoud Bolhassani <masoud.bolhassani@gmail.com>
 *
 * This file is part of Trafix.
 *
 * Trafix is released under the GNU General Public License v3 (GPL-3.0).
 * See LICENSE file for details.
 */

#include "test_common.h"
#include "trfx_utils.h"

static int test_format_bytes(void) {
  char buf[16];

  format_bytes(512.0, buf, sizeof(buf));
  ASSERT_STR_EQ(buf, "512M");

  format_bytes(1536.0, buf, sizeof(buf));
  ASSERT_STR_EQ(buf, "1.5G");

  return 0;
}

int main(void) {
  if (test_format_bytes() != 0)
    return 1;

  return 0;
}
