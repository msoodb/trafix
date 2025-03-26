#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "trfx_config.h"

// Global configuration variables
int alert_bandwidth = 100;
char filter_ip[16] = "0.0.0.0";
char filter_process[50] = "";

void read_config(const char *config_file) {
    FILE *file = fopen(config_file, "r");
    if (!file) {
        perror("Error opening config file");
        return;
    }

    char line[256];
    while (fgets(line, sizeof(line), file)) {
        line[strcspn(line, "\n")] = 0;
        if (line[0] == '#' || line[0] == '\0') continue;

        if (strncmp(line, "alert_bandwidth", 15) == 0) {
            sscanf(line, "alert_bandwidth = %d", &alert_bandwidth);
        } else if (strncmp(line, "filter_ip", 9) == 0) {
            sscanf(line, "filter_ip = %15s", filter_ip);
        } else if (strncmp(line, "filter_process", 14) == 0) {
            sscanf(line, "filter_process = %49s", filter_process);
        }
    }

    fclose(file);
}
