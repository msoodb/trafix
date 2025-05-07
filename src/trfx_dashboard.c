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

#include "trfx_globals.h"
#include "trfx_procinfo.h"
#include "trfx_threads.h"

#define TOTAL_ROWS 3
#define ROW1_MODULES 4
#define MAX_ROW2_MODULES 3
#define ROW3_MODULES 1

#define FIXED_ROW1_HEIGHT 11
#define FIXED_ROW3_HEIGHT 4

#define KEY_ESC 27

extern int ROW2_MODULES;

typedef struct {
  const char *name;
  void *(*thread_func)(void *);
} Module;

const int dynamic_module_indexes[] = {
    DYNAMIC_MODULE_CONNINFO, DYNAMIC_MODULE_NETINFO, DYNAMIC_MODULE_PROCINFO,
    DYNAMIC_MODULE_BANDWIDTH};

Module modules[] = {
    {"Connections", connection_info_thread},
    {"Network", network_info_thread},
    {"Processes", process_info_thread},
    {"Bandwidth", bandwidth_info_thread},
    {NULL, NULL} // Sentinel
};

typedef struct {
  pthread_t thread_id;
  int module_index; // -1 = none
  WINDOW *window;
} WindowSlot;
// WindowSlot row2_slots[ROW2_MODULES];
WindowSlot *row2_slots = NULL;

/*
  Functions dashboard
*/
WINDOW *create_bordered_window(int height, int width, int y, int x,
                               int color_pair) {
  WINDOW *win = newwin(height, width, y, x);
  if (win) {
    pthread_mutex_lock(&ncurses_mutex);
    wattron(win, COLOR_PAIR(color_pair));
    box(win, 0, 0);
    wattroff(win, COLOR_PAIR(color_pair));
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

int select_module() {
  screen_paused = 1;

  const char *module_names[] = {" Connections ", " Network Information ",
                                " Processes ", " Bandwidth "};
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
    screen_paused = 0;
    return -1;
  }

  keypad(popup, TRUE);
  int selected_index = 0;
  int ch;
  while (1) {
    pthread_mutex_lock(&ncurses_mutex);
    werase(popup);
    wattron(popup, COLOR_PAIR(COLOR_BORDER));
    box(popup, 0, 0);
    wattroff(popup, COLOR_PAIR(COLOR_BORDER));
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
  screen_paused = 0;
  return selected_index;
}

void pause_screen() {
  screen_paused = 1;

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
    screen_paused = 0;
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

  screen_paused = 0;
}

void change_window_module(int slot_idx) {
  if (row2_slots[slot_idx].module_index != -1) {
    pthread_cancel(row2_slots[slot_idx].thread_id);
    pthread_join(row2_slots[slot_idx].thread_id, NULL);
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
    x_offset += row2_widths[i];
  }
}

void cleanup_row2_modules() {
  if (row2_slots == NULL) return;

  for (int i = 0; i < ROW2_MODULES; i++) {
    if (row2_slots[i].thread_id) {
      pthread_cancel(row2_slots[i].thread_id);
      pthread_join(row2_slots[i].thread_id, NULL);
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

void load_row2_modules(int row2_height, int screen_width, int row2_y) {
  int *row2_widths = malloc(ROW2_MODULES * sizeof(int));
  if (!row2_widths) {
    endwin();
    fprintf(stderr, "Failed to allocate memory for row2_widths\n");
    exit(EXIT_FAILURE);
  }

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
    if (pthread_create(&row2_slots[i].thread_id, NULL, modules[i].thread_func,
                       arg) != 0) {
      free(arg);
    }
  }

  free(row2_widths);
}

void handle_keypress(int ch, WINDOW *sys_win, WINDOW *cpu_win, WINDOW *mem_win,
                     WINDOW *disk_win, int row2_height, int screen_width,
                     int row2_y) {
  switch (ch) {
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

    load_row2_modules(row2_height, screen_width, row2_y);
    break;

  case 'r':
  case 'R':
    for (int i = 0; i < STATIC_MODULE_COUNT; i++)
      force_refresh_flags[i] = 1;
    refresh_static_windows(sys_win, cpu_win, mem_win, disk_win);
    break;

  case 'p':
  case 'P':
    pause_screen();
    break;

  default:
    // Do nothing or handle other keys if needed
    break;
 }
}

void start_dashboard() {
  initscr();
  noecho();
  curs_set(FALSE);
  mousemask(0, NULL);
  start_color();
  use_default_colors();

  init_pair(COLOR_HEADER, COLOR_CYAN, -1);
  init_pair(COLOR_DATA_GREEN, COLOR_GREEN, -1);
  init_pair(COLOR_DATA_RED, COLOR_RED, -1);
  init_pair(COLOR_DATA_YELLOW, COLOR_YELLOW, -1);
  init_pair(COLOR_BORDER, COLOR_CYAN, -1);

  int screen_height, screen_width;
  getmaxyx(stdscr, screen_height, screen_width);

  row2_slots = calloc(ROW2_MODULES, sizeof(WindowSlot));
  if (!row2_slots) {
    endwin();
    fprintf(stderr, "Failed to allocate memory for row2_slots\n");
    exit(EXIT_FAILURE);
  }

  const int row1_height = FIXED_ROW1_HEIGHT;
  const int row3_height = FIXED_ROW3_HEIGHT;
  const int row2_height = screen_height - row1_height - row3_height;

  int row1_widths[ROW1_MODULES] = {
      (int)(screen_width * 0.25), (int)(screen_width * 0.20),
      (int)(screen_width * 0.25),
      screen_width - (int)(screen_width * 0.25) - (int)(screen_width * 0.20) -
          (int)(screen_width * 0.25)};
  /*int row2_widths[ROW2_MODULES] = {screen_width / 3, screen_width / 3,
    screen_width - 2 * (screen_width / 3)};*/
  int *row2_widths = malloc(ROW2_MODULES * sizeof(int));
  if (!row2_widths) {
    endwin();
    fprintf(stderr, "Failed to allocate memory for row2_widths\n");
    exit(EXIT_FAILURE);
  }
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

  int row3_widths[ROW3_MODULES] = {screen_width};

  int row1_y = 0, row2_y = row1_height, row3_y = row1_height + row2_height;

  WINDOW *sys_win = newwin(row1_height, row1_widths[0], row1_y, 0);
  WINDOW *cpu_win = newwin(row1_height, row1_widths[1], row1_y, row1_widths[0]);
  WINDOW *mem_win = newwin(row1_height, row1_widths[2], row1_y,
                           row1_widths[0] + row1_widths[1]);
  WINDOW *disk_win = newwin(row1_height, row1_widths[3], row1_y,
                            row1_widths[0] + row1_widths[1] + row1_widths[2]);
  WINDOW *help_win = newwin(row3_height, row3_widths[0], row3_y, 0);

  pthread_t sys_tid, cpu_tid, mem_tid, disk_tid, help_tid;
  pthread_create(&sys_tid, NULL, system_info_thread, sys_win);
  pthread_create(&cpu_tid, NULL, cpu_info_thread, cpu_win);
  pthread_create(&mem_tid, NULL, memory_info_thread, mem_win);
  pthread_create(&disk_tid, NULL, disk_info_thread, disk_win);

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
  pthread_create(&help_tid, NULL, help_info_thread, help_win);

  sleep(1);
  ready = 1;

  int ch;
  while ((ch = getch()) != 'q' && ch != 'Q') {
    handle_keypress(ch, sys_win, cpu_win, mem_win, disk_win, row2_height, screen_width, row2_y);
  }

  free(row2_widths);
  endwin();
}
