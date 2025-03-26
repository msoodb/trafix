#include <ncurses.h>
#include "trfx_top.h"

void print_top_talkers(WINDOW *win) {
    int row = 1;
    
    // Title line (bold)
    attron(A_BOLD);
    mvwprintw(win, row++, 1, "%-20s | %15s", "IP Address", "Data Transferred (MB)");
    attroff(A_BOLD);

    // Divider line
    mvwprintw(win, row++, 1, "----------------------------------------------");

    // Example top talker data
    mvwprintw(win, row++, 1, "%-20s | %15.1f", "192.168.1.101", 350.7);
    mvwprintw(win, row++, 1, "%-20s | %15.1f", "192.168.1.102", 290.4);
    mvwprintw(win, row++, 1, "%-20s | %15.1f", "192.168.1.103", 125.2);

    wrefresh(win);  // Refresh the window to show updates
}
