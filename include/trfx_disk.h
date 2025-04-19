#ifndef TRFX_DISK_H
#define TRFX_DISK_H

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
