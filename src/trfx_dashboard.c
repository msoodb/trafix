/*
 * Copyright (C) 2025 Masoud Bolhassani <masoud.bolhassani@gmail.com>
 *
 * This file is part of Trafix.
 *
 * Trafix is released under the GNU General Public License v3 (GPL-3.0).
 * See LICENSE file for details.
 */

#include "trfx_dashboard.h"

#include <ncurses.h>
#include <pthread.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "trfx_config.h"
#include "trfx_globals.h"
#include "trfx_runtime.h"
#include "trfx_procinfo.h"
#include "trfx_threads.h"

#define TOTAL_ROWS 3
#define ROW1_MODULES 4
#define MAX_ROW2_MODULES 3
#define ROW3_MODULES 1

#define FIXED_ROW1_HEIGHT 11
#define FIXED_ROW3_HEIGHT 4
#define MIN_ROW2_HEIGHT 3
#define MIN_TUI_WIDTH 50

#define KEY_ESC 27

extern int ROW2_MODULES;

typedef struct {
  const char *name;
  void *(*thread_func)(void *);
} Module;

const int dynamic_module_indexes[] = {
  DYNAMIC_MODULE_CONNINFO, DYNAMIC_MODULE_NETINFO, DYNAMIC_MODULE_PROCINFO, DYNAMIC_MODULE_PROC_COMPACT_INFO,
    DYNAMIC_MODULE_SOCKET_OWNERS};

Module modules[] = {
    {"Connections", connection_info_thread},
    {"Network", network_info_thread},
    {"Processes", process_info_thread},
    {"Processes Compact", process_compact_info_thread},
    {"Socket Owners", socket_owner_info_thread},
    {NULL, NULL} // Sentinel
};

typedef struct {
  pthread_t thread_id;
  int module_index; // -1 = none
  WINDOW *window;
  volatile int stop_requested;
} WindowSlot;
// WindowSlot row2_slots[ROW2_MODULES];
WindowSlot *row2_slots = NULL;

static int calculate_row2_height(int screen_height) {
  int top_height = SHOW_TOP_PANELS ? FIXED_ROW1_HEIGHT : 0;
  int row2_height = screen_height - top_height;
  return row2_height < MIN_ROW2_HEIGHT ? MIN_ROW2_HEIGHT : row2_height;
}

static int tui_size_is_too_small(int screen_height, int screen_width) {
  int min_height = (SHOW_TOP_PANELS ? FIXED_ROW1_HEIGHT : 0) + MIN_ROW2_HEIGHT;
  return screen_width < MIN_TUI_WIDTH ||
         screen_height < min_height;
}

static void calculate_row1_widths(int screen_width,
                                  int row1_widths[ROW1_MODULES]) {
  row1_widths[0] = (int)(screen_width * 0.25);
  row1_widths[1] = (int)(screen_width * 0.20);
  row1_widths[2] = (int)(screen_width * 0.25);
  row1_widths[3] = screen_width - row1_widths[0] - row1_widths[1] -
                   row1_widths[2];
}

static void calculate_row2_widths(int screen_width, int row2_widths[]) {
  if (ROW2_MODULES == 1) {
    row2_widths[0] = screen_width;
  } else if (ROW2_MODULES == 2) {
    row2_widths[0] = screen_width / 2;
    row2_widths[1] = screen_width - row2_widths[0];
  } else if (ROW2_MODULES == 3) {
    row2_widths[0] = screen_width / 3;
    row2_widths[1] = screen_width / 3;
    row2_widths[2] = screen_width - row2_widths[0] - row2_widths[1];
  }
}

static void draw_small_terminal_message(int screen_height, int screen_width) {
  pthread_mutex_lock(&ncurses_mutex);
  erase();
  box(stdscr, 0, 0);
  if (screen_height > 2 && screen_width > 4)
    mvprintw(screen_height / 2, 2, "Terminal too small. Resize or press q.");
  refresh();
  pthread_mutex_unlock(&ncurses_mutex);
}

static int init_color_pair_checked(short pair, short foreground,
                                   short background) {
  return init_pair(pair, foreground, background) == OK;
}

