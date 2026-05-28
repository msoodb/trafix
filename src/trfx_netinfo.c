/*
 * Copyright (C) 2025 Masoud Bolhassani <masoud.bolhassani@gmail.com>
 *
 * This file is part of Trafix.
 *
 * Trafix is released under the GNU General Public License v3 (GPL-3.0).
 * See LICENSE file for details.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <ifaddrs.h>
#include <arpa/inet.h>
#include <unistd.h>
#include "trfx_netinfo.h"

#define LINE_BUFFER 256

static TrfxInterfaceStat prev_stats[TRFX_MAX_INTERFACES];
static int prev_count = 0;
static int initialized = 0;

int trfx_is_valid_interface_name(const char *ifname) {
    if (!ifname || ifname[0] == '\0')
        return 0;

    for (const char *p = ifname; *p; p++) {
        if (!(isalnum((unsigned char)*p) || *p == '_' || *p == '-' ||
              *p == '.' || *p == ':')) {
            return 0;
        }
    }

    return 1;
}

char *get_ip_address(const char *ifname) {
    struct ifaddrs *ifaddr, *ifa;
    static char ip[INET_ADDRSTRLEN];

    if (!trfx_is_valid_interface_name(ifname))
        return NULL;

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

    // Check if input is NULL
    if (!trfx_is_valid_interface_name(ifname)) {
        return NULL;
    }

    // Build the command safely
    snprintf(cmd, sizeof(cmd), "iw dev %s link 2>/dev/null", ifname);

    // Execute the command
    fp = popen(cmd, "r");
    if (!fp) return NULL;

    ssid[0] = '\0';

    // Read output and search for SSID
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
    if (!trfx_is_valid_interface_name(iface_name))
        return 0;

    char path[128];
    snprintf(path, sizeof(path), "/sys/class/net/%s/wireless", iface_name);
    return access(path, F_OK) == 0;  // Exists = Wi-Fi
}

int is_vpn_interface(const char *iface_name) {
    if (!trfx_is_valid_interface_name(iface_name))
        return 0;

    return strncmp(iface_name, "tun", 3) == 0 ||
           strncmp(iface_name, "ppp", 3) == 0 ||
           strncmp(iface_name, "wg", 2) == 0;
}

int trfx_parse_default_route_line(const char *line, char *gateway,
                                  size_t gateway_size, char *metric,
                                  size_t metric_size) {
    char copy[256];

    if (!line || !gateway || gateway_size == 0 || !metric || metric_size == 0)
        return 0;

    snprintf(gateway, gateway_size, "N/A");
    snprintf(metric, metric_size, "N/A");

    snprintf(copy, sizeof(copy), "%s", line);

    char *token = strtok(copy, " \t\n");
    if (!token || strcmp(token, "default") != 0)
        return 0;

    while ((token = strtok(NULL, " \t\n")) != NULL) {
        if (strcmp(token, "via") == 0) {
            char *value = strtok(NULL, " \t\n");
            if (value)
                snprintf(gateway, gateway_size, "%s", value);
        } else if (strcmp(token, "metric") == 0) {
            char *value = strtok(NULL, " \t\n");
            if (value)
                snprintf(metric, metric_size, "%s", value);
        }
    }

    return 1;
}

char* get_gateway_ip() {
    FILE *fp = popen("ip route 2>/dev/null", "r");
    if (!fp) return NULL;

    static char gateway[64];
    char metric[64];
    char buffer[256];
    snprintf(gateway, sizeof(gateway), "N/A");

    while (fgets(buffer, sizeof(buffer), fp)) {
        if (trfx_parse_default_route_line(buffer, gateway, sizeof(gateway),
                                          metric, sizeof(metric))) {
            break;
        }
    }

    pclose(fp);
    return gateway;
}

// Function to get the default gateway and metric
void get_default_gateway_and_metric(char *gateway, char *metric) {
    FILE *fp = popen("ip route 2>/dev/null", "r");
    if (!fp) {
        strcpy(gateway, "N/A");
        strcpy(metric, "N/A");
        return;
    }

    char buffer[256];
    strcpy(gateway, "N/A");
    strcpy(metric, "N/A");
    while (fgets(buffer, sizeof(buffer), fp)) {
        if (trfx_parse_default_route_line(buffer, gateway, 64, metric, 64)) {
            break;
        }
    }

    pclose(fp);
}

// Function to get the routing table summary
void get_routing_table_summary(char *routing_table) {
    FILE *fp = popen("ip route", "r");
    if (!fp) {
        return;
    }

    routing_table[0] = '\0';  // Clear the string
    char buffer[512];
    while (fgets(buffer, sizeof(buffer), fp) != NULL) {
        strcat(routing_table, buffer);  // Append the output to routing_table string
    }
    pclose(fp);
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

int trfx_parse_interface_stats_file(FILE *fp, TrfxInterfaceStat stats[],
                                    int max_stats) {
    char line[LINE_BUFFER];
    int count = 0;

    if (!fp || !stats || max_stats <= 0) {
        return -1;
    }

    // Skip headers safely
    if (fgets(line, LINE_BUFFER, fp) == NULL) {
        return -1;
    }
    if (fgets(line, LINE_BUFFER, fp) == NULL) {
        return -1;
    }

    while (fgets(line, LINE_BUFFER, fp) && count < max_stats) {
        TrfxInterfaceStat *stat = &stats[count];
        if (sscanf(line, " %31[^:]: %lu %*u %*u %*u %*u %*u %*u %*u %lu",
                   stat->name, &stat->rx_bytes, &stat->tx_bytes) == 3) {
            count++;
        }
    }

    return count;
}

int trfx_parse_interface_stats_path(const char *path, TrfxInterfaceStat stats[],
                                    int max_stats) {
    FILE *fp = fopen(path, "r");
    if (!fp) {
        return -1;
    }

    int count = trfx_parse_interface_stats_file(fp, stats, max_stats);
    fclose(fp);
    return count;
}

int trfx_read_interface_stats(TrfxInterfaceStat stats[], int max_stats) {
    return trfx_parse_interface_stats_path("/proc/net/dev", stats, max_stats);
}

static TrfxInterfaceStat* find_prev_stat(const char *name) {
    for (int i = 0; i < prev_count; i++) {
        if (strcmp(prev_stats[i].name, name) == 0)
            return &prev_stats[i];
    }
    return NULL;
}

void trfx_format_net_bytes(double bytes, char *buf, size_t bufsize) {
    if (bytes < 1024) {
        snprintf(buf, bufsize, "%.2f B", bytes);
    } else if (bytes < 1024 * 1024) {
        snprintf(buf, bufsize, "%.2f KB", bytes / 1024);
    } else if (bytes < 1024 * 1024 * 1024) {
        snprintf(buf, bufsize, "%.2f MB", bytes / (1024 * 1024));
    } else {
        snprintf(buf, bufsize, "%.2f GB", bytes / (1024 * 1024 * 1024));
    }
}

void trfx_format_interface_usage_line(const char *name, double tx_bytes,
                                      double rx_bytes, char *buf,
                                      size_t bufsize) {
    char formatted_sent[20];
    char formatted_recv[20];

    trfx_format_net_bytes(tx_bytes, formatted_sent, sizeof(formatted_sent));
    trfx_format_net_bytes(rx_bytes, formatted_recv, sizeof(formatted_recv));

    snprintf(buf, bufsize, " %-15.15s | %10s | %10s", name, formatted_sent,
             formatted_recv);
}

// Function to get bandwidth usage, modified to use format_bytes
char** get_interfaces_usage(int *num_interfaces) {
    TrfxInterfaceStat curr_stats[TRFX_MAX_INTERFACES];
    int curr_count = trfx_read_interface_stats(curr_stats, TRFX_MAX_INTERFACES);

    if (curr_count < 0) {
        *num_interfaces = 0;
        return NULL;
    }

    char **data = (char **)malloc(curr_count * sizeof(char *));
    if (!data) {
        exit(1);
    }

    for (int i = 0; i < curr_count; i++) {
        data[i] = (char *)malloc(128 * sizeof(char));
        if (!data[i]) {
            exit(1);
        }

        double delta_tx = 0, delta_rx = 0;

        // Calculate delta if previous stats exist
        if (initialized) {
            TrfxInterfaceStat *prev = find_prev_stat(curr_stats[i].name);
            if (prev) {
                delta_tx = curr_stats[i].tx_bytes - prev->tx_bytes;
                delta_rx = curr_stats[i].rx_bytes - prev->rx_bytes;
            }
        }

        trfx_format_interface_usage_line(curr_stats[i].name, delta_tx,
                                         delta_rx, data[i], 128);
    }

    // Update previous state
    memcpy(prev_stats, curr_stats, sizeof(TrfxInterfaceStat) * curr_count);
    prev_count = curr_count;
    initialized = 1;

    *num_interfaces = curr_count;
    return data;
}

void free_interfaces_usage(char **data, int num_interfaces) {
    for (int i = 0; i < num_interfaces; i++) {
        free(data[i]);
    }
    free(data);
}
