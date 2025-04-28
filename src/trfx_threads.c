/*
 * Copyright (C) 2025 Masoud Bolhassani <masoud.bolhassani@gmail.com>
 *
 * This file is part of Trafix.
 *
 * Trafix is released under the GNU General Public License v3 (GPL-3.0).
 * See LICENSE file for details.
 */

#include <unistd.h>
#include <ncurses.h>

#include "trfx_threads.h"
#include "trfx_globals.h"
#include "trfx_sysinfo.h"

void wait_until_ready() {
  pthread_mutex_lock(&ready_mutex); // Lock before checking the ready flag
  while (!ready) {
    pthread_mutex_unlock(&ready_mutex); // Unlock before sleeping
    usleep(10000);                      // Sleep before checking again
    pthread_mutex_lock(&ready_mutex);   // Lock again before checking
  }
  pthread_mutex_unlock(&ready_mutex); // Unlock after we're done
}

void *system_info_thread(void *arg) {
  WINDOW *win = (WINDOW *)arg;
  wait_until_ready();

  while (1) {
    if (screen_paused) {
      usleep(100000);
      continue;
    }

    int row = 1;
    int line = 2;
    int label_width = 16;
    SystemOverview sysinfo = get_system_overview();

    pthread_mutex_lock(&ncurses_mutex);

    werase(win);

    wattron(win, COLOR_PAIR(COLOR_BORDER));
    box(win, 0, 0);
    wattroff(win, COLOR_PAIR(COLOR_BORDER));

    mvwprintw(win, row++, line, "%*s: %s", label_width, "Hostname",
              sysinfo.hostname);
    mvwprintw(win, row++, line, "%*s: %s", label_width, "OS",
              sysinfo.os_version);
    mvwprintw(win, row++, line, "%*s: %s", label_width, "Kernel",
              sysinfo.kernel_version);
    mvwprintw(win, row++, line, "%*s: %s", label_width, "Uptime",
              sysinfo.uptime);
    mvwprintw(win, row++, line, "%*s: %s", label_width, "Load Avg",
              sysinfo.load_avg);
    mvwprintw(win, row++, line, "%*s: %s", label_width, "Logged-in Users",
              sysinfo.logged_in_users);

    wrefresh(win);

    pthread_mutex_unlock(&ncurses_mutex);

    for (int i = 0; i < 50; i++) {
      if (force_refresh_flags[STATIC_MODULE_SYSINFO]) {
        force_refresh_flags[STATIC_MODULE_SYSINFO] = 0;
        break;
      }
      sleep(1);
    }
  }
  return NULL;
}
