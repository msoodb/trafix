#ifndef TRFX_CONFIG_H
#define TRFX_CONFIG_H

#define CONFIG_FILE "config/config.cfg"

extern int alert_bandwidth;
extern char filter_ip[16];
extern char filter_process[50];

void trfx_read_config(const char *config_file);

#endif /*TRFX_CONFIG_H*/
