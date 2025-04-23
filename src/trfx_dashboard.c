#include <ncurses.h>
#include <unistd.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <pthread.h>
#include "trfx_connections.h"
#include "trfx_utils.h"
#include "trfx_netinfo.h"
#include "trfx_cpu.h"
#include "trfx_disk.h"
#include "trfx_wifi.h"
#include "trfx_sysinfo.h"
#include "trfx_meminfo.h"
#include "trfx_procinfo.h"

#define COLOR_TITLE       1
#define COLOR_SECTION     2
#define COLOR_DATA        3
#define COLOR_DATA_RED    4
#define COLOR_DATA_YELLOW 5
#define COLOR_DATA_GREEN  6
#define COLOR_HEADER      7
#define COLOR_BORDER 8

#define TOTAL_ROWS 3
#define ROW1_COLS 4
#define ROW2_COLS 4
#define ROW3_COLS 2

#define FIXED_ROW1_HEIGHT 6
#define FIXED_ROW2_HEIGHT 0

#define MAX_DISKS 16

#define KPI_USAGE_WARN 70.0
#define KPI_USAGE_CRIT 90.0
#define BAR_WIDTH 16

volatile int ready = 0;
pthread_mutex_t ready_mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t ncurses_mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t global_var_mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t memory_info_mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t disk_info_mutex = PTHREAD_MUTEX_INITIALIZER;

void wait_until_ready() {
    pthread_mutex_lock(&ready_mutex);  // Lock before checking the ready flag
    while (!ready) {
        pthread_mutex_unlock(&ready_mutex);  // Unlock before sleeping
        usleep(10000);                        // Sleep before checking again
        pthread_mutex_lock(&ready_mutex);     // Lock again before checking
    }
    pthread_mutex_unlock(&ready_mutex);  // Unlock after we're done
}

void *system_info_thread(void *arg) {
  WINDOW *win = (WINDOW *)arg;
  wait_until_ready();

  while (1) {
    int row = 1;
    int line = 2;
    int label_width = 16;
    SystemOverview sysinfo = get_system_overview();

    pthread_mutex_lock(&ncurses_mutex);
	
    werase(win);

    wattron(win, COLOR_PAIR(COLOR_BORDER));
    box(win, 0, 0);
    wattroff(win, COLOR_PAIR(COLOR_BORDER));
 
    mvwprintw(win, row++, line, "%*s: %s", label_width, "Hostname",    sysinfo.hostname);
    mvwprintw(win, row++, line, "%*s: %s", label_width, "OS",          sysinfo.os_version);
    mvwprintw(win, row++, line, "%*s: %s", label_width, "Kernel",           sysinfo.kernel_version);
    mvwprintw(win, row++, line, "%*s: %s", label_width, "Uptime",           sysinfo.uptime);
    mvwprintw(win, row++, line, "%*s: %s", label_width, "Load Avg",         sysinfo.load_avg);
    mvwprintw(win, row++, line, "%*s: %s", label_width, "Logged-in Users",  sysinfo.logged_in_users);
    
    wrefresh(win);

    pthread_mutex_unlock(&ncurses_mutex);
	  
    sleep(5);
  }
  return NULL;
}

