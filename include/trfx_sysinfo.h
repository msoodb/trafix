/*
 * Copyright (C) 2025 Masoud Bolhassani <masoud.bolhassani@gmail.com>
 *
 * This file is part of Trafix.
 *
 * Trafix is released under the GNU General Public License v3 (GPL-3.0).
 * See LICENSE file for details.
 */

#ifndef TRFX_SYSINFO_H
#define TRFX_SYSINFO_H

typedef struct {
    char hostname[64];
    char os_version[128];
    char kernel_version[64];
    char uptime[64];
    char load_avg[64];
    char logged_in_users[128];
} SystemOverview;

SystemOverview get_system_overview();

#endif