static void init_dashboard_colors(void) {
  trfx_colors_enabled = 0;

  if (!has_colors())
    return;

  if (start_color() == ERR)
    return;

  short background = -1;
  if (use_default_colors() == ERR)
    background = COLOR_BLACK;

  if (!init_color_pair_checked(COLOR_HEADER, COLOR_CYAN, background) ||
      !init_color_pair_checked(COLOR_DATA_GREEN, COLOR_GREEN, background) ||
      !init_color_pair_checked(COLOR_DATA_RED, COLOR_RED, background) ||
      !init_color_pair_checked(COLOR_DATA_YELLOW, COLOR_YELLOW, background) ||
      !init_color_pair_checked(COLOR_BORDER, COLOR_CYAN, background)) {
    trfx_colors_enabled = 0;
    return;
  }

  trfx_colors_enabled = 1;
}

/*
  Functions dashboard
*/
WINDOW *create_bordered_window(int height, int width, int y, int x,
                               int color_pair) {
  WINDOW *win = newwin(height, width, y, x);
  if (win) {
    pthread_mutex_lock(&ncurses_mutex);
    wattron(win, trfx_color_attr(color_pair));
    box(win, 0, 0);
    wattroff(win, trfx_color_attr(color_pair));
    wrefresh(win);
    pthread_mutex_unlock(&ncurses_mutex);
  }
  return win;
}

void draw_centered_message(WINDOW *win, const char *message) {
  int height, width;
  getmaxyx(win, height, width);
  mvwprintw(win, height / 2, (width - strlen(message)) / 2, "%s", message);
  wrefresh(win);
}

int find_module_slot_by_name(const char *target_name) {
  if (!target_name)
    return -1;
  for (int i = 0; i < ROW2_MODULES; i++) {
    int module_index = row2_slots[i].module_index;
    if (module_index != -1 && modules[module_index].name &&
        strcmp(modules[module_index].name, target_name) == 0) {
      return i;
    }
  }
  return -1;
}

void start_process_info_thread(WINDOW *win, int module_index) {
  ThreadArg *arg = malloc(sizeof(ThreadArg));
  if (!arg) {
    perror("malloc");
    return;
  }
  arg->window = win;
  arg->module_index = module_index;
  arg->stop_requested = NULL;
  pthread_t tid;
  if (pthread_create(&tid, NULL, process_info_thread, arg) != 0) {
    perror("pthread_create");
    free(arg);
  }
}

void refresh_static_windows(WINDOW *sys_win, WINDOW *cpu_win, WINDOW *mem_win,
                            WINDOW *disk_win) {
  pthread_mutex_lock(&ncurses_mutex);
  WINDOW *wins[] = {sys_win, cpu_win, mem_win, disk_win};
  for (int i = 0; i < 4; ++i) {
    werase(wins[i]);
    box(wins[i], 0, 0);
    wrefresh(wins[i]);
  }
  pthread_mutex_unlock(&ncurses_mutex);
}