void *cpu_info_thread(void *arg) {
  WINDOW *win = (WINDOW *)arg;
  wait_until_ready();

  extern int temp_warn_yellow;
  extern int temp_warn_red;

  while (1) {
    CPUInfo cpu = get_cpu_info();

    pthread_mutex_lock(&ncurses_mutex);

    werase(win);

    int h, w;
    getmaxyx(win, h, w);
    (void)w;

    // Apply color before drawing border
    wattron(win, COLOR_PAIR(COLOR_BORDER));
    box(win, 0, 0);
    wattroff(win, COLOR_PAIR(COLOR_BORDER));

    int row = 0;
    int line = 3;

    // Title
    if (row < h - 1) {
      wattron(win, A_BOLD);
      mvwprintw(win, row++, line, " CPU Information ");
      wattroff(win, A_BOLD);
    }

    pthread_mutex_lock(&global_var_mutex);  
    int temp_color = 0;
    if (cpu.temperature >= temp_warn_red) {
      temp_color = COLOR_DATA_RED;
    } else if (cpu.temperature >= temp_warn_yellow) {
      temp_color = COLOR_DATA_YELLOW;
    }
    pthread_mutex_unlock(&global_var_mutex);
    
    // Avg usage and temperature
    if (row < h - 1) {
      wmove(win, row, line);
      wprintw(win, "Average: ");

      wattron(win, A_BOLD);
      wprintw(win, "%.1f%%", cpu.avg_usage);
      wattroff(win, A_BOLD);

      wprintw(win, " Temperature: ");

      wattron(win, A_BOLD);
      if (temp_color) {
        wattron(win, COLOR_PAIR(temp_color));
        wprintw(win, "%.1f °C", cpu.temperature);
        wattroff(win, COLOR_PAIR(temp_color));
      } else {
        wprintw(win, "%.1f °C", cpu.temperature);
      }
      wattroff(win, A_BOLD);

      row++;
    }

    char filled_char = '=';
    char empty_char = ' ';
    
    for (int i = 0; i < cpu.num_cores && row < h - 1; ++i) {
      float usage = cpu.usage_per_core[i];
      float freq = cpu.frequency_per_core[i];

      int filled = (int)((usage / 100.0) * BAR_WIDTH);
      char bar[BAR_WIDTH + 1];
      for (int j = 0; j < BAR_WIDTH; ++j) {
        bar[j] = j < filled ? filled_char : empty_char;
      }
      bar[BAR_WIDTH] = '\0';

      int usage_color = COLOR_PAIR(0);
      if (usage >= KPI_USAGE_CRIT) {
        usage_color = COLOR_PAIR(COLOR_DATA_RED);
      } else if (usage >= KPI_USAGE_WARN) {
        usage_color = COLOR_PAIR(COLOR_DATA_YELLOW);
      } else {
        usage_color = COLOR_PAIR(COLOR_DATA_GREEN);
      }

      if (row < h - 1) {
        mvwprintw(win, row++, line, "C%-2d [", i);
        wattron(win, usage_color);
        wprintw(win, "%s", bar);
        wattroff(win, usage_color);
        wprintw(win, "] %5.1f%%  %4.0f MHz", usage, freq);
      }
    }

    wrefresh(win);
    pthread_mutex_unlock(&ncurses_mutex);
    sleep(1);
  }

  return NULL;
}

void *memory_info_thread(void *arg) {
  WINDOW *win = (WINDOW *)arg;
  wait_until_ready();

  while (1) {
    pthread_mutex_lock(&ncurses_mutex);
    
    werase(win);

    wattron(win, COLOR_PAIR(COLOR_BORDER));
    box(win, 0, 0);
    wattroff(win, COLOR_PAIR(COLOR_BORDER));

    pthread_mutex_lock(&memory_info_mutex);
    MemoryInfo mem = get_memory_info();
    pthread_mutex_unlock(&memory_info_mutex);
    
    float total = mem.total_ram / 1024.0f;
    float free = mem.free_ram / 1024.0f;
    float used = mem.used_ram / 1024.0f;
    float swap_used = mem.used_swap / 1024.0f;
    float swap_total = mem.total_swap / 1024.0f;

    int row = 0;
    int line = 3;
    
    // Headers
    wattron(win, A_BOLD);
    mvwprintw(win, row++, 2, " Memory Usage ");
    wattroff(win, A_BOLD);
    wattron(win, COLOR_PAIR(COLOR_HEADER));
    mvwprintw(win, row++, line, "%-12s  %-10s  %-10s  %-10s", "Type", "Total(MiB)", "Used(MiB)", "Free(MiB)");
    wattroff(win, COLOR_PAIR(COLOR_HEADER));
    mvwprintw(win, row++, line, "%-12s  %-10.1f  %-10.1f  %-10.1f", "RAM", total, used, free);
    mvwprintw(win, row++, line, "%-12s  %-10.1f  %-10.1f  %-10s", "Swap", swap_total, swap_used, "-");
   
    // Additional percentage
    mvwprintw(win, row++, 2, "RAM used: %.1f%%", mem.mem_percent);

    wrefresh(win);
    pthread_mutex_unlock(&ncurses_mutex);
    
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

    pthread_mutex_lock(&disk_info_mutex);
    int ndisk = get_disk_info(disks, MAX_DISKS, &total_used, &total_total);
    pthread_mutex_unlock(&disk_info_mutex);

    pthread_mutex_lock(&ncurses_mutex);

    werase(win);

    int h, w;
    getmaxyx(win, h, w);

    wattron(win, COLOR_PAIR(COLOR_BORDER));
    box(win, 0, 0);
    wattroff(win, COLOR_PAIR(COLOR_BORDER));

    int row = 0;                   // Start after top border
    int col = 2;                   // Two-space indent
    int usable_width = w - 5;      // 2 spaces + border on each side

    // Title
    wattron(win, A_BOLD);
    mvwprintw(win, row++, col, "%.*s", usable_width, " Disk Information ");
    wattroff(win, A_BOLD);

    // Header
    wattron(win, COLOR_PAIR(COLOR_HEADER));
    mvwprintw(win, row++, col, "%.*s", usable_width,
              "Mount      Filesystem              Used     Total    Usage");
    wattroff(win, COLOR_PAIR(COLOR_HEADER));

    // Disk rows
    for (int i = 0; i < ndisk && row < h - 2; ++i) {
      if (disks[i].total_mb <= 0.0) continue;

      char used_buf[16], total_buf[16], line[256];
      format_bytes(disks[i].used_mb, used_buf, sizeof(used_buf));
      format_bytes(disks[i].total_mb, total_buf, sizeof(total_buf));

      snprintf(line, sizeof(line),
               "%-10.10s %-20.20s %7s %8s  %5.1f%%",
               disks[i].mount_point,
               disks[i].filesystem,
               used_buf,
               total_buf,
               disks[i].usage_percent);

      mvwprintw(win, row++, col, "%.*s", usable_width, line);
    }

    // Totals
    if (row < h - 1 && total_total > 0.0) {
      char used_buf[16], total_buf[16], line[256];
      format_bytes(total_used, used_buf, sizeof(used_buf));
      format_bytes(total_total, total_buf, sizeof(total_buf));
      double usage_percent = (total_used / total_total) * 100.0;

      snprintf(line, sizeof(line),
               "%-10s %-20s %7s %8s  %5.1f%%",
               "Total", "-",
               used_buf, total_buf,
               usage_percent);

      mvwprintw(win, row++, col, "%.*s", usable_width, line);
    }

    wrefresh(win);
    pthread_mutex_unlock(&ncurses_mutex);

    sleep(3);
  }

  return NULL;
}

