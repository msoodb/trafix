/*
 * Copyright (C) 2025 Masoud Bolhassani <masoud.bolhassani@gmail.com>
 *
 * This file is part of Trafix.
 *
 * Trafix is released under the GNU General Public License v3 (GPL-3.0).
 * See LICENSE file for details.
 */

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
