// trfx_utils.c
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

  // Print only up to max_width and make sure it doesn’t touch the right border
  mvwprintw(win, y, x, "%.*s", max_width, buffer);
}
