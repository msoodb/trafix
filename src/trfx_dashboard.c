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
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <time.h>

#include "trfx_config.h"
#include "trfx_actions.h"
#include "trfx_diagnostics.h"
#include "trfx_globals.h"
#include "trfx_connections.h"
#include "trfx_layout.h"
#include "trfx_runtime.h"
#include "trfx_procinfo.h"
#include "trfx_threads.h"
#include "trfx_support_views.h"
#include "trfx_utils.h"

#define TOTAL_ROWS 3
#define ROW1_MODULES 4
#define PRIMARY_PANE_SLOTS 1
#define MAX_PRIMARY_MODULE_CHOICES 3
#define ROW3_MODULES 1

#define FIXED_ROW1_HEIGHT 11
#define FIXED_ROW3_HEIGHT 4
#define MIN_ROW2_HEIGHT 3
#define MIN_TUI_WIDTH 50

#define KEY_ESC 27

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
// WindowSlot row2_slots[PRIMARY_PANE_SLOTS];
WindowSlot *row2_slots = NULL;
static WINDOW *support_window = NULL;
static pthread_t support_thread_id;
static volatile int support_stop_requested = 0;
static int support_thread_active = 0;
static TrfxTwoColumnLayoutState dashboard_layout_state;

static int calculate_row2_height(int screen_height) {
  int top_height = SHOW_TOP_PANELS ? FIXED_ROW1_HEIGHT : 0;
  int row2_height = screen_height - top_height;
  return row2_height < MIN_ROW2_HEIGHT ? MIN_ROW2_HEIGHT : row2_height;
}

static int calculate_row2_y(void) {
  return SHOW_TOP_PANELS ? FIXED_ROW1_HEIGHT : 0;
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
  if (PRIMARY_PANE_SLOTS == 1) {
    row2_widths[0] = screen_width;
  } else if (PRIMARY_PANE_SLOTS == 2) {
    row2_widths[0] = screen_width / 2;
    row2_widths[1] = screen_width - row2_widths[0];
  } else if (PRIMARY_PANE_SLOTS == 3) {
    row2_widths[0] = screen_width / 3;
    row2_widths[1] = screen_width / 3;
    row2_widths[2] = screen_width - row2_widths[0] - row2_widths[1];
  }
}

static int get_module_array_index_by_dynamic_index(int module_index) {
  for (int i = 0; modules[i].name != NULL; i++) {
    if (dynamic_module_indexes[i] == module_index)
      return i;
  }

  return 0;
}

static void load_row2_modules_with_selection(int row2_height, int screen_width,
                                             int row2_y,
                                             const int *selected_modules);
static void destroy_support_column(void);
static void start_support_column_thread(WINDOW *win);
WINDOW *create_bordered_window(int height, int width, int y, int x,
                               int color_pair);
void cleanup_row2_modules(void);

static void draw_small_terminal_message(int screen_height, int screen_width) {
  pthread_mutex_lock(&ncurses_mutex);
  erase();
  box(stdscr, 0, 0);
  if (screen_height > 2 && screen_width > 4)
    mvprintw(screen_height / 2, 2, "Terminal too small. Resize or press q.");
  refresh();
  pthread_mutex_unlock(&ncurses_mutex);
}

