#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <stdio.h>

#define MAX_CORES 8  // Adjust based on the number of cores
#define STAT_FILE "/proc/stat"

// Global arrays to store previous values
static int total_user_prev[MAX_CORES] = {0};
static int total_nice_prev[MAX_CORES] = {0};
static int total_system_prev[MAX_CORES] = {0};
static int total_idle_prev[MAX_CORES] = {0};
static int total_time_prev[MAX_CORES] = {0};

// Function to parse CPU statistics and calculate the usage
char** get_cpu_usage(int *num_cores) {
    FILE *fp;
    char line[256];
    int total_user, total_nice, total_system, total_idle;
    
    fp = fopen(STAT_FILE, "r");
    if (fp == NULL) {
        perror("Error opening /proc/stat");
        exit(1);
    }

    *num_cores = 0;

    // Allocate memory for CPU data
    char **cpu_data = (char **)malloc(MAX_CORES * sizeof(char *));
    for (int i = 0; i < MAX_CORES; i++) {
        cpu_data[i] = (char *)malloc(50 * sizeof(char));
    }

    while (fgets(line, sizeof(line), fp) != NULL) {
        if (strncmp(line, "cpu", 3) == 0 && line[3] != ' ') {
            int core_id;
            if (sscanf(line, "cpu%d %d %d %d %d", &core_id, &total_user, &total_nice, &total_system, &total_idle) == 5) {
                if (*num_cores >= MAX_CORES) break;

                int total_time = total_user + total_nice + total_system + total_idle;
                int delta_total = total_time - total_time_prev[core_id];
                int delta_work = (total_user + total_nice + total_system) - 
                                 (total_user_prev[core_id] + total_nice_prev[core_id] + total_system_prev[core_id]);

                // Prevent division by zero
                float usage = (delta_total > 0) ? ((float)delta_work / delta_total) * 100.0 : 0.0;

                // Store the formatted result
		snprintf(cpu_data[*num_cores], 50, "CPU%d  |  %.2f %s", core_id, usage, "");


                // Update previous values for next iteration
                total_user_prev[core_id] = total_user;
                total_nice_prev[core_id] = total_nice;
                total_system_prev[core_id] = total_system;
                total_idle_prev[core_id] = total_idle;
                total_time_prev[core_id] = total_time;

                (*num_cores)++;
            }
        }
    }

    fclose(fp);
    return cpu_data;
}
