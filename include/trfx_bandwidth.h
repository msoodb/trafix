#ifndef TRFX_BANDWIDTH_H
#define TRFX_BANDWIDTH_H

const char *generate_random_interface_name();
char** get_bandwidth_usage(int *num_interfaces);
void free_bandwidth_usage(char **data, int num_interfaces);

#endif
