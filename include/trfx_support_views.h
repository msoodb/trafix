/*
 * Copyright (C) 2025 Masoud Bolhassani <masoud.bolhassani@gmail.com>
 *
 * This file is part of Trafix.
 *
 * Trafix is released under the GNU General Public License v3 (GPL-3.0).
 * See LICENSE file for details.
 */

#ifndef TRFX_SUPPORT_VIEWS_H
#define TRFX_SUPPORT_VIEWS_H

#include <stddef.h>

typedef enum {
  TRFX_SUPPORT_VIEW_OVERVIEW = 0,
  TRFX_SUPPORT_VIEW_LOGS,
  TRFX_SUPPORT_VIEW_DIAGNOSTICS,
  TRFX_SUPPORT_VIEW_ROUTE_DNS,
  TRFX_SUPPORT_VIEW_NETWORK_HEALTH,
  TRFX_SUPPORT_VIEW_BANDWIDTH,
  TRFX_SUPPORT_VIEW_CONNECTION_DETAIL,
  TRFX_SUPPORT_VIEW_ACTION_AUDIT,
  TRFX_SUPPORT_VIEW_COUNT
} TrfxSupportViewId;

typedef struct {
  TrfxSupportViewId id;
  const char *title;
  const char *description;
} TrfxSupportViewSpec;

size_t trfx_support_view_count(void);
size_t trfx_support_view_default_index(void);
const TrfxSupportViewSpec *trfx_support_view_spec_at(size_t index);
const TrfxSupportViewSpec *trfx_support_view_default(void);
const TrfxSupportViewSpec *trfx_support_view_by_id(TrfxSupportViewId id);
TrfxSupportViewId trfx_support_view_id_at(size_t index);
size_t trfx_support_view_index_for_id(TrfxSupportViewId id);
size_t trfx_support_view_next_index(size_t current_index, int delta);

#endif // TRFX_SUPPORT_VIEWS_H
