/*
 * Copyright (C) 2025 Masoud Bolhassani <masoud.bolhassani@gmail.com>
 *
 * This file is part of Trafix.
 *
 * Trafix is released under the GNU General Public License v3 (GPL-3.0).
 * See LICENSE file for details.
 */

#ifndef TRFX_DASHBOARD_H
#define TRFX_DASHBOARD_H

typedef struct {
    int key;
    const char *description;
} Hotkey;

void *cpu_info_thread(void *arg);
void *memory_info_thread(void *arg);
void *disk_info_thread(void *arg);
void *connection_info_thread(void *arg);
void *network_info_thread(void *arg);
void *process_info_thread(void *arg);
void *bandwidth_info_thread(void *arg);
void *help_info_thread(void *arg);

void start_dashboard();

#endif
