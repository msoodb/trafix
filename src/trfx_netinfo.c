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
#include <errno.h>
#include <ifaddrs.h>
#include <arpa/inet.h>
#include <unistd.h>
#include "trfx_netinfo.h"
#include "trfx_wifi.h"

extern WifiInfo get_wifi_info(const char *iface) __attribute__((weak));
extern char *get_mac_address(const char *iface) __attribute__((weak));
extern int get_connection_info(ConnectionInfo *connections, int max_conns)
    __attribute__((weak));
extern int get_socket_owner_info(SocketOwnerInfo *owners, int max_owners)
    __attribute__((weak));

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

static void init_route_summary(TrfxRouteSummary *summary) {
    if (!summary)
        return;

    summary->has_default = 0;
    snprintf(summary->destination, sizeof(summary->destination), "N/A");
    snprintf(summary->gateway, sizeof(summary->gateway), "N/A");
    snprintf(summary->interface, sizeof(summary->interface), "N/A");
    snprintf(summary->metric, sizeof(summary->metric), "N/A");
}

int trfx_parse_route_summary_line(const char *line,
                                  TrfxRouteSummary *summary) {
    char copy[256];

    if (!line || !summary)
        return 0;

    snprintf(copy, sizeof(copy), "%s", line);

    char *token = strtok(copy, " \t\n");
    if (!token || strcmp(token, "default") != 0)
        return 0;

    init_route_summary(summary);
    summary->has_default = 1;
    snprintf(summary->destination, sizeof(summary->destination), "default");

    while ((token = strtok(NULL, " \t\n")) != NULL) {
        if (strcmp(token, "via") == 0) {
            char *value = strtok(NULL, " \t\n");
            if (value)
                snprintf(summary->gateway, sizeof(summary->gateway), "%.63s",
                         value);
        } else if (strcmp(token, "dev") == 0) {
            char *value = strtok(NULL, " \t\n");
            if (value)
                snprintf(summary->interface, sizeof(summary->interface), "%.31s",
                         value);
        } else if (strcmp(token, "metric") == 0) {
            char *value = strtok(NULL, " \t\n");
            if (value)
                snprintf(summary->metric, sizeof(summary->metric), "%.31s",
                         value);
        }
    }

    return 1;
}

TrfxCollectorStatus trfx_collect_route_summary_file(FILE *fp,
                                                    TrfxRouteSummary *summary) {
    char line[256];

    if (!fp || !summary)
        return TRFX_COLLECTOR_INVALID_ARGUMENT;

    init_route_summary(summary);

    while (fgets(line, sizeof(line), fp)) {
        if (trfx_parse_route_summary_line(line, summary))
            return TRFX_COLLECTOR_OK;
    }

    return TRFX_COLLECTOR_PARSE_FAILED;
}

TrfxCollectorStatus trfx_collect_route_summary_path(const char *path,
                                                    TrfxRouteSummary *summary,
                                                    char *error,
                                                    size_t error_size) {
    if (error && error_size > 0)
        error[0] = '\0';

    if (!path || path[0] == '\0' || !summary) {
        if (error && error_size > 0)
            snprintf(error, error_size, "invalid argument");
        return TRFX_COLLECTOR_INVALID_ARGUMENT;
    }

    FILE *fp = fopen(path, "r");
    if (!fp) {
        if (error && error_size > 0)
            snprintf(error, error_size, "open failed: %s", strerror(errno));
        return TRFX_COLLECTOR_OPEN_FAILED;
    }

    TrfxCollectorStatus status = trfx_collect_route_summary_file(fp, summary);
    fclose(fp);
    if (status == TRFX_COLLECTOR_PARSE_FAILED && error && error_size > 0)
        snprintf(error, error_size, "default route not found");

    return status;
}

static void init_dns_summary(TrfxDnsSummary *summary) {
    if (!summary)
        return;

    summary->count = 0;
    for (int i = 0; i < TRFX_MAX_DNS_SERVERS; i++)
        summary->servers[i][0] = '\0';
}

