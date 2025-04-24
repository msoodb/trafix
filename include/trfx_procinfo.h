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
#define MAX_LINE_LEN 256

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

int get_top_processes_by_mem(ProcessInfo *list, int max_count);

#endif
