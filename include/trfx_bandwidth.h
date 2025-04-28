/*
 * Copyright (C) 2025 Masoud Bolhassani <masoud.bolhassani@gmail.com>
 *
 * This file is part of Trafix.
 *
 * Trafix is released under the GNU General Public License v3 (GPL-3.0).
 * See LICENSE file for details.
 */

#ifndef TRFX_BANDWIDTH_H
#define TRFX_BANDWIDTH_H

#define MAX_BANDWIDTH_CONNECTIONS 256

typedef struct {
    char pid[16];
    char process[64];
    char laddr[64];
    char lport[8];
    char raddr[64];
    char rport[8];
    char proto[8]; // "TCP" or "UDP"
    unsigned long sent_kb;
    unsigned long recv_kb;
} BandwidthInfo;

int get_bandwidth_info(BandwidthInfo *bandwidths, int max_conns);

#endif // TRFX_BANDWIDTH_H
