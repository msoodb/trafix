/*
 * Copyright (C) 2025 Masoud Bolhassani <masoud.bolhassani@gmail.com>
 *
 * This file is part of Trafix.
 *
 * Trafix is released under the GNU General Public License v3 (GPL-3.0).
 * See LICENSE file for details.
 */

#include "trfx_support_views.h"

static const TrfxSupportViewSpec support_views[] = {
    {TRFX_SUPPORT_VIEW_OVERVIEW, "Overview",
     "Route, DNS, alerts, and live support context."},
    {TRFX_SUPPORT_VIEW_LOGS, "Logs",
     "Recent log lines and troubleshooting output."},
    {TRFX_SUPPORT_VIEW_DIAGNOSTICS, "Diagnostics",
     "Collected network, system, and alert correlation."},
    {TRFX_SUPPORT_VIEW_ROUTE_DNS, "Route and DNS",
     "Default route, DNS, and active interface consistency."},
    {TRFX_SUPPORT_VIEW_NETWORK_HEALTH, "Network Health",
     "CPU, memory, disk, and network pressure correlation."},
    {TRFX_SUPPORT_VIEW_BANDWIDTH, "Bandwidth Detail",
     "Focused top-flow bandwidth and recent trend detail."},
    {TRFX_SUPPORT_VIEW_CONNECTION_DETAIL, "Connection Detail",
     "Focused connection ownership and activity detail."},
    {TRFX_SUPPORT_VIEW_ACTION_AUDIT, "Action Audit",
     "Recent kill, drop, and review actions."},
};

size_t trfx_support_view_count(void) {
  return sizeof(support_views) / sizeof(support_views[0]);
}

size_t trfx_support_view_default_index(void) {
  return 0;
}

const TrfxSupportViewSpec *trfx_support_view_spec_at(size_t index) {
  if (index >= trfx_support_view_count())
    return NULL;

  return &support_views[index];
}

const TrfxSupportViewSpec *trfx_support_view_default(void) {
  return trfx_support_view_spec_at(trfx_support_view_default_index());
}

const TrfxSupportViewSpec *trfx_support_view_by_id(TrfxSupportViewId id) {
  for (size_t i = 0; i < trfx_support_view_count(); i++) {
    if (support_views[i].id == id)
      return &support_views[i];
  }

  return NULL;
}

TrfxSupportViewId trfx_support_view_id_at(size_t index) {
  const TrfxSupportViewSpec *spec = trfx_support_view_spec_at(index);
  return spec ? spec->id : TRFX_SUPPORT_VIEW_OVERVIEW;
}

size_t trfx_support_view_index_for_id(TrfxSupportViewId id) {
  for (size_t i = 0; i < trfx_support_view_count(); i++) {
    if (support_views[i].id == id)
      return i;
  }

  return trfx_support_view_default_index();
}

size_t trfx_support_view_next_index(size_t current_index, int delta) {
  size_t count = trfx_support_view_count();
  int next_index;

  if (count == 0)
    return 0;

  if (current_index >= count)
    current_index = trfx_support_view_default_index();

  next_index = (int)current_index + delta;
  while (next_index < 0)
    next_index += (int)count;
  while (next_index >= (int)count)
    next_index -= (int)count;

  return (size_t)next_index;
}
