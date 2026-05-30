/*
 * Copyright (C) 2025 Masoud Bolhassani <masoud.bolhassani@gmail.com>
 *
 * This file is part of Trafix.
 *
 * Trafix is released under the GNU General Public License v3 (GPL-3.0).
 * See LICENSE file for details.
 */

// trfx_utils.c
#include "trfx_globals.h"
#include "trfx_utils.h"
#include <stdio.h>

void format_bytes(double mb, char *buf, size_t bufsize) {
    if (mb >= 1024)
        snprintf(buf, bufsize, "%.1fG", mb / 1024);
    else
        snprintf(buf, bufsize, "%.0fM", mb);
}

void safe_mvwprintw(WINDOW *win, int y, int x, int max_width, const char *fmt,
                    ...) {
  int h, w;
  getmaxyx(win, h, w);

  if (y < 0 || y >= h - 1 || x < 1 || x >= w - 1) {
    return; // Out of bounds or touching borders
  }

  va_list args;
  va_start(args, fmt);

  // Format into buffer
  char buffer[1024];
  vsnprintf(buffer, sizeof(buffer), fmt, args);
  va_end(args);

  char clipped[1024];
  trfx_clip_text(buffer, clipped, sizeof(clipped), max_width);
  mvwprintw(win, y, x, "%s", clipped);
}

void trfx_clip_text(const char *src, char *dest, size_t dest_size,
                    int max_width) {
  if (!dest || dest_size == 0)
    return;

  if (!src || max_width <= 0) {
    dest[0] = '\0';
    return;
  }

  size_t limit = (size_t)max_width;
  if (limit >= dest_size)
    limit = dest_size - 1;

  snprintf(dest, dest_size, "%.*s", (int)limit, src);
}

void trfx_format_endpoint_for_tui(const char *value, char *buf,
                                  size_t bufsize) {
  const char *suffix;
  size_t suffix_len;
  size_t prefix_len;

  if (!buf || bufsize == 0)
    return;

  if (!value) {
    snprintf(buf, bufsize, "-");
    return;
  }

  if (strlen(value) < bufsize) {
    snprintf(buf, bufsize, "%s", value);
    return;
  }

  if (bufsize <= 4) {
    snprintf(buf, bufsize, "%.*s", (int)(bufsize - 1), value);
    return;
  }

  suffix = strrchr(value, ':');
  if (suffix && suffix > value) {
    suffix_len = strlen(suffix);
    if (suffix_len + 4 < bufsize) {
      prefix_len = bufsize - suffix_len - 4;
      snprintf(buf, bufsize, "%.*s...%s", (int)prefix_len, value, suffix);
      return;
    }
  }

  snprintf(buf, bufsize, "%.*s...", (int)(bufsize - 4), value);
}

void trfx_print_clipped(WINDOW *win, int y, int x, const char *line) {
  int h, w;
  getmaxyx(win, h, w);

  if (y < 0 || y >= h - 1 || x < 1 || x >= w - 1)
    return;

  char clipped[1024];
  trfx_clip_text(line, clipped, sizeof(clipped), w - x - 1);
  mvwprintw(win, y, x, "%s", clipped);
}

void trfx_draw_box(WINDOW *win, int color_pair) {
  if (!win)
    return;

  wattron(win, trfx_color_attr(color_pair));
  box(win, 0, 0);
  wattroff(win, trfx_color_attr(color_pair));
}

void trfx_print_empty_state(WINDOW *win, const char *message) {
  int h, w;
  getmaxyx(win, h, w);

  if (h < 3 || w < 4)
    return;

  trfx_print_clipped(win, h / 2, 2, message ? message : "No data available");
}
