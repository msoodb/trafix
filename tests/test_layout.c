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

static int test_two_column_layout_null_safety(void) {
  ASSERT_INT_EQ(trfx_two_column_layout_secondary_visible(NULL), 0);
  ASSERT_INT_EQ(trfx_two_column_layout_primary_width_percent(NULL), 70);
  ASSERT_INT_EQ(trfx_two_column_layout_secondary_width_percent(NULL), 30);

  trfx_two_column_layout_set_secondary_visible(NULL, 1);
  trfx_two_column_layout_toggle_secondary(NULL);

  return 0;
}

int main(void) {
  if (test_two_column_layout_defaults() != 0)
    return 1;
  if (test_two_column_layout_toggle() != 0)
    return 1;
  if (test_two_column_layout_null_safety() != 0)
    return 1;
  return 0;
}
