// trfx_utils.c
#include <stdio.h>

void format_bytes(double mb, char *buf, size_t bufsize) {
    if (mb >= 1024)
        snprintf(buf, bufsize, "%.1fG", mb / 1024);
    else
        snprintf(buf, bufsize, "%.0fM", mb);
}
