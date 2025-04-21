#include <ncurses.h>
#include <unistd.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <pthread.h>
#include "trfx_connections.h"
#include "trfx_utils.h"
#include "trfx_bandwidth.h"
#include "trfx_top.h"
#include "trfx_cpu.h"
#include "trfx_disk.h"
#include "trfx_wifi.h"
#include "trfx_sysinfo.h"
#include "trfx_meminfo.h"
#include "trfx_procinfo.h"

#define COLOR_TITLE    1
#define COLOR_SECTION  2
#define COLOR_DATA     3
#define COLOR_DATA_RED 4
#define COLOR_HEADER   5

#define TOTAL_ROWS 3
#define ROW1_COLS 4
#define ROW2_COLS 4
#define ROW3_COLS 2

#define FIXED_ROW1_HEIGHT 6
#define FIXED_ROW2_HEIGHT 0

#define MAX_DISKS 16

volatile int ready = 0;
pthread_mutex_t ready_mutex = PTHREAD_MUTEX_INITIALIZER;

void wait_until_ready() {
    while (!ready) usleep(10000);
}

void *system_info_thread(void *arg) {
  WINDOW *win = (WINDOW *)arg;
  wait_until_ready();

  while (1) {
    int row = 0;
    int line = 0;
    int label_width = 16;
    SystemOverview sysinfo = get_system_overview();

    werase(win);
    mvwprintw(win, row++, line, "%*s: %s", label_width, "Hostname",    sysinfo.hostname);
    mvwprintw(win, row++, line, "%*s: %s", label_width, "OS",          sysinfo.os_version);
    mvwprintw(win, row++, line, "%*s: %s", label_width, "Kernel",           sysinfo.kernel_version);
    mvwprintw(win, row++, line, "%*s: %s", label_width, "Uptime",           sysinfo.uptime);
    mvwprintw(win, row++, line, "%*s: %s", label_width, "Load Avg",         sysinfo.load_avg);
    mvwprintw(win, row++, line, "%*s: %s", label_width, "Logged-in Users",  sysinfo.logged_in_users);

    wrefresh(win);
    sleep(5);
  }
  return NULL;
}

void *cpu_info_thread(void *arg) {
  WINDOW *win = (WINDOW *)arg;
  wait_until_ready();

  while (1) {
    int row = 0;
    int line = 1;
    CPUInfo cpu = get_cpu_info();

    werase(win);
    wattron(win, A_BOLD);
    mvwprintw(win, row++, 0, "CPU Information");
    wattroff(win, A_BOLD);

    mvwprintw(win, row++, line, "Avg Usage: %.1f%%   Temperature: %.1f °C",
              cpu.avg_usage, cpu.temperature >= 0 ? cpu.temperature : 0.0);

    int coreCount = 0;
    for (int i = 0; i < cpu.num_cores; i++) {
      mvwprintw(win, row, line + coreCount * 20, "C%d: %04.1f%%, %.0f MHz", i,
                cpu.usage_per_core[i], cpu.frequency_per_core[i]);
      coreCount++;

      if (coreCount == 2) {
        coreCount = 0;
        row++;
      }
    }
    wrefresh(win);
    sleep(1);
  }
  return NULL;
}

void *memory_info_thread(void *arg) {
  WINDOW *win = (WINDOW *)arg;
  wait_until_ready();

  while (1) {
    int row = 1;
    MemoryInfo mem = get_memory_info();

    werase(win);
    float total = mem.total_ram / 1024.0f;
    float free = mem.free_ram / 1024.0f;
    float used = mem.used_ram / 1024.0f;
    float swap_used = mem.used_swap / 1024.0f;
    float swap_total = mem.total_swap / 1024.0f;

    mvwprintw(win, row++, 2,
              "Memory (MiB)> total: %.1f, free: %.1f, used: %.1f (%.1f%%)",
              total, free, used, mem.mem_percent);
    mvwprintw(win, row++, 2, "  Swap (MiB)> total: %.1f, used: %.1f",
              swap_total, swap_used);

    wrefresh(win);
    sleep(2);
  }
  return NULL;
}

