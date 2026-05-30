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

static int test_clip_text(void) {
  char buf[16];

  trfx_clip_text("abcdef", buf, sizeof(buf), 3);
  ASSERT_STR_EQ(buf, "abc");

  trfx_clip_text("abcdef", buf, sizeof(buf), 32);
  ASSERT_STR_EQ(buf, "abcdef");

  trfx_clip_text(NULL, buf, sizeof(buf), 32);
  ASSERT_STR_EQ(buf, "");

  trfx_clip_text("abcdef", buf, sizeof(buf), 0);
  ASSERT_STR_EQ(buf, "");

  return 0;
}

static int test_format_endpoint(void) {
  char buf[32];

  trfx_format_endpoint_for_tui("127.0.0.1:8080", buf, sizeof(buf));
  ASSERT_STR_EQ(buf, "127.0.0.1:8080");

  trfx_format_endpoint_for_tui("[2001:db8::1]:443", buf, sizeof(buf));
  ASSERT_STR_EQ(buf, "[2001:db8::1]:443");

  trfx_format_endpoint_for_tui("[2001:db8:abcd:ef01:2345:6789:abcd:ef01]:443",
                               buf, sizeof(buf));
  ASSERT_STR_EQ(buf, "[2001:db8:abcd:ef01:2345...:443");

  trfx_format_endpoint_for_tui(NULL, buf, sizeof(buf));
  ASSERT_STR_EQ(buf, "-");

  return 0;
}

int main(void) {
  if (test_format_bytes() != 0)
    return 1;

  if (test_clip_text() != 0)
    return 1;

  if (test_format_endpoint() != 0)
    return 1;

  return 0;
}
