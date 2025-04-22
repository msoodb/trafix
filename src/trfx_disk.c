#include <stdio.h>
#include <mntent.h>
#include <string.h>
#include <sys/statvfs.h>
#include "trfx_disk.h"

int get_disk_info(DiskInfo *disks, int max_disks, double *total_used_mb, double *total_total_mb) {
    FILE *fp = setmntent("/etc/mtab", "r");
    if (!fp)
        return 0;

    struct mntent *mnt;
    int count = 0;

    // Initialize totals
    if (total_used_mb) *total_used_mb = 0.0;
    if (total_total_mb) *total_total_mb = 0.0;

    while ((mnt = getmntent(fp)) && count < max_disks) {
        struct statvfs vfs;
        if (statvfs(mnt->mnt_dir, &vfs) != 0)
            continue;

        double total_mb = (double)vfs.f_blocks * vfs.f_frsize / 1024.0 / 1024.0;
        double free_mb  = (double)vfs.f_bfree  * vfs.f_frsize / 1024.0 / 1024.0;
        double used_mb  = total_mb - free_mb;
        double usage    = total_mb > 0 ? (used_mb / total_mb) * 100.0 : 0.0;

        // Copy mount point and filesystem name
        strncpy(disks[count].mount_point, mnt->mnt_dir, sizeof(disks[count].mount_point) - 1);
        disks[count].mount_point[sizeof(disks[count].mount_point) - 1] = '\0';

        strncpy(disks[count].filesystem, mnt->mnt_fsname, sizeof(disks[count].filesystem) - 1);
        disks[count].filesystem[sizeof(disks[count].filesystem) - 1] = '\0';

        // Assign values
        disks[count].used_mb = used_mb;
        disks[count].total_mb = total_mb;
        disks[count].usage_percent = usage;
        disks[count].temperature = -1.0;  // Placeholder

        // Update totals
        if (total_used_mb) *total_used_mb += used_mb;
        if (total_total_mb) *total_total_mb += total_mb;

        count++;
    }

    endmntent(fp);
    return count;
}
