/*
 * Copyright (C) 2025 Masoud Bolhassani <masoud.bolhassani@gmail.com>
 *
 * This file is part of Trafix.
 *
 * Trafix is released under the GNU General Public License v3 (GPL-3.0).
 * See LICENSE file for details.
 */

#ifndef TRFX_MEMINFO_H
#define TRFX_MEMINFO_H

typedef struct {
    long total_ram;
    long used_ram;
    long free_ram;
    long total_swap;
    long used_swap;
    float mem_percent;
    char top_processes[512];
} MemoryInfo;

MemoryInfo get_memory_info();

#endif
