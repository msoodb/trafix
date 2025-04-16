#ifndef TRFX_BANDWIDTH_H
#define TRFX_BANDWIDTH_H

char *get_gateway_ip();
char* get_dns_servers();
const char *generate_random_interface_name();
char *get_ip_address(const char *ifname);
char *get_wifi_ssid(const char *ifname);
int is_wifi_interface(const char *iface_name);
int is_vpn_interface(const char *iface_name);
char** get_bandwidth_usage(int *num_interfaces);
void free_bandwidth_usage(char **data, int num_interfaces);

#endif
