#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <time.h>
#include "trafix.h"
#include "trfx_config.h"
#include "trfx_connections.h"
#include "trfx_bandwidth.h"
#include "trfx_top.h"
#include "trfx_dashboard.h"

void print_usage(const char *prog_name) {
    printf("Usage: %s [-c] [-b] [-t] [-d] [-h]\n", prog_name);
    printf("  -c  Show active network connections\n");
    printf("  -b  Show bandwidth usage\n");
    printf("  -t  Show top talkers\n");
    printf("  -d  Show interactive dashboard\n");
    printf("  -h  Show this help message\n");
}

int main(int argc, char *argv[]) {
    srand(time(NULL));

    read_config(CONFIG_FILE);

    int opt;
    int show_connections = 0, show_bandwidth = 0, show_top_talkers = 0, show_dashboard = 1;

    while ((opt = getopt(argc, argv, "cbtdh")) != -1) {
        switch (opt) {
            case 'c': show_connections = 1; break;
            case 'b': show_bandwidth = 1; break;
            case 't': show_top_talkers = 1; break;
            case 'd': show_dashboard = 1; break;
            case 'h': print_usage(argv[0]); return 0;
            default: print_usage(argv[0]); return 1;
        }
    }

    if (show_dashboard) {
        start_dashboard();
    } else {
        if (!show_connections && !show_bandwidth && !show_top_talkers) {
            print_usage(argv[0]);
            return 1;
        }
    }

    return 0;
}
