#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/reboot.h>
#include "infra_config.h"
#include "infra_state.h"
#include "infra_types.h"
#include "infra_defs.h"
#include "infra_compat.h"
#include "dev_model_api.h"
#include "wrappers.h"

#include "linkkit_solo.h"
#include "linkkit_ota.h"
#include "POSIXTimer.h"

void register_version_cb(int (*cb)(char *));
static timer_t g_ota_timer;
static enum OTA_TYPE g_ota_state = 0;
static char g_ota_version[64];

static int (*ota_state_cb)(const int, void *);
void register_ota_state_cb(int (*cb)(const int, void *))
{
    ota_state_cb = cb;
}
static void (*ota_complete_cb)(void);
void register_ota_complete_cb(void (*cb)(void))
{
    ota_complete_cb = cb;
}

static int get_ota_version(char *software_ver) //获取软件版本号
{
    if (software_ver == NULL)
        return 0;
    strcpy(software_ver, g_ota_version);
    return strlen(software_ver);
}

void fota_event_handler(const char *version)
{
    POSIXTimerSet(g_ota_timer, 0, 0);
    // char buffer[64] = {0};
    // if (strcmp(version, buffer) == 0)
    // {
    //     EXAMPLE_TRACE("fota_event_handler Firmware Version the same");
    //     set_ota_state(OTA_NO_FIRMWARE, NULL);
    // }
    // else
    // {
    EXAMPLE_TRACE("fota_event_handler New Firmware Version");
    set_ota_state(OTA_NEW_FIRMWARE, (void *)version);
    // }
}

int get_ota_state(void)
{
    return g_ota_state;
}

void set_ota_state(enum OTA_TYPE ota_state, void *arg)
{
    g_ota_state = ota_state;
    if (ota_state_cb)
        ota_state_cb(ota_state, arg);
}

int request_fota_image(void)
{
    int res = 0;

    EXAMPLE_TRACE("request_fota_image:%s", g_ota_version);
    set_ota_state(OTA_IDLE, NULL);
    res = IOT_Linkkit_Query(EXAMPLE_MASTER_DEVID, ITM_MSG_REQUEST_FOTA_IMAGE, (unsigned char *)g_ota_version, strlen(g_ota_version));
    EXAMPLE_TRACE("request_fota_image res:%d", res);
    if (res != 0)
    {
        set_ota_state(OTA_NO_FIRMWARE, NULL);
    }
    else
    {
        POSIXTimerSet(g_ota_timer, 0, 10);
    }
    return res;
}

int download_fota_image(void)
{
    // if (get_ota_state() != OTA_NEW_FIRMWARE)
    // {
    //     set_ota_state(OTA_NO_FIRMWARE, NULL);
    //     return -1;
    // }
    int res = 0;
    char buffer[2048] = {0};
    int buffer_length = 2048;
    set_ota_state(OTA_DOWNLOAD_START, NULL);
    res = IOT_Linkkit_Query(EXAMPLE_MASTER_DEVID, ITM_MSG_QUERY_FOTA_DATA, (unsigned char *)buffer, buffer_length);
    if (res != 0)
    {
        set_ota_state(OTA_DOWNLOAD_FAIL, NULL);
        return res;
    }
    set_ota_state(OTA_DOWNLOAD_SUCCESS, NULL);
    sleep(1);
    set_ota_state(OTA_INSTALL_START, NULL);

    system("chmod 777 " otafilename);
    system("cd /tmp &&" otafilename);
    if (ota_complete_cb != NULL)
        ota_complete_cb();

    set_ota_state(OTA_INSTALL_SUCCESS, NULL);
    sync();
    reboot(RB_AUTOBOOT);
    return res;
}

static void POSIXTimer_cb(union sigval val)
{
    EXAMPLE_TRACE("request_fota_image timeout");
    set_ota_state(OTA_NO_FIRMWARE, NULL);
}

int linkkit_ota_init(const char *version)
{
    register_version_cb(get_ota_version);
    strcpy(g_ota_version, version);
    g_ota_timer = POSIXTimerCreate(0, POSIXTimer_cb);
    return 0;
}

void linkkit_ota_deinit(void)
{
    if (g_ota_timer != NULL)
    {
        POSIXTimerDelete(g_ota_timer);
        g_ota_timer = NULL;
    }
}