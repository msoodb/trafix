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

char *get_gateway_ip();
char *get_dns_servers();
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
int trfx_read_interface_stats(TrfxInterfaceStat stats[], int max_stats);
void trfx_format_net_bytes(double bytes, char *buf, size_t bufsize);
void trfx_format_interface_usage_line(const char *name, double tx_bytes,
                                      double rx_bytes, char *buf,
                                      size_t bufsize);
char** get_interfaces_usage(int *num_interfaces);
void free_interfaces_usage(char **data, int num_interfaces);

#endif
