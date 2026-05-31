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
} TrfxTwoColumnLayoutState;

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

#endif // TRFX_LAYOUT_H
