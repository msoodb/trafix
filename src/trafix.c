#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <string.h>
#include "trafix.h"
#include "trfx_config.h"

void print_active_connections() {
    printf("Active Network Connections:\n");
    for (int i = 0; i < 5; i++) {
        printf("Connection %d: IP 192.168.1.%d:%d -> 10.0.0.%d:%d [Status: ESTABLISHED]\n",
               i+1, rand() % 255, rand() % 65535, rand() % 255, rand() % 65535);
    }
}

void print_bandwidth_usage() {
    printf("\nBandwidth Usage:\n");
    printf("Incoming: %d MB/s\n", rand() % 100);
    printf("Outgoing: %d MB/s\n", rand() % 100);
}

void print_top_talkers() {
    printf("\nTop Talkers:\n");
    for (int i = 0; i < 3; i++) {
        printf("Talker %d: IP 192.168.1.%d - %d MB/s\n", i+1, rand() % 255, rand() % 100);
    }
}

int main(int argc, char *argv[]) {
    srand(time(NULL));  // Seed for random number generation

    // Read configuration
    trfx_read_config(CONFIG_FILE);

    printf("Configuration Loaded:\n");
    printf("Alert Bandwidth: %d MB\n", alert_bandwidth);
    printf("Filter IP: %s\n", filter_ip);
    printf("Filter Process: %s\n", filter_process);

    if (argc < 2) {
        printf("Usage: %s [connections|bandwidth|top]\n", argv[0]);
        return 1;
    }

    if (strcmp(argv[1], "connections") == 0) {
        print_active_connections();
    } else if (strcmp(argv[1], "bandwidth") == 0) {
        print_bandwidth_usage();
    } else if (strcmp(argv[1], "top") == 0) {
        print_top_talkers();
    } else {
        printf("Invalid command. Use 'connections', 'bandwidth', or 'top'.\n");
    }

    return 0;
}
