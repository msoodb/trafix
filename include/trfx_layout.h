/*
 * Copyright (C) 2025 Masoud Bolhassani <masoud.bolhassani@gmail.com>
 *
 * This file is part of Trafix.
 *
 * Trafix is released under the GNU General Public License v3 (GPL-3.0).
 * See LICENSE file for details.
 */

#ifndef TRFX_LAYOUT_H
#define TRFX_LAYOUT_H

typedef struct {
  int secondary_visible;
  int primary_width_percent;
  int secondary_width_percent;
  int secondary_state_index;
} TrfxTwoColumnLayoutState;

typedef struct {
  int primary_x;
  int primary_y;
  int primary_width;
  int primary_height;
  int secondary_x;
  int secondary_y;
  int secondary_width;
  int secondary_height;
  int secondary_visible;
} TrfxTwoColumnLayoutGeometry;

void trfx_two_column_layout_init(TrfxTwoColumnLayoutState *state);
int trfx_two_column_layout_secondary_visible(
    const TrfxTwoColumnLayoutState *state);
void trfx_two_column_layout_set_secondary_visible(
    TrfxTwoColumnLayoutState *state, int visible);
void trfx_two_column_layout_toggle_secondary(
    TrfxTwoColumnLayoutState *state);
int trfx_two_column_layout_primary_width_percent(
    const TrfxTwoColumnLayoutState *state);
int trfx_two_column_layout_secondary_width_percent(
    const TrfxTwoColumnLayoutState *state);
int trfx_two_column_layout_secondary_state_index(
    const TrfxTwoColumnLayoutState *state);
void trfx_two_column_layout_set_secondary_state_index(
    TrfxTwoColumnLayoutState *state, int state_index);
int trfx_two_column_layout_compute_geometry(
    const TrfxTwoColumnLayoutState *state, int origin_y, int origin_x,
    int total_height, int total_width, TrfxTwoColumnLayoutGeometry *geometry);

#endif // TRFX_LAYOUT_H
