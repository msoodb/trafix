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

#include <stddef.h>

#define CONFIG_FILE "/etc/trafix/config.cfg"

extern int TEMP_WARN_RED;
extern int TEMP_WARN_YELLOW;
extern int ALERT_MEMORY_WARN_PERCENT;
extern int ALERT_DISK_WARN_PERCENT;
extern int ALERT_REQUIRE_DEFAULT_ROUTE;
extern int ALERT_REQUIRE_DNS;
extern int SHOW_TOP_PANELS;
extern int TUI_REFRESH_INTERVAL_MS;
extern int TUI_PAUSE_INTERVAL_MS;
extern int TUI_READY_CHECK_INTERVAL_MS;
extern int TUI_SMALL_PANEL_REFRESH_MS;

void read_config(const char *config_file);
int trfx_load_runtime_config(const char *profile_name, char *error,
                             size_t error_size);

#endif
