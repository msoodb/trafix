/*
 * Copyright (C) 2025 Masoud Bolhassani <masoud.bolhassani@gmail.com>
 *
 * This file is part of Trafix.
 *
 * Trafix is released under the GNU General Public License v3 (GPL-3.0).
 * See LICENSE file for details.
 */

#include "test_common.h"
#include "trfx_globals.h"

#include <ncurses.h>

static int test_color_attr_fallback(void) {
  trfx_colors_enabled = 0;
  ASSERT_INT_EQ(trfx_color_attr(COLOR_BORDER), A_NORMAL);

  trfx_colors_enabled = 1;
  ASSERT_INT_EQ(trfx_color_attr(COLOR_BORDER), COLOR_PAIR(COLOR_BORDER));

  return 0;
}

int main(void) {
  if (test_color_attr_fallback() != 0)
    return 1;

  return 0;
}
