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

/*
 * Read top processes into the list.
 * Returns number of processes read.
 */
int get_top_processes(ProcessInfo *list, int max_count, SortType sort_type)
{
    if (!list || max_count <= 0)
        return 0;

    const char *cmd = "ps -eo pid,user,pri,ni,vsize,rss,stat,pcpu,pmem,time,comm "
                      "--sort=-pmem 2>/dev/null | head -n 50";
    FILE *fp = popen(cmd, "r");
    if (!fp)
        return 0;

    char line[MAX_LINE_LEN];
    int count = 0;

    // Skip header
    if (!fgets(line, sizeof(line), fp)) {
        pclose(fp);
        return 0;
    }

    while (fgets(line, sizeof(line), fp) && count < max_count) {
        ProcessInfo *p = &list[count];

        memset(p, 0, sizeof(ProcessInfo));

        sscanf(line, "%15s %31s %3s %3s %15s %15s %15s %1s %7s %7s %15s %63[^\n]",
               p->pid, p->user, p->pr, p->ni, p->virt, p->res, p->shr, p->state,
               p->cpu, p->mem, p->time, p->command);

        count++;
    }

    pclose(fp);

    // Sort after fetching
    sort_processes(list, count, sort_type);

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