static int dns_summary_contains(const TrfxDnsSummary *summary,
                                const char *server) {
    for (int i = 0; summary && server && i < summary->count; i++) {
        if (strcmp(summary->servers[i], server) == 0)
            return 1;
    }
    return 0;
}

TrfxCollectorStatus trfx_collect_dns_summary_file(FILE *fp,
                                                  TrfxDnsSummary *summary) {
    char line[256];

    if (!fp || !summary)
        return TRFX_COLLECTOR_INVALID_ARGUMENT;

    init_dns_summary(summary);

    while (fgets(line, sizeof(line), fp)) {
        char server[64];
        if (sscanf(line, " nameserver %63s", server) != 1)
            continue;

        if (dns_summary_contains(summary, server))
            continue;

        if (summary->count < TRFX_MAX_DNS_SERVERS) {
            snprintf(summary->servers[summary->count],
                     sizeof(summary->servers[summary->count]), "%.63s",
                     server);
            summary->count++;
        }
    }

    return TRFX_COLLECTOR_OK;
}

TrfxCollectorStatus trfx_collect_dns_summary_path(const char *path,
                                                  TrfxDnsSummary *summary,
                                                  char *error,
                                                  size_t error_size) {
    if (error && error_size > 0)
        error[0] = '\0';

    if (!path || path[0] == '\0' || !summary) {
        if (error && error_size > 0)
            snprintf(error, error_size, "invalid argument");
        return TRFX_COLLECTOR_INVALID_ARGUMENT;
    }

    FILE *fp = fopen(path, "r");
    if (!fp) {
        if (error && error_size > 0)
            snprintf(error, error_size, "open failed: %s", strerror(errno));
        return TRFX_COLLECTOR_OPEN_FAILED;
    }

    TrfxCollectorStatus status = trfx_collect_dns_summary_file(fp, summary);
    fclose(fp);
    return status;
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

TrfxInterfaceStatsResult trfx_collect_interface_stats_path(const char *path) {
    TrfxInterfaceStatsResult result = {0};
    FILE *fp;

    result.status = TRFX_COLLECTOR_OK;

    if (!path || path[0] == '\0') {
        result.status = TRFX_COLLECTOR_INVALID_ARGUMENT;
        snprintf(result.error, sizeof(result.error), "invalid path");
        return result;
    }

    fp = fopen(path, "r");
    if (!fp) {
        result.status = TRFX_COLLECTOR_OPEN_FAILED;
        snprintf(result.error, sizeof(result.error), "open failed: %s",
                 strerror(errno));
        return result;
    }

    result.count = trfx_parse_interface_stats_file(fp, result.stats,
                                                   TRFX_MAX_INTERFACES);
    fclose(fp);

    if (result.count < 0) {
        result.status = TRFX_COLLECTOR_PARSE_FAILED;
        result.count = 0;
        snprintf(result.error, sizeof(result.error),
                 "failed to parse interface stats");
    }

    return result;
}

int trfx_read_interface_stats(TrfxInterfaceStat stats[], int max_stats) {
    TrfxInterfaceStatsResult result =
        trfx_collect_interface_stats_path("/proc/net/dev");

    if (result.status != TRFX_COLLECTOR_OK)
        return -1;

    int count = result.count < max_stats ? result.count : max_stats;
    memcpy(stats, result.stats, sizeof(TrfxInterfaceStat) * count);
    return count;
}

static TrfxInterfaceStat* find_prev_stat(const char *name) {
    for (int i = 0; i < prev_count; i++) {
        if (strcmp(prev_stats[i].name, name) == 0)
            return &prev_stats[i];
    }
    return NULL;
}

static const TrfxInterfaceStat *find_stat(const TrfxInterfaceStat stats[],
                                          int count, const char *name) {
    for (int i = 0; stats && i < count; i++) {
        if (strcmp(stats[i].name, name) == 0)
            return &stats[i];
    }
    return NULL;
}

int trfx_calculate_interface_rates(const TrfxInterfaceStat previous[],
                                   int previous_count,
                                   const TrfxInterfaceStat current[],
                                   int current_count, double elapsed_seconds,
                                   TrfxInterfaceRate rates[], int max_rates) {
    int count = 0;

    if (!current || !rates || current_count < 0 || max_rates <= 0 ||
        elapsed_seconds <= 0.0) {
        return -1;
    }

    for (int i = 0; i < current_count && count < max_rates; i++) {
        const TrfxInterfaceStat *prev =
            find_stat(previous, previous_count, current[i].name);
        unsigned long rx_delta = 0;
        unsigned long tx_delta = 0;

        if (prev) {
            if (current[i].rx_bytes >= prev->rx_bytes)
                rx_delta = current[i].rx_bytes - prev->rx_bytes;
            if (current[i].tx_bytes >= prev->tx_bytes)
                tx_delta = current[i].tx_bytes - prev->tx_bytes;
        }

        snprintf(rates[count].name, sizeof(rates[count].name), "%.31s",
                 current[i].name);
        rates[count].rx_bytes_per_sec = (double)rx_delta / elapsed_seconds;
        rates[count].tx_bytes_per_sec = (double)tx_delta / elapsed_seconds;
        count++;
    }

    return count;
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

    snprintf(buf, bufsize, " %-15.15s | %8s/s | %8s/s", name, formatted_sent,
             formatted_recv);
}

// Interface bandwidth in Trafix means byte rates from interface counters.
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

        TrfxInterfaceRate rate = {0};

        if (initialized) {
            TrfxInterfaceStat *prev = find_prev_stat(curr_stats[i].name);
            trfx_calculate_interface_rates(prev, prev ? 1 : 0, &curr_stats[i],
                                           1, 1.0, &rate, 1);
        } else {
            snprintf(rate.name, sizeof(rate.name), "%.31s",
                     curr_stats[i].name);
        }

        trfx_format_interface_usage_line(curr_stats[i].name,
                                         rate.tx_bytes_per_sec,
                                         rate.rx_bytes_per_sec, data[i], 128);
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

static void copy_text(char *dest, size_t dest_size, const char *src,
                      const char *fallback) {
    if (!dest || dest_size == 0)
        return;

    if (src && src[0] != '\0') {
        snprintf(dest, dest_size, "%s", src);
    } else {
        snprintf(dest, dest_size, "%s", fallback ? fallback : "N/A");
    }
}

static int should_skip_snapshot_interface(const char *name) {
    if (!name || name[0] == '\0')
        return 1;

    return strcmp(name, "lo") == 0 || strncmp(name, "br-", 3) == 0 ||
           strncmp(name, "docker", 6) == 0 || strncmp(name, "veth", 4) == 0 ||
           strncmp(name, "virbr", 5) == 0 || strncmp(name, "vmnet", 5) == 0;
}

static int connection_is_listener(const ConnectionInfo *connection) {
    if (!connection)
        return 0;

    return strcmp(connection->state, "LISTEN") == 0 ||
           strcmp(connection->state, "UNCONN") == 0;
}

static void set_text(char *dest, size_t dest_size, const char *value,
                     const char *fallback) {
    if (!dest || dest_size == 0)
        return;

    if (value && value[0] != '\0') {
        snprintf(dest, dest_size, "%s", value);
    } else {
        snprintf(dest, dest_size, "%s", fallback ? fallback : "N/A");
    }
}

static void trim_line_end(char *text) {
    size_t len;

    if (!text)
        return;

    len = strlen(text);
    while (len > 0 && (text[len - 1] == '\n' || text[len - 1] == '\r')) {
        text[len - 1] = '\0';
        len--;
    }
}

static void read_interface_operstate(const char *name, char *operstate,
                                     size_t operstate_size) {
    char path[128];
    FILE *fp;

    if (!operstate || operstate_size == 0)
        return;

    snprintf(operstate, operstate_size, "unknown");

    if (!trfx_is_valid_interface_name(name))
        return;

    snprintf(path, sizeof(path), "/sys/class/net/%s/operstate", name);
    fp = fopen(path, "r");
    if (!fp)
        return;

    if (fgets(operstate, (int)operstate_size, fp) == NULL) {
        snprintf(operstate, operstate_size, "unknown");
    } else {
        trim_line_end(operstate);
        if (operstate[0] == '\0')
            snprintf(operstate, operstate_size, "unknown");
    }

    fclose(fp);
}

static void read_interface_carrier(const char *name, char *carrier,
                                   size_t carrier_size) {
    char path[128];
    FILE *fp;
    char buf[16];

    if (!carrier || carrier_size == 0)
        return;

    snprintf(carrier, carrier_size, "unknown");

    if (!trfx_is_valid_interface_name(name))
        return;

    snprintf(path, sizeof(path), "/sys/class/net/%s/carrier", name);
    fp = fopen(path, "r");
    if (!fp)
        return;

    if (fgets(buf, sizeof(buf), fp) != NULL) {
        trim_line_end(buf);
        if (strcmp(buf, "1") == 0) {
            snprintf(carrier, carrier_size, "up");
        } else if (strcmp(buf, "0") == 0) {
            snprintf(carrier, carrier_size, "down");
        }
    }

    fclose(fp);
}

static void collect_interface_addresses(const char *name, char *ipv4,
                                        size_t ipv4_size, char *ipv6,
                                        size_t ipv6_size) {
    struct ifaddrs *ifaddr, *ifa;

    if (!ipv4 || ipv4_size == 0 || !ipv6 || ipv6_size == 0)
        return;

    snprintf(ipv4, ipv4_size, "N/A");
    snprintf(ipv6, ipv6_size, "N/A");

    if (!trfx_is_valid_interface_name(name))
        return;

    if (getifaddrs(&ifaddr) == -1)
        return;

    for (ifa = ifaddr; ifa; ifa = ifa->ifa_next) {
        if (!ifa->ifa_addr || strcmp(ifa->ifa_name, name) != 0)
            continue;

        if (ifa->ifa_addr->sa_family == AF_INET && strcmp(ipv4, "N/A") == 0) {
            char addr[INET_ADDRSTRLEN];
            struct sockaddr_in *sa = (struct sockaddr_in *)ifa->ifa_addr;
            if (inet_ntop(AF_INET, &sa->sin_addr, addr, sizeof(addr)))
                snprintf(ipv4, ipv4_size, "%s", addr);
        } else if (ifa->ifa_addr->sa_family == AF_INET6 &&
                   strcmp(ipv6, "N/A") == 0) {
            char addr[INET6_ADDRSTRLEN];
            struct sockaddr_in6 *sa6 = (struct sockaddr_in6 *)ifa->ifa_addr;
            if (inet_ntop(AF_INET6, &sa6->sin6_addr, addr, sizeof(addr)))
                snprintf(ipv6, ipv6_size, "%s", addr);
        }
    }

    freeifaddrs(ifaddr);
}

static void init_interface_status(TrfxInterfaceStatus *status) {
    if (!status)
        return;

    memset(status, 0, sizeof(*status));
    set_text(status->operstate, sizeof(status->operstate), "unknown", "unknown");
    set_text(status->carrier, sizeof(status->carrier), "unknown", "unknown");
    set_text(status->type, sizeof(status->type), "N/A", "N/A");
    set_text(status->ipv4, sizeof(status->ipv4), "N/A", "N/A");
    set_text(status->ipv6, sizeof(status->ipv6), "N/A", "N/A");
    set_text(status->ssid, sizeof(status->ssid), "N/A", "N/A");
    set_text(status->mac, sizeof(status->mac), "N/A", "N/A");
}

static void fill_interface_status(const TrfxInterfaceStat *stat,
                                  TrfxInterfaceStatus *status) {
    WifiInfo wifi = {"N/A", "N/A", "N/A", "N/A"};

    if (!stat || !status)
        return;

    init_interface_status(status);
    snprintf(status->name, sizeof(status->name), "%.31s", stat->name);
    status->rx_bytes = stat->rx_bytes;
    status->tx_bytes = stat->tx_bytes;

    read_interface_operstate(stat->name, status->operstate,
                             sizeof(status->operstate));
    read_interface_carrier(stat->name, status->carrier,
                           sizeof(status->carrier));
    collect_interface_addresses(stat->name, status->ipv4, sizeof(status->ipv4),
                                status->ipv6, sizeof(status->ipv6));

    status->has_ipv4 = strcmp(status->ipv4, "N/A") != 0;
    status->has_ipv6 = strcmp(status->ipv6, "N/A") != 0;
    status->is_wifi = is_wifi_interface(stat->name);
    status->is_vpn = is_vpn_interface(stat->name);
    status->is_up = (strcmp(status->operstate, "up") == 0 ||
                     strcmp(status->carrier, "up") == 0);

    if (status->is_vpn) {
        set_text(status->type, sizeof(status->type), "VPN", "VPN");
    } else if (strcmp(stat->name, "lo") == 0) {
        set_text(status->type, sizeof(status->type), "Loopback", "Loopback");
    } else if (status->is_wifi) {
        set_text(status->type, sizeof(status->type), "Wi-Fi", "Wi-Fi");
        if (get_wifi_info) {
            wifi = get_wifi_info(stat->name);
            set_text(status->ssid, sizeof(status->ssid), wifi.ssid, "N/A");
        }
    } else {
        set_text(status->type, sizeof(status->type), "Ethernet", "Ethernet");
    }

    if (get_mac_address)
        set_text(status->mac, sizeof(status->mac), get_mac_address(stat->name),
                 "N/A");
}

void trfx_init_interface_statuses(TrfxInterfaceStatusResult *status) {
    if (!status)
        return;

    memset(status, 0, sizeof(*status));
    status->status = TRFX_COLLECTOR_PARSE_FAILED;
    status->error[0] = '\0';
}

TrfxCollectorStatus trfx_collect_interface_statuses(
    const TrfxInterfaceStatsResult *interfaces, TrfxInterfaceStatusResult *status,
    char *error, size_t error_size) {
    int i;

    if (!interfaces || !status)
        return TRFX_COLLECTOR_INVALID_ARGUMENT;

    if (error && error_size > 0)
        error[0] = '\0';

    trfx_init_interface_statuses(status);
    status->status = interfaces->status;

    if (interfaces->status == TRFX_COLLECTOR_INVALID_ARGUMENT) {
        snprintf(status->error, sizeof(status->error), "%s",
                 interfaces->error[0] ? interfaces->error
                                       : "invalid interface snapshot");
        if (error && error_size > 0)
            snprintf(error, error_size, "%s", status->error);
        return status->status;
    }

    for (i = 0; i < interfaces->count && i < TRFX_MAX_INTERFACES; i++) {
        fill_interface_status(&interfaces->stats[i], &status->items[i]);
        status->count++;
    }

    return status->status;
}

void trfx_format_route_consistency_summary(const TrfxNetworkSnapshot *snapshot,
                                           char *summary,
                                           size_t summary_size) {
    int has_route;
    int has_active;
    int route_matches_active;

    if (!snapshot || !summary || summary_size == 0)
        return;

    has_route = snapshot->route_status == TRFX_COLLECTOR_OK &&
                snapshot->route.has_default &&
                snapshot->route.interface[0] != '\0' &&
                strcmp(snapshot->route.interface, "N/A") != 0;
    has_active = snapshot->has_active_interface &&
                 snapshot->active_interface[0] != '\0';
    route_matches_active =
        has_route && has_active &&
        strcmp(snapshot->route.interface, snapshot->active_interface) == 0;

    if (!has_route && !has_active) {
        snprintf(summary, summary_size, "Route check: unavailable");
    } else if (route_matches_active) {
        snprintf(summary, summary_size,
                 "Route check: route %s | active %s | IP %s | ok",
                 snapshot->route.interface, snapshot->active_interface,
                 has_active && snapshot->active_ip[0] ? snapshot->active_ip
                                                      : "N/A");
    } else if (has_route && has_active) {
        snprintf(summary, summary_size,
                 "Route check: route %s | active %s | IP %s | mismatch",
                 snapshot->route.interface, snapshot->active_interface,
                 snapshot->active_ip[0] ? snapshot->active_ip : "N/A");
    } else if (has_route) {
        snprintf(summary, summary_size,
                 "Route check: route %s | active unavailable | IP %s | partial",
                 snapshot->route.interface,
                 snapshot->active_ip[0] ? snapshot->active_ip : "N/A");
    } else {
        snprintf(summary, summary_size,
                 "Route check: active %s | IP %s | partial",
                 snapshot->active_interface,
                 snapshot->active_ip[0] ? snapshot->active_ip : "N/A");
    }
}

void trfx_init_network_snapshot(TrfxNetworkSnapshot *snapshot) {
    if (!snapshot)
        return;

    memset(snapshot, 0, sizeof(*snapshot));

    snapshot->interfaces.status = TRFX_COLLECTOR_PARSE_FAILED;
    trfx_init_interface_statuses(&snapshot->interface_statuses);
    snapshot->route_status = TRFX_COLLECTOR_PARSE_FAILED;
    snapshot->dns_status = TRFX_COLLECTOR_PARSE_FAILED;

    snapshot->route.has_default = 0;
    snprintf(snapshot->route.destination, sizeof(snapshot->route.destination),
             "N/A");
    snprintf(snapshot->route.gateway, sizeof(snapshot->route.gateway), "N/A");
    snprintf(snapshot->route.interface, sizeof(snapshot->route.interface),
             "N/A");
    snprintf(snapshot->route.metric, sizeof(snapshot->route.metric), "N/A");

    snapshot->active_interface[0] = '\0';
    snapshot->active_ip[0] = '\0';
    copy_text(snapshot->active_type, sizeof(snapshot->active_type), NULL,
              "N/A");
    snapshot->active_ssid[0] = '\0';
    snapshot->active_mac[0] = '\0';

    snapshot->vpn_interface[0] = '\0';
    snapshot->vpn_ip[0] = '\0';
}

TrfxCollectorStatus trfx_collect_network_snapshot(TrfxNetworkSnapshot *snapshot,
                                                  char *error,
                                                  size_t error_size) {
    TrfxCollectorStatus final_status = TRFX_COLLECTOR_OK;
    TrfxNetworkSnapshot local_snapshot;

    if (!snapshot)
        return TRFX_COLLECTOR_INVALID_ARGUMENT;

    if (error && error_size > 0)
        error[0] = '\0';

    trfx_init_network_snapshot(&local_snapshot);

    local_snapshot.interfaces = trfx_collect_interface_stats_path(
        "/proc/net/dev");
    if (local_snapshot.interfaces.status != TRFX_COLLECTOR_OK &&
        final_status == TRFX_COLLECTOR_OK) {
        final_status = local_snapshot.interfaces.status;
    }
    trfx_collect_interface_statuses(&local_snapshot.interfaces,
                                    &local_snapshot.interface_statuses, error,
                                    error_size);

    FILE *route_fp = popen("ip route 2>/dev/null", "r");
    if (route_fp) {
        local_snapshot.route_status =
            trfx_collect_route_summary_file(route_fp, &local_snapshot.route);
        pclose(route_fp);
    } else {
        local_snapshot.route_status = TRFX_COLLECTOR_OPEN_FAILED;
    }
    if (local_snapshot.route_status != TRFX_COLLECTOR_OK &&
        final_status == TRFX_COLLECTOR_OK) {
        final_status = local_snapshot.route_status;
    }

    local_snapshot.dns_status = trfx_collect_dns_summary_path(
        "/etc/resolv.conf", &local_snapshot.dns, error, error_size);
    if (local_snapshot.dns_status != TRFX_COLLECTOR_OK &&
        final_status == TRFX_COLLECTOR_OK) {
        final_status = local_snapshot.dns_status;
    }

    for (int i = 0; i < local_snapshot.interfaces.count; i++) {
        const char *name = local_snapshot.interfaces.stats[i].name;
        char *ip = NULL;

        if (should_skip_snapshot_interface(name))
            continue;

        ip = get_ip_address(name);
        if (ip) {
            local_snapshot.has_active_interface = 1;
            snprintf(local_snapshot.active_interface,
                     sizeof(local_snapshot.active_interface), "%.31s", name);
            snprintf(local_snapshot.active_ip, sizeof(local_snapshot.active_ip),
                     "%.63s", ip);
            snprintf(local_snapshot.active_type, sizeof(local_snapshot.active_type),
                     "%s", is_wifi_interface(name) ? "Wi-Fi" : "Ethernet");

            if (is_wifi_interface(name) && get_wifi_info) {
                WifiInfo wifi = get_wifi_info(name);
                copy_text(local_snapshot.active_ssid,
                          sizeof(local_snapshot.active_ssid), wifi.ssid, "N/A");
            } else {
                copy_text(local_snapshot.active_ssid,
                          sizeof(local_snapshot.active_ssid), NULL, "N/A");
            }

            if (get_mac_address) {
                copy_text(local_snapshot.active_mac,
                          sizeof(local_snapshot.active_mac),
                          get_mac_address(name), "N/A");
            } else {
                copy_text(local_snapshot.active_mac,
                          sizeof(local_snapshot.active_mac), NULL, "N/A");
            }
            break;
        }
    }

    for (int i = 0; i < local_snapshot.interfaces.count; i++) {
        const char *name = local_snapshot.interfaces.stats[i].name;
        char *vpn_ip = NULL;

        if (!is_vpn_interface(name))
            continue;

        vpn_ip = get_ip_address(name);
        if (vpn_ip) {
            local_snapshot.has_vpn_interface = 1;
            snprintf(local_snapshot.vpn_interface,
                     sizeof(local_snapshot.vpn_interface), "%.31s", name);
            snprintf(local_snapshot.vpn_ip, sizeof(local_snapshot.vpn_ip), "%.63s",
                     vpn_ip);
            break;
        }
    }

    if (get_connection_info) {
        local_snapshot.connection_count =
            get_connection_info(local_snapshot.connections, MAX_CONNECTIONS);
        if (local_snapshot.connection_count < 0)
            local_snapshot.connection_count = 0;
    } else {
        local_snapshot.connection_count = 0;
    }

    local_snapshot.listener_count = 0;
    for (int i = 0; i < local_snapshot.connection_count &&
                    local_snapshot.listener_count < MAX_CONNECTIONS;
         i++) {
        if (!connection_is_listener(&local_snapshot.connections[i]))
            continue;

        local_snapshot.listeners[local_snapshot.listener_count++] =
            local_snapshot.connections[i];
    }

    if (get_socket_owner_info) {
        local_snapshot.socket_owner_count = get_socket_owner_info(
            local_snapshot.socket_owners, MAX_SOCKET_OWNERS);
        if (local_snapshot.socket_owner_count < 0)
            local_snapshot.socket_owner_count = 0;
    } else {
        local_snapshot.socket_owner_count = 0;
    }

    *snapshot = local_snapshot;
    return final_status;
}

void trfx_init_network_sample_buffer(TrfxNetworkSampleBuffer *buffer) {
    if (!buffer)
        return;

    memset(buffer, 0, sizeof(*buffer));
}

void trfx_network_sample_buffer_push(TrfxNetworkSampleBuffer *buffer,
                                     const TrfxNetworkSnapshot *snapshot,
                                     time_t captured_at) {
    size_t slot;

    if (!buffer || !snapshot)
        return;

    slot = (buffer->head + buffer->count) % TRFX_NETWORK_SAMPLE_HISTORY;
    buffer->samples[slot].captured_at = captured_at;
    buffer->samples[slot].snapshot = *snapshot;

    if (buffer->count < TRFX_NETWORK_SAMPLE_HISTORY) {
        buffer->count++;
    } else {
        buffer->head = (buffer->head + 1) % TRFX_NETWORK_SAMPLE_HISTORY;
    }
}

size_t trfx_network_sample_buffer_count(const TrfxNetworkSampleBuffer *buffer) {
    if (!buffer)
        return 0;

    return buffer->count;
}

const TrfxNetworkSample *trfx_network_sample_buffer_at(
    const TrfxNetworkSampleBuffer *buffer, size_t index) {
    size_t slot;

    if (!buffer || index >= buffer->count)
        return NULL;

    slot = (buffer->head + index) % TRFX_NETWORK_SAMPLE_HISTORY;
    return &buffer->samples[slot];
}
