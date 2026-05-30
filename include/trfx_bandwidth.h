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

#include "trfx_netinfo.h"

#define TRFX_MAX_BANDWIDTH_FLOWS 64

typedef enum {
  TRFX_BW_MODE_UNSUPPORTED = 0,
  TRFX_BW_MODE_INTERFACE_FALLBACK,
  TRFX_BW_MODE_PROCESS_ESTIMATED,
  TRFX_BW_MODE_SOCKET_ESTIMATED
} TrfxBandwidthMode;

typedef struct {
  char label[128];
  char pid[16];
  char process[64];
  char local[64];
  char remote[64];
  char proto[8];
  char detail[64];
  double rx_bytes_per_sec;
  double tx_bytes_per_sec;
} TrfxBandwidthFlow;

typedef struct {
  TrfxBandwidthMode mode;
  char source[128];
  int interface_count;
  TrfxInterfaceRate interface_rates[TRFX_MAX_INTERFACES];
  int flow_count;
  TrfxBandwidthFlow flows[TRFX_MAX_BANDWIDTH_FLOWS];
} TrfxBandwidthReport;

void trfx_init_bandwidth_report(TrfxBandwidthReport *report);
TrfxCollectorStatus trfx_collect_bandwidth_report(
    const TrfxNetworkSampleBuffer *buffer, TrfxBandwidthReport *report,
    char *error, size_t error_size);
const char *trfx_bandwidth_mode_name(TrfxBandwidthMode mode);

#endif // TRFX_BANDWIDTH_H
