/*
 * Copyright (C) 2025 Masoud Bolhassani <masoud.bolhassani@gmail.com>
 *
 * This file is part of Trafix.
 *
 * Trafix is released under the GNU General Public License v3 (GPL-3.0).
 * See LICENSE file for details.
 */

#include "test_common.h"
#include "trfx_runtime.h"

static int test_runtime_flags(void) {
  trfx_runtime_reset();

  ASSERT_INT_EQ(trfx_runtime_is_ready(), 0);
  ASSERT_INT_EQ(trfx_runtime_is_paused(), 0);
  ASSERT_INT_EQ(trfx_runtime_should_stop(), 0);

  trfx_runtime_set_ready(1);
  trfx_runtime_set_paused(1);
  trfx_runtime_request_stop();

  ASSERT_INT_EQ(trfx_runtime_is_ready(), 1);
  ASSERT_INT_EQ(trfx_runtime_is_paused(), 1);
  ASSERT_INT_EQ(trfx_runtime_should_stop(), 1);

  return 0;
}

static int test_runtime_refresh_flags(void) {
  trfx_runtime_reset();

  ASSERT_INT_EQ(trfx_runtime_consume_static_refresh(STATIC_MODULE_CPUINFO), 0);

  trfx_runtime_request_static_refresh_all();
  ASSERT_INT_EQ(trfx_runtime_consume_static_refresh(STATIC_MODULE_SYSINFO), 1);
  ASSERT_INT_EQ(trfx_runtime_consume_static_refresh(STATIC_MODULE_SYSINFO), 0);
  ASSERT_INT_EQ(trfx_runtime_consume_static_refresh(STATIC_MODULE_CPUINFO), 1);
  ASSERT_INT_EQ(trfx_runtime_consume_static_refresh(-1), 0);
  ASSERT_INT_EQ(trfx_runtime_consume_static_refresh(STATIC_MODULE_COUNT), 0);

  return 0;
}

int main(void) {
  if (test_runtime_flags() != 0)
    return 1;

  if (test_runtime_refresh_flags() != 0)
    return 1;

  return 0;
}
