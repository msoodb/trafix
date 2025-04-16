#include <stdio.h>
#include <string.h>
#include "trfx_wifi.h"

WifiInfo get_wifi_info(const char *iface) {
    WifiInfo info = {"N/A", "N/A", "N/A", "N/A"};
    char cmd[128];
    snprintf(cmd, sizeof(cmd), "iw dev %s link", iface);

    FILE *fp = popen(cmd, "r");
    if (!fp) return info;

    char line[256];
    while (fgets(line, sizeof(line), fp)) {
        char *p;
        if ((p = strstr(line, "SSID:")))
            sscanf(p + 5, " %63[^\n]", info.ssid);
        else if ((p = strstr(line, "signal:")))
            sscanf(p + 7, " %31[^\n]", info.signal_strength);
        else if ((p = strstr(line, "tx bitrate:")))
            sscanf(p + 11, " %31[^\n]", info.bitrate);
        else if ((p = strstr(line, "freq:")))
            sscanf(p + 5, " %31[^\n]", info.freq);
    }

    pclose(fp);
    return info;
}

char* get_mac_address(const char *iface) {
    static char mac[32] = "N/A";
    char cmd[128];
    snprintf(cmd, sizeof(cmd), "ip link show %s", iface);

    FILE *fp = popen(cmd, "r");
    if (!fp) return mac;

    char line[256];
    while (fgets(line, sizeof(line), fp)) {
        char *ptr = strstr(line, "link/ether");
        if (ptr) {
            sscanf(ptr, "link/ether %31s", mac);
            break;
        }
    }

    pclose(fp);
    return mac;
}
