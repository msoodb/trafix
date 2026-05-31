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

#include <ncurses.h>

#include "trfx_bandwidth.h"
#include "trfx_connections.h"

typedef struct {
  int module_index;
  WINDOW *window;
  volatile int *stop_requested;
} ThreadArg;

void wait_until_ready();

void *system_info_thread(void *arg);
void *memory_info_thread(void *arg);
void *disk_info_thread(void *arg);
void *cpu_info_thread(void *arg);

void *process_info_thread(void *arg);
void *process_compact_info_thread(void *arg);
void *connection_info_thread(void *arg);
void *socket_owner_info_thread(void *arg);
void *support_info_thread(void *arg);
void *network_info_thread(void *arg);

void *help_info_thread(void *arg);

void trfx_bandwidth_state_init(void);
int trfx_bandwidth_state_copy(TrfxNetworkSampleBuffer *samples,
                              TrfxBandwidthReport *report, int *focus_index);
void trfx_bandwidth_state_move_focus(int delta);
  
#endif // TRFX_THREADS_H