void *process_info_thread(void *arg) {
  WINDOW *win = (WINDOW *)arg;
  wait_until_ready();

  while (1) {
    ProcessInfo list[MAX_PROCESSES];
    int count = get_top_processes_by_mem(list, MAX_PROCESSES);
    int row = 1;

    werase(win);
    wattron(win, COLOR_PAIR(COLOR_HEADER));
    mvwprintw(
        win, row++, 1,
        "  PID    USER      PR  NI    VIRT    RES      SHR S   %%CPU %%MEM   "
        "TIME+     COMMAND               ");
    wattroff(win, COLOR_PAIR(COLOR_HEADER));

    for (int i = 0; i < count && row < getmaxy(win) - 1; i++) {
      mvwprintw(win, row++, 1,
                "%7s %-10s %2s %2s %8s %7s %7s %1s %5s %5s %10s %-20.20s",
                list[i].pid,    // right-aligned numeric
                list[i].user,   // left-aligned string
                list[i].pr,     // right-aligned numeric
                list[i].ni,     // right-aligned numeric
                list[i].virt,   // right-aligned numeric
                list[i].res,    // right-aligned numeric
                list[i].shr,    // right-aligned numeric
                list[i].state,  // 1 char
                list[i].cpu,    // right-aligned percentage
                list[i].mem,    // right-aligned percentage
                list[i].time,   // fixed width time
                list[i].command // left-aligned string, max 20 chars
      );
    }

    wrefresh(win);
    sleep(2);
  }

  return NULL;
}

void *disk_info_thread(void *arg) {
  WINDOW *win = (WINDOW *)arg;
  wait_until_ready();

  while (1) {
    DiskInfo disks[MAX_DISKS];
    double total_used = 0.0, total_total = 0.0;
    int ndisk = get_disk_info(disks, MAX_DISKS, &total_used, &total_total);

    werase(win);

    int row = 1;
    wattron(win, COLOR_PAIR(COLOR_HEADER));
    mvwprintw(
        win, row++, 1,
        "Mount      Filesystem              Used     Total    Usage   Temp");
    wattroff(win, COLOR_PAIR(COLOR_HEADER));

    for (int i = 0; i < ndisk && row < getmaxy(win) - 2; ++i) {
      char used_buf[16], total_buf[16];
      format_bytes(disks[i].used_mb, used_buf, sizeof(used_buf));
      format_bytes(disks[i].total_mb, total_buf, sizeof(total_buf));

      mvwprintw(win, row++, 1, "%-10.10s %-20.20s %7s %8s  %5.1f%%  %5.1f°C",
                disks[i].mount_point, disks[i].filesystem, used_buf, total_buf,
                disks[i].usage_percent, disks[i].temperature);
    }

    // Print totals
    double usage_percent =
        (total_total > 0) ? (total_used / total_total) * 100.0 : 0.0;
    char used_buf[16], total_buf[16];
    format_bytes(total_used, used_buf, sizeof(used_buf));
    format_bytes(total_total, total_buf, sizeof(total_buf));

    mvwprintw(win, row++, 1, "%-10s %-20s %7s %8s  %5.1f%%  %s", "Total", "-",
              used_buf, total_buf, usage_percent, "- °C");

    wrefresh(win);
    sleep(3);
  }

  return NULL;
}