static void update_toggle_layout(WINDOW *sys_win, WINDOW *cpu_win,
                                 WINDOW *mem_win, WINDOW *disk_win) {
  int screen_height, screen_width;
  getmaxyx(stdscr, screen_height, screen_width);

  if (tui_size_is_too_small(screen_height, screen_width)) {
    draw_small_terminal_message(screen_height, screen_width);
    return;
  }

  int row1_widths[ROW1_MODULES];
  calculate_row1_widths(screen_width, row1_widths);

  const int row1_height = FIXED_ROW1_HEIGHT;
  const int row2_height = calculate_row2_height(screen_height);
  const int row2_y = calculate_row2_y();
  TrfxTwoColumnLayoutGeometry layout_geometry;

  trfx_two_column_layout_compute_geometry(&dashboard_layout_state, row2_y, 0,
                                          row2_height, screen_width,
                                          &layout_geometry);

  int preserved_modules[MAX_PRIMARY_MODULE_CHOICES] = {0};
  if (row2_slots) {
    for (int i = 0; i < PRIMARY_PANE_SLOTS &&
                    i < MAX_PRIMARY_MODULE_CHOICES; i++)
      preserved_modules[i] = row2_slots[i].module_index;
  }

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

  pthread_mutex_unlock(&ncurses_mutex);

  destroy_support_column();
  cleanup_row2_modules();
  row2_slots = calloc(PRIMARY_PANE_SLOTS, sizeof(WindowSlot));
  if (!row2_slots) {
    endwin();
    fprintf(stderr, "Failed to allocate memory for row2_slots\n");
    exit(EXIT_FAILURE);
  }

  load_row2_modules_with_selection(row2_height, layout_geometry.primary_width,
                                   row2_y,
                                   preserved_modules);

  if (layout_geometry.secondary_visible && layout_geometry.secondary_width > 0) {
    support_window = create_bordered_window(
        row2_height, layout_geometry.secondary_width, row2_y,
        layout_geometry.secondary_x, COLOR_BORDER);
    if (support_window)
      start_support_column_thread(support_window);
  }

  if (SHOW_TOP_PANELS)
    trfx_runtime_request_static_refresh_all();
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

static WINDOW *create_plain_window(int height, int width, int y, int x) {
  return newwin(height, width, y, x);
}

static void format_popup_time(time_t value, char *buf, size_t buf_size) {
  struct tm tm_value;

  if (!buf || buf_size == 0)
    return;

  if (localtime_r(&value, &tm_value) == NULL) {
    snprintf(buf, buf_size, "unknown");
    return;
  }

  strftime(buf, buf_size, "%H:%M:%S", &tm_value);
}

static void resume_dashboard_after_popup(void) {
  trfx_runtime_set_paused(0);
  trfx_runtime_request_static_refresh_all();
}

static const TrfxBandwidthFlow *find_hot_flow_for_connection(
    const TrfxConnectionSummary *connection, const TrfxBandwidthReport *report) {
  if (!connection || !report)
    return NULL;

  for (int i = 0; i < report->flow_count; i++) {
    const TrfxBandwidthFlow *flow = &report->flows[i];

    if ((flow->proto[0] != '\0' &&
         strcmp(flow->proto, connection->protocol) == 0 &&
         strcmp(flow->local, connection->local_endpoint) == 0 &&
         strcmp(flow->remote, connection->remote_endpoint) == 0) ||
        (flow->pid[0] != '\0' && strcmp(flow->pid, connection->pid) == 0 &&
         flow->process[0] != '\0' &&
         strcmp(flow->process, connection->process) == 0)) {
      return flow;
    }
  }

  return NULL;
}

static void __attribute__((unused)) show_bandwidth_detail_popup(void) {
  TrfxNetworkSampleBuffer samples;
  TrfxBandwidthReport report;
  int focus_index = 0;
  int sample_count;
  int screen_height, screen_width;

  trfx_runtime_set_paused(1);

  if (!trfx_bandwidth_state_copy(&samples, &report, &focus_index))
    goto out;

  if (report.flow_count <= 0)
    goto out;

  if (focus_index < 0)
    focus_index = 0;
  if (focus_index >= report.flow_count)
    focus_index = report.flow_count - 1;

  const TrfxBandwidthFlow *flow = &report.flows[focus_index];
  const char *title = "Bandwidth Detail";
  char rx[32];
  char tx[32];
  char time_line[64];
  char detail_line[256];
  char lines[8][256];
  int line_count = 0;

  trfx_format_net_bytes(flow->rx_bytes_per_sec, rx, sizeof(rx));
  trfx_format_net_bytes(flow->tx_bytes_per_sec, tx, sizeof(tx));

  snprintf(lines[line_count++], sizeof(lines[0]), "Mode: %s",
           trfx_bandwidth_mode_name(report.mode));
  snprintf(lines[line_count++], sizeof(lines[0]), "Source: %s", report.source);
  snprintf(lines[line_count++], sizeof(lines[0]), "PID: %s | Process: %s",
           flow->pid[0] ? flow->pid : "-", flow->process[0] ? flow->process : "-");
  snprintf(lines[line_count++], sizeof(lines[0]), "Proto: %s | %s -> %s",
           flow->proto[0] ? flow->proto : "-", flow->local[0] ? flow->local : "-",
           flow->remote[0] ? flow->remote : "-");
  snprintf(lines[line_count++], sizeof(lines[0]), "Rank: %d/%d | %s",
           focus_index + 1, report.flow_count,
           flow->detail[0] ? flow->detail : flow->label);
  snprintf(lines[line_count++], sizeof(lines[0]), "Bandwidth: rx %s/s | tx %s/s",
           rx, tx);

  sample_count = (int)trfx_network_sample_buffer_count(&samples);
  for (int i = sample_count - 1; i >= 0 && line_count < 8; i--) {
    const TrfxNetworkSample *sample =
        trfx_network_sample_buffer_at(&samples, (size_t)i);
    if (!sample)
      continue;

    format_popup_time(sample->captured_at, time_line, sizeof(time_line));
    snprintf(detail_line, sizeof(detail_line),
             "Sample %d: %s | interfaces %d | connections %d | owners %d", i + 1,
             time_line, sample->snapshot.interfaces.count,
             sample->snapshot.connection_count,
             sample->snapshot.socket_owner_count);
    snprintf(lines[line_count++], sizeof(lines[0]), "%s", detail_line);
  }

  getmaxyx(stdscr, screen_height, screen_width);
  int popup_width = (int)strlen(title) + 6;
  for (int i = 0; i < line_count; i++) {
    int line_width = (int)strlen(lines[i]) + 4;
    if (line_width > popup_width)
      popup_width = line_width;
  }
  if (popup_width > screen_width - 4)
    popup_width = screen_width - 4;
  if (popup_width < 60)
    popup_width = 60;

  int popup_height = line_count + 4;
  if (popup_height > screen_height - 2)
    popup_height = screen_height - 2;

  int popup_y = (screen_height - popup_height) / 2;
  int popup_x = (screen_width - popup_width) / 2;

  WINDOW *popup = create_bordered_window(popup_height, popup_width, popup_y,
                                         popup_x, COLOR_BORDER);
  if (!popup)
    goto out;

  pthread_mutex_lock(&ncurses_mutex);
  wattron(popup, A_BOLD);
  mvwprintw(popup, 0, 2, " %s ", title);
  wattroff(popup, A_BOLD);
  for (int i = 0; i < line_count && i < popup_height - 2; i++)
    trfx_print_clipped(popup, i + 1, 2, lines[i]);
  wrefresh(popup);
  pthread_mutex_unlock(&ncurses_mutex);

  int ch;
  while ((ch = getch()) != KEY_ESC && ch != '\n' && ch != KEY_ENTER)
    ;

  pthread_mutex_lock(&ncurses_mutex);
  werase(popup);
  wrefresh(popup);
  delwin(popup);
  pthread_mutex_unlock(&ncurses_mutex);

out:
  resume_dashboard_after_popup();
}

void show_connection_detail_popup(void) {
  TrfxConnectionSummaryResult connections;
  TrfxNetworkSnapshot snapshot;
  TrfxBandwidthReport bandwidth_report;
  char snapshot_error[128] = {0};
  char rx[32];
  char tx[32];
  char footer[] = "Press Enter, Esc, or q to close.";
  char title[] = "Connection Detail";
  char lines[10][256];
  int line_count = 0;
  int focus_index = 0;
  int screen_height, screen_width;
  int popup_width;
  int popup_height;
  WINDOW *popup;

  trfx_runtime_set_paused(1);
  trfx_connection_state_init();
  trfx_init_connection_summary_result(&connections);
  trfx_connection_state_copy(&connections, &focus_index);

  getmaxyx(stdscr, screen_height, screen_width);

  if (connections.count <= 0) {
    snprintf(lines[line_count++], sizeof(lines[0]), "Selection: unavailable");
    snprintf(lines[line_count++], sizeof(lines[0]), "No visible connections");
  } else {
    if (focus_index < 0)
      focus_index = 0;
    if (focus_index >= connections.count)
      focus_index = connections.count - 1;

    trfx_init_network_snapshot(&snapshot);
    trfx_collect_network_snapshot(&snapshot, snapshot_error,
                                 sizeof(snapshot_error));
    trfx_init_bandwidth_report(&bandwidth_report);
    trfx_bandwidth_state_copy(NULL, &bandwidth_report, NULL);

    const TrfxConnectionSummary *connection = &connections.rows[focus_index];
    const TrfxBandwidthFlow *flow =
        find_hot_flow_for_connection(connection, &bandwidth_report);
    char local[64];
    char remote[64];
    char owner_line[256];
    char activity_line[256];
    char context_line[256];

    trfx_format_endpoint_for_tui(connection->local_endpoint, local,
                                 sizeof(local));
    trfx_format_endpoint_for_tui(connection->remote_endpoint, remote,
                                 sizeof(remote));

    snprintf(lines[line_count++], sizeof(lines[0]), "Selection: %d/%d",
             focus_index + 1, connections.count);
    snprintf(lines[line_count++], sizeof(lines[0]), "Proto: %s | State: %s",
             connection->protocol[0] ? connection->protocol : "-",
             connection->state[0] ? connection->state : "-");
    snprintf(lines[line_count++], sizeof(lines[0]), "Local: %s", local);
    snprintf(lines[line_count++], sizeof(lines[0]), "Remote: %s", remote);

    if (connection->has_owner) {
      snprintf(owner_line, sizeof(owner_line), "Owner: UID %s | PID %s | %s",
               connection->uid[0] ? connection->uid : "-",
               connection->pid[0] ? connection->pid : "-",
               connection->process[0] ? connection->process : "-");
    } else {
      snprintf(owner_line, sizeof(owner_line),
               "Owner: unavailable for this connection");
    }
    snprintf(lines[line_count++], sizeof(lines[0]), "%s", owner_line);

    if (flow) {
      trfx_format_net_bytes(flow->rx_bytes_per_sec, rx, sizeof(rx));
      trfx_format_net_bytes(flow->tx_bytes_per_sec, tx, sizeof(tx));
      snprintf(activity_line, sizeof(activity_line),
               "Activity: measured top flow | rx %s/s | tx %s/s", rx, tx);
    } else {
      snprintf(activity_line, sizeof(activity_line),
               "Activity: not among measured top flows");
    }
    snprintf(lines[line_count++], sizeof(lines[0]), "%s", activity_line);

    snprintf(context_line, sizeof(context_line),
             "Context: %d rows | owners %d | %s", connections.count,
             snapshot.socket_owner_count,
             snapshot_error[0] ? snapshot_error : "snapshot ok");
    snprintf(lines[line_count++], sizeof(lines[0]), "%s", context_line);
  }

  popup_width = (int)strlen(title) + 6;
  for (int i = 0; i < line_count; i++) {
    int line_width = (int)strlen(lines[i]) + 4;
    if (line_width > popup_width)
      popup_width = line_width;
  }
  if (popup_width > screen_width - 4)
    popup_width = screen_width - 4;
  if (popup_width < 72)
    popup_width = 72;

  popup_height = line_count + 4;
  if (popup_height > screen_height - 2)
    popup_height = screen_height - 2;

  int popup_y = (screen_height - popup_height) / 2;
  int popup_x = (screen_width - popup_width) / 2;

  popup = create_bordered_window(popup_height, popup_width, popup_y, popup_x,
                                 COLOR_BORDER);
  if (!popup) {
    resume_dashboard_after_popup();
    return;
  }

  pthread_mutex_lock(&ncurses_mutex);
  werase(popup);
  wattron(popup, trfx_color_attr(COLOR_BORDER));
  box(popup, 0, 0);
  wattroff(popup, trfx_color_attr(COLOR_BORDER));
  wattron(popup, A_BOLD);
  mvwprintw(popup, 0, 2, " %s ", title);
  wattroff(popup, A_BOLD);

  for (int i = 0; i < line_count && i < popup_height - 2; i++)
    trfx_print_clipped(popup, i + 1, 2, lines[i]);
  trfx_print_clipped(popup, popup_height - 2, 2, footer);
  wrefresh(popup);
  pthread_mutex_unlock(&ncurses_mutex);

  while (1) {
    int ch = wgetch(popup);
    if (ch == KEY_ESC || ch == '\n' || ch == KEY_ENTER || ch == 10 ||
        ch == 'q' || ch == 'Q')
      break;
  }

  pthread_mutex_lock(&ncurses_mutex);
  werase(popup);
  wrefresh(popup);
  delwin(popup);
  pthread_mutex_unlock(&ncurses_mutex);
  resume_dashboard_after_popup();
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
  for (int i = 0; i < PRIMARY_PANE_SLOTS; i++) {
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

  typedef struct {
    const char *key;
    const char *description;
  } HotkeyHelpRow;

  static const HotkeyHelpRow hotkeys[] = {
      {"[s]", "Change the process sort order"},
      {"[r]", "Refresh all panels immediately"},
      {"[c]", "Change the primary module"},
      {"[d]", "Show bandwidth detail in the support dock"},
      {"[x]", "Open the modal process kill confirmation"},
      {"[z]", "Open the modal connection drop confirmation"},
      {"[a]", "Show action audit in the support dock"},
      {"[g]", "Show diagnostics in the support dock"},
      {"[n]", "Show route and DNS in the support dock"},
      {"[v]", "Show network health in the support dock"},
      {"[l]", "Cycle the support dock view"},
      {"[t]", "Show or hide the top system panels"},
      {"[J/K]", "Move the selected connection row"},
      {"[o]", "Show connection detail in the support dock"},
      {"[p]", "Pause or resume live updates"},
      {"[h]", "Open this help popup"},
      {"[q]", "Quit Trafix"},
  };
  const int hotkey_count = (int)(sizeof(hotkeys) / sizeof(hotkeys[0]));

  int screen_height, screen_width;
  getmaxyx(stdscr, screen_height, screen_width);

  const char *title = "Hotkeys";
  const char *subtitle =
      "Primary and support columns stay visible. `l` cycles support views.";
  const int key_col_width = 8;
  int popup_height = hotkey_count + 6;
  int popup_width = (int)strlen(title) + 8;
  {
    int subtitle_width = (int)strlen(subtitle) + 4;
    if (subtitle_width > popup_width)
      popup_width = subtitle_width;
  }
  for (int i = 0; i < hotkey_count; ++i) {
    int line_width = key_col_width + (int)strlen(hotkeys[i].description) + 6;
    if (line_width > popup_width)
      popup_width = line_width;
  }
  if (popup_width > screen_width - 4)
    popup_width = screen_width - 4;
  if (popup_width < 54)
    popup_width = 54;

  int popup_y = (screen_height - popup_height) / 2;
  int popup_x = (screen_width - popup_width) / 2;

  WINDOW *popup = create_bordered_window(popup_height, popup_width, popup_y,
                                         popup_x, COLOR_BORDER);
  if (!popup) {
    resume_dashboard_after_popup();
    return;
  }

  keypad(popup, FALSE);
  nodelay(popup, TRUE);

  pthread_mutex_lock(&ncurses_mutex);
  werase(popup);
  wattron(popup, trfx_color_attr(COLOR_BORDER));
  box(popup, 0, 0);
  wattroff(popup, trfx_color_attr(COLOR_BORDER));
  wattron(popup, A_BOLD);
  mvwprintw(popup, 1, 2, "%s", title);
  wattroff(popup, A_BOLD);
  trfx_print_clipped(popup, 2, 2, subtitle);
  mvwprintw(popup, 3, 2, "%-7s %s", "Key", "Description");
  for (int i = 0; i < hotkey_count; ++i)
    mvwprintw(popup, i + 4, 2, "%-7s", hotkeys[i].key);
  for (int i = 0; i < hotkey_count; ++i)
    trfx_print_clipped(popup, i + 4, 11, hotkeys[i].description);
  trfx_print_clipped(popup, popup_height - 2, 2,
                     "Press Esc, Enter, or q to close.");
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
  resume_dashboard_after_popup();
}

void show_diagnostics_popup(void) {
  TrfxDiagnosticsSnapshot snapshot;
  TrfxAlertSummary alerts;
  TrfxCollectorStatus status;
  char error[256];
  char alerts_line[384];
  int screen_height, screen_width;
  int popup_width = 104;
  int visible_lines;
  int line_count = 0;
  char title[] = "Diagnostics Logs";
  char footer[] = "Press Enter, Esc, or q to close.";
  char line[384];
  char time_text[32];
  WINDOW *popup;
  time_t now;

  trfx_runtime_set_paused(1);
  trfx_init_diagnostics_snapshot(&snapshot);
  trfx_init_alert_summary(&alerts);
  status = trfx_collect_diagnostics_snapshot(&snapshot, error, sizeof(error));
  trfx_collect_diagnostics_alerts(&snapshot, &alerts);

  snprintf(alerts_line, sizeof(alerts_line), "Alerts: ");
  if (trfx_diagnostics_alert_count(&alerts) == 0) {
    strncat(alerts_line, "none", sizeof(alerts_line) - strlen(alerts_line) - 1);
  } else {
    for (size_t i = 0; i < trfx_diagnostics_alert_count(&alerts); i++) {
      const char *alert = trfx_diagnostics_alert_at(&alerts, i);
      if (!alert)
        continue;
      if (i > 0)
        strncat(alerts_line, "; ",
                sizeof(alerts_line) - strlen(alerts_line) - 1);
      strncat(alerts_line, alert,
              sizeof(alerts_line) - strlen(alerts_line) - 1);
    }
  }

  getmaxyx(stdscr, screen_height, screen_width);
  visible_lines = screen_height - 7;
  if (status != TRFX_COLLECTOR_OK && error[0] != '\0')
    visible_lines--;
  if (visible_lines < 1)
    visible_lines = 1;

  if (popup_width > screen_width - 4)
    popup_width = screen_width - 4;
  if (popup_width < 72)
    popup_width = 72;

  int popup_height = visible_lines + 4;
  if (status != TRFX_COLLECTOR_OK && error[0] != '\0')
    popup_height++;
  if (popup_height > screen_height - 2)
    popup_height = screen_height - 2;
  int popup_y = (screen_height - popup_height) / 2;
  int popup_x = (screen_width - popup_width) / 2;
  popup = create_bordered_window(popup_height, popup_width, popup_y, popup_x,
                                 COLOR_BORDER);
  if (!popup) {
    resume_dashboard_after_popup();
    return;
  }

  pthread_mutex_lock(&ncurses_mutex);
  werase(popup);
  wattron(popup, trfx_color_attr(COLOR_BORDER));
  box(popup, 0, 0);
  wattroff(popup, trfx_color_attr(COLOR_BORDER));
  wattron(popup, A_BOLD);
  mvwprintw(popup, 0, 2, " %s ", title);
  wattroff(popup, A_BOLD);

  if (status != TRFX_COLLECTOR_OK && error[0] != '\0') {
    trfx_print_clipped(popup, 1, 2, error);
    trfx_print_clipped(popup, 2, 2, alerts_line);
  } else {
    trfx_print_clipped(popup, 1, 2, alerts_line);
  }

  if (trfx_diagnostics_log_count(&snapshot.logs) == 0) {
    trfx_print_clipped(popup, status != TRFX_COLLECTOR_OK && error[0] != '\0'
                                 ? 3
                                 : 2,
                       2, "No readable log lines available.");
  } else {
    now = time(NULL);
    format_popup_time(now, time_text, sizeof(time_text));
    line_count = (int)trfx_diagnostics_log_count(&snapshot.logs);
    if (line_count > visible_lines)
      line_count = visible_lines;
    for (int i = 0; i < line_count; i++) {
      const TrfxDiagnosticsLogLine *entry =
          trfx_diagnostics_log_at(&snapshot.logs, (size_t)i);
      if (!entry)
        continue;

      snprintf(line, sizeof(line), "[%s] %s | %s", entry->source,
               time_text, entry->text);
      trfx_print_clipped(popup,
                         i + (status != TRFX_COLLECTOR_OK && error[0] != '\0' ? 3 : 2),
                         2, line);
    }
  }

  trfx_print_clipped(popup, popup_height - 2, 2, footer);
  wrefresh(popup);
  pthread_mutex_unlock(&ncurses_mutex);

  while (1) {
    int ch = wgetch(popup);
    if (ch == KEY_ESC || ch == '\n' || ch == KEY_ENTER || ch == 10 ||
        ch == 'q' || ch == 'Q')
      break;
  }

  pthread_mutex_lock(&ncurses_mutex);
  werase(popup);
  wrefresh(popup);
  delwin(popup);
  pthread_mutex_unlock(&ncurses_mutex);
  resume_dashboard_after_popup();
}

static void __attribute__((unused)) show_route_dns_popup(void) {
  TrfxDiagnosticsSnapshot snapshot;
  char error[256];
  char route_line[256];
  char dns_line[256];
  char active_line[256];
  char footer[] = "Press Enter, Esc, or q to close.";
  int screen_height, screen_width;
  int popup_width = 88;
  int popup_height = 9;
  WINDOW *popup;

  trfx_runtime_set_paused(1);
  trfx_init_diagnostics_snapshot(&snapshot);
  trfx_collect_diagnostics_snapshot(&snapshot, error, sizeof(error));

  getmaxyx(stdscr, screen_height, screen_width);
  if (popup_width > screen_width - 4)
    popup_width = screen_width - 4;
  if (popup_width < 68)
    popup_width = 68;
  if (popup_height > screen_height - 2)
    popup_height = screen_height - 2;

  int popup_y = (screen_height - popup_height) / 2;
  int popup_x = (screen_width - popup_width) / 2;
  popup = create_bordered_window(popup_height, popup_width, popup_y, popup_x,
                                 COLOR_BORDER);
  if (!popup) {
    resume_dashboard_after_popup();
    return;
  }

  if (snapshot.network.route.has_default) {
    snprintf(route_line, sizeof(route_line), "Route: default via %s dev %s metric %s",
             snapshot.network.route.gateway, snapshot.network.route.interface,
             snapshot.network.route.metric);
  } else {
    snprintf(route_line, sizeof(route_line), "Route: unavailable");
  }

  if (snapshot.network.dns.count > 0) {
    char dns_body[192] = "";
    for (int i = 0; i < snapshot.network.dns.count; i++) {
      if (i > 0)
        strncat(dns_body, ", ", sizeof(dns_body) - strlen(dns_body) - 1);
      strncat(dns_body, snapshot.network.dns.servers[i],
              sizeof(dns_body) - strlen(dns_body) - 1);
    }
    snprintf(dns_line, sizeof(dns_line), "DNS: %s", dns_body);
  } else {
    snprintf(dns_line, sizeof(dns_line), "DNS: unavailable");
  }

  if (snapshot.network.has_active_interface) {
    snprintf(active_line, sizeof(active_line),
             "Active: %s %s %s%s%s", snapshot.network.active_interface,
             snapshot.network.active_type,
             snapshot.network.active_ip[0] ? snapshot.network.active_ip : "N/A",
             snapshot.network.active_ssid[0] ? " | SSID " : "",
             snapshot.network.active_ssid[0] ? snapshot.network.active_ssid : "");
  } else {
    snprintf(active_line, sizeof(active_line), "Active: unavailable");
  }

  pthread_mutex_lock(&ncurses_mutex);
  werase(popup);
  wattron(popup, trfx_color_attr(COLOR_BORDER));
  box(popup, 0, 0);
  wattroff(popup, trfx_color_attr(COLOR_BORDER));
  wattron(popup, A_BOLD);
  mvwprintw(popup, 0, 2, " Route And DNS Checks ");
  wattroff(popup, A_BOLD);
  trfx_print_clipped(popup, 1, 2, route_line);
  trfx_print_clipped(popup, 2, 2, dns_line);
  trfx_print_clipped(popup, 3, 2, active_line);
  trfx_print_clipped(popup, 5, 2, footer);
  if (error[0] != '\0')
    trfx_print_clipped(popup, 6, 2, error);
  wrefresh(popup);
  pthread_mutex_unlock(&ncurses_mutex);

  while (1) {
    int ch = wgetch(popup);
    if (ch == KEY_ESC || ch == '\n' || ch == KEY_ENTER || ch == 10 ||
        ch == 'q' || ch == 'Q')
      break;
  }

  pthread_mutex_lock(&ncurses_mutex);
  werase(popup);
  wrefresh(popup);
  delwin(popup);
  pthread_mutex_unlock(&ncurses_mutex);
  resume_dashboard_after_popup();
}

static void __attribute__((unused)) show_network_health_popup(void) {
  TrfxDiagnosticsSnapshot snapshot;
  char error[256];
  char cpu_line[256];
  char memory_line[256];
  char disk_line[256];
  char process_line[256];
  char network_line[256];
  char footer[] = "Press Enter, Esc, or q to close.";
  int screen_height, screen_width;
  int popup_width = 96;
  int popup_height = 10;
  WINDOW *popup;

  trfx_runtime_set_paused(1);
  trfx_init_diagnostics_snapshot(&snapshot);
  trfx_collect_diagnostics_snapshot(&snapshot, error, sizeof(error));

  getmaxyx(stdscr, screen_height, screen_width);
  if (popup_width > screen_width - 4)
    popup_width = screen_width - 4;
  if (popup_width < 72)
    popup_width = 72;
  if (popup_height > screen_height - 2)
    popup_height = screen_height - 2;

  int popup_y = (screen_height - popup_height) / 2;
  int popup_x = (screen_width - popup_width) / 2;
  popup = create_bordered_window(popup_height, popup_width, popup_y, popup_x,
                                 COLOR_BORDER);
  if (!popup) {
    resume_dashboard_after_popup();
    return;
  }

  snprintf(cpu_line, sizeof(cpu_line), "CPU: avg %.1f%% | temp %.1fC | cores %d",
           snapshot.cpu.avg_usage, snapshot.cpu.temperature,
           snapshot.cpu.num_cores);
  snprintf(memory_line, sizeof(memory_line),
           "Memory: %.1f%% | RAM %ld/%ld | SWAP %ld/%ld",
           snapshot.memory.mem_percent, snapshot.memory.used_ram,
           snapshot.memory.total_ram, snapshot.memory.used_swap,
           snapshot.memory.total_swap);
  snprintf(disk_line, sizeof(disk_line),
           "Disk: %d mounts | %.1f/%.1f MB used", snapshot.disk_count,
           snapshot.disk_total_used_mb, snapshot.disk_total_mb);
  snprintf(process_line, sizeof(process_line),
           "Process pressure: %d collected | top %s", snapshot.processes.count,
           snapshot.processes.count > 0 ? snapshot.processes.processes[0].command
                                        : "unavailable");

  if (snapshot.network.route.has_default && snapshot.network.dns.count > 0 &&
      snapshot.network.has_active_interface) {
    snprintf(network_line, sizeof(network_line),
             "Network: route, DNS, and active interface present");
  } else if (!snapshot.network.route.has_default &&
             snapshot.network.dns.count == 0) {
    snprintf(network_line, sizeof(network_line),
             "Network: route and DNS data unavailable");
  } else {
    snprintf(network_line, sizeof(network_line),
             "Network: partial snapshot | route %s | DNS %s | active %s",
             snapshot.network.route.has_default ? "ok" : "missing",
             snapshot.network.dns.count > 0 ? "ok" : "missing",
             snapshot.network.has_active_interface ? "ok" : "missing");
  }

  pthread_mutex_lock(&ncurses_mutex);
  werase(popup);
  wattron(popup, trfx_color_attr(COLOR_BORDER));
  box(popup, 0, 0);
  wattroff(popup, trfx_color_attr(COLOR_BORDER));
  wattron(popup, A_BOLD);
  mvwprintw(popup, 0, 2, " Network Health Correlation ");
  wattroff(popup, A_BOLD);
  trfx_print_clipped(popup, 1, 2, network_line);
  trfx_print_clipped(popup, 2, 2, cpu_line);
  trfx_print_clipped(popup, 3, 2, memory_line);
  trfx_print_clipped(popup, 4, 2, disk_line);
  trfx_print_clipped(popup, 5, 2, process_line);
  trfx_print_clipped(popup, 7, 2, footer);
  if (error[0] != '\0')
    trfx_print_clipped(popup, 8, 2, error);
  wrefresh(popup);
  pthread_mutex_unlock(&ncurses_mutex);

  while (1) {
    int ch = wgetch(popup);
    if (ch == KEY_ESC || ch == '\n' || ch == KEY_ENTER || ch == 10 ||
        ch == 'q' || ch == 'Q')
      break;
  }

  pthread_mutex_lock(&ncurses_mutex);
  werase(popup);
  wrefresh(popup);
  delwin(popup);
  pthread_mutex_unlock(&ncurses_mutex);
  resume_dashboard_after_popup();
}

int show_action_review_popup(const TrfxActionReview *review) {
  const char *title = "Confirm Action";
  char line1[256];
  char line2[256];
  char line3[256];
  char detail_body[240];
  int screen_height, screen_width;
  int popup_height = 8;
  int popup_width = 72;
  int confirmed = 0;

  if (!review)
    return 0;

  trfx_runtime_set_paused(1);

  snprintf(line1, sizeof(line1), "Action: %.248s",
           review->request.label[0] ? review->request.label : "unknown");
  trfx_clip_text(review->details[0] ? review->details : "unknown",
                 detail_body, sizeof(detail_body), 239);
  snprintf(line2, sizeof(line2), "Details: %s", detail_body);
  snprintf(line3, sizeof(line3), "State: %s",
           trfx_action_permission_status_name(review->permission));

  getmaxyx(stdscr, screen_height, screen_width);
  if (popup_width > screen_width - 4)
    popup_width = screen_width - 4;
  if (popup_width < 50)
    popup_width = 50;
  if (popup_height > screen_height - 2)
    popup_height = screen_height - 2;

  int popup_y = (screen_height - popup_height) / 2;
  int popup_x = (screen_width - popup_width) / 2;

  WINDOW *popup = create_bordered_window(popup_height, popup_width, popup_y,
                                         popup_x, COLOR_BORDER);
  if (!popup) {
    resume_dashboard_after_popup();
    return 0;
  }

  pthread_mutex_lock(&ncurses_mutex);
  werase(popup);
  wattron(popup, trfx_color_attr(COLOR_BORDER));
  box(popup, 0, 0);
  wattroff(popup, trfx_color_attr(COLOR_BORDER));
  wattron(popup, A_BOLD);
  mvwprintw(popup, 0, 2, " %s ", title);
  wattroff(popup, A_BOLD);
  trfx_print_clipped(popup, 1, 2, line1);
  trfx_print_clipped(popup, 2, 2, line2);
  trfx_print_clipped(popup, 3, 2, line3);
  trfx_print_clipped(popup, 5, 2, "Enter to confirm, Esc to cancel.");
  wrefresh(popup);
  pthread_mutex_unlock(&ncurses_mutex);

  while (1) {
    int ch = wgetch(popup);
    if (ch == KEY_ENTER || ch == '\n' || ch == 10) {
      confirmed = 1;
      break;
    }
    if (ch == KEY_ESC || ch == 'q' || ch == 'Q')
      break;
  }

  pthread_mutex_lock(&ncurses_mutex);
  werase(popup);
  wrefresh(popup);
  delwin(popup);
  pthread_mutex_unlock(&ncurses_mutex);
  resume_dashboard_after_popup();

  return confirmed;
}

static void show_action_feedback_popup(const char *title, const char *message) {
  int screen_height, screen_width;
  int popup_height = 6;
  int popup_width = 72;
  WINDOW *popup;

  if (!message)
    message = "no details";

  getmaxyx(stdscr, screen_height, screen_width);
  if (popup_width > screen_width - 4)
    popup_width = screen_width - 4;
  if (popup_width < 40)
    popup_width = 40;
  if (popup_height > screen_height - 2)
    popup_height = screen_height - 2;

  int popup_y = (screen_height - popup_height) / 2;
  int popup_x = (screen_width - popup_width) / 2;

  popup = create_bordered_window(popup_height, popup_width, popup_y, popup_x,
                                 COLOR_BORDER);
  if (!popup) {
    resume_dashboard_after_popup();
    return;
  }

  pthread_mutex_lock(&ncurses_mutex);
  werase(popup);
  wattron(popup, trfx_color_attr(COLOR_BORDER));
  box(popup, 0, 0);
  wattroff(popup, trfx_color_attr(COLOR_BORDER));
  wattron(popup, A_BOLD);
  mvwprintw(popup, 0, 2, " %s ", title ? title : "Message");
  wattroff(popup, A_BOLD);
  trfx_print_clipped(popup, 1, 2, message);
  trfx_print_clipped(popup, 3, 2, "Press Enter or Esc to close.");
  wrefresh(popup);
  pthread_mutex_unlock(&ncurses_mutex);

  while (1) {
    int ch = wgetch(popup);
    if (ch == KEY_ESC || ch == '\n' || ch == KEY_ENTER || ch == 10 || ch == 'q' || ch == 'Q')
      break;
  }

  pthread_mutex_lock(&ncurses_mutex);
  werase(popup);
  wrefresh(popup);
  delwin(popup);
  pthread_mutex_unlock(&ncurses_mutex);
  resume_dashboard_after_popup();
}

static void show_action_result_popup(const TrfxActionRequest *request,
                                     const TrfxActionResult *result) {
  int screen_height, screen_width;
  int popup_height = 8;
  int popup_width = 84;
  char time_line[32];
  char action_line[256];
  char detail_line[256];
  char status_line[256];
  char message_line[256];
  char detail_body[240];
  char message_body[240];
  time_t now;
  WINDOW *popup;

  if (!request || !result)
    return;

  trfx_runtime_set_paused(1);
  now = time(NULL);
  format_popup_time(now, time_line, sizeof(time_line));
  snprintf(action_line, sizeof(action_line), "Action: %s",
           request->label[0] ? request->label : "unknown");
  trfx_clip_text(request->description[0] ? request->description : "unknown",
                 detail_body, sizeof(detail_body), 239);
  snprintf(detail_line, sizeof(detail_line), "Target: %s", detail_body);
  snprintf(status_line, sizeof(status_line), "Result: %s",
           trfx_action_result_status_name(result->status));
  trfx_clip_text(result->message[0] ? result->message : "no details",
                 message_body, sizeof(message_body), 239);
  snprintf(message_line, sizeof(message_line), "Message: %s", message_body);

  getmaxyx(stdscr, screen_height, screen_width);
  if (popup_width > screen_width - 4)
    popup_width = screen_width - 4;
  if (popup_width < 60)
    popup_width = 60;
  if (popup_height > screen_height - 2)
    popup_height = screen_height - 2;

  int popup_y = (screen_height - popup_height) / 2;
  int popup_x = (screen_width - popup_width) / 2;
  popup = create_bordered_window(popup_height, popup_width, popup_y, popup_x,
                                 COLOR_BORDER);
  if (!popup) {
    resume_dashboard_after_popup();
    return;
  }

  pthread_mutex_lock(&ncurses_mutex);
  werase(popup);
  wattron(popup, trfx_color_attr(COLOR_BORDER));
  box(popup, 0, 0);
  wattroff(popup, trfx_color_attr(COLOR_BORDER));
  wattron(popup, A_BOLD);
  mvwprintw(popup, 0, 2, " Action Result ");
  wattroff(popup, A_BOLD);
  trfx_print_clipped(popup, 1, 2, time_line);
  trfx_print_clipped(popup, 2, 2, action_line);
  trfx_print_clipped(popup, 3, 2, detail_line);
  trfx_print_clipped(popup, 4, 2, status_line);
  trfx_print_clipped(popup, 5, 2, message_line);
  trfx_print_clipped(popup, popup_height - 2, 2,
                     "Press Enter or Esc to close.");
  wrefresh(popup);
  pthread_mutex_unlock(&ncurses_mutex);

  while (1) {
    int ch = wgetch(popup);
    if (ch == KEY_ESC || ch == '\n' || ch == KEY_ENTER || ch == 10 ||
        ch == 'q' || ch == 'Q')
      break;
  }

  pthread_mutex_lock(&ncurses_mutex);
  werase(popup);
  wrefresh(popup);
  delwin(popup);
  pthread_mutex_unlock(&ncurses_mutex);
  resume_dashboard_after_popup();
}

static void __attribute__((unused)) show_action_audit_popup(void) {
  size_t count = trfx_action_audit_count();
  int screen_height, screen_width;
  int visible_entries;
  int popup_height;
  int popup_width = 96;
  const char *footer = "Newest first. Press Enter, Esc, or q to close.";
  char line[512];
  char time_text[32];
  char label_body[64];
  char detail_body[128];
  char message_body[128];
  WINDOW *popup;

  trfx_runtime_set_paused(1);
  getmaxyx(stdscr, screen_height, screen_width);

  visible_entries = screen_height - 5;
  if (visible_entries < 1)
    visible_entries = 1;
  if ((size_t)visible_entries > count)
    visible_entries = (int)count;
  if (visible_entries < 1)
    visible_entries = 1;

  popup_height = visible_entries + 4;
  if (popup_height > screen_height - 2)
    popup_height = screen_height - 2;
  if (popup_width > screen_width - 4)
    popup_width = screen_width - 4;
  if (popup_width < 72)
    popup_width = 72;

  int popup_y = (screen_height - popup_height) / 2;
  int popup_x = (screen_width - popup_width) / 2;
  popup = create_bordered_window(popup_height, popup_width, popup_y, popup_x,
                                 COLOR_BORDER);
  if (!popup) {
    resume_dashboard_after_popup();
    return;
  }

  pthread_mutex_lock(&ncurses_mutex);
  werase(popup);
  wattron(popup, trfx_color_attr(COLOR_BORDER));
  box(popup, 0, 0);
  wattroff(popup, trfx_color_attr(COLOR_BORDER));
  wattron(popup, A_BOLD);
  mvwprintw(popup, 0, 2, " Action Audit ");
  wattroff(popup, A_BOLD);

  if (count == 0) {
    trfx_print_clipped(popup, 1, 2, "No recorded actions yet.");
  } else {
    for (int i = 0; i < visible_entries; i++) {
      const TrfxActionAuditEntry *entry = trfx_action_audit_at((size_t)i);
      if (!entry)
        continue;

      format_popup_time(entry->when, time_text, sizeof(time_text));
      trfx_clip_text(entry->request.label[0] ? entry->request.label : "unknown",
                     label_body, sizeof(label_body), 32);
      trfx_clip_text(entry->request.description[0] ? entry->request.description
                                                   : "unknown",
                     detail_body, sizeof(detail_body), 96);
      trfx_clip_text(entry->message[0] ? entry->message : "no details",
                     message_body, sizeof(message_body), 96);
      snprintf(line, sizeof(line), "%s | %.32s | %.96s | %.16s | %.96s",
               time_text, label_body, detail_body,
               trfx_action_result_status_name(entry->status), message_body);
      trfx_print_clipped(popup, i + 1, 2, line);
    }
  }

  trfx_print_clipped(popup, popup_height - 2, 2, footer);
  wrefresh(popup);
  pthread_mutex_unlock(&ncurses_mutex);

  while (1) {
    int ch = wgetch(popup);
    if (ch == KEY_ESC || ch == '\n' || ch == KEY_ENTER || ch == 10 ||
        ch == 'q' || ch == 'Q')
      break;
  }

  pthread_mutex_lock(&ncurses_mutex);
  werase(popup);
  wrefresh(popup);
  delwin(popup);
  pthread_mutex_unlock(&ncurses_mutex);
  resume_dashboard_after_popup();
}

static int select_process_for_kill(ProcessInfo *selected) {
  ProcessInfo processes[MAX_PROCESSES];
  int count = get_top_processes(processes, MAX_PROCESSES, current_sort_type);
  int screen_height, screen_width;
  int visible_count = count < 8 ? count : 8;
  int selected_index = 0;

  if (!selected)
    return 0;

  if (count <= 0) {
    show_action_feedback_popup("Kill Process", "No processes available");
    return 0;
  }

  getmaxyx(stdscr, screen_height, screen_width);

  int popup_height = visible_count + 5;
  int popup_width = 88;
  if (popup_width > screen_width - 4)
    popup_width = screen_width - 4;
  if (popup_width < 60)
    popup_width = 60;
  if (popup_height > screen_height - 2)
    popup_height = screen_height - 2;

  int popup_y = (screen_height - popup_height) / 2;
  int popup_x = (screen_width - popup_width) / 2;
  WINDOW *popup = create_bordered_window(popup_height, popup_width, popup_y,
                                         popup_x, COLOR_BORDER);
  if (!popup)
    return 0;

  keypad(popup, TRUE);
  while (1) {
    pthread_mutex_lock(&ncurses_mutex);
    werase(popup);
    wattron(popup, trfx_color_attr(COLOR_BORDER));
    box(popup, 0, 0);
    wattroff(popup, trfx_color_attr(COLOR_BORDER));
    wattron(popup, A_BOLD);
    mvwprintw(popup, 0, 2, " Select Process To Kill ");
    wattroff(popup, A_BOLD);
    mvwprintw(popup, 1, 2, "%-7s %-16s %-8s %s", "PID", "USER", "CPU",
              "COMMAND");
    for (int i = 0; i < visible_count; i++) {
      int row = i + 2;
      if (i == selected_index)
        wattron(popup, A_REVERSE);
      mvwprintw(popup, row, 2, "%-7s %-16.16s %-8.8s %-s", processes[i].pid,
                processes[i].user, processes[i].cpu, processes[i].command);
      if (i == selected_index)
        wattroff(popup, A_REVERSE);
    }
    trfx_print_clipped(popup, popup_height - 2, 2,
                       "Enter to confirm, Esc to cancel.");
    wrefresh(popup);
    pthread_mutex_unlock(&ncurses_mutex);

    int ch = wgetch(popup);
    if (ch == KEY_UP) {
      selected_index = (selected_index - 1 + visible_count) % visible_count;
    } else if (ch == KEY_DOWN) {
      selected_index = (selected_index + 1) % visible_count;
    } else if (ch == KEY_ENTER || ch == '\n' || ch == 10) {
      *selected = processes[selected_index];
      break;
    } else if (ch == KEY_ESC || ch == 'q' || ch == 'Q') {
      memset(selected, 0, sizeof(*selected));
      return 0;
    }
  }

  pthread_mutex_lock(&ncurses_mutex);
  werase(popup);
  wrefresh(popup);
  delwin(popup);
  pthread_mutex_unlock(&ncurses_mutex);

  return 1;
}

static void handle_process_kill_action(void) {
  ProcessInfo target;
  TrfxActionRequest request;
  TrfxActionReview review;
  TrfxActionResult result;
  char error[256];
  unsigned int target_uid = 0;

  memset(&target, 0, sizeof(target));
  if (!select_process_for_kill(&target))
    return;

  trfx_action_request_set_process_kill(&request, target.pid, target.command);

  if (!trfx_lookup_process_uid(target.pid, &target_uid, error, sizeof(error))) {
    show_action_feedback_popup("Kill Process", error[0] ? error : "process not found");
    return;
  }

  trfx_prepare_action_review(&review, &request, (unsigned int)geteuid(),
                             target_uid, 1);
  if (!show_action_review_popup(&review)) {
    result = trfx_execute_action_request(&request, 0, (unsigned int)geteuid(),
                                         error, sizeof(error));
    show_action_result_popup(&request, &result);
    return;
  }

  result = trfx_execute_action_request(&request, 1, (unsigned int)geteuid(),
                                       error, sizeof(error));
  show_action_result_popup(&request, &result);
}

static int select_connection_for_drop(ConnectionInfo *selected) {
  ConnectionInfo connections[MAX_CONNECTIONS];
  int count = get_connection_info(connections, MAX_CONNECTIONS);
  int visible_count = count < 8 ? count : 8;
  int selected_index = 0;
  int screen_height, screen_width;

  if (!selected)
    return 0;

  if (count <= 0) {
    show_action_feedback_popup("Drop Connection", "No connections available");
    return 0;
  }

  getmaxyx(stdscr, screen_height, screen_width);
  int popup_height = visible_count + 5;
  int popup_width = 108;
  if (popup_width > screen_width - 4)
    popup_width = screen_width - 4;
  if (popup_width < 72)
    popup_width = 72;
  if (popup_height > screen_height - 2)
    popup_height = screen_height - 2;

  int popup_y = (screen_height - popup_height) / 2;
  int popup_x = (screen_width - popup_width) / 2;
  WINDOW *popup = create_bordered_window(popup_height, popup_width, popup_y,
                                         popup_x, COLOR_BORDER);
  if (!popup)
    return 0;

  keypad(popup, TRUE);
  while (1) {
    pthread_mutex_lock(&ncurses_mutex);
    werase(popup);
    wattron(popup, trfx_color_attr(COLOR_BORDER));
    box(popup, 0, 0);
    wattroff(popup, trfx_color_attr(COLOR_BORDER));
    wattron(popup, A_BOLD);
    mvwprintw(popup, 0, 2, " Select Connection To Drop ");
    wattroff(popup, A_BOLD);
    mvwprintw(popup, 1, 2, "%-6s %-22s %-22s %-12s %-7s %-16s", "PROTO",
              "LOCAL", "REMOTE", "STATE", "PID", "PROCESS");
    for (int i = 0; i < visible_count; i++) {
      int row = i + 2;
      if (i == selected_index)
        wattron(popup, A_REVERSE);
      mvwprintw(popup, row, 2, "%-6.6s %-22.22s %-22.22s %-12.12s %-7.7s %-16.16s",
                connections[i].protocol, connections[i].local_addr,
                connections[i].remote_addr, connections[i].state,
                connections[i].pid, connections[i].process);
      if (i == selected_index)
        wattroff(popup, A_REVERSE);
    }
    trfx_print_clipped(popup, popup_height - 2, 2,
                       "Enter to confirm, Esc to cancel.");
    wrefresh(popup);
    pthread_mutex_unlock(&ncurses_mutex);

    int ch = wgetch(popup);
    if (ch == KEY_UP) {
      selected_index = (selected_index - 1 + visible_count) % visible_count;
    } else if (ch == KEY_DOWN) {
      selected_index = (selected_index + 1) % visible_count;
    } else if (ch == KEY_ENTER || ch == '\n' || ch == 10) {
      *selected = connections[selected_index];
      break;
    } else if (ch == KEY_ESC || ch == 'q' || ch == 'Q') {
      memset(selected, 0, sizeof(*selected));
      return 0;
    }
  }

  pthread_mutex_lock(&ncurses_mutex);
  werase(popup);
  wrefresh(popup);
  delwin(popup);
  pthread_mutex_unlock(&ncurses_mutex);

  return 1;
}

static void handle_connection_drop_action(void) {
  ConnectionInfo target;
  TrfxActionRequest request;
  TrfxActionReview review;
  TrfxActionResult result;
  char error[256];

  memset(&target, 0, sizeof(target));
  if (!select_connection_for_drop(&target))
    return;

  trfx_action_request_set_connection_drop(&request, &target);
  trfx_prepare_action_review(&review, &request, (unsigned int)geteuid(), 0, 1);
  if (!show_action_review_popup(&review)) {
    result = trfx_execute_action_request(&request, 0, (unsigned int)geteuid(),
                                         error, sizeof(error));
    show_action_result_popup(&request, &result);
    return;
  }

  result = trfx_execute_action_request(&request, 1, (unsigned int)geteuid(),
                                       error, sizeof(error));
  show_action_result_popup(&request, &result);
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
    resume_dashboard_after_popup();
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
  resume_dashboard_after_popup();
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
    resume_dashboard_after_popup();
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
  resume_dashboard_after_popup();
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
  for (int i = 0; i < PRIMARY_PANE_SLOTS; i++) {
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

  for (int i = 0; i < PRIMARY_PANE_SLOTS; i++) {
    if (row2_slots[i].thread_id) {
      row2_slots[i].stop_requested = 1;
    }
  }

  for (int i = 0; i < PRIMARY_PANE_SLOTS; i++) {
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

static void destroy_support_column(void) {
  if (support_thread_active) {
    support_stop_requested = 1;
    pthread_join(support_thread_id, NULL);
    support_thread_active = 0;
  }

  if (support_window) {
    pthread_mutex_lock(&ncurses_mutex);
    delwin(support_window);
    pthread_mutex_unlock(&ncurses_mutex);
    support_window = NULL;
  }
}

static void start_support_column_thread(WINDOW *win) {
  ThreadArg *arg = malloc(sizeof(ThreadArg));
  if (!arg) {
    fprintf(stderr, "Failed to allocate memory for support ThreadArg\n");
    return;
  }

  arg->module_index = 0;
  arg->window = win;
  support_stop_requested = 0;
  arg->stop_requested = &support_stop_requested;

  if (pthread_create(&support_thread_id, NULL, support_info_thread, arg) != 0) {
    free(arg);
    return;
  }

  support_thread_active = 1;
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

static void load_row2_modules_with_selection(int row2_height, int screen_width,
                                             int row2_y,
                                             const int *selected_modules) {
  int *row2_widths = malloc(PRIMARY_PANE_SLOTS * sizeof(int));
  if (!row2_widths) {
    endwin();
    fprintf(stderr, "Failed to allocate memory for row2_widths\n");
    exit(EXIT_FAILURE);
  }

  calculate_row2_widths(screen_width, row2_widths);

  create_row2_windows(row2_height, row2_widths, row2_y);

  for (int i = 0; i < PRIMARY_PANE_SLOTS; i++) {
    int module_index =
        selected_modules ? selected_modules[i]
                         : get_module_index_by_name(modules[i].name);
    int module_slot = get_module_array_index_by_dynamic_index(module_index);

    row2_slots[i].module_index = module_index;

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
    if (pthread_create(&row2_slots[i].thread_id, NULL,
                       modules[module_slot].thread_func, arg) != 0) {
      free(arg);
    }
  }

  free(row2_widths);
}

void load_row2_modules(int row2_height, int screen_width, int row2_y) {
  load_row2_modules_with_selection(row2_height, screen_width, row2_y, NULL);
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
  int *row2_widths = malloc(PRIMARY_PANE_SLOTS * sizeof(int));
  TrfxTwoColumnLayoutGeometry layout_geometry;
  if (!row2_widths)
    return;

  calculate_row1_widths(screen_width, row1_widths);
  const int row1_height = SHOW_TOP_PANELS ? FIXED_ROW1_HEIGHT : 0;
  const int row2_height = calculate_row2_height(screen_height);
  const int row2_y = calculate_row2_y();
  trfx_two_column_layout_compute_geometry(&dashboard_layout_state, row2_y, 0,
                                          row2_height, screen_width,
                                          &layout_geometry);
  calculate_row2_widths(layout_geometry.primary_width, row2_widths);

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
  for (int i = 0; i < PRIMARY_PANE_SLOTS; i++) {
    if (row2_slots[i].window) {
      wresize(row2_slots[i].window, row2_height, row2_widths[i]);
      mvwin(row2_slots[i].window, row2_y, x_offset);
      touchwin(row2_slots[i].window);
      wrefresh(row2_slots[i].window);
    }
    x_offset += row2_widths[i];
  }

  if (layout_geometry.secondary_visible && layout_geometry.secondary_width > 0) {
    if (!support_window) {
      support_window = create_bordered_window(
          row2_height, layout_geometry.secondary_width, row2_y,
          layout_geometry.secondary_x, COLOR_BORDER);
    } else {
      wresize(support_window, row2_height, layout_geometry.secondary_width);
      mvwin(support_window, row2_y, layout_geometry.secondary_x);
      touchwin(support_window);
      wrefresh(support_window);
    }
    if (support_window && !support_thread_active)
      start_support_column_thread(support_window);
  } else {
    destroy_support_column();
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
    change_window_module(0);
    break;
  }
    
  case 's':
  case 'S':
    current_sort_type = (current_sort_type + 1) % SORT_MAX;
    break;

  case 'c':
  case 'C':
    change_window_module(0);
    break;

  case 'r':
  case 'R':
    trfx_runtime_request_static_refresh_all();
    if (SHOW_TOP_PANELS)
      refresh_static_windows(sys_win, cpu_win, mem_win, disk_win);
    break;

  case 'j':
  case KEY_DOWN:
    trfx_bandwidth_state_move_focus(1);
    break;

  case 'k':
  case KEY_UP:
    trfx_bandwidth_state_move_focus(-1);
    break;

  case 'J':
    trfx_connection_state_move_focus(1);
    break;

  case 'K':
    trfx_connection_state_move_focus(-1);
    break;

  case 'O':
  case 'o':
    trfx_support_view_set_selected_index(
        trfx_support_view_index_for_id(TRFX_SUPPORT_VIEW_CONNECTION_DETAIL));
    trfx_support_view_request_refresh();
    break;

  case 'd':
  case 'D':
  case KEY_ENTER:
  case 10:
    trfx_support_view_set_selected_index(
        trfx_support_view_index_for_id(TRFX_SUPPORT_VIEW_BANDWIDTH));
    trfx_support_view_request_refresh();
    break;

  case 'x':
  case 'X':
    handle_process_kill_action();
    break;

  case 'z':
  case 'Z':
    handle_connection_drop_action();
    break;

  case 'a':
  case 'A':
    trfx_support_view_set_selected_index(
        trfx_support_view_index_for_id(TRFX_SUPPORT_VIEW_ACTION_AUDIT));
    trfx_support_view_request_refresh();
    break;

  case 'g':
  case 'G':
    trfx_support_view_set_selected_index(
        trfx_support_view_index_for_id(TRFX_SUPPORT_VIEW_DIAGNOSTICS));
    trfx_support_view_request_refresh();
    break;

  case 'n':
  case 'N':
    trfx_support_view_set_selected_index(
        trfx_support_view_index_for_id(TRFX_SUPPORT_VIEW_ROUTE_DNS));
    trfx_support_view_request_refresh();
    break;

  case 'v':
  case 'V':
    trfx_support_view_set_selected_index(
        trfx_support_view_index_for_id(TRFX_SUPPORT_VIEW_NETWORK_HEALTH));
    trfx_support_view_request_refresh();
    break;

  case 'l':
  case 'L':
    trfx_support_view_cycle_selected_index(ch == 'L' ? -1 : 1);
    trfx_support_view_request_refresh();
    break;

  case 'p':
  case 'P':
    pause_screen();
    break;

  case 't':
  case 'T':
    SHOW_TOP_PANELS = !SHOW_TOP_PANELS;
    update_toggle_layout(sys_win, cpu_win, mem_win, disk_win);
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
  trfx_two_column_layout_init(&dashboard_layout_state);

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

  row2_slots = calloc(PRIMARY_PANE_SLOTS, sizeof(WindowSlot));
  if (!row2_slots) {
    endwin();
    fprintf(stderr, "Failed to allocate memory for row2_slots\n");
    exit(EXIT_FAILURE);
  }

  const int row1_height = FIXED_ROW1_HEIGHT;
  const int row2_height = calculate_row2_height(screen_height);
  TrfxTwoColumnLayoutGeometry layout_geometry;

  int row1_widths[ROW1_MODULES] = {0};
  calculate_row1_widths(screen_width, row1_widths);
  int row1_y = 0, row2_y = calculate_row2_y();
  trfx_two_column_layout_compute_geometry(&dashboard_layout_state, row2_y, 0,
                                          row2_height, screen_width,
                                          &layout_geometry);

  WINDOW *sys_win = NULL;
  WINDOW *cpu_win = NULL;
  WINDOW *mem_win = NULL;
  WINDOW *disk_win = NULL;

  sys_win = create_plain_window(row1_height, row1_widths[0], row1_y, 0);
  cpu_win = create_plain_window(row1_height, row1_widths[1], row1_y,
                                row1_widths[0]);
  mem_win = create_plain_window(row1_height, row1_widths[2], row1_y,
                                row1_widths[0] + row1_widths[1]);
  disk_win = create_plain_window(row1_height, row1_widths[3], row1_y,
                                 row1_widths[0] + row1_widths[1] +
                                     row1_widths[2]);

  pthread_t sys_tid, cpu_tid, mem_tid, disk_tid;
  pthread_create(&sys_tid, NULL, system_info_thread, sys_win);
  pthread_create(&cpu_tid, NULL, cpu_info_thread, cpu_win);
  pthread_create(&mem_tid, NULL, memory_info_thread, mem_win);
  pthread_create(&disk_tid, NULL, disk_info_thread, disk_win);

  /*create_row2_windows(row2_height, row2_widths, row2_y);
  for (int i = 0; i < PRIMARY_PANE_SLOTS; i++) {
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
  load_row2_modules(row2_height, layout_geometry.primary_width, row2_y);

  if (layout_geometry.secondary_visible && layout_geometry.secondary_width > 0) {
    support_window = create_bordered_window(
        row2_height, layout_geometry.secondary_width, row2_y,
        layout_geometry.secondary_x, COLOR_BORDER);
    if (support_window)
      start_support_column_thread(support_window);
  }

  sleep(1);
  trfx_runtime_set_ready(1);

  int ch;
  while ((ch = getch()) != 'q' && ch != 'Q') {
    handle_keypress(ch, sys_win, cpu_win, mem_win, disk_win);
  }

  trfx_runtime_request_stop();

  pthread_join(sys_tid, NULL);
  pthread_join(cpu_tid, NULL);
  pthread_join(mem_tid, NULL);
  pthread_join(disk_tid, NULL);
  destroy_support_column();
  cleanup_row2_modules();

  destroy_window(&sys_win);
  destroy_window(&cpu_win);
  destroy_window(&mem_win);
  destroy_window(&disk_win);

  endwin();
}
