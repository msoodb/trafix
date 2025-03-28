#include <ncurses.h>
#include <unistd.h>
#include <stdlib.h>
#include "trfx_connections.h"
#include "trfx_bandwidth.h"
#include "trfx_top.h"
#include "trfx_cpu.h"
#include "trfx_disk.h"

#define COLOR_TITLE 1
#define COLOR_SECTION 2
#define COLOR_DATA 3

#define ROWS 2
#define COLS 3


// Function to refresh and display active connections in the window
void display_active_connections(WINDOW *win) {
    int num_connections = 0;
    
    // Get the active connections (dynamically allocated)
    char **active_connections = get_active_connections(&num_connections);

    // Clear previous content
    werase(win);  // Clears the window content without affecting the border

    // Draw the border around the window
    box(win, 0, 0);  

    // Display the headers
    attron(A_BOLD);
    mvwprintw(win, 1, 2, "%-20s -> %-20s  %s", "Source IP:Port", "Dest IP:Port", "Status");
    attroff(A_BOLD);
    mvwprintw(win, 2, 2, "------------------------------------------------------------");

    // Display the active connections
    for (int i = 0; i < num_connections; i++) {
        mvwprintw(win, 3 + i, 2, "%s", active_connections[i]);
    }

    // Refresh the window to show updates
    wrefresh(win);

    // Free the memory after use
    free_active_connections(active_connections, num_connections);
}


// Function to display bandwidth usage in the given window
void display_bandwidth_usage(WINDOW *win) {
    int num_interfaces = 0;

    // Get the bandwidth usage (dynamically allocated)
    char **bandwidth_usage = get_bandwidth_usage(&num_interfaces);

    // Clear previous content
    werase(win);  // Clears the window content without affecting the border
    
    int row = 1;

    // Draw the border around the window
    box(win, 0, 0);

    // Title line (bold)
    attron(A_BOLD);
    mvwprintw(win, row++, 2, "%-15s | %10s | %10s", "Interface", "Sent (MB)", "Received (MB)");
    attroff(A_BOLD);

    // Divider line
    mvwprintw(win, row++, 1, "---------------------------------------------");

    // Display the bandwidth usage
    for (int i = 0; i < num_interfaces; i++) {
        mvwprintw(win, row++, 1, "%s", bandwidth_usage[i]);
    }

    wrefresh(win);  // Refresh the window to show updates

    // Free the dynamically allocated bandwidth data
    free_bandwidth_usage(bandwidth_usage, num_interfaces);
}

#include <unistd.h>

// Function to display CPU usage per core in the given window
void display_cpu_data(WINDOW *win) {
    int num_cores = 0;

    // Get CPU usage for each core
    char **cpu_data = get_cpu_usage(&num_cores);

    int row = 1;

    // Draw the border around the window
    box(win, 0, 0);

    // Title line (bold)
    attron(A_BOLD);
    mvwprintw(win, row++, 2, "%-15s | %10s", "Core", "CPU Usage (%)");
    attroff(A_BOLD);

    // Divider line
    mvwprintw(win, row++, 1, "---------------------------------------------");

    // Display the CPU usage per core
    for (int i = 0; i < num_cores; i++) {
      mvwprintw(win, row++, 1, "%s", cpu_data[i]);
    }

    // Refresh the window to show updates
    wrefresh(win);

    // Free the memory for CPU data
    for (int i = 0; i < num_cores; i++) {
        free(cpu_data[i]);
    }
    free(cpu_data);

    // Delay to update every second or as needed
    napms(1000);  // Delay in milliseconds, e.g., 1000 ms = 1 second
}

// Function to display disk usage per partition in the given window
void display_disk_data(WINDOW *win) {
    int num_partitions = 0;
    
    // Get disk usage for each partition
    char **disk_data = get_disk_usage(&num_partitions);
    
    int row = 1;
    
    // Draw the border around the window
    box(win, 0, 0);
    
    // Title line (bold)
    attron(A_BOLD);
    mvwprintw(win, row++, 2, "%-15s | %10s", "Partition", "Disk Usage (%)");
    attroff(A_BOLD);
    
    // Divider line
    mvwprintw(win, row++, 1, "---------------------------------------------");
    
    // Display the disk usage per partition
    for (int i = 0; i < num_partitions; i++) {
        mvwprintw(win, row++, 1, "%s", disk_data[i]);
    }
    
    // Refresh the window to show updates
    wrefresh(win);
    
    // Free the memory for disk data
    for (int i = 0; i < num_partitions; i++) {
        free(disk_data[i]);
    }
    free(disk_data);
    
    // Delay to update every second or as needed
    napms(1000);  // Delay in milliseconds, e.g., 1000 ms = 1 second
}