void *process_info_thread(void *arg) {
  WINDOW *win = (WINDOW *)arg;
  wait_until_ready();

  while (1) {

    pthread_mutex_lock(&ncurses_mutex);
    int h, w;
    getmaxyx(win, h, w);
    werase(win);

    wattron(win, COLOR_PAIR(COLOR_BORDER));
    box(win, 0, 0);
    wattroff(win, COLOR_PAIR(COLOR_BORDER));

    if (h < 5) {
      mvwprintw(win, 1, 2, "Window too small");
      wrefresh(win);
      pthread_mutex_unlock(&ncurses_mutex);
      sleep(2);
      continue;
    }

    pthread_mutex_unlock(&ncurses_mutex);
    
    ProcessInfo list[MAX_PROCESSES];
    int count = get_top_processes_by_mem(list, MAX_PROCESSES);

    pthread_mutex_lock(&ncurses_mutex);

    int row = 0;

    wattron(win, A_BOLD);
    mvwprintw(win, row++, 2, " Process Information ");
    wattroff(win, A_BOLD);

    int compact_mode = 1;  // 0 = full, 1 = compact
    int max_rows = h - 2;

    if (compact_mode) {
      const char *header = "  PID    USER        %CPU  %MEM   COMMAND";
      char clipped_header[1024];
      strncpy(clipped_header, header, w - 2);
      clipped_header[w - 2] = '\0';

      wattron(win, COLOR_PAIR(COLOR_HEADER));
      mvwprintw(win, row++, 1, "%s", clipped_header);
      wattroff(win, COLOR_PAIR(COLOR_HEADER));

      for (int i = 0; i < count && row < max_rows; i++) {
        char line[1024];
        snprintf(line, sizeof(line), "%7.7s %-10.10s %5.5s %5.5s %-20.20s",
                 list[i].pid, list[i].user, list[i].cpu, list[i].mem,
                 list[i].command);

        line[w - 2] = '\0';
        mvwprintw(win, row++, 1, "%s", line);
      }

    } else {
      const char *header = "  PID    USER      PR  NI    VIRT    RES      SHR "
                           "S   %%CPU %%MEM   TIME+     COMMAND               ";
      char clipped_header[1024];
      strncpy(clipped_header, header, w - 2);
      clipped_header[w - 2] = '\0';

      wattron(win, COLOR_PAIR(COLOR_HEADER));
      mvwprintw(win, row++, 1, "%s", clipped_header);
      wattroff(win, COLOR_PAIR(COLOR_HEADER));

      for (int i = 0; i < count && row < max_rows; i++) {
        char line[1024];
        snprintf(line, sizeof(line),
                 "%7.7s %-10.10s %2.2s %2.2s %8.8s %7.7s %7.7s %1.1s %5.5s "
                 "%5.5s %10.10s %-20.20s",
                 list[i].pid, list[i].user, list[i].pr, list[i].ni,
                 list[i].virt, list[i].res, list[i].shr, list[i].state,
                 list[i].cpu, list[i].mem, list[i].time, list[i].command);

        line[w - 2] = '\0';
        mvwprintw(win, row++, 1, "%s", line);
      }
    }

    wrefresh(win);
    pthread_mutex_unlock(&ncurses_mutex);
    
    sleep(2);
  }

  return NULL;
}

