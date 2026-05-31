#include "test_common.h"
#include "trfx_layout.h"

static int test_two_column_layout_defaults(void) {
  TrfxTwoColumnLayoutState state;

  trfx_two_column_layout_init(&state);

  ASSERT_INT_EQ(trfx_two_column_layout_secondary_visible(&state), 1);
  ASSERT_INT_EQ(trfx_two_column_layout_primary_width_percent(&state), 70);
  ASSERT_INT_EQ(trfx_two_column_layout_secondary_width_percent(&state), 30);

  return 0;
}

static int test_two_column_layout_toggle(void) {
  TrfxTwoColumnLayoutState state;

  trfx_two_column_layout_init(&state);
  trfx_two_column_layout_set_secondary_visible(&state, 0);
  ASSERT_INT_EQ(trfx_two_column_layout_secondary_visible(&state), 0);

  trfx_two_column_layout_toggle_secondary(&state);
  ASSERT_INT_EQ(trfx_two_column_layout_secondary_visible(&state), 1);

  trfx_two_column_layout_toggle_secondary(&state);
  ASSERT_INT_EQ(trfx_two_column_layout_secondary_visible(&state), 0);

  return 0;
}

static int test_two_column_layout_geometry(void) {
  TrfxTwoColumnLayoutState state;
  TrfxTwoColumnLayoutGeometry geometry;

  trfx_two_column_layout_init(&state);

  ASSERT_INT_EQ(
      trfx_two_column_layout_compute_geometry(&state, 2, 4, 40, 100, &geometry),
      1);
  ASSERT_INT_EQ(geometry.primary_x, 4);
  ASSERT_INT_EQ(geometry.primary_y, 2);
  ASSERT_INT_EQ(geometry.primary_width, 70);
  ASSERT_INT_EQ(geometry.primary_height, 40);
  ASSERT_INT_EQ(geometry.secondary_x, 74);
  ASSERT_INT_EQ(geometry.secondary_y, 2);
  ASSERT_INT_EQ(geometry.secondary_width, 30);
  ASSERT_INT_EQ(geometry.secondary_height, 40);
  ASSERT_INT_EQ(geometry.secondary_visible, 1);

  trfx_two_column_layout_set_secondary_visible(&state, 0);
  ASSERT_INT_EQ(
      trfx_two_column_layout_compute_geometry(&state, 0, 0, 20, 50, &geometry),
      1);
  ASSERT_INT_EQ(geometry.primary_width, 50);
  ASSERT_INT_EQ(geometry.secondary_width, 0);
  ASSERT_INT_EQ(geometry.secondary_visible, 0);

  return 0;
}

static int test_two_column_layout_null_safety(void) {
  ASSERT_INT_EQ(trfx_two_column_layout_secondary_visible(NULL), 0);
  ASSERT_INT_EQ(trfx_two_column_layout_primary_width_percent(NULL), 70);
  ASSERT_INT_EQ(trfx_two_column_layout_secondary_width_percent(NULL), 30);
  ASSERT_INT_EQ(trfx_two_column_layout_secondary_state_index(NULL), 0);

  trfx_two_column_layout_set_secondary_visible(NULL, 1);
  trfx_two_column_layout_toggle_secondary(NULL);
  trfx_two_column_layout_set_secondary_state_index(NULL, 2);

  return 0;
}

static int test_two_column_layout_state_restore(void) {
  TrfxTwoColumnLayoutState state;

  trfx_two_column_layout_init(&state);
  trfx_two_column_layout_set_secondary_state_index(&state, 2);
  trfx_two_column_layout_set_secondary_visible(&state, 0);
  ASSERT_INT_EQ(trfx_two_column_layout_secondary_state_index(&state), 2);
  trfx_two_column_layout_set_secondary_visible(&state, 1);
  ASSERT_INT_EQ(trfx_two_column_layout_secondary_state_index(&state), 2);

  return 0;
}

static int test_two_column_layout_narrow_fallback(void) {
  TrfxTwoColumnLayoutState state;
  TrfxTwoColumnLayoutGeometry geometry;

  trfx_two_column_layout_init(&state);
  ASSERT_INT_EQ(
      trfx_two_column_layout_compute_geometry(&state, 0, 0, 20, 80, &geometry),
      1);
  ASSERT_INT_EQ(geometry.secondary_visible, 0);
  ASSERT_INT_EQ(geometry.primary_width, 80);
  ASSERT_INT_EQ(geometry.secondary_width, 0);

  return 0;
}

int main(void) {
  if (test_two_column_layout_defaults() != 0)
    return 1;
  if (test_two_column_layout_toggle() != 0)
    return 1;
  if (test_two_column_layout_geometry() != 0)
    return 1;
  if (test_two_column_layout_state_restore() != 0)
    return 1;
  if (test_two_column_layout_narrow_fallback() != 0)
    return 1;
  if (test_two_column_layout_null_safety() != 0)
    return 1;
  return 0;
}