void show_hotkeys_popup(void) {
  trfx_runtime_set_paused(1);

  static const char *hotkeys[] = {
      "[1-3] Switch Panel",
      "[s] Sort Processes",
      "[r] Refresh",
      "[c] Columns",
      "[p] Pause",
      "[h] Help",
      "[q] Quit",
      "Press ESC or Enter to close.",
  };
  const int hotkey_count = (int)(sizeof(hotkeys) / sizeof(hotkeys[0]));

  int screen_height, screen_width;
  getmaxyx(stdscr, screen_height, screen_width);

  const char *title = "Hotkeys";
  int popup_height = hotkey_count + 4;
  int popup_width = (int)strlen(title) + 6;
  for (int i = 0; i < hotkey_count; ++i) {
    int line_width = (int)strlen(hotkeys[i]) + 4;
    if (line_width > popup_width)
      popup_width = line_width;
  }
  if (popup_width > screen_width - 4)
    popup_width = screen_width - 4;
  if (popup_width < 28)
    popup_width = 28;

  int popup_y = (screen_height - popup_height) / 2;
  int popup_x = (screen_width - popup_width) / 2;

  WINDOW *popup = create_bordered_window(popup_height, popup_width, popup_y,
                                         popup_x, COLOR_BORDER);
  if (!popup) {
    trfx_runtime_set_paused(0);
    return;
  }

  keypad(popup, FALSE);
  nodelay(popup, TRUE);

  pthread_mutex_lock(&ncurses_mutex);
  werase(popup);
  wattron(popup, trfx_color_attr(COLOR_BORDER));
  box(popup, 0, 0);
  wattroff(popup, trfx_color_attr(COLOR_BORDER));
  mvwprintw(popup, 1, 2, "%s", title);
  for (int i = 0; i < hotkey_count; ++i)
    mvwprintw(popup, i + 2, 2, "%s", hotkeys[i]);
  wrefresh(popup);
  pthread_mutex_unlock(&ncurses_mutex);

  while (1) {
    int ch = wgetch(popup);
    if (ch == ERR) {
      usleep(10000);
      continue;
    }
    if (ch == KEY_ESC || ch == '\n' || ch == '\r' || ch == 'q' || ch == 'Q')
      break;
  }

  pthread_mutex_lock(&ncurses_mutex);
  werase(popup);
  wrefresh(popup);
  delwin(popup);
  pthread_mutex_unlock(&ncurses_mutex);

  trfx_runtime_set_paused(0);
}

int select_module() {
  trfx_runtime_set_paused(1);

  const char *module_names[] = {" Connections ", " Network Information ",
                                " Processes ", " Processes Compact ", " Socket Owners "};
  int module_count = sizeof(module_names) / sizeof(module_names[0]);

  int screen_height, screen_width;
  getmaxyx(stdscr, screen_height, screen_width);

  int popup_height = module_count + 4;
  int popup_width = 40;
  int popup_y = (screen_height - popup_height) / 2;
  int popup_x = (screen_width - popup_width) / 2;

  WINDOW *popup = create_bordered_window(popup_height, popup_width, popup_y,
                                         popup_x, COLOR_BORDER);
  if (!popup) {
    trfx_runtime_set_paused(0);
    return -1;
  }

  keypad(popup, TRUE);
  int selected_index = 0;
  int ch;
  while (1) {
    pthread_mutex_lock(&ncurses_mutex);
    werase(popup);
    wattron(popup, trfx_color_attr(COLOR_BORDER));
    box(popup, 0, 0);
    wattroff(popup, trfx_color_attr(COLOR_BORDER));
    mvwprintw(popup, 1, 2, "Select a module:");
    for (int i = 0; i < module_count; i++) {
      if (i == selected_index)
        wattron(popup, A_REVERSE);
      mvwprintw(popup, i + 2, 4, "%s", module_names[i]);
      if (i == selected_index)
        wattroff(popup, A_REVERSE);
    }
    wrefresh(popup);
    pthread_mutex_unlock(&ncurses_mutex);

    ch = wgetch(popup);
    if (ch == KEY_UP)
      selected_index = (selected_index - 1 + module_count) % module_count;
    else if (ch == KEY_DOWN)
      selected_index = (selected_index + 1) % module_count;
    else if (ch == KEY_ENTER || ch == 10)
      break;
    else if (ch == KEY_ESC) {
      selected_index = -1;
      break;
    }
  }

  pthread_mutex_lock(&ncurses_mutex);
  werase(popup);
  wrefresh(popup);
  delwin(popup);
  pthread_mutex_unlock(&ncurses_mutex);
  trfx_runtime_set_paused(0);
  return selected_index;
}

