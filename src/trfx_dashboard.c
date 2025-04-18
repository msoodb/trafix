#include <ncurses.h>
#include <unistd.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include "trfx_connections.h"
#include "trfx_bandwidth.h"
#include "trfx_top.h"
#include "trfx_cpu.h"
#include "trfx_disk.h"
#include "trfx_wifi.h"
#include "trfx_sysinfo.h"
#include "trfx_meminfo.h"
#include "trfx_procinfo.h"

#define COLOR_TITLE 1
#define COLOR_SECTION 2
#define COLOR_DATA 3
#define COLOR_DATA_RED 4

#define ROWS 2
#define COLS 3

#define ROW1_COLS 4
#define ROW2_COLS 3
#define ROW3_COLS 1
#define TOTAL_ROWS 3

// Function to refresh and display active connections in the window
void display_active_connections(WINDOW *win) {
    int num_connections = 0;
    
    // Get the active connections (dynamically allocated)
    char **active_connections = get_active_connections(&num_connections);

    // Clear previous content
    werase(win);  // Clears the window content without affecting the border

    // Display the headers
    attron(A_BOLD);
    mvwprintw(win, 1, 4, "%-20s -> %-20s  %s", "Source IP:Port", "Dest IP:Port", "Status");
    attroff(A_BOLD);
    mvwprintw(win, 2, 4, "------------------------------------------------------------");

    // Display the active connections
    for (int i = 0; i < num_connections; i++) {
        mvwprintw(win, 3 + i, 4, "%s", active_connections[i]);
    }

    // Refresh the window to show updates
    wrefresh(win);

    // Free the memory after use
    free_active_connections(active_connections, num_connections);
}



// Function to display CPU usage per core in the given window
void display_cpu_data(WINDOW *win) {
  int num_cores = 0;

  // Get CPU usage for each core
  char **cpu_data = get_cpu_usage(&num_cores);

  int row = 1;

  // Title line (bold)
  attron(A_BOLD);
  mvwprintw(win, row++, 2, "%-8s | %10s", "Core", "CPU Usage (%)");
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
  napms(1000); // Delay in milliseconds, e.g., 1000 ms = 1 second
}

// Function to display system info in the given window
void display_system_info(WINDOW *win) {
    int row = 1;
    SystemOverview sysinfo = get_system_overview();

    mvwprintw(win, row++, 2, "  Hostname: %s", sysinfo.hostname);
    mvwprintw(win, row++, 2, "  OS: %s", sysinfo.os_version);
    mvwprintw(win, row++, 2, "  Kernel: %s", sysinfo.kernel_version);
    mvwprintw(win, row++, 2, "  Uptime: %s", sysinfo.uptime);
    mvwprintw(win, row++, 2, "  Load Avg: %s", sysinfo.load_avg);
    mvwprintw(win, row++, 2, "  Logged-in Users: %s", sysinfo.logged_in_users);

    wrefresh(win);
    napms(1000);
}

void display_process_info(WINDOW *win) {
ProcessInfo list[MAX_PROCESSES];
    int count = get_top_processes_by_mem(list, MAX_PROCESSES);
    int row = 1;

    wattron(win, COLOR_PAIR(1));
    mvwprintw(win, row++, 1, "  PID     USER     PR NI     VIRT    RES    SHR S %%CPU %%MEM     TIME+  COMMAND");
    wattroff(win, COLOR_PAIR(1));

    for (int i = 0; i < count && row < getmaxy(win) - 1; i++) {
        mvwprintw(win, row++, 1, "%6s %-8s %2s %2s %7s %6s %6s %s %5s %5s %10s %.20s",
                  list[i].pid, list[i].user, list[i].pr, list[i].ni,
                  list[i].virt, list[i].res, list[i].shr, list[i].state,
                  list[i].cpu, list[i].mem, list[i].time, list[i].command);
    }

    wrefresh(win);
    napms(1000);
}

