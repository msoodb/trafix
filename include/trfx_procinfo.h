/*
 * Copyright (C) 2025 Masoud Bolhassani <masoud.bolhassani@gmail.com>
 *
 * This file is part of Trafix.
 *
 * Trafix is released under the GNU General Public License v3 (GPL-3.0).
 * See LICENSE file for details.
 */

#ifndef TRFX_PROCINFO_H
#define TRFX_PROCINFO_H

#define MAX_PROCESSES 128
#define MAX_LINE_LEN 512


typedef struct {
    char pid[16];
    char user[32];
    char pr[4];
    char ni[4];
    char virt[16];
    char res[16];
    char shr[16];
    char state[2];
    char cpu[8];
    char mem[8];
    char time[16];
    char command[64];
} ProcessInfo;

typedef enum { SORT_BY_MEM, SORT_BY_CPU, SORT_BY_PID, SORT_MAX } SortType;
extern SortType current_sort_type;

#define SORT_BY_MEM 0
#define SORT_BY_CPU 1
#define SORT_BY_PID 2

int get_top_processes(ProcessInfo *list, int max_count, SortType sort_type);
void sort_processes(ProcessInfo *list, int count, SortType sort_type);

#endif /* TRFX_PROCINFO_H */