void pause_screen() {
  trfx_runtime_set_paused(1);

  int screen_height, screen_width;
  getmaxyx(stdscr, screen_height, screen_width);

  const char *message = "Paused. Press ESC to continue.";
  int popup_height = 5;
  int popup_width = strlen(message) + 4;
  int popup_y = (screen_height - popup_height) / 2;
  int popup_x = (screen_width - popup_width) / 2;

  WINDOW *popup = create_bordered_window(popup_height, popup_width, popup_y,
                                         popup_x, COLOR_BORDER);
  if (!popup) {
    trfx_runtime_set_paused(0);
    return;
  }

  draw_centered_message(popup, message);
  while (getch() != KEY_ESC)
    ;

  pthread_mutex_lock(&ncurses_mutex);
  werase(popup);
  wrefresh(popup);
  delwin(popup);
  pthread_mutex_unlock(&ncurses_mutex);

  trfx_runtime_set_paused(0);
}

void change_window_module(int slot_idx) {
  if (row2_slots[slot_idx].module_index != -1) {
    row2_slots[slot_idx].stop_requested = 1;
    pthread_join(row2_slots[slot_idx].thread_id, NULL);
    row2_slots[slot_idx].thread_id = 0;
    row2_slots[slot_idx].module_index = -1;
  }

  int selected_module = select_module();
  if (selected_module == -1)
    return;

  row2_slots[slot_idx].module_index = selected_module;

  ThreadArg *arg = malloc(sizeof(ThreadArg));
  if (!arg)
    return;
  arg->module_index = slot_idx; //selected_module;
  //arg->module_index = selected_module;
  arg->window = row2_slots[slot_idx].window;
  row2_slots[slot_idx].stop_requested = 0;
  arg->stop_requested = &row2_slots[slot_idx].stop_requested;

  if (pthread_create(&row2_slots[slot_idx].thread_id, NULL,
                     modules[selected_module].thread_func, arg) != 0) {
    free(arg);
  }
}

int get_module_index_by_name(const char *name) {
  for (int i = 0; modules[i].name != NULL; i++) {
    if (strcmp(modules[i].name, name) == 0)
      return dynamic_module_indexes[i];
  }
  return -1;
}

void create_row2_windows(int row2_height, int *row2_widths, int row2_y) {
  int x_offset = 0;
  for (int i = 0; i < ROW2_MODULES; i++) {
    WINDOW *win = newwin(row2_height, row2_widths[i], row2_y, x_offset);
    row2_slots[i].window = win;
    row2_slots[i].module_index = get_module_index_by_name(modules[i].name);
    row2_slots[i].thread_id = 0;
    row2_slots[i].stop_requested = 0;
    x_offset += row2_widths[i];
  }
}

void cleanup_row2_modules() {
  if (row2_slots == NULL) return;

  for (int i = 0; i < ROW2_MODULES; i++) {
    if (row2_slots[i].thread_id) {
      row2_slots[i].stop_requested = 1;
    }
  }

  for (int i = 0; i < ROW2_MODULES; i++) {
    if (row2_slots[i].thread_id) {
      pthread_join(row2_slots[i].thread_id, NULL);
      row2_slots[i].thread_id = 0;
    }
    if (row2_slots[i].window) {
      pthread_mutex_lock(&ncurses_mutex);
      werase(row2_slots[i].window);
      wrefresh(row2_slots[i].window);
      delwin(row2_slots[i].window);
      pthread_mutex_unlock(&ncurses_mutex);
    }
  }
  free(row2_slots);
  row2_slots = NULL;
}

static void destroy_window(WINDOW **win) {
  if (!win || !*win)
    return;

  pthread_mutex_lock(&ncurses_mutex);
  werase(*win);
  wrefresh(*win);
  delwin(*win);
  pthread_mutex_unlock(&ncurses_mutex);
  *win = NULL;
}

void load_row2_modules(int row2_height, int screen_width, int row2_y) {
  int *row2_widths = malloc(ROW2_MODULES * sizeof(int));
  if (!row2_widths) {
    endwin();
    fprintf(stderr, "Failed to allocate memory for row2_widths\n");
    exit(EXIT_FAILURE);
  }

  calculate_row2_widths(screen_width, row2_widths);

  create_row2_windows(row2_height, row2_widths, row2_y);

  for (int i = 0; i < ROW2_MODULES; i++) {
    ThreadArg *arg = malloc(sizeof(ThreadArg));
    if (!arg) {
      endwin();
      fprintf(stderr, "Failed to allocate memory for ThreadArg\n");
      exit(EXIT_FAILURE);
    }
    arg->module_index = i;
    arg->window = row2_slots[i].window;
    row2_slots[i].stop_requested = 0;
    arg->stop_requested = &row2_slots[i].stop_requested;
    if (pthread_create(&row2_slots[i].thread_id, NULL, modules[i].thread_func,
                       arg) != 0) {
      free(arg);
    }
  }

  free(row2_widths);
}