void *connection_info_thread(void *arg) {
  WINDOW *win = (WINDOW *)arg;
  wait_until_ready();

  while (1) {
    ConnectionInfo connections[MAX_CONNECTIONS];
    int nconn = get_connection_info(connections, MAX_CONNECTIONS);

    werase(win);
    wattron(win, COLOR_PAIR(COLOR_HEADER));
    mvwprintw(win, 1, 2, "Active Connections");
    mvwprintw(win, 2, 2, "%-6s %-22s %-22s %-15s", "Proto", "Local Address",
              "Remote Address", "State");
    wattroff(win, COLOR_PAIR(COLOR_HEADER));

    int y = 3;
    for (int i = 0; i < nconn && y < getmaxy(win) - 1; ++i) {
      mvwprintw(win, y++, 2, "%-6s %-22s %-22s %-15s", connections[i].protocol,
                connections[i].local_addr, connections[i].remote_addr,
                connections[i].state);
    }

    wrefresh(win);
    sleep(3);
  }

  return NULL;
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

    wattron(win, A_BOLD);
    mvwprintw(win, row++, 0, "Network Information");
    wattroff(win, A_BOLD);

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

/*void start_dashboard() {
  initscr();
  start_color();
  use_default_colors();
  init_pair(COLOR_HEADER, COLOR_BLACK, COLOR_WHITE);  // black text on white
  background init_pair(COLOR_TITLE, COLOR_CYAN, -1); init_pair(COLOR_SECTION,
  COLOR_YELLOW, -1); init_pair(COLOR_DATA, COLOR_WHITE, -1);
  init_pair(COLOR_DATA_RED, COLOR_RED, -1);
  noecho();
  curs_set(FALSE);

  int screen_height, screen_width;
  getmaxyx(stdscr, screen_height, screen_width);

  int row_heights[TOTAL_ROWS] = {
    FIXED_ROW1_HEIGHT,
    FIXED_ROW2_HEIGHT,
    screen_height - FIXED_ROW1_HEIGHT - FIXED_ROW2_HEIGHT
  };

  int col_counts[TOTAL_ROWS] = { ROW1_COLS, ROW2_COLS, ROW3_COLS };

  WINDOW *sections[TOTAL_ROWS][ROW1_COLS];  // Max columns = 4

  // Define specific widths for each row's columns
  int row1_widths[ROW1_COLS] = {
    (int)(screen_width * 0.25),
    (int)(screen_width * 0.20),
    (int)(screen_width * 0.50),
    (int)(screen_width * 0.05)
  };
  int row2_widths[ROW2_COLS] = {
    screen_width / 3,
    screen_width / 3,
    screen_width - 2 * (screen_width / 3)
  };
  int row3_widths[ROW3_COLS] = {
    (int)(screen_width * 0.50),
    (int)(screen_width * 0.50)
  };

 int* widths[] = { row1_widths, row2_widths, row3_widths };

  int y_offset = 0;
  for (int row = 0; row < TOTAL_ROWS; row++) {
    int x_offset = 0;
    for (int col = 0; col < col_counts[row]; col++) {
      sections[row][col] = newwin(
        row_heights[row],
        widths[row][col],
        y_offset,
        x_offset
      );
      nodelay(sections[row][col], TRUE);
      x_offset += widths[row][col];
    }
    y_offset += row_heights[row];
  }

  nodelay(stdscr, TRUE);

  int ch;
  while (1) {
    // Displaying dashboard content
    display_system_info(sections[0][0]);
    display_cpu_info(sections[0][1]);
    //display_memory_info(sections[0][2]);
    display_disk_data(sections[0][2]);

    //display_bandwidth_usage(sections[0][0]);
    //display_active_connections(sections[0][2]);
    //display_disk_data(sections[0][3]);


    display_process_info(sections[2][0]);
    display_bandwidth_usage(sections[2][1]);

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
  }*/

void start_dashboard() {
  initscr();
  noecho();
  curs_set(FALSE);
  start_color();
  use_default_colors();
  init_pair(COLOR_HEADER, COLOR_CYAN, -1);

  int height = LINES / 2;
  int width = COLS / 3;

  WINDOW *sys_win = newwin(height, width, 0, 0);
  WINDOW *cpu_win = newwin(height, width, 0, width);
  WINDOW *mem_win = newwin(height, width, 0, 2 * width);
  WINDOW *disk_win = newwin(height, width, height, 0);
  WINDOW *proc_win = newwin(height, width, height, width);
  WINDOW *conn_win = newwin(height, width, height, 2 * width);

  pthread_t sys_tid, cpu_tid, mem_tid, disk_tid, proc_tid, conn_tid;
  pthread_create(&sys_tid, NULL, system_info_thread, sys_win);
  pthread_create(&cpu_tid, NULL, cpu_info_thread, cpu_win);
  pthread_create(&mem_tid, NULL, memory_info_thread, mem_win);
  pthread_create(&disk_tid, NULL, disk_info_thread, disk_win);
  pthread_create(&proc_tid, NULL, process_info_thread, proc_win);
  pthread_create(&conn_tid, NULL, connection_info_thread, conn_win);

  sleep(1);
  ready = 1;

  getch();
  endwin();
}
