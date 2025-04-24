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

#define CONFIG_FILE "config/config.cfg"

extern int alert_bandwidth;
extern char filter_ip[16];
extern char filter_process[50];

void read_config(const char *config_file);

#endif
