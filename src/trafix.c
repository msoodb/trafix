/*
 * Copyright (C) 2025 Masoud Bolhassani <masoud.bolhassani@gmail.com>
 *
 * This file is part of Trafix.
 *
 * Trafix is released under the GNU General Public License v3 (GPL-3.0).
 * See LICENSE file for details.
 */

#include <stdlib.h>
#include <unistd.h>
#include <time.h>
#include "trfx_config.h"
#include "trfx_dashboard.h"

int main() {
    srand(time(NULL));
    read_config(CONFIG_FILE);
    start_dashboard();
    return 0;
}
