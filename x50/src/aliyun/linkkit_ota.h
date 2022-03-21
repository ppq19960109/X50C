#ifndef __LINKKIT_OTA_H_
#define __LINKKIT_OTA_H_

#include "wrappers_defs.h"

enum OTA_TYPE
{
    OTA_IDLE = 0x00,
    OTA_NO_FIRMWARE,
    OTA_NEW_FIRMWARE,

    OTA_DOWNLOAD_START,
    OTA_DOWNLOAD_FAIL,
    OTA_DOWNLOAD_SUCCESS,
    OTA_INSTALL_START,
    OTA_INSTALL_FAIL,
    OTA_INSTALL_SUCCESS,
};

int request_fota_image(void);
int download_fota_image(void);

void register_ota_state_cb(int (*cb)(const int, void *));
void register_ota_complete_cb(void (*cb)(void));

int get_ota_state(void);
void set_ota_state(enum OTA_TYPE ota_state, void *arg);
void fota_event_handler(const char *version);

int linkkit_ota_init(const char *version);
void linkkit_ota_deinit(void);
#endif