static void resize_dashboard_windows(WINDOW *sys_win, WINDOW *cpu_win,
                                     WINDOW *mem_win, WINDOW *disk_win) {
  int screen_height, screen_width;
  getmaxyx(stdscr, screen_height, screen_width);

  if (tui_size_is_too_small(screen_height, screen_width)) {
    draw_small_terminal_message(screen_height, screen_width);
    return;
  }

  int row1_widths[ROW1_MODULES];
  int *row2_widths = malloc(ROW2_MODULES * sizeof(int));
  if (!row2_widths)
    return;

  calculate_row1_widths(screen_width, row1_widths);
  calculate_row2_widths(screen_width, row2_widths);

  const int row1_height = SHOW_TOP_PANELS ? FIXED_ROW1_HEIGHT : 0;
  const int row2_height = calculate_row2_height(screen_height);
  const int row2_y = row1_height;

  pthread_mutex_lock(&ncurses_mutex);
  endwin();
  refresh();
  clear();

  if (SHOW_TOP_PANELS) {
    wresize(sys_win, row1_height, row1_widths[0]);
    mvwin(sys_win, 0, 0);
    wresize(cpu_win, row1_height, row1_widths[1]);
    mvwin(cpu_win, 0, row1_widths[0]);
    wresize(mem_win, row1_height, row1_widths[2]);
    mvwin(mem_win, 0, row1_widths[0] + row1_widths[1]);
    wresize(disk_win, row1_height, row1_widths[3]);
    mvwin(disk_win, 0, row1_widths[0] + row1_widths[1] + row1_widths[2]);
  }

  int x_offset = 0;
  for (int i = 0; i < ROW2_MODULES; i++) {
    if (row2_slots[i].window) {
      wresize(row2_slots[i].window, row2_height, row2_widths[i]);
      mvwin(row2_slots[i].window, row2_y, x_offset);
    }
    x_offset += row2_widths[i];
  }

  pthread_mutex_unlock(&ncurses_mutex);

  trfx_runtime_request_static_refresh_all();
  free(row2_widths);
}

void handle_keypress(int ch, WINDOW *sys_win, WINDOW *cpu_win, WINDOW *mem_win,
                     WINDOW *disk_win) {
  switch (ch) {
  case KEY_RESIZE:
    resize_dashboard_windows(sys_win, cpu_win, mem_win, disk_win);
    break;

  case '1':
  case '2':
  case '3': {
    int slot = ch - '1';
    if (slot < ROW2_MODULES) {
      change_window_module(slot);
    }
    break;
  }
    
  case 's':
  case 'S':
    current_sort_type = (current_sort_type + 1) % SORT_MAX;
    break;

  case 'c':
  case 'C':
    cleanup_row2_modules();

    ROW2_MODULES++;
    if (ROW2_MODULES > MAX_ROW2_MODULES) {
      ROW2_MODULES = 1;
    }

    row2_slots = calloc(ROW2_MODULES, sizeof(WindowSlot));
    if (!row2_slots) {
      endwin();
      fprintf(stderr, "Failed to allocate memory for row2_slots\n");
      exit(EXIT_FAILURE);
    }

    int current_height, current_width;
    getmaxyx(stdscr, current_height, current_width);
    if (tui_size_is_too_small(current_height, current_width)) {
      draw_small_terminal_message(current_height, current_width);
      break;
    }
    int row2_y = SHOW_TOP_PANELS ? FIXED_ROW1_HEIGHT : 0;
    load_row2_modules(calculate_row2_height(current_height), current_width,
                      row2_y);
    break;

  case 'r':
  case 'R':
    trfx_runtime_request_static_refresh_all();
    if (SHOW_TOP_PANELS)
      refresh_static_windows(sys_win, cpu_win, mem_win, disk_win);
    break;

  case 'p':
  case 'P':
    pause_screen();
    break;

  case KEY_F(1):
  case 'h':
  case 'H':
    show_hotkeys_popup();
    break;

  default:
    // Do nothing or handle other keys if needed
    break;
 }
}

