/*
 * Copyright (C) 2025 Masoud Bolhassani <masoud.bolhassani@gmail.com>
 *
 * This file is part of Trafix.
 *
 * Trafix is released under the GNU General Public License v3 (GPL-3.0).
 * See LICENSE file for details.
 */

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include "trfx_sysinfo.h"

SystemOverview get_system_overview() {
    SystemOverview info;
    strcpy(info.hostname, "N/A");
    strcpy(info.os_version, "N/A");
    strcpy(info.kernel_version, "N/A");
    strcpy(info.uptime, "N/A");
    strcpy(info.load_avg, "N/A");
    strcpy(info.logged_in_users, "N/A");

    // Hostname
    gethostname(info.hostname, sizeof(info.hostname));

    // OS Version
    FILE *fp = fopen("/etc/os-release", "r");
    if (fp) {
        char line[256];
        while (fgets(line, sizeof(line), fp)) {
            if (strncmp(line, "PRETTY_NAME=", 12) == 0) {
                sscanf(line, "PRETTY_NAME=\"%127[^\"]\"", info.os_version);
                break;
            }
        }
        fclose(fp);
    }

    // Kernel Version
    fp = popen("uname -r 2>/dev/null", "r");
    if (fp) {
        fgets(info.kernel_version, sizeof(info.kernel_version), fp);
        info.kernel_version[strcspn(info.kernel_version, "\n")] = 0;
        pclose(fp);
    }

    // Uptime
    fp = fopen("/proc/uptime", "r");
    if (fp) {
        double uptime_secs;
        if (fscanf(fp, "%lf", &uptime_secs) == 1) {
            int days = uptime_secs / 86400;
            int hours = ((int)uptime_secs % 86400) / 3600;
            int minutes = ((int)uptime_secs % 3600) / 60;
            snprintf(info.uptime, sizeof(info.uptime), "%dd %dh %dm", days, hours, minutes);
        }
        fclose(fp);
    }

    // Load averages
    fp = fopen("/proc/loadavg", "r");
    if (fp) {
        float avg1, avg5, avg15;
        if (fscanf(fp, "%f %f %f", &avg1, &avg5, &avg15) == 3) {
            snprintf(info.load_avg, sizeof(info.load_avg), "%.2f %.2f %.2f", avg1, avg5, avg15);
        }
        fclose(fp);
    }

    // Logged-in users
    fp = popen("who | awk '{print $1}' | sort | uniq | tr '\\n' ' '", "r");
    if (fp) {
        fgets(info.logged_in_users, sizeof(info.logged_in_users), fp);
        info.logged_in_users[strcspn(info.logged_in_users, "\n")] = 0;
        pclose(fp);
    }

    return info;
}
