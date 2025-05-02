/*
 * Copyright (C) 2025 Masoud Bolhassani <masoud.bolhassani@gmail.com>
 *
 * This file is part of Trafix.
 *
 * Trafix is released under the GNU General Public License v3 (GPL-3.0).
 * See LICENSE file for details.
 */

#ifndef TRFX_CONFIG_H
#define TRFX_CONFIG_H

#define CONFIG_FILE "/etc/trafix/config.cfg"

extern int temp_warn_red;
extern int temp_warn_red;

void read_config(const char *config_file);

#endif
