#ifndef TRFX_CONNECTIONS_H
#define TRFX_CONNECTIONS_H

char** get_active_connections(int *num_connections);
void free_active_connections(char **data, int num_connections);

#endif