void *connection_info_thread(void *arg) {
  WINDOW *win = (WINDOW *)arg;
  wait_until_ready();

  while (1) {
    ConnectionInfo connections[MAX_CONNECTIONS];
    int nconn = get_connection_info(connections, MAX_CONNECTIONS);

    pthread_mutex_lock(&ncurses_mutex);
    
    werase(win);

    wattron(win, COLOR_PAIR(COLOR_BORDER));
    box(win, 0, 0);
    wattroff(win, COLOR_PAIR(COLOR_BORDER));

    wattron(win, A_BOLD);
    mvwprintw(win, 0, 2, " Active Connections ");
    wattroff(win, A_BOLD);  
    
    
    wattron(win, COLOR_PAIR(COLOR_HEADER));
    mvwprintw(win, 1, 2, "%-6s %-22s %-22s %-15s", "Proto", "Local Address",
              "Remote Address", "State");
    wattroff(win, COLOR_PAIR(COLOR_HEADER));

    int y = 2;
    for (int i = 0; i < nconn && y < getmaxy(win) - 1; ++i) {
      mvwprintw(win, y++, 2, "%-6s %-22s %-22s %-15s", connections[i].protocol,
                connections[i].local_addr, connections[i].remote_addr,
                connections[i].state);
    }

    wrefresh(win);
    pthread_mutex_unlock(&ncurses_mutex);
    
    sleep(1);
  }

  return NULL;
}

