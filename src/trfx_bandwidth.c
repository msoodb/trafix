/*
 * Copyright (C) 2025 Masoud Bolhassani <masoud.bolhassani@gmail.com>
 *
 * This file is part of Trafix.
 *
 * Trafix is released under the GNU General Public License v3 (GPL-3.0).
 * See LICENSE file for details.
 */

#include "trfx_bandwidth.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

static int connection_has_owner(const ConnectionInfo *connection) {
  if (!connection)
    return 0;

  return strcmp(connection->pid, "-") != 0 ||
         strcmp(connection->process, "-") != 0;
}

static double sum_interface_rate(const TrfxInterfaceRate *rates, int count,
                                 int use_rx) {
  double total = 0.0;

  for (int i = 0; i < count; i++) {
    total += use_rx ? rates[i].rx_bytes_per_sec : rates[i].tx_bytes_per_sec;
  }

  return total;
}

static void copy_rate_interfaces(TrfxBandwidthReport *report,
                                 const TrfxInterfaceRate *rates, int count) {
  if (!report)
    return;

  report->interface_count = count;
  for (int i = 0; i < count && i < TRFX_MAX_INTERFACES; i++)
    report->interface_rates[i] = rates[i];
}

static double flow_total_bytes_per_sec(const TrfxBandwidthFlow *flow) {
  if (!flow)
    return 0.0;

  return flow->rx_bytes_per_sec + flow->tx_bytes_per_sec;
}

static int compare_bandwidth_flows(const void *left, const void *right) {
  const TrfxBandwidthFlow *a = (const TrfxBandwidthFlow *)left;
  const TrfxBandwidthFlow *b = (const TrfxBandwidthFlow *)right;
  double total_a = flow_total_bytes_per_sec(a);
  double total_b = flow_total_bytes_per_sec(b);

  if (total_a < total_b)
    return 1;
  if (total_a > total_b)
    return -1;
  return strcmp(a->label, b->label);
}

static void sort_bandwidth_flows(TrfxBandwidthReport *report) {
  if (!report || report->flow_count <= 1)
    return;

  qsort(report->flows, (size_t)report->flow_count,
        sizeof(report->flows[0]), compare_bandwidth_flows);
}

static void format_connection_label(const ConnectionInfo *connection, char *buf,
                                    size_t buf_size) {
  if (!buf || buf_size == 0)
    return;

  if (!connection) {
    snprintf(buf, buf_size, "unknown");
    return;
  }

  snprintf(buf, buf_size, "%s %s -> %s", connection->protocol,
           connection->local_addr, connection->remote_addr);
}

typedef struct {
  char pid[16];
  char process[64];
  int socket_count;
} ProcessGroup;

static int find_process_group(ProcessGroup groups[], int group_count,
                              const SocketOwnerInfo *owner) {
  if (!groups || !owner)
    return -1;

  for (int i = 0; i < group_count; i++) {
    if (strcmp(groups[i].pid, owner->pid) == 0 &&
        strcmp(groups[i].process, owner->process) == 0) {
      return i;
    }
  }

  return -1;
}

void trfx_init_bandwidth_report(TrfxBandwidthReport *report) {
  if (!report)
    return;

  memset(report, 0, sizeof(*report));
  report->mode = TRFX_BW_MODE_UNSUPPORTED;
  snprintf(report->source, sizeof(report->source), "insufficient samples");
}

const char *trfx_bandwidth_mode_name(TrfxBandwidthMode mode) {
  switch (mode) {
  case TRFX_BW_MODE_INTERFACE_FALLBACK:
    return "interface fallback";
  case TRFX_BW_MODE_PROCESS_ESTIMATED:
    return "process estimated";
  case TRFX_BW_MODE_SOCKET_ESTIMATED:
    return "socket estimated";
  case TRFX_BW_MODE_UNSUPPORTED:
  default:
    return "unsupported";
  }
}

