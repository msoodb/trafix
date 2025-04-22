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
