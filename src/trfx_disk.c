/*
 * Copyright (C) 2025 Masoud Bolhassani <masoud.bolhassani@gmail.com>
 *
 * This file is part of Trafix.
 *
 * Trafix is released under the GNU General Public License v3 (GPL-3.0).
 * See LICENSE file for details.
 */
#include "trfx_disk.h"
#include <mntent.h>
#include <stdio.h>
#include <string.h>
#include <sys/statvfs.h>

int get_disk_info(DiskInfo *disks, int max_disks, double *total_used_mb,
                  double *total_total_mb) {
  FILE *fp = setmntent("/etc/mtab", "r");
  if (!fp)
    return 0;

  struct mntent *mnt;
  int count = 0;

  if (total_used_mb)
    *total_used_mb = 0.0;
  if (total_total_mb)
    *total_total_mb = 0.0;

  while ((mnt = getmntent(fp))) {
    const char *fs = mnt->mnt_fsname;
    const char *dir = mnt->mnt_dir;

    // Skip known virtual/unwanted filesystems
    if (strncmp(fs, "tmpfs", 5) == 0 || strncmp(fs, "devtmpfs", 8) == 0 ||
        strncmp(fs, "efivarfs", 8) == 0 || strncmp(fs, "fuse", 4) == 0 ||
        strstr(fs, "squashfs") != NULL || strstr(fs, "overlay") != NULL ||
        strstr(fs, "cgroup") != NULL || strstr(fs, "proc") != NULL ||
        strstr(fs, "sysfs") != NULL || strstr(fs, "debugfs") != NULL)
      continue;

    // Skip runtime and system mount points
    if (strstr(dir, "/run/") == dir || strstr(dir, "/dev/") == dir ||
        strstr(dir, "/sys/") == dir || strstr(dir, "/proc/") == dir)
      continue;

    struct statvfs vfs;
    if (statvfs(dir, &vfs) != 0) {
      continue;
    }

    double total_mb = (double)vfs.f_blocks * vfs.f_frsize / 1024.0 / 1024.0;
    double free_mb = (double)vfs.f_bfree * vfs.f_frsize / 1024.0 / 1024.0;
    double used_mb = total_mb - free_mb;

    if (total_mb <= 0.0 || used_mb <= 0.0)
      continue;

    double usage = (used_mb / total_mb) * 100.0;

    if (count >= max_disks)
      break;

    // Populate disk info
    strncpy(disks[count].mount_point, dir,
            sizeof(disks[count].mount_point) - 1);
    disks[count].mount_point[sizeof(disks[count].mount_point) - 1] = '\0';

    strncpy(disks[count].filesystem, fs, sizeof(disks[count].filesystem) - 1);
    disks[count].filesystem[sizeof(disks[count].filesystem) - 1] = '\0';

    disks[count].used_mb = used_mb;
    disks[count].total_mb = total_mb;
    disks[count].usage_percent = usage;
    disks[count].temperature = -1.0; // Not available

    // Accumulate totals
    if (total_used_mb)
      *total_used_mb += used_mb;
    if (total_total_mb)
      *total_total_mb += total_mb;

    count++;
  }

  endmntent(fp);
  return count;
}
