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
#include "dev_reset_api.h"
#include "dev_model_api.h"
#include "wrappers.h"

#include "cJSON.h"

#include "linkkit_solo.h"
#include "linkkit_func.h"
#include "cloud_platform_task.h"
#include "POSIXTimer.h"
#include "database.h"

static timer_t g_ota_timer;
static enum OTA_TYPE g_ota_state = 0;

static void linkkit_devrst_evt_handle(iotx_devrst_evt_type_t evt, void *msg)
{
    switch (evt)
    {
    case IOTX_DEVRST_EVT_RECEIVED:
    {
        iotx_devrst_evt_recv_msg_t *recv_msg = (iotx_devrst_evt_recv_msg_t *)msg;

        EXAMPLE_TRACE("Receive Reset Responst");
        EXAMPLE_TRACE("Msg ID: %d", recv_msg->msgid);
        EXAMPLE_TRACE("Payload: %.*s", recv_msg->payload_len, recv_msg->payload);

        char rst = 0x00;
        HAL_Kv_Set(LINKKIT_RST, &rst, sizeof(rst), 0);
    }
    break;

    default:
        break;
    }
}

int linkkit_dev_reset(void)
{
    int res = 0;
    iotx_dev_meta_info_t reset_meta_info;
    memset(&reset_meta_info, 0, sizeof(iotx_dev_meta_info_t));
    get_linkkit_dev_quad(reset_meta_info.product_key, NULL, reset_meta_info.device_name, NULL);
    if (strlen(reset_meta_info.product_key) == 0 || strlen(reset_meta_info.device_name) == 0)
        return -1;

    res = IOT_DevReset_Report(&reset_meta_info, linkkit_devrst_evt_handle, NULL);
    if (res < 0)
    {
        EXAMPLE_TRACE("IOT_DevReset_Report Failed\n");
        return -1;
    }
    return res;
}

int linkkit_unbind(void)
{
    char rst = 0x01;
    HAL_Kv_Set(LINKKIT_RST, &rst, sizeof(rst), 0);

    return linkkit_dev_reset();
}

int linkkit_unbind_check(void)
{
    int len = 1;
    char rst = 0;
    int ret;
    ret = HAL_Kv_Get(LINKKIT_RST, &rst, &len);
    if (ret < 0)
    {
        EXAMPLE_TRACE("linkkit_unbind_check %s not exits\n", LINKKIT_RST);
        return -1;
    }
    EXAMPLE_TRACE("linkkit_unbind_check rst:%d", rst);
    if (rst == 0)
    {
        return -1;
    }
    return linkkit_dev_reset();
}

void fota_event_handler(const char *version)
{
    char buffer[64] = {0};
    POSIXTimerSet(g_ota_timer, 0, 0);
    get_software_version(buffer);
    if (strcmp(version, buffer) == 0)
    {
        EXAMPLE_TRACE("fota_event_handler Firmware Version the same");
        set_ota_state(OTA_NO_FIRMWARE, NULL);
    }
    else
    {
        EXAMPLE_TRACE("fota_event_handler New Firmware Version");
        set_ota_state(OTA_NEW_FIRMWARE, (void *)version);
    }
}

int get_ota_progress(void)
{
    return 0;
}

static int (*ota_state_cb)(const int, void *);
void register_ota_state_cb(int (*cb)(const int, void *))
{
    ota_state_cb = cb;
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
    char buffer[64] = {0};
    get_software_version(buffer);
    int buffer_length = strlen(buffer);
    EXAMPLE_TRACE("request_fota_image:%s,%d", buffer, buffer_length);
    set_ota_state(OTA_IDLE, NULL);
    res = IOT_Linkkit_Query(EXAMPLE_MASTER_DEVID, ITM_MSG_REQUEST_FOTA_IMAGE, (unsigned char *)buffer, buffer_length);
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
    if (get_ota_state() != OTA_NEW_FIRMWARE)
    {
        set_ota_state(OTA_NO_FIRMWARE, NULL);
        return -1;
    }
    int res = 0;
    char buffer[1024] = {0};
    int buffer_length = 1024;
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
    // sync();
    system("chmod 777 " otafilename);
    system("cd /tmp &&" otafilename);
    databse_drop_table(RECIPE_TABLE_NAME);

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
int linkkit_func_init(void)
{
    register_unbind_cb(linkkit_unbind_check);
    g_ota_timer = POSIXTimerCreate(0, POSIXTimer_cb);
    EXAMPLE_TRACE("linkkit_func_init timer:%p", g_ota_timer);
    return 0;
}

void linkkit_func_deinit(void)
{
    if (g_ota_timer != NULL)
    {
        POSIXTimerDelete(g_ota_timer);
        g_ota_timer = NULL;
    }
}