/*
 * Copyright (C) 2025 Masoud Bolhassani <masoud.bolhassani@gmail.com>
 *
 * This file is part of Trafix.
 *
 * Trafix is released under the GNU General Public License v3 (GPL-3.0).
 * See LICENSE file for details.
 */

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <ctype.h>
#include "trfx_wifi.h"

int is_valid_iface(const char *iface) {
    if (!iface || strlen(iface) > 15) return 0; // max iface name length
    for (size_t i = 0; iface[i]; ++i) {
        if (!isalnum(iface[i]) && iface[i] != '_')
            return 0;
    }
    return 1;
}

WifiInfo get_wifi_info(const char *iface) {
    WifiInfo info = {"N/A", "N/A", "N/A", "N/A"};
    if (!is_valid_iface(iface)) return info;

    char cmd[160];
    snprintf(cmd, sizeof(cmd), "iw dev %s link 2>/dev/null", iface);

    FILE *fp = popen(cmd, "r");
    if (!fp) return info;

    char line[256];
    while (fgets(line, sizeof(line), fp)) {
        if (strstr(line, "SSID:")) {
            sscanf(line, "SSID: %63[^\n]", info.ssid);
        } else if (strstr(line, "signal:")) {
            sscanf(line, "signal: %31[^\n]", info.signal_strength);
        } else if (strstr(line, "tx bitrate:")) {
            sscanf(line, "tx bitrate: %31[^\n]", info.bitrate);
        } else if (strstr(line, "freq:")) {
            sscanf(line, "freq: %31[^\n]", info.freq);
        }
    }

    pclose(fp);
    return info;
}

char* get_mac_address(const char *iface) {
    static char mac[32] = "N/A";

    if (!is_valid_iface(iface)) return mac;

    char cmd[160];
    snprintf(cmd, sizeof(cmd), "ip link show %s 2>/dev/null", iface);

    FILE *fp = popen(cmd, "r");
    if (!fp) return mac;

    char line[256];
    while (fgets(line, sizeof(line), fp)) {
        char *ptr = strstr(line, "link/ether");
        if (ptr) {
            // sscanf reads after "link/ether ", so skip it manually
            char temp[32] = {0};
            if (sscanf(ptr + 11, "%31s", temp) == 1) {
                strncpy(mac, temp, sizeof(mac) - 1);
                mac[sizeof(mac) - 1] = '\0'; // ensure null-termination
            }
            break;
        }
    }

    pclose(fp);
    return mac;
}
