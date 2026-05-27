/*
 * Copyright (C) 2025 Masoud Bolhassani <masoud.bolhassani@gmail.com>
 *
 * This file is part of Trafix.
 *
 * Trafix is released under the GNU General Public License v3 (GPL-3.0).
 * See LICENSE file for details.
 */

#include "trfx_app.h"

#include <stdlib.h>
#include <time.h>

#include "trfx_config.h"
#include "trfx_dashboard.h"

int trfx_run_tui(void) {
  srand(time(NULL));
  read_config(CONFIG_FILE);
  start_dashboard();
  return 0;
}
