/*
 * Copyright (C) 2025 Masoud Bolhassani <masoud.bolhassani@gmail.com>
 *
 * This file is part of Trafix.
 *
 * Trafix is released under the GNU General Public License v3 (GPL-3.0).
 * See LICENSE file for details.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "trfx_meminfo.h"

MemoryInfo get_memory_info() {
    MemoryInfo info = {0};
    FILE *fp = fopen("/proc/meminfo", "r");

    long mem_total = 0, mem_free = 0, buffers = 0, cached = 0;
    long swap_total = 0, swap_free = 0;

    if (fp) {
        char label[64];
        long value;
        while (fscanf(fp, "%63s %ld kB\n", label, &value) == 2) {
            if (strcmp(label, "MemTotal:") == 0) mem_total = value;
            else if (strcmp(label, "MemFree:") == 0) mem_free = value;
            else if (strcmp(label, "Buffers:") == 0) buffers = value;
            else if (strcmp(label, "Cached:") == 0) cached = value;
            else if (strcmp(label, "SwapTotal:") == 0) swap_total = value;
            else if (strcmp(label, "SwapFree:") == 0) swap_free = value;
        }
        fclose(fp);
    }

    info.total_ram = mem_total;
    info.free_ram = mem_free + buffers + cached;
    info.used_ram = mem_total - info.free_ram;
    info.total_swap = swap_total;
    info.used_swap = swap_total - swap_free;
    info.mem_percent = mem_total ? (100.0f * info.used_ram / mem_total) : 0.0f;

    // Get top 3 memory-consuming processes
    fp = popen("ps -eo pid,comm,%mem --sort=-%mem 2>/dev/null | head -n 4", "r");
    if (fp) {
        char line[128];
        if (fgets(line, sizeof(line), fp) == NULL) {
          pclose(fp);
          return info;
        }
        info.top_processes[0] = '\0';
        while (fgets(line, sizeof(line), fp)) {
            strncat(info.top_processes, line, sizeof(info.top_processes) - strlen(info.top_processes) - 1);
        }
        pclose(fp);
    }

    return info;
}
