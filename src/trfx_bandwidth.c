#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ifaddrs.h>
#include <arpa/inet.h>
#include <unistd.h>

#define MAX_INTERFACES 20
#define LINE_BUFFER 256

typedef struct {
    char name[32];
    unsigned long rx_bytes;
    unsigned long tx_bytes;
} NetStat;

static NetStat prev_stats[MAX_INTERFACES];
static int prev_count = 0;
static int initialized = 0;


char *get_ip_address(const char *ifname) {
    struct ifaddrs *ifaddr, *ifa;
    static char ip[INET_ADDRSTRLEN];

    if (getifaddrs(&ifaddr) == -1) {
        perror("getifaddrs");
        return NULL;
    }

    for (ifa = ifaddr; ifa; ifa = ifa->ifa_next) {
        if (ifa->ifa_addr && strcmp(ifa->ifa_name, ifname) == 0 &&
            ifa->ifa_addr->sa_family == AF_INET) {
            struct sockaddr_in *sa = (struct sockaddr_in *)ifa->ifa_addr;
            inet_ntop(AF_INET, &sa->sin_addr, ip, sizeof(ip));
            freeifaddrs(ifaddr);
            return ip;
        }
    }

    freeifaddrs(ifaddr);
    return NULL;
}

char *get_wifi_ssid(const char *ifname) {
    static char ssid[128];
    FILE *fp;
    char cmd[256], line[256];

    snprintf(cmd, sizeof(cmd), "iw dev %s link 2>/dev/null", ifname);
    fp = popen(cmd, "r");
    if (!fp) return NULL;

    ssid[0] = '\0';

    while (fgets(line, sizeof(line), fp)) {
        char *p = strstr(line, "SSID:");
        if (p) {
            sscanf(p + 5, "%127[^\n]", ssid);
            break;
        }
    }

    pclose(fp);
    return ssid[0] ? ssid : NULL;
}

int is_wifi_interface(const char *iface_name) {
    char path[128];
    snprintf(path, sizeof(path), "/sys/class/net/%s/wireless", iface_name);
    return access(path, F_OK) == 0;  // Exists = Wi-Fi
}

int is_vpn_interface(const char *iface_name) {
    return strncmp(iface_name, "tun", 3) == 0 ||
           strncmp(iface_name, "ppp", 3) == 0 ||
           strncmp(iface_name, "wg", 2) == 0;
}

char* get_gateway_ip() {
    FILE *fp = popen("ip route | grep default | awk '{print $3}'", "r");
    if (!fp) return NULL;

    static char gateway[64];
    if (fgets(gateway, sizeof(gateway), fp)) {
        gateway[strcspn(gateway, "\n")] = '\0';  // Remove newline
    } else {
        strcpy(gateway, "N/A");
    }
    pclose(fp);
    return gateway;
}

char* get_dns_servers() {
    FILE *fp = fopen("/etc/resolv.conf", "r");
    if (!fp) return NULL;

    static char dns_list[256];
    dns_list[0] = '\0';  // ✅ Clear previous data on each call

    char line[128];
    char *dns_entries[10];
    int dns_count = 0;

    while (fgets(line, sizeof(line), fp)) {
        if (strncmp(line, "nameserver", 10) == 0) {
            char *dns = strchr(line, ' ');
            if (dns) {
                dns += 1;
                dns[strcspn(dns, "\n")] = '\0';  // Remove newline

                // Check for duplicates
                int duplicate = 0;
                for (int i = 0; i < dns_count; i++) {
                    if (strcmp(dns_entries[i], dns) == 0) {
                        duplicate = 1;
                        break;
                    }
                }

                if (!duplicate && dns_count < 10) {
                    dns_entries[dns_count++] = strdup(dns);
                }
            }
        }
    }
    fclose(fp);

    if (dns_count == 0) {
        strcpy(dns_list, "N/A");
    } else {
        for (int i = 0; i < dns_count; i++) {
            strcat(dns_list, dns_entries[i]);
            if (i < dns_count - 1)
                strcat(dns_list, ", ");
            free(dns_entries[i]);  // Free allocated strings
        }
    }

    return dns_list;
}

static int read_net_stats(NetStat stats[], int *count) {
    FILE *fp = fopen("/proc/net/dev", "r");
    if (!fp) {
        perror("Failed to open /proc/net/dev");
        return -1;
    }

    char line[LINE_BUFFER];
    *count = 0;

    // Skip headers
    fgets(line, LINE_BUFFER, fp);
    fgets(line, LINE_BUFFER, fp);

    while (fgets(line, LINE_BUFFER, fp) && *count < MAX_INTERFACES) {
        NetStat *stat = &stats[*count];
        if (sscanf(line, " %31[^:]: %lu %*u %*u %*u %*u %*u %*u %*u %lu",
                   stat->name, &stat->rx_bytes, &stat->tx_bytes) == 3) {
            (*count)++;
        }
    }

    fclose(fp);
    return 0;
}

static NetStat* find_prev_stat(const char *name) {
    for (int i = 0; i < prev_count; i++) {
        if (strcmp(prev_stats[i].name, name) == 0)
            return &prev_stats[i];
    }
    return NULL;
}

char** get_bandwidth_usage(int *num_interfaces) {
    NetStat curr_stats[MAX_INTERFACES];
    int curr_count = 0;

    if (read_net_stats(curr_stats, &curr_count) != 0) {
        *num_interfaces = 0;
        return NULL;
    }

    char **data = (char **)malloc(curr_count * sizeof(char *));
    if (!data) {
        perror("Memory allocation failed");
        exit(1);
    }

    for (int i = 0; i < curr_count; i++) {
        data[i] = (char *)malloc(128 * sizeof(char));
        if (!data[i]) {
            perror("Memory allocation failed");
            exit(1);
        }

        float delta_tx = 0, delta_rx = 0;

        if (initialized) {
            NetStat *prev = find_prev_stat(curr_stats[i].name);
            if (prev) {
                delta_tx = (curr_stats[i].tx_bytes - prev->tx_bytes) / (1024.0 * 1024.0);
                delta_rx = (curr_stats[i].rx_bytes - prev->rx_bytes) / (1024.0 * 1024.0);
            }
        }

	snprintf(data[i], 128, " %-15.15s | %10.2f | %10.2f", curr_stats[i].name, delta_tx, delta_rx);

    }

    // Update previous state
    memcpy(prev_stats, curr_stats, sizeof(NetStat) * curr_count);
    prev_count = curr_count;
    initialized = 1;

    *num_interfaces = curr_count;
    return data;
}

void free_bandwidth_usage(char **data, int num_interfaces) {
    for (int i = 0; i < num_interfaces; i++) {
        free(data[i]);
    }
    free(data);
}
