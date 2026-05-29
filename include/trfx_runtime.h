/*
 * Copyright (C) 2025 Masoud Bolhassani <masoud.bolhassani@gmail.com>
 *
 * This file is part of Trafix.
 *
 * Trafix is released under the GNU General Public License v3 (GPL-3.0).
 * See LICENSE file for details.
 */

#ifndef TRFX_RUNTIME_H
#define TRFX_RUNTIME_H

#include <pthread.h>

#include "trfx_globals.h"

typedef struct {
  pthread_mutex_t lock;
  int ready;
  int paused;
  int stop_requested;
  int force_refresh_flags[STATIC_MODULE_COUNT];
} TrfxRuntimeState;

TrfxRuntimeState *trfx_runtime_state(void);
void trfx_runtime_reset(void);

void trfx_runtime_set_ready(int ready);
int trfx_runtime_is_ready(void);

void trfx_runtime_set_paused(int paused);
int trfx_runtime_is_paused(void);

void trfx_runtime_request_stop(void);
int trfx_runtime_should_stop(void);

void trfx_runtime_request_static_refresh_all(void);
int trfx_runtime_consume_static_refresh(int module_index);

#endif // TRFX_RUNTIME_H