void start_dashboard() {
  trfx_runtime_reset();

  initscr();
  noecho();
  keypad(stdscr, TRUE);
  curs_set(FALSE);
  mousemask(0, NULL);
  init_dashboard_colors();

  int screen_height, screen_width;
  getmaxyx(stdscr, screen_height, screen_width);

  if (tui_size_is_too_small(screen_height, screen_width))
    draw_small_terminal_message(screen_height, screen_width);

  row2_slots = calloc(ROW2_MODULES, sizeof(WindowSlot));
  if (!row2_slots) {
    endwin();
    fprintf(stderr, "Failed to allocate memory for row2_slots\n");
    exit(EXIT_FAILURE);
  }

  const int row1_height = SHOW_TOP_PANELS ? FIXED_ROW1_HEIGHT : 0;
  const int row2_height = calculate_row2_height(screen_height);

  int row1_widths[ROW1_MODULES] = {0};
  if (SHOW_TOP_PANELS)
    calculate_row1_widths(screen_width, row1_widths);
  int row1_y = 0, row2_y = row1_height;

  WINDOW *sys_win = NULL;
  WINDOW *cpu_win = NULL;
  WINDOW *mem_win = NULL;
  WINDOW *disk_win = NULL;

  if (SHOW_TOP_PANELS) {
    sys_win = newwin(row1_height, row1_widths[0], row1_y, 0);
    cpu_win = newwin(row1_height, row1_widths[1], row1_y, row1_widths[0]);
    mem_win = newwin(row1_height, row1_widths[2], row1_y,
                     row1_widths[0] + row1_widths[1]);
    disk_win = newwin(row1_height, row1_widths[3], row1_y,
                      row1_widths[0] + row1_widths[1] + row1_widths[2]);
  }

  pthread_t sys_tid, cpu_tid, mem_tid, disk_tid;
  if (SHOW_TOP_PANELS) {
    pthread_create(&sys_tid, NULL, system_info_thread, sys_win);
    pthread_create(&cpu_tid, NULL, cpu_info_thread, cpu_win);
    pthread_create(&mem_tid, NULL, memory_info_thread, mem_win);
    pthread_create(&disk_tid, NULL, disk_info_thread, disk_win);
  }

  /*create_row2_windows(row2_height, row2_widths, row2_y);
  for (int i = 0; i < ROW2_MODULES; i++) {
    ThreadArg *arg = malloc(sizeof(ThreadArg));
    if (!arg) {
      endwin();
      fprintf(stderr, "Failed to allocate memory for ThreadArg\n");
      exit(EXIT_FAILURE);
    }
    arg->module_index = i;
    //arg->module_index = row2_slots[i].module_index;
    arg->window = row2_slots[i].window;
    if (pthread_create(&row2_slots[i].thread_id, NULL, modules[i].thread_func,
                       arg) != 0) {
      free(arg);
    }
    }*/
  load_row2_modules(row2_height, screen_width, row2_y);

  sleep(1);
  trfx_runtime_set_ready(1);

  int ch;
  while ((ch = getch()) != 'q' && ch != 'Q') {
    handle_keypress(ch, sys_win, cpu_win, mem_win, disk_win);
  }

  trfx_runtime_request_stop();

  if (SHOW_TOP_PANELS) {
    pthread_join(sys_tid, NULL);
    pthread_join(cpu_tid, NULL);
    pthread_join(mem_tid, NULL);
    pthread_join(disk_tid, NULL);
  }
  cleanup_row2_modules();

  destroy_window(&sys_win);
  destroy_window(&cpu_win);
  destroy_window(&mem_win);
  destroy_window(&disk_win);

  endwin();
}
