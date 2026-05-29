/*
 * Copyright (C) 2025 Masoud Bolhassani <masoud.bolhassani@gmail.com>
 *
 * This file is part of Trafix.
 *
 * Trafix is released under the GNU General Public License v3 (GPL-3.0).
 * See LICENSE file for details.
 */

#include "trfx_runtime.h"

#include <string.h>

static TrfxRuntimeState runtime_state = {
    PTHREAD_MUTEX_INITIALIZER,
    0,
    0,
    0,
    {0},
};

TrfxRuntimeState *trfx_runtime_state(void) { return &runtime_state; }

void trfx_runtime_reset(void) {
  pthread_mutex_lock(&runtime_state.lock);
  runtime_state.ready = 0;
  runtime_state.paused = 0;
  runtime_state.stop_requested = 0;
  memset(runtime_state.force_refresh_flags, 0,
         sizeof(runtime_state.force_refresh_flags));
  pthread_mutex_unlock(&runtime_state.lock);
}

void trfx_runtime_set_ready(int ready) {
  pthread_mutex_lock(&runtime_state.lock);
  runtime_state.ready = ready ? 1 : 0;
  pthread_mutex_unlock(&runtime_state.lock);
}

int trfx_runtime_is_ready(void) {
  int ready;
  pthread_mutex_lock(&runtime_state.lock);
  ready = runtime_state.ready;
  pthread_mutex_unlock(&runtime_state.lock);
  return ready;
}

void trfx_runtime_set_paused(int paused) {
  pthread_mutex_lock(&runtime_state.lock);
  runtime_state.paused = paused ? 1 : 0;
  pthread_mutex_unlock(&runtime_state.lock);
}

int trfx_runtime_is_paused(void) {
  int paused;
  pthread_mutex_lock(&runtime_state.lock);
  paused = runtime_state.paused;
  pthread_mutex_unlock(&runtime_state.lock);
  return paused;
}

void trfx_runtime_request_stop(void) {
  pthread_mutex_lock(&runtime_state.lock);
  runtime_state.stop_requested = 1;
  pthread_mutex_unlock(&runtime_state.lock);
}

int trfx_runtime_should_stop(void) {
  int should_stop;
  pthread_mutex_lock(&runtime_state.lock);
  should_stop = runtime_state.stop_requested;
  pthread_mutex_unlock(&runtime_state.lock);
  return should_stop;
}

void trfx_runtime_request_static_refresh_all(void) {
  pthread_mutex_lock(&runtime_state.lock);
  for (int i = 0; i < STATIC_MODULE_COUNT; i++)
    runtime_state.force_refresh_flags[i] = 1;
  pthread_mutex_unlock(&runtime_state.lock);
}

int trfx_runtime_consume_static_refresh(int module_index) {
  int requested = 0;

  if (module_index < 0 || module_index >= STATIC_MODULE_COUNT)
    return 0;

  pthread_mutex_lock(&runtime_state.lock);
  requested = runtime_state.force_refresh_flags[module_index];
  runtime_state.force_refresh_flags[module_index] = 0;
  pthread_mutex_unlock(&runtime_state.lock);

  return requested;
}
