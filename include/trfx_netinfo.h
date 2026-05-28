/*
 * Copyright (C) 2025 Masoud Bolhassani <masoud.bolhassani@gmail.com>
 *
 * This file is part of Trafix.
 *
 * Trafix is released under the GNU General Public License v3 (GPL-3.0).
 * See LICENSE file for details.
 */

#ifndef TRFX_NETINFO_H
#define TRFX_NETINFO_H

#include <stdio.h>

#define TRFX_MAX_INTERFACES 20

typedef struct {
    char name[32];
    unsigned long rx_bytes;
    unsigned long tx_bytes;
} TrfxInterfaceStat;

typedef struct {
    char name[32];
    double rx_bytes_per_sec;
    double tx_bytes_per_sec;
} TrfxInterfaceRate;

typedef enum {
    TRFX_COLLECTOR_OK = 0,
    TRFX_COLLECTOR_INVALID_ARGUMENT,
    TRFX_COLLECTOR_OPEN_FAILED,
    TRFX_COLLECTOR_PARSE_FAILED
} TrfxCollectorStatus;

typedef struct {
    TrfxCollectorStatus status;
    int count;
    char error[128];
    TrfxInterfaceStat stats[TRFX_MAX_INTERFACES];
} TrfxInterfaceStatsResult;

typedef struct {
    int has_default;
    char destination[64];
    char gateway[64];
    char interface[32];
    char metric[32];
} TrfxRouteSummary;

char *get_gateway_ip();
char *get_dns_servers();
int trfx_is_valid_interface_name(const char *ifname);
int trfx_parse_default_route_line(const char *line, char *gateway,
                                  size_t gateway_size, char *metric,
                                  size_t metric_size);
int trfx_parse_route_summary_line(const char *line, TrfxRouteSummary *summary);
TrfxCollectorStatus trfx_collect_route_summary_file(FILE *fp,
                                                    TrfxRouteSummary *summary);
TrfxCollectorStatus trfx_collect_route_summary_path(const char *path,
                                                    TrfxRouteSummary *summary,
                                                    char *error,
                                                    size_t error_size);
void get_default_gateway_and_metric(char *gateway, char *metric);
void get_routing_table_summary(char *routing_table);
const char *generate_random_interface_name();
char *get_ip_address(const char *ifname);
char *get_wifi_ssid(const char *ifname);
int is_wifi_interface(const char *iface_name);
int is_vpn_interface(const char *iface_name);
int trfx_parse_interface_stats_file(FILE *fp, TrfxInterfaceStat stats[],
                                    int max_stats);
int trfx_parse_interface_stats_path(const char *path, TrfxInterfaceStat stats[],
                                    int max_stats);
TrfxInterfaceStatsResult trfx_collect_interface_stats_path(const char *path);
int trfx_read_interface_stats(TrfxInterfaceStat stats[], int max_stats);
int trfx_calculate_interface_rates(const TrfxInterfaceStat previous[],
                                   int previous_count,
                                   const TrfxInterfaceStat current[],
                                   int current_count, double elapsed_seconds,
                                   TrfxInterfaceRate rates[], int max_rates);
void trfx_format_net_bytes(double bytes, char *buf, size_t bufsize);
void trfx_format_interface_usage_line(const char *name, double tx_bytes,
                                      double rx_bytes, char *buf,
                                      size_t bufsize);
char** get_interfaces_usage(int *num_interfaces);
void free_interfaces_usage(char **data, int num_interfaces);

#endif
