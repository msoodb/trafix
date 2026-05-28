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
#include <sys/utsname.h>
#include <utmpx.h>
#include <unistd.h>
#include "trfx_sysinfo.h"

#define TRFX_LOGIN_NAME_MAX 32

static void append_unique_user(char users[][TRFX_LOGIN_NAME_MAX], int *user_count,
                               const char *user) {
    if (!user || user[0] == '\0')
        return;

    for (int i = 0; i < *user_count; i++) {
        if (strncmp(users[i], user, TRFX_LOGIN_NAME_MAX - 1) == 0)
            return;
    }

    if (*user_count < 32) {
        snprintf(users[*user_count], TRFX_LOGIN_NAME_MAX, "%.*s",
                 TRFX_LOGIN_NAME_MAX - 1, user);
        (*user_count)++;
    }
}

static void get_logged_in_users(char *buf, size_t bufsize) {
    char users[32][TRFX_LOGIN_NAME_MAX];
    int user_count = 0;

    if (!buf || bufsize == 0)
        return;

    buf[0] = '\0';

    setutxent();
    struct utmpx *entry;
    while ((entry = getutxent()) != NULL) {
        if (entry->ut_type == USER_PROCESS) {
            append_unique_user(users, &user_count, entry->ut_user);
        }
    }
    endutxent();

    if (user_count == 0) {
        snprintf(buf, bufsize, "N/A");
        return;
    }

    for (int i = 0; i < user_count; i++) {
        if (i > 0)
            strncat(buf, " ", bufsize - strlen(buf) - 1);
        strncat(buf, users[i], bufsize - strlen(buf) - 1);
    }
}

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
    info.hostname[sizeof(info.hostname) - 1] = '\0';

    // OS Version
    FILE *fp = fopen("/etc/os-release", "r");
    if (fp) {
        char line[256];
        while (fgets(line, sizeof(line), fp)) {
            if (strncmp(line, "PRETTY_NAME=", 12) == 0) {
                if (sscanf(line, "PRETTY_NAME=\"%127[^\"]\"", info.os_version) == 1) {
                    break;
                }
            }
        }
        fclose(fp);
    }

    // Kernel Version
    struct utsname uts;
    if (uname(&uts) == 0) {
        strncpy(info.kernel_version, uts.release,
                sizeof(info.kernel_version) - 1);
        info.kernel_version[sizeof(info.kernel_version) - 1] = '\0';
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
    get_logged_in_users(info.logged_in_users, sizeof(info.logged_in_users));

    return info;
}
