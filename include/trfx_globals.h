// trfx_globals.h
#ifndef TRFX_GLOBALS_H
#define TRFX_GLOBALS_H

#include <pthread.h>
#include <signal.h>

extern pthread_mutex_t ncurses_mutex;
extern pthread_mutex_t ready_mutex;
extern pthread_mutex_t global_var_mutex;
extern pthread_mutex_t memory_info_mutex;
extern pthread_mutex_t disk_info_mutex;

extern volatile int ready;
extern volatile sig_atomic_t screen_paused;

#define STATIC_MODULE_SYSINFO 0
#define STATIC_MODULE_CPUINFO 1
#define STATIC_MODULE_MEMINFO 2
#define STATIC_MODULE_DISKINFO 3
#define STATIC_MODULE_COUNT 4

#define DYNAMIC_MODULE_NETINFO 0
#define DYNAMIC_MODULE_CONNINFO 1
#define DYNAMIC_MODULE_PROCINFO 2
#define DYNAMIC_MODULE_BANDWIDTH 3
#define DYNAMIC_MODULE_COUNT 4

#define COLOR_TITLE 1
#define COLOR_SECTION 2
#define COLOR_DATA 3
#define COLOR_DATA_RED 4
#define COLOR_DATA_YELLOW 5
#define COLOR_DATA_GREEN 6
#define COLOR_HEADER 7
#define COLOR_BORDER 8

#define CPU_USAGE_WARN 70.0
#define CPU_USAGE_CRIT 90.0
#define CPU_BAR_WIDTH 16

extern volatile int force_refresh_flags[STATIC_MODULE_COUNT];

#endif // TRFX_GLOBALS_H
