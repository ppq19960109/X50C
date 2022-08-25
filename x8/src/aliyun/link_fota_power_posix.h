#ifndef __LINK_FOTA_POWER_H_
#define __LINK_FOTA_POWER_H_

#include "aiot_ota_api.h"
void link_fota_power_start(void *fota_handle);
int link_fota_power_report_version(char *cur_version);
int link_fota_power_download_firmware(void);
int link_fota_power_query_firmware(void);

int get_ota_power_state(void);
void register_ota_power_progress_cb(void (*cb)(const int));
void register_ota_power_install_cb(int (*cb)(char *));
void register_ota_power_state_cb(int (*cb)(const int, void *));
void register_ota_power_query_timer_start_cb(int (*cb)(void));
void register_ota_power_query_timer_stop_cb(int (*cb)(void));
void ota_power_query_timer_end(void);
void register_ota_power_complete_cb(void (*cb)(void));

void ota_power_recv_handler(void *ota_handle, aiot_ota_recv_t *ota_msg, void *userdata);
#endif
