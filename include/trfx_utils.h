/*
 * Copyright (C) 2025 Masoud Bolhassani <masoud.bolhassani@gmail.com>
 *
 * This file is part of Trafix.
 *
 * Trafix is released under the GNU General Public License v3 (GPL-3.0).
 * See LICENSE file for details.
 */

// trfx_utils.h
#ifndef TRFX_UTILS_H
#define TRFX_UTILS_H

#include <stdio.h>
#include <stdarg.h>
#include <string.h>
#include <ncurses.h>

void format_bytes(double mb, char *buf, size_t bufsize);
void trfx_clip_text(const char *src, char *dest, size_t dest_size,
                    int max_width);
void safe_mvwprintw(WINDOW *win, int y, int x, int max_width, const char *fmt, ...);
void trfx_print_clipped(WINDOW *win, int y, int x, const char *line);
void trfx_draw_box(WINDOW *win, int color_pair);
void trfx_print_empty_state(WINDOW *win, const char *message);

#endif
