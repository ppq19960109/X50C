#ifndef __LINK_FOTA_POWER_H_
#define __LINK_FOTA_POWER_H_

#include "link_fota_posix.h"
int link_fota_power_start(void *mqtt_handle);
int link_fota_power_report_version(char *cur_version);
void link_fota_power_stop(void);
int link_fota_power_download_firmware(void);
int link_fota_power_query_firmware(void);

int get_ota_power_state(void);
void register_ota_power_progress_cb(void (*cb)(const int));
void register_ota_power_state_cb(int (*cb)(const int, void *));
void register_ota_power_query_timer_start_cb(int (*cb)(void));
void register_ota_power_query_timer_stop_cb(int (*cb)(void));
void ota_power_query_timer_cb(void);
void register_ota_power_complete_cb(void (*cb)(void));
#endif
