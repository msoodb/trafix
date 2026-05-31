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
