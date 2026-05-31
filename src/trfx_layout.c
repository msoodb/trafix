/*
 * Copyright (C) 2025 Masoud Bolhassani <masoud.bolhassani@gmail.com>
 *
 * This file is part of Trafix.
 *
 * Trafix is released under the GNU General Public License v3 (GPL-3.0).
 * See LICENSE file for details.
 */

#include "trfx_layout.h"

static int clamp_percent(int value, int fallback) {
  if (value <= 0 || value >= 100)
    return fallback;
  return value;
}

void trfx_two_column_layout_init(TrfxTwoColumnLayoutState *state) {
  if (!state)
    return;

  state->secondary_visible = 1;
  state->primary_width_percent = 70;
  state->secondary_width_percent = 30;
}

int trfx_two_column_layout_secondary_visible(
    const TrfxTwoColumnLayoutState *state) {
  return state ? state->secondary_visible : 0;
}

void trfx_two_column_layout_set_secondary_visible(
    TrfxTwoColumnLayoutState *state, int visible) {
  if (!state)
    return;

  state->secondary_visible = visible ? 1 : 0;
}

void trfx_two_column_layout_toggle_secondary(
    TrfxTwoColumnLayoutState *state) {
  if (!state)
    return;

  state->secondary_visible = !state->secondary_visible;
}

int trfx_two_column_layout_primary_width_percent(
    const TrfxTwoColumnLayoutState *state) {
  if (!state)
    return 70;

  return clamp_percent(state->primary_width_percent, 70);
}

int trfx_two_column_layout_secondary_width_percent(
    const TrfxTwoColumnLayoutState *state) {
  if (!state)
    return 30;

  return clamp_percent(state->secondary_width_percent, 30);
}

int trfx_two_column_layout_compute_geometry(
    const TrfxTwoColumnLayoutState *state, int origin_y, int origin_x,
    int total_height, int total_width, TrfxTwoColumnLayoutGeometry *geometry) {
  int primary_width;
  int secondary_width;
  int primary_percent;
  int secondary_percent;

  if (!geometry || total_height < 0 || total_width < 0)
    return 0;

  geometry->primary_x = origin_x;
  geometry->primary_y = origin_y;
  geometry->primary_height = total_height;
  geometry->secondary_x = origin_x;
  geometry->secondary_y = origin_y;
  geometry->secondary_height = total_height;
  geometry->secondary_visible =
      trfx_two_column_layout_secondary_visible(state);

  if (!geometry->secondary_visible || total_width <= 0) {
    geometry->primary_width = total_width;
    geometry->secondary_width = 0;
    geometry->secondary_x = origin_x + total_width;
    return 1;
  }

  primary_percent = trfx_two_column_layout_primary_width_percent(state);
  secondary_percent = trfx_two_column_layout_secondary_width_percent(state);
  if (primary_percent + secondary_percent <= 0) {
    primary_percent = 70;
    secondary_percent = 30;
  }

  primary_width = (total_width * primary_percent) / 100;
  if (primary_width < 1)
    primary_width = 1;
  if (primary_width >= total_width && total_width > 1)
    primary_width = total_width - 1;

  secondary_width = total_width - primary_width;
  if (secondary_width < 0)
    secondary_width = 0;

  geometry->primary_width = primary_width;
  geometry->secondary_width = secondary_width;
  geometry->secondary_x = origin_x + primary_width;
  return 1;
}
