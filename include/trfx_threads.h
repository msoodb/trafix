/*
 * Copyright (C) 2025 Masoud Bolhassani <masoud.bolhassani@gmail.com>
 *
 * This file is part of Trafix.
 *
 * Trafix is released under the GNU General Public License v3 (GPL-3.0).
 * See LICENSE file for details.
 */

#ifndef TRFX_THREADS_H
#define TRFX_THREADS_H

typedef struct {
  int module_index;
  WINDOW *window;
} ThreadArg;

void wait_until_ready();

void *system_info_thread(void *arg);
void *memory_info_thread(void *arg);
void *disk_info_thread(void *arg);
void *cpu_info_thread(void *arg);

void *process_info_thread(void *arg);
void *connection_info_thread(void *arg);
void *bandwidth_info_thread(void *arg);
void *network_info_thread(void *arg);

void *help_info_thread(void *arg);
  
#endif // TRFX_THREADS_H
