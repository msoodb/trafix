#include <ncurses.h>
#include <stdlib.h>
#include <time.h>

// Function to generate random interface name
const char* generate_random_interface_name() {
    static const char *interfaces[] = {"eth0", "wlan0", "lo", "eth1", "wlan1"};
    return interfaces[rand() % 5]; // Randomly select an interface from the list
}

// Function to generate random bandwidth data (dynamically allocated)
char** get_bandwidth_usage(int *num_interfaces) {
    // Random number of interfaces between 2 and 5
    *num_interfaces = rand() % 4 + 2;

    // Allocate memory for the interfaces
    char **data = (char **)malloc(*num_interfaces * sizeof(char *));
    if (data == NULL) {
        perror("Failed to allocate memory for bandwidth data");
        exit(1);
    }

    // Allocate memory for each interface string (e.g., 100 chars per interface)
    for (int i = 0; i < *num_interfaces; i++) {
        data[i] = (char *)malloc(100 * sizeof(char)); // Adjust size as needed
        if (data[i] == NULL) {
            perror("Failed to allocate memory for interface string");
            exit(1);
        }
    }

    // Generate random bandwidth data for each interface
    for (int i = 0; i < *num_interfaces; i++) {
        float sent_data = (rand() % 1000) / 10.0;   // Random sent data in MB
        float received_data = (rand() % 1000) / 10.0; // Random received data in MB
        snprintf(data[i], 100, "%-15s | %10.1f | %10.1f", generate_random_interface_name(), sent_data, received_data);
    }

    return data;
}

// Function to free the dynamically allocated memory for bandwidth data
void free_bandwidth_usage(char **data, int num_interfaces) {
    for (int i = 0; i < num_interfaces; i++) {
        free(data[i]);
    }
    free(data);
}
