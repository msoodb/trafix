#include <stdio.h>
#include <stdlib.h>

char** get_active_connections(int *num_connections) {
    // Number of active connections
    *num_connections = 3;

    // Allocate memory for the connections
    char **data = (char **)malloc(*num_connections * sizeof(char *));
    
    // Check if memory allocation was successful
    if (data == NULL) {
        perror("Failed to allocate memory");
        exit(1);
    }

    // Allocate memory for each connection string
    for (int i = 0; i < *num_connections; i++) {
        data[i] = (char *)malloc(100 * sizeof(char));
        if (data[i] == NULL) {
            perror("Failed to allocate memory for connection");
            exit(1);
        }
    }

    // Generate active connection data
    for (int i = 0; i < *num_connections; i++) {
        snprintf(data[i], 100, "192.168.1.%3d:%-5d -> 10.0.0.%3d:%-5d  ESTABLISHED",
                 rand() % 255, rand() % 65535, rand() % 255, rand() % 65535);
    }

    return data;
}

void free_active_connections(char **data, int num_connections) {
    // Free the allocated memory for each connection string
    for (int i = 0; i < num_connections; i++) {
        free(data[i]);
    }
    // Free the memory for the array of strings
    free(data);
}
