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

void handle_keypress(int ch, int screen_height, int screen_width);
void start_dashboard();

#endif
