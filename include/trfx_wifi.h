#ifndef TRFX_WIFI_H
#define TRFX_WIFI_H

typedef struct {
    char ssid[64];
    char signal_strength[32];
    char bitrate[32];
    char freq[32];
} WifiInfo;


WifiInfo get_wifi_info(const char *iface);
char* get_mac_address(const char *iface);

#endif
