// trfx_utils.h
#ifndef TRFX_UTILS_H
#define TRFX_UTILS_H

#include <stdio.h>
#include <stdarg.h>
#include <string.h>
#include <ncurses.h>

void format_bytes(double mb, char *buf, size_t bufsize);
void safe_mvwprintw(WINDOW *win, int y, int x, int max_width, const char *fmt, ...);

#endif
