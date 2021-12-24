#ifndef __LINKKIT_FUNC_H_
#define __LINKKIT_FUNC_H_

#include "wrappers_defs.h"
#include "wrappers.h"
enum OTA_TYPE
{
    OTA_IDLE= 0x00,
    OTA_NO_FIRMWARE ,
    OTA_NEW_FIRMWARE,
    
    OTA_DOWNLOAD_START,
    OTA_DOWNLOAD_FAIL,
    OTA_DOWNLOAD_SUCCESS,
    OTA_INSTALL_START,
    OTA_INSTALL_FAIL,
    OTA_INSTALL_SUCCESS,
};

int linkkit_unbind(void);
int request_fota_image(void);
int download_fota_image(void);
int get_ota_progress(void);
void register_ota_state_cb(int (*cb)(const int, void *));
int get_ota_state(void);
void set_ota_state(enum OTA_TYPE ota_state,void* arg);
void fota_event_handler(const char *version);

int linkkit_func_init(void);
void linkkit_func_deinit(void);
#endif
