/*
 * Copyright (C) 2025 Masoud Bolhassani <masoud.bolhassani@gmail.com>
 *
 * This file is part of Trafix.
 *
 * Trafix is released under the GNU General Public License v3 (GPL-3.0).
 * See LICENSE file for details.
 */

#ifndef TRFX_DISK_H
#define TRFX_DISK_H

#define MAX_DISKS 100

typedef struct {
    char mount_point[64];
    char filesystem[32];
    double used_mb;
    double total_mb;
    double usage_percent;
    double temperature;
} DiskInfo;

int get_disk_info(DiskInfo *disks, int max_disks, double *total_used_mb, double *total_total_mb);

#endif
