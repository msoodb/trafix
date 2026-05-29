/*
 * Copyright (C) 2025 Masoud Bolhassani <masoud.bolhassani@gmail.com>
 *
 * This file is part of Trafix.
 *
 * Trafix is released under the GNU General Public License v3 (GPL-3.0).
 * See LICENSE file for details.
 */

#include "trfx_procinfo.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

TrfxProcessResult trfx_collect_processes(SortType sort_type)
{
    TrfxProcessResult result;
    memset(&result, 0, sizeof(result));
    result.status = TRFX_PROCESS_COLLECTOR_OK;

    const char *cmd = "ps -eo pid,user,pri,ni,vsize,rss,stat,pcpu,pmem,time,comm "
                      "--sort=-pmem 2>/dev/null | head -n 50";
    FILE *fp = popen(cmd, "r");
    if (!fp) {
        result.status = TRFX_PROCESS_COLLECTOR_OPEN_FAILED;
        snprintf(result.error, sizeof(result.error), "Process collector unavailable");
        return result;
    }

    char line[MAX_LINE_LEN];

    // Skip header
    if (!fgets(line, sizeof(line), fp)) {
        pclose(fp);
        result.status = TRFX_PROCESS_COLLECTOR_READ_FAILED;
        snprintf(result.error, sizeof(result.error), "Process data unavailable");
        return result;
    }

    while (fgets(line, sizeof(line), fp) && result.count < MAX_PROCESSES) {
        ProcessInfo *p = &result.processes[result.count];

        memset(p, 0, sizeof(ProcessInfo));

        sscanf(line, "%15s %31s %3s %3s %15s %15s %15s %1s %7s %7s %15s %63[^\n]",
               p->pid, p->user, p->pr, p->ni, p->virt, p->res, p->shr, p->state,
               p->cpu, p->mem, p->time, p->command);

        result.count++;
    }

    pclose(fp);

    // Sort after fetching
    sort_processes(result.processes, result.count, sort_type);

    return result;
}

/*
 * Read top processes into the list.
 * Returns number of processes read.
 */
int get_top_processes(ProcessInfo *list, int max_count, SortType sort_type)
{
    if (!list || max_count <= 0)
        return 0;

    TrfxProcessResult result = trfx_collect_processes(sort_type);
    if (result.status != TRFX_PROCESS_COLLECTOR_OK)
        return 0;

    int count = result.count < max_count ? result.count : max_count;
    memcpy(list, result.processes, (size_t)count * sizeof(ProcessInfo));

    return count;
}

/*
 * Compare functions for sorting
 */

static int compare_by_mem(const void *a, const void *b) {
    const ProcessInfo *p1 = (const ProcessInfo *)a;
    const ProcessInfo *p2 = (const ProcessInfo *)b;
    float mem1 = atof(p1->mem);
    float mem2 = atof(p2->mem);
    return (mem2 > mem1) - (mem2 < mem1); // Descending
}

static int compare_by_cpu(const void *a, const void *b) {
    const ProcessInfo *p1 = (const ProcessInfo *)a;
    const ProcessInfo *p2 = (const ProcessInfo *)b;
    float cpu1 = atof(p1->cpu);
    float cpu2 = atof(p2->cpu);
    return (cpu2 > cpu1) - (cpu2 < cpu1); // Descending
}

static int compare_by_pid(const void *a, const void *b) {
    const ProcessInfo *p1 = (const ProcessInfo *)a;
    const ProcessInfo *p2 = (const ProcessInfo *)b;
    int pid1 = atoi(p1->pid);
    int pid2 = atoi(p2->pid);
    return pid1 - pid2; // Ascending
}

/*
 * Sorts the list of processes by given sort type
 */
void sort_processes(ProcessInfo *list, int count, SortType sort_type) {
    switch (sort_type) {
    case SORT_BY_MEM:
        qsort(list, count, sizeof(ProcessInfo), compare_by_mem);
        break;
    case SORT_BY_CPU:
        qsort(list, count, sizeof(ProcessInfo), compare_by_cpu);
        break;
    case SORT_BY_PID:
        qsort(list, count, sizeof(ProcessInfo), compare_by_pid);
        break;
    default:
        break;
    }
}