void display_memory_info(WINDOW *win) {
    int row = 1;
    MemoryInfo mem = get_memory_info();

    float total = mem.total_ram / 1024.0f;
    float free = mem.free_ram / 1024.0f;
    float used = mem.used_ram / 1024.0f;
    float swap_used = mem.used_swap / 1024.0f;
    float swap_total = mem.total_swap / 1024.0f;

    // Memory line
    mvwprintw(win, row++, 2, "Memory (MiB) > total: %.1f,  free: %.1f,  used: %.1f  (%.1f%%)",
              total, free, used, mem.mem_percent);

    // Swap line
    mvwprintw(win, row++, 2, "Swap   (MiB) > total: %.1f,  used: %.1f", swap_total, swap_used);

    // Top memory-consuming processes
    row++;
    mvwprintw(win, row++, 2, "Top Memory Processes:");
    char *line = strtok(mem.top_processes, "\n");
    while (line && row < getmaxy(win) - 1) {
        mvwprintw(win, row++, 4, "%s", line);
        line = strtok(NULL, "\n");
    }

    wrefresh(win);
    napms(1000);
}

// Function to display bandwidth usage in the given window
void display_bandwidth_usage(WINDOW *win) {
    int num_interfaces = 0;
    int max_rows, max_cols;
    getmaxyx(win, max_rows, max_cols);
    (void)max_cols;


    char **bandwidth_usage = get_bandwidth_usage(&num_interfaces);

    werase(win);

    int row = 1;

    // Detect default interface with IP
    const char *connected_if = NULL;
    const char *ssid = NULL;
    char *ip = NULL;

    for (int i = 0; i < num_interfaces; i++) {
      char name[32];
      sscanf(bandwidth_usage[i], " %31s", name);

      // Skip loopback (lo)
      if (strcmp(name, "lo") == 0 || strncmp(name, "br-", 3) == 0 ||
          strncmp(name, "docker", 6) == 0 || strncmp(name, "veth", 4) == 0 ||
          strncmp(name, "virbr", 5) == 0 || strncmp(name, "vmnet", 5) == 0) {
        continue; // Skip virtual interfaces
      }

      ip = get_ip_address(name);
      if (ip) {
        connected_if = name;
        ssid = get_wifi_ssid(name);
        break;
      }
    }

    const char *vpn_if = NULL;
    const char *vpn_ip = NULL;

    for (int i = 0; i < num_interfaces; i++) {
      char name[32];
      sscanf(bandwidth_usage[i], " %31s", name);

      if (is_vpn_interface(name)) {
        char *vip = get_ip_address(name);
        if (vip) {
          vpn_if = name;
          vpn_ip = vip;
          break;
        }
      }
    }

    // Gateway and DNS Info
    //const char *gateway_ip = get_gateway_ip();
    const char *dns = get_dns_servers();
    char gateway[64] = {0};
    char metric[64] = {0};
    //char routing_table[1024] = {0};

    get_default_gateway_and_metric(gateway, metric);

    //mvwprintw(win, row++, 2, "Gateway IP: %s", gateway_ip);
    mvwprintw(win, row++, 2, "Default Gateway: %s | Metric: %s", gateway, metric);
    mvwprintw(win, row++, 2, "DNS Servers: %s", dns);
    row++;

    // Connection info header
    wattron(win, A_BOLD);
    if (connected_if) {
      const char *type = is_wifi_interface(connected_if) ? "Wi-Fi" : "Ethernet";
      if (is_wifi_interface(connected_if) && ssid)
        mvwprintw(win, row++, 2, "> Connected: %s (%s: %s)  |  IP: %s",
                  connected_if, type, ssid, ip);
      else
        mvwprintw(win, row++, 2, "> Connected: %s (%s)       |  IP: %s",
                  connected_if, type, ip);
    } else {
      mvwprintw(win, row++, 2, "No active network connection detected.");
    }
    wattroff(win, A_BOLD);

    if (vpn_if && vpn_ip) {
      wattron(win, A_BOLD);
      wattron(win, COLOR_PAIR(COLOR_DATA_RED)); // Optional: color VPN line
      mvwprintw(win, row++, 2, "VPN Active: %s  |  IP: %s", vpn_if, vpn_ip);
      wattroff(win, COLOR_PAIR(COLOR_DATA));
      wattron(win, A_BOLD);
    }
    row++;

    char *mac = get_mac_address(connected_if);
    WifiInfo wifi = get_wifi_info(connected_if);

    if (ssid) {
      mvwprintw(win, row++, 2, "  Wi-Fi: %s", ssid);
      mvwprintw(win, row++, 2, "  Signal Strength: %s dBm", wifi.signal_strength);
      mvwprintw(win, row++, 2, "  Bitrate: %s", wifi.bitrate);
      mvwprintw(win, row++, 2, "  Frequency: %s MHz", wifi.freq);
      mvwprintw(win, row++, 2, "  MAC Address: %s", mac);
    }

    row++;
    
    // Table headers
    wattron(win, A_BOLD);
    mvwprintw(win, row++, 2, "%-15s | %10s | %10s", "Interface", "Sent",
              "Received");
    wattroff(win, A_BOLD);
    mvwprintw(win, row++, 1, "---------------------------------------------");

    for (int i = 0; i < num_interfaces && row < max_rows - 1; i++) {
      mvwprintw(win, row++, 1, "%s", bandwidth_usage[i]);
    }
    
    wrefresh(win);
    free_bandwidth_usage(bandwidth_usage, num_interfaces);
}

