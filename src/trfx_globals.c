/*
 * Copyright (C) 2025 Masoud Bolhassani <masoud.bolhassani@gmail.com>
 *
 * This file is part of Trafix.
 *
 * Trafix is released under the GNU General Public License v3 (GPL-3.0).
 * See LICENSE file for details.
 */

// trfx_globals.c
#include "trfx_globals.h"

pthread_mutex_t ncurses_mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t ready_mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t global_var_mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t memory_info_mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t disk_info_mutex = PTHREAD_MUTEX_INITIALIZER;

volatile int ready = 0;
volatile sig_atomic_t screen_paused = 0;

volatile int force_refresh_flags[STATIC_MODULE_COUNT] = {0};
