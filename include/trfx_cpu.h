#ifndef TRFX_CPU_H
#define TRFX_CPU_H

#define MAX_CPU_CORES 128
#define MAX_CPU_PROCESSES 64

typedef struct {
    float usage_per_core[MAX_CPU_CORES];
    float avg_usage;
    int num_cores;
    float frequency_per_core[MAX_CPU_CORES];
    float temperature; // -1 if unavailable
} CPUInfo;

CPUInfo get_cpu_info();

#endif
