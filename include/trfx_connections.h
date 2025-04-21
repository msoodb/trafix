#ifndef TRFX_CONNECTIONS_H
#define TRFX_CONNECTIONS_H

#define MAX_CONNECTIONS 512

typedef struct {
    char protocol[8];
    char local_addr[64];
    char remote_addr[64];
    char state[32];
} ConnectionInfo;

int get_connection_info(ConnectionInfo *connections, int max_conns);

#endif // TRFX_CONNECTIONS_H
