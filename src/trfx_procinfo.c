#include "trfx_procinfo.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int get_top_processes_by_mem(ProcessInfo *list, int max_count) {
    char cmd[256] = "ps -eo pid,user,pri,ni,vsize,rss,stat,pcpu,pmem,time,comm --sort=-pmem  2>/dev/null | head -n 50";
    FILE *fp = popen(cmd, "r");
    if (!fp) return 0;

    char line[MAX_LINE_LEN];
    int count = 0;

    // skip header
    fgets(line, sizeof(line), fp);

    while (fgets(line, sizeof(line), fp) && count < max_count) {
        ProcessInfo *p = &list[count];

        sscanf(line, "%15s %31s %3s %3s %15s %15s %15s %1s %7s %7s %15s %63[^\n]",
               p->pid, p->user, p->pr, p->ni, p->virt, p->res, p->shr,
               p->state, p->cpu, p->mem, p->time, p->command);

        count++;
    }

    pclose(fp);
    return count;
}
