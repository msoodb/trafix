/*
 * Copyright (C) 2025 Masoud Bolhassani <masoud.bolhassani@gmail.com>
 *
 * This file is part of Trafix.
 *
 * Trafix is released under the GNU General Public License v3 (GPL-3.0).
 * See LICENSE file for details.
 */

#include "test_common.h"
#include "trfx_version.h"

#ifndef TRFX_VERSION
#define TRFX_VERSION "unknown"
#endif

static int test_get_version(void) {
  ASSERT_STR_EQ(trfx_get_version(), TRFX_VERSION);
  return 0;
}

int main(void) {
  if (test_get_version() != 0)
    return 1;

  return 0;
}
