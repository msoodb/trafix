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

typedef struct {
    char pid[16];
    char user[32];
    char cpu[8];
    char mem[8];
    char time[16];
    char command[64];
} CPUProcessInfo;

CPUInfo get_cpu_info();
int get_top_processes_by_cpu(CPUProcessInfo *list, int max);

#endif