void *network_info_thread(void *arg) {
  WINDOW *win = (WINDOW *)arg;
  wait_until_ready();

  while (1) {
    int num_interfaces = 0;
    int max_rows, max_cols;

    char **bandwidth_usage = get_bandwidth_usage(&num_interfaces);

    const char *connected_if = NULL;
    const char *ssid = NULL;
    char *ip = NULL;

    for (int i = 0; i < num_interfaces; i++) {
      char name[32];
      sscanf(bandwidth_usage[i], " %31s", name);

      if (strcmp(name, "lo") == 0 || strncmp(name, "br-", 3) == 0 ||
          strncmp(name, "docker", 6) == 0 || strncmp(name, "veth", 4) == 0 ||
          strncmp(name, "virbr", 5) == 0 || strncmp(name, "vmnet", 5) == 0) {
        continue;
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

    const char *dns = get_dns_servers();
    char gateway[64] = {0};
    char metric[64] = {0};
    get_default_gateway_and_metric(gateway, metric);

    char *mac = get_mac_address(connected_if);
    WifiInfo wifi = get_wifi_info(connected_if);

    // Lock only for ncurses rendering
    pthread_mutex_lock(&ncurses_mutex);

    getmaxyx(win, max_rows, max_cols);
    (void)max_cols;
    int row = 0;
    int line = 4;
    int max_lines = max_rows - 1;

    werase(win);
    wattron(win, COLOR_PAIR(COLOR_BORDER));
    box(win, 0, 0);
    wattroff(win, COLOR_PAIR(COLOR_BORDER));

    wattron(win, A_BOLD);
    mvwprintw(win, row++, 2, " Network Information ");
    wattroff(win, A_BOLD);

    mvwprintw(win, row++, line, "Default Gateway: %s | Metric: %s", gateway, metric);
    mvwprintw(win, row++, line, "DNS Servers: %s", dns);
    row++;

    if (connected_if) {
      const char *type = is_wifi_interface(connected_if) ? "Wi-Fi" : "Ethernet";
      if (is_wifi_interface(connected_if) && ssid)
        mvwprintw(win, row++, line, "> Connected: %s (%s: %s)  |  IP: %s",
                  connected_if, type, ssid, ip);
      else
        mvwprintw(win, row++, line, "> Connected: %s (%s)       |  IP: %s",
                  connected_if, type, ip);
    } else {
      mvwprintw(win, row++, line, "No active network connection detected.");
    }

    if (vpn_if && vpn_ip) {
      wattron(win, COLOR_PAIR(COLOR_DATA_RED));
      mvwprintw(win, row++, line, "VPN Active: %s  |  IP: %s", vpn_if, vpn_ip);
      wattroff(win, COLOR_PAIR(COLOR_DATA_RED));
    }

    row++;
    if (ssid) {
      mvwprintw(win, row++, line, "  Wi-Fi: %s", ssid);
      mvwprintw(win, row++, line, "  Signal Strength: %s dBm", wifi.signal_strength);
      mvwprintw(win, row++, line, "  Bitrate: %s", wifi.bitrate);
      mvwprintw(win, row++, line, "  Frequency: %s MHz", wifi.freq);
      mvwprintw(win, row++, line, "  MAC Address: %s", mac);
    }

    row++;
    wattron(win, A_BOLD);
    mvwprintw(win, row++, line, "%-15s | %10s | %10s", "Interface", "Sent", "Received");
    wattroff(win, A_BOLD);
    mvwprintw(win, row++, line, "---------------------------------------------");

    for (int i = 0; i < num_interfaces && row < max_lines; i++) {
      mvwprintw(win, row++, line, "%s", bandwidth_usage[i]);
    }

    wrefresh(win);
    pthread_mutex_unlock(&ncurses_mutex);

    free_bandwidth_usage(bandwidth_usage, num_interfaces);
    sleep(3);
  }

  return NULL;
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

  // Fixed heights for each row
  const int row1_height = 11;
  const int row2_height = screen_height - row1_height;

  // Row 1 widths (4 columns)
  int row1_widths[4] = {
    (int)(screen_width * 0.25),
    (int)(screen_width * 0.20),
    (int)(screen_width * 0.25),
    screen_width - ((int)(screen_width * 0.25) +
		    (int)(screen_width * 0.20) +
		    (int)(screen_width * 0.25))
  };

  // Row 2 widths (2 columns)
  int row2_widths[3] = {
    screen_width / 3,
    screen_width / 3,
    screen_width - ((int)(screen_width / 3) + (int)(screen_width / 3))
  };

  // Y offsets
  int row1_y = 0;
  int row2_y = row1_height;

  // Create row 1 windows
  WINDOW *sys_win = newwin(row1_height, row1_widths[0], row1_y, 0);
  WINDOW *cpu_win = newwin(row1_height, row1_widths[1], row1_y, row1_widths[0]);
  WINDOW *mem_win = newwin(row1_height, row1_widths[2], row1_y, row1_widths[0] + row1_widths[1]);
  WINDOW *disk_win = newwin(row1_height, row1_widths[3], row1_y,row1_widths[0] + row1_widths[1] + row1_widths[2]);

  // Create row 2 windows  
  WINDOW *conn_win = newwin(row2_height, row2_widths[0], row2_y, 0);
  WINDOW *net_win = newwin(row2_height, row2_widths[1], row2_y, row2_widths[0]);
  WINDOW *proc_win = newwin(row2_height, row2_widths[2], row2_y, row2_widths[0] + row2_widths[1]);
  
 
  // Launch threads
  pthread_t sys_tid;
  pthread_t cpu_tid;
  pthread_t mem_tid;
  pthread_t disk_tid;  
  pthread_t net_tid;
  pthread_t conn_tid;
  pthread_t proc_tid;
  
  pthread_create(&sys_tid, NULL, system_info_thread, sys_win);
  pthread_create(&cpu_tid, NULL, cpu_info_thread, cpu_win);
  pthread_create(&mem_tid, NULL, memory_info_thread, mem_win);
  pthread_create(&disk_tid, NULL, disk_info_thread, disk_win);

  pthread_create(&conn_tid, NULL, connection_info_thread, conn_win);
  pthread_create(&net_tid, NULL, network_info_thread, net_win);
  pthread_create(&proc_tid, NULL, process_info_thread, proc_win);
  
  sleep(1);
  ready = 1;

  int ch;
  while ((ch = getch()) != 'q' && ch != 'Q') {
    // Just wait for q or Q
  }
  endwin();
}
