/*
 * Copyright (C) 2025 Masoud Bolhassani <masoud.bolhassani@gmail.com>
 *
 * This file is part of Trafix.
 *
 * Trafix is released under the GNU General Public License v3 (GPL-3.0).
 * See LICENSE file for details.
 */

#include "test_common.h"
#include "trfx_utils.h"

#include <ncurses.h>
#include <stdlib.h>

typedef struct {
  SCREEN *screen;
  FILE *input;
  FILE *output;
} CursesFixture;

static int curses_fixture_start(CursesFixture *fixture) {
  if (!fixture)
    return 0;

  fixture->screen = NULL;
  fixture->input = NULL;
  fixture->output = NULL;

  if (setenv("TERM", "xterm", 1) != 0)
    return 0;

  fixture->input = tmpfile();
  fixture->output = tmpfile();
  if (!fixture->input || !fixture->output)
    return 0;

  fixture->screen = newterm(NULL, fixture->output, fixture->input);
  if (!fixture->screen)
    return 0;

  set_term(fixture->screen);
  noecho();
  cbreak();
  keypad(stdscr, TRUE);
  return 1;
}

static void curses_fixture_stop(CursesFixture *fixture) {
  if (!fixture)
    return;

  endwin();
  if (fixture->screen)
    delscreen(fixture->screen);
  if (fixture->input)
    fclose(fixture->input);
  if (fixture->output)
    fclose(fixture->output);
}

static chtype window_cell(WINDOW *win, int y, int x) {
  return mvwinch(win, y, x);
}

static int test_box_header_and_content_order(void) {
  CursesFixture fixture;
  WINDOW *win;

  if (!curses_fixture_start(&fixture))
    return 1;

  win = newwin(8, 32, 0, 0);
  if (!win) {
    curses_fixture_stop(&fixture);
    return 1;
  }

  trfx_draw_box(win, 0);
  trfx_print_clipped(win, 1, 2, "Header");
  trfx_print_clipped(win, 2, 2, "Content line");

  ASSERT_INT_EQ((window_cell(win, 0, 0) & A_CHARTEXT),
                (ACS_ULCORNER & A_CHARTEXT));
  ASSERT_INT_EQ((window_cell(win, 0, 31) & A_CHARTEXT),
                (ACS_URCORNER & A_CHARTEXT));
  ASSERT_INT_EQ((window_cell(win, 7, 0) & A_CHARTEXT),
                (ACS_LLCORNER & A_CHARTEXT));
  ASSERT_INT_EQ((window_cell(win, 7, 31) & A_CHARTEXT),
                (ACS_LRCORNER & A_CHARTEXT));
  ASSERT_INT_EQ((window_cell(win, 1, 2) & A_CHARTEXT), 'H');
  ASSERT_INT_EQ((window_cell(win, 2, 2) & A_CHARTEXT), 'C');
  ASSERT_INT_EQ((window_cell(win, 2, 31) & A_CHARTEXT),
                (ACS_VLINE & A_CHARTEXT));

  delwin(win);
  curses_fixture_stop(&fixture);
  return 0;
}

static int test_clipped_content_preserves_border(void) {
  CursesFixture fixture;
  WINDOW *win;
  const char *message = "This line is intentionally much longer than the pane width";

  if (!curses_fixture_start(&fixture))
    return 1;

  win = newwin(6, 24, 0, 0);
  if (!win) {
    curses_fixture_stop(&fixture);
    return 1;
  }

  trfx_draw_box(win, 0);
  const chtype right_border = window_cell(win, 2, 23);
  trfx_print_clipped(win, 2, 2, message);

  ASSERT_INT_EQ((window_cell(win, 2, 2) & A_CHARTEXT), 'T');
  ASSERT_INT_EQ(window_cell(win, 2, 23), right_border);

  delwin(win);
  curses_fixture_stop(&fixture);
  return 0;
}

static int test_empty_state_centering(void) {
  CursesFixture fixture;
  WINDOW *win;
  char row[64];

  if (!curses_fixture_start(&fixture))
    return 1;

  win = newwin(7, 30, 0, 0);
  if (!win) {
    curses_fixture_stop(&fixture);
    return 1;
  }

  trfx_draw_box(win, 0);
  trfx_print_empty_state(win, "No visible data");

  ASSERT_INT_EQ(mvwinnstr(win, 3, 1, row, (int)sizeof(row) - 1) != ERR, 1);
  ASSERT_INT_EQ(strstr(row, "No visible data") != NULL, 1);
  ASSERT_INT_EQ((window_cell(win, 0, 0) & A_CHARTEXT),
                (ACS_ULCORNER & A_CHARTEXT));

  delwin(win);
  curses_fixture_stop(&fixture);
  return 0;
}

static int test_frame_first_redraw_consistency(void) {
  CursesFixture fixture;
  WINDOW *win;
  const char *long_line =
      "This update arrives after the frame is ready and should stay clipped";

  if (!curses_fixture_start(&fixture))
    return 1;

  win = newwin(9, 36, 0, 0);
  if (!win) {
    curses_fixture_stop(&fixture);
    return 1;
  }

  trfx_draw_box(win, 0);
  trfx_print_clipped(win, 1, 2, "Support Panel");
  trfx_print_empty_state(win, "Waiting for data");
  trfx_print_clipped(win, 5, 2, long_line);

  ASSERT_INT_EQ((window_cell(win, 0, 0) & A_CHARTEXT),
                (ACS_ULCORNER & A_CHARTEXT));
  ASSERT_INT_EQ((window_cell(win, 0, 35) & A_CHARTEXT),
                (ACS_URCORNER & A_CHARTEXT));
  ASSERT_INT_EQ((window_cell(win, 8, 0) & A_CHARTEXT),
                (ACS_LLCORNER & A_CHARTEXT));
  ASSERT_INT_EQ((window_cell(win, 8, 35) & A_CHARTEXT),
                (ACS_LRCORNER & A_CHARTEXT));
  ASSERT_INT_EQ((window_cell(win, 1, 2) & A_CHARTEXT), 'S');
  ASSERT_INT_EQ((window_cell(win, 5, 2) & A_CHARTEXT), 'T');
  ASSERT_INT_EQ((window_cell(win, 5, 35) & A_CHARTEXT),
                (ACS_VLINE & A_CHARTEXT));

  delwin(win);
  curses_fixture_stop(&fixture);
  return 0;
}

int main(void) {
  if (test_box_header_and_content_order() != 0)
    return 1;
  if (test_clipped_content_preserves_border() != 0)
    return 1;
  if (test_empty_state_centering() != 0)
    return 1;
  if (test_frame_first_redraw_consistency() != 0)
    return 1;
  return 0;
}