void zoom_section(WINDOW *win) {
    int screen_width, screen_height;
    getmaxyx(stdscr, screen_height, screen_width);

    // Create a full-screen window for the zoomed section
    WINDOW *zoom_win = newwin(screen_height, screen_width, 0, 0);
    box(zoom_win, 0, 0);  // Draw the border for the zoomed window
    mvwprintw(zoom_win, 1, 2, "[ ZOOM MODE ] Press Ctrl+R to return");

    // Copy contents of the original section window into the zoomed window
    overwrite(win, zoom_win);

    // Set nodelay on the zoom window to avoid blocking
    nodelay(zoom_win, TRUE);

    // Refresh zoomed window to make sure it's updated
    wrefresh(zoom_win);

    int ch;
    while (1) {
        ch = getch();

        // Exit zoom mode when Ctrl+R (18) is pressed
        if (ch == 18) {
            break;
        }

        // Refresh the zoomed window to keep it updated
        wrefresh(zoom_win);
    }

    // Clean up zoomed window and return to the main dashboard
    delwin(zoom_win);
}

void start_dashboard() {
    initscr();
    start_color();
    use_default_colors();
    init_pair(COLOR_TITLE, COLOR_CYAN, -1);
    init_pair(COLOR_SECTION, COLOR_YELLOW, -1);
    init_pair(COLOR_DATA, COLOR_WHITE, -1);
    noecho();
    curs_set(FALSE);

    int screen_height, screen_width;
    getmaxyx(stdscr, screen_height, screen_width);

    int box_height = screen_height / ROWS;
    int box_width = screen_width / COLS;

    WINDOW *sections[ROWS][COLS];

    // Initialize windows and draw borders immediately
    for (int i = 0; i < ROWS; i++) {
        for (int j = 0; j < COLS; j++) {
            sections[i][j] = newwin(box_height, box_width, i * box_height, j * box_width);
            box(sections[i][j], 0, 0);  // Draw border around each section
        }
    }

    // Refresh the screen after drawing the borders
    refresh();
    for (int i = 0; i < ROWS; i++) {
        for (int j = 0; j < COLS; j++) {
            wrefresh(sections[i][j]);   // Ensure the borders are visible
        }
    }

    // Set nodelay to non-blocking for main window and sections
    nodelay(stdscr, TRUE);
    for (int i = 0; i < ROWS; i++) {
        for (int j = 0; j < COLS; j++) {
            nodelay(sections[i][j], TRUE);
        }
    }

    int ch;
    while (1) {
        // Title
        attron(COLOR_PAIR(COLOR_TITLE) | A_BOLD);
        //mvprintw(0, (screen_width - 40) / 2, "=== Trafix Network Monitoring Dashboard ===");
        attroff(COLOR_PAIR(COLOR_TITLE) | A_BOLD);

	display_active_connections(sections[0][0]);  // Active connections
	display_bandwidth_usage(sections[0][1]);  // Bandwidth usage
	display_cpu_data(sections[0][2]);
	display_disk_data(sections[1][0]);
	
        // Refresh each window to display content
        for (int i = 0; i < ROWS; i++) {
            for (int j = 0; j < COLS; j++) {
                wrefresh(sections[i][j]);
            }
        }

        refresh();

        // Get user input with non-blocking
        ch = getch();

        // Check for zoom keys (1-6 for zoom in, Ctrl+R for zoom out)
        if (ch == '1') zoom_section(sections[0][0]); // Zoom Section 1
        if (ch == '2') zoom_section(sections[0][1]); // Zoom Section 2
        if (ch == '3') zoom_section(sections[0][2]); // Zoom Section 3
        if (ch == '4') zoom_section(sections[1][0]); // Zoom Section 4
        if (ch == '5') zoom_section(sections[1][1]); // Zoom Section 5
        if (ch == '6') zoom_section(sections[1][2]); // Zoom Section 6

        // Check for quit
        if (ch == 'q' || ch == 'Q') break;

        // Small delay to prevent 100% CPU usage
        usleep(100000);
    }

    // Cleanup
    for (int i = 0; i < ROWS; i++) {
        for (int j = 0; j < COLS; j++) {
            delwin(sections[i][j]);
        }
    }
    endwin();
}
