#include "test_common.h"
#include "trfx_support_views.h"

static int test_support_view_registry(void) {
  ASSERT_INT_EQ((int)trfx_support_view_count(), (int)TRFX_SUPPORT_VIEW_COUNT);
  ASSERT_INT_EQ((int)trfx_support_view_default_index(), 0);

  const TrfxSupportViewSpec *default_view = trfx_support_view_default();
  ASSERT_INT_EQ(default_view != NULL, 1);
  ASSERT_INT_EQ(default_view->id, TRFX_SUPPORT_VIEW_OVERVIEW);
  ASSERT_STR_EQ(default_view->title, "Overview");
  ASSERT_INT_EQ((int)strlen(default_view->description) > 0, 1);

  for (size_t i = 0; i < trfx_support_view_count(); i++) {
    const TrfxSupportViewSpec *spec = trfx_support_view_spec_at(i);
    ASSERT_INT_EQ(spec != NULL, 1);
    ASSERT_INT_EQ(spec->id, trfx_support_view_id_at(i));
    ASSERT_INT_EQ((int)strlen(spec->title) > 0, 1);
    ASSERT_INT_EQ((int)strlen(spec->description) > 0, 1);
  }

  ASSERT_INT_EQ(trfx_support_view_spec_at(trfx_support_view_count()) == NULL, 1);
  ASSERT_INT_EQ(trfx_support_view_by_id(TRFX_SUPPORT_VIEW_ACTION_AUDIT) != NULL,
                1);
  ASSERT_INT_EQ((int)trfx_support_view_index_for_id(TRFX_SUPPORT_VIEW_NETWORK_HEALTH),
                4);
  ASSERT_INT_EQ((int)trfx_support_view_index_for_id(TRFX_SUPPORT_VIEW_COUNT), 0);

  return 0;
}

static int test_support_view_cycle(void) {
  ASSERT_INT_EQ((int)trfx_support_view_next_index(0, 1), 1);
  ASSERT_INT_EQ((int)trfx_support_view_next_index(0, -1), 7);
  ASSERT_INT_EQ((int)trfx_support_view_next_index(7, 1), 0);
  ASSERT_INT_EQ((int)trfx_support_view_next_index(99, 1), 1);
  return 0;
}

static int test_support_view_selection_state(void) {
  trfx_support_view_set_selected_index(3);
  ASSERT_INT_EQ((int)trfx_support_view_selected_index(), 3);
  ASSERT_INT_EQ(trfx_support_view_selected() != NULL, 1);
  ASSERT_STR_EQ(trfx_support_view_selected()->title, "Route and DNS");

  ASSERT_INT_EQ((int)trfx_support_view_cycle_selected_index(1), 4);
  ASSERT_INT_EQ((int)trfx_support_view_cycle_selected_index(-5), 7);
  ASSERT_INT_EQ((int)trfx_support_view_cycle_selected_index(1), 0);
  trfx_support_view_request_refresh();
  ASSERT_INT_EQ(trfx_support_view_consume_refresh_request(), 1);
  ASSERT_INT_EQ(trfx_support_view_consume_refresh_request(), 0);
  return 0;
}

static int test_support_view_selector_formatting(void) {
  char line[256];
  const TrfxSupportViewSpec *spec = trfx_support_view_default();

  trfx_support_view_format_selector_line(spec, 0, line, sizeof(line));
  ASSERT_STR_EQ(line, "Overview - Route, DNS, alerts, and live support context.");

  trfx_support_view_format_selector_line(spec, 1, line, sizeof(line));
  ASSERT_STR_EQ(line, "Overview");

  trfx_support_view_format_selector_line(NULL, 0, line, sizeof(line));
  ASSERT_STR_EQ(line, "Unknown view");

  return 0;
}

static int test_support_view_null_safety(void) {
  ASSERT_INT_EQ(trfx_support_view_spec_at((size_t)-1) == NULL, 1);
  ASSERT_INT_EQ(trfx_support_view_by_id((TrfxSupportViewId)999) == NULL, 1);
  trfx_support_view_format_selector_line(NULL, 1, NULL, 0);
  return 0;
}

int main(void) {
  if (test_support_view_registry() != 0)
    return 1;
  if (test_support_view_cycle() != 0)
    return 1;
  if (test_support_view_selection_state() != 0)
    return 1;
  if (test_support_view_selector_formatting() != 0)
    return 1;
  if (test_support_view_null_safety() != 0)
    return 1;
  return 0;
}
