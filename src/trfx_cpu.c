/*
 *
 * Copyright (C) 2025 Masoud Bolhassani <masoud.bolhassani@gmail.com>
 * This file is part of Trafix.
 *
 * Trafix is released under the GNU General Public License v3 (GPL-3.0).
 * See LICENSE file for details.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include "trfx_cpu.h"

CPUInfo get_cpu_info() {
    CPUInfo info = {0};
    unsigned long long prev_total[MAX_CPU_CORES + 1], prev_idle[MAX_CPU_CORES + 1];
    unsigned long long curr_total[MAX_CPU_CORES + 1], curr_idle[MAX_CPU_CORES + 1];

    // Gather initial stats
    for (int i = 0; i <= MAX_CPU_CORES; ++i)
        prev_total[i] = prev_idle[i] = 0;

    FILE *fp = fopen("/proc/stat", "r");
    if (!fp) return info;

    char line[256];
    int idx = 0;
    while (fgets(line, sizeof(line), fp)) {
        if (strncmp(line, "cpu", 3) != 0) break;

        unsigned long long user, nice, system, idle_, iowait, irq, softirq, steal;
        sscanf(line, "%*s %llu %llu %llu %llu %llu %llu %llu %llu",
               &user, &nice, &system, &idle_, &iowait, &irq, &softirq, &steal);

        prev_idle[idx] = idle_;
        prev_total[idx] = user + nice + system + idle_ + iowait + irq + softirq + steal;
        idx++;
    }
    fclose(fp);

    usleep(100000); // wait 100ms

    // Gather second stats
    fp = fopen("/proc/stat", "r");
    if (!fp) return info;

    idx = 0;
    while (fgets(line, sizeof(line), fp)) {
        if (strncmp(line, "cpu", 3) != 0) break;

        unsigned long long user, nice, system, idle_, iowait, irq, softirq, steal;
        sscanf(line, "%*s %llu %llu %llu %llu %llu %llu %llu %llu",
               &user, &nice, &system, &idle_, &iowait, &irq, &softirq, &steal);

        curr_idle[idx] = idle_;
        curr_total[idx] = user + nice + system + idle_ + iowait + irq + softirq + steal;

        unsigned long long delta_total = curr_total[idx] - prev_total[idx];
        unsigned long long delta_idle = curr_idle[idx] - prev_idle[idx];

        float usage = 100.0f * (delta_total - delta_idle) / (float)delta_total;
        if (idx == 0) {
            info.avg_usage = usage;
        } else {
            info.usage_per_core[idx - 1] = usage;
        }

        idx++;
    }

    info.num_cores = idx - 1;
    fclose(fp);

    // Read CPU frequencies
    for (int i = 0; i < info.num_cores; ++i) {
      char path[128];
      snprintf(path, sizeof(path),
               "/sys/devices/system/cpu/cpu%d/cpufreq/scaling_cur_freq", i);
      FILE *f = fopen(path, "r");
      if (f) {
        int khz;
        if (fscanf(f, "%d", &khz) == 1) {
          info.frequency_per_core[i] = khz / 1000.0f;
        } else {
          info.frequency_per_core[i] = 0.0f; // fallback on parse failure
        }
        fclose(f);
      } else {
        info.frequency_per_core[i] = 0.0f; // fallback on open failure
      }
    }

    // Try to get CPU temperature
    FILE *sensors = popen("sensors 2>/dev/null | grep 'Core 0:'", "r");
    if (sensors) {
        char line[128];
        if (fgets(line, sizeof(line), sensors)) {
            float temp;
            if (sscanf(line, "Core 0: +%f", &temp) == 1) {
                info.temperature = temp;
            }
        }
        pclose(sensors);
    } else {
        info.temperature = -1.0f;
    }

    return info;
}
