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

extern int TEMP_WARN_RED;
extern int TEMP_WARN_YELLOW;
extern int ROW2_MODULES;
extern int TUI_REFRESH_INTERVAL_MS;
extern int TUI_PAUSE_INTERVAL_MS;
extern int TUI_READY_CHECK_INTERVAL_MS;
extern int TUI_SMALL_PANEL_REFRESH_MS;

void read_config(const char *config_file);

#endif
