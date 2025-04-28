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

void wait_until_ready();
void *system_info_thread(void *arg);

#endif // TRFX_THREADS_H
