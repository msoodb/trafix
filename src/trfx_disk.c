#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <stdio.h>
#include <sys/statvfs.h>

#define MAX_PARTITIONS 8  // Adjust based on the number of partitions
#define MOUNT_FILE "/proc/mounts"
#define STAT_FILE "/proc/diskstats"

// Structure to store disk usage data
typedef struct {
    char partition[32];
    int used;
    int total;
} DiskUsage;

// Helper function to check if a partition is a real disk
int is_real_disk(const char *partition, const char *fs) {
    const char *invalid_fs[] = {
        "tmpfs", "devtmpfs", "devpts", "sysfs", "securityfs", "cgroup2", "cgroup",
        "proc", "pstore", "efivarfs", "bpf", "configfs", "systemd-1", "hugetlbfs",
        "mqueue", "tracefs", "overlay"
    };

    for (size_t i = 0; i < sizeof(invalid_fs) / sizeof(invalid_fs[0]); i++) {
        if (strcmp(fs, invalid_fs[i]) == 0) {
            return 0; // Not a real disk
        }
    }

    // Ensure the partition name starts with "/dev/" (e.g., "/dev/sda1")
    if (strncmp(partition, "/dev/", 5) != 0) {
        return 0; // Skip non-device partitions
    }

    return 1; // It's a real disk
}

char** get_disk_usage(int *num_partitions) {
    FILE *fp;
    char line[256];
    *num_partitions = 0;

    // Allocate memory for disk data
    char **disk_data = (char **)malloc(MAX_PARTITIONS * sizeof(char *));
    for (int i = 0; i < MAX_PARTITIONS; i++) {
        disk_data[i] = (char *)malloc(50 * sizeof(char));
    }

    fp = fopen(MOUNT_FILE, "r");
    if (fp == NULL) {
        perror("Error opening /proc/mounts");
        exit(1);
    }

    while (fgets(line, sizeof(line), fp) != NULL && *num_partitions < MAX_PARTITIONS) {
        char partition[32], mountpoint[128], fs[32];
        if (sscanf(line, "%31s %127s %31s", partition, mountpoint, fs) == 3) {
            if (!is_real_disk(partition, fs)) continue;  // Skip non-storage filesystems

            struct statvfs stats;
            if (statvfs(mountpoint, &stats) == 0) {
                int total = (stats.f_blocks * stats.f_frsize) / (1024 * 1024);
                int free = (stats.f_bfree * stats.f_frsize) / (1024 * 1024);
                int used = total - free;
                float usage = (total > 0) ? ((float)used / total) * 100.0 : 0.0;

                snprintf(disk_data[*num_partitions], 50, "  %-15s | %10.2f %%", partition, usage);
                (*num_partitions)++;
            }
        }
    }

    fclose(fp);
    return disk_data;
}