// Function to display disk usage per partition in the given window
void display_disk_data(WINDOW *win) {
  int num_partitions = 0;

  // Get disk usage for each partition
  char **disk_data = get_disk_usage(&num_partitions);

  int row = 1;

  // Title line (bold)
  attron(A_BOLD);
  mvwprintw(win, row++, 2, "%-16s | %10s", "Partition", "Disk Usage (%)");
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
  napms(1000); // Delay in milliseconds, e.g., 1000 ms = 1 second
}

void zoom_section(WINDOW *win) {
  int screen_width, screen_height;
  getmaxyx(stdscr, screen_height, screen_width);

  // Create a full-screen window for the zoomed section
  WINDOW *zoom_win = newwin(screen_height, screen_width, 0, 0);
  box(zoom_win, 0, 0); // Draw the border for the zoomed window
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
  init_pair(COLOR_DATA_RED, COLOR_RED, -1);
  init_pair(1, COLOR_BLACK, COLOR_WHITE);
  noecho();
  curs_set(FALSE);

  int screen_height, screen_width;
  getmaxyx(stdscr, screen_height, screen_width);

  int row_heights[TOTAL_ROWS] = {
    screen_height / 3,
    screen_height / 3,
    screen_height - 2 * (screen_height / 3)
  };

  int col_counts[TOTAL_ROWS] = { ROW1_COLS, ROW2_COLS, ROW3_COLS };

  WINDOW *sections[TOTAL_ROWS][ROW1_COLS];  // Max columns in any row = 4

  int y_offset = 0;
  for (int row = 0; row < TOTAL_ROWS; row++) {
    int col_width = screen_width / col_counts[row];
    for (int col = 0; col < col_counts[row]; col++) {
      sections[row][col] = newwin(
        row_heights[row],
        col_width,
        y_offset,
        col * col_width
      );
      nodelay(sections[row][col], TRUE);
    }
    y_offset += row_heights[row];
  }

  nodelay(stdscr, TRUE);

  int ch;
  while (1) {
    // You can customize which data goes where
    display_bandwidth_usage(sections[1][1]);
    display_cpu_data(sections[0][1]);
    display_active_connections(sections[0][2]);
    display_disk_data(sections[0][3]);


    display_process_info(sections[1][0]);
    display_system_info(sections[0][0]);
    //display_process_info(sections[1][1]);
    display_memory_info(sections[1][2]);

    

    // Refresh all windows
    for (int row = 0; row < TOTAL_ROWS; row++) {
      for (int col = 0; col < col_counts[row]; col++) {
        wrefresh(sections[row][col]);
      }
    }

    ch = getch();
    if (ch == '1') zoom_section(sections[0][0]);
    if (ch == '2') zoom_section(sections[0][1]);
    if (ch == '3') zoom_section(sections[0][2]);
    if (ch == '4') zoom_section(sections[0][3]);
    if (ch == '5') zoom_section(sections[1][0]);
    if (ch == '6') zoom_section(sections[1][1]);
    if (ch == '7') zoom_section(sections[1][2]);
    if (ch == '8') zoom_section(sections[2][0]);
    if (ch == 'q' || ch == 'Q') break;

    usleep(100000);
  }

  // Cleanup
  for (int row = 0; row < TOTAL_ROWS; row++) {
    for (int col = 0; col < col_counts[row]; col++) {
      delwin(sections[row][col]);
    }
  }

  endwin();
}