TrfxCollectorStatus trfx_collect_bandwidth_report(
    const TrfxNetworkSampleBuffer *buffer, TrfxBandwidthReport *report,
    char *error, size_t error_size) {
  const TrfxNetworkSample *previous;
  const TrfxNetworkSample *current;
  TrfxInterfaceRate rates[TRFX_MAX_INTERFACES];
  int rate_count;
  double elapsed_seconds;
  double total_rx;
  double total_tx;

  if (error && error_size > 0)
    error[0] = '\0';

  if (!buffer || !report) {
    if (error && error_size > 0)
      snprintf(error, error_size, "invalid argument");
    return TRFX_COLLECTOR_INVALID_ARGUMENT;
  }

  trfx_init_bandwidth_report(report);
  if (trfx_network_sample_buffer_count(buffer) < 2) {
    snprintf(report->source, sizeof(report->source),
             "need at least two samples for bandwidth estimates");
    return TRFX_COLLECTOR_OK;
  }

  previous =
      trfx_network_sample_buffer_at(buffer, trfx_network_sample_buffer_count(buffer) - 2);
  current = trfx_network_sample_buffer_at(buffer,
                                          trfx_network_sample_buffer_count(buffer) - 1);
  if (!previous || !current) {
    snprintf(report->source, sizeof(report->source),
             "sample history unavailable");
    return TRFX_COLLECTOR_OK;
  }

  elapsed_seconds = difftime(current->captured_at, previous->captured_at);
  if (elapsed_seconds <= 0.0)
    elapsed_seconds = 1.0;

  rate_count = trfx_calculate_interface_rates(
      previous->snapshot.interfaces.stats, previous->snapshot.interfaces.count,
      current->snapshot.interfaces.stats, current->snapshot.interfaces.count,
      elapsed_seconds, rates, TRFX_MAX_INTERFACES);
  if (rate_count < 0)
    rate_count = 0;

  copy_rate_interfaces(report, rates, rate_count);
  total_rx = sum_interface_rate(rates, rate_count, 1);
  total_tx = sum_interface_rate(rates, rate_count, 0);

  if (current->snapshot.connection_count > 0) {
    int owned_count = 0;
    for (int i = 0; i < current->snapshot.connection_count; i++) {
      if (connection_has_owner(&current->snapshot.connections[i]))
        owned_count++;
    }

    if (owned_count > 0) {
      double rx_share = total_rx / (double)owned_count;
      double tx_share = total_tx / (double)owned_count;

      report->mode = TRFX_BW_MODE_SOCKET_ESTIMATED;
      snprintf(report->source, sizeof(report->source),
               "estimated from interface deltas and socket ownership");

      for (int i = 0, out = 0; i < current->snapshot.connection_count &&
                                  out < TRFX_MAX_BANDWIDTH_FLOWS;
           i++) {
        const ConnectionInfo *connection = &current->snapshot.connections[i];
        if (!connection_has_owner(connection))
          continue;

        TrfxBandwidthFlow *flow = &report->flows[out++];
        snprintf(flow->pid, sizeof(flow->pid), "%s", connection->pid);
        snprintf(flow->process, sizeof(flow->process), "%s",
                 connection->process);
        snprintf(flow->local, sizeof(flow->local), "%s", connection->local_addr);
        snprintf(flow->remote, sizeof(flow->remote), "%s",
                 connection->remote_addr);
        snprintf(flow->proto, sizeof(flow->proto), "%s", connection->protocol);
        format_connection_label(connection, flow->label, sizeof(flow->label));
        snprintf(flow->detail, sizeof(flow->detail), "socket %d/%d", out,
                 owned_count);
        flow->rx_bytes_per_sec = rx_share;
        flow->tx_bytes_per_sec = tx_share;
        report->flow_count = out;
      }
      sort_bandwidth_flows(report);
      return TRFX_COLLECTOR_OK;
    }
  }

  if (current->snapshot.socket_owner_count > 0) {
    ProcessGroup groups[TRFX_MAX_BANDWIDTH_FLOWS];
    int group_count = 0;
    int total_sockets = 0;

    for (int i = 0; i < current->snapshot.socket_owner_count; i++) {
      const SocketOwnerInfo *owner = &current->snapshot.socket_owners[i];
      int group_index = find_process_group(groups, group_count, owner);

      if (group_index == -1 && group_count < TRFX_MAX_BANDWIDTH_FLOWS) {
        group_index = group_count++;
        snprintf(groups[group_index].pid, sizeof(groups[group_index].pid), "%s",
                 owner->pid);
        snprintf(groups[group_index].process,
                 sizeof(groups[group_index].process), "%s", owner->process);
        groups[group_index].socket_count = 0;
      }

      if (group_index != -1) {
        groups[group_index].socket_count++;
        total_sockets++;
      }
    }

    if (group_count > 0 && total_sockets > 0) {
      report->mode = TRFX_BW_MODE_PROCESS_ESTIMATED;
      snprintf(report->source, sizeof(report->source),
               "estimated from interface deltas and socket ownership");

      for (int i = 0; i < group_count && i < TRFX_MAX_BANDWIDTH_FLOWS; i++) {
        TrfxBandwidthFlow *flow = &report->flows[i];
        double share = (double)groups[i].socket_count / (double)total_sockets;

        snprintf(flow->pid, sizeof(flow->pid), "%.15s", groups[i].pid);
        snprintf(flow->process, sizeof(flow->process), "%.63s",
                 groups[i].process);
        snprintf(flow->label, sizeof(flow->label), "%.63s", groups[i].process);
        snprintf(flow->detail, sizeof(flow->detail), "%d sockets",
                 groups[i].socket_count);
        flow->rx_bytes_per_sec = total_rx * share;
        flow->tx_bytes_per_sec = total_tx * share;
        report->flow_count = i + 1;
      }
      sort_bandwidth_flows(report);
      return TRFX_COLLECTOR_OK;
    }
  }

  report->mode = TRFX_BW_MODE_INTERFACE_FALLBACK;
  snprintf(report->source, sizeof(report->source),
           "interface-level fallback only");
  return TRFX_COLLECTOR_OK;
}
