#include "main.h"

#include "uds_protocol.h"
#include "uds_tcp_server.h"

#include "cloud_platform_task.h"
#include "ota_task.h"
#include "link_fota_posix.h"
#include "link_solo.h"
#include "ecb_uart_parse_msg.h"

static void ota_OTASlientUpgrade_set(void)
{
    unsigned char buf[4];
    int len = 0;
    buf[len++] = 0xf9;
    buf[len++] = 0x01;
    buf[len++] = 0x05;
    ecb_uart_send_ota_msg(buf, len, 0);
}

static void *OTAState_cb(void *ptr, void *arg)
{
    cJSON *item = cJSON_CreateNumber(get_ota_state());
    return item;
}

static void *OTARquest_cb(void *ptr, void *arg)
{
    if (NULL == arg)
        return NULL;
    cJSON *item = (cJSON *)arg;
    set_OtaCmdPushType(1);
    if (item->valueint == 0)
    {
        link_fota_query_firmware();
    }
    else
    {
        link_fota_download_firmware();
    }
    return NULL;
}

static void *OTAProgress_cb(void *ptr, void *arg)
{
    return NULL;
}
static char NewVersion[64] = {0};
static void *OTANewVersion_cb(void *ptr, void *arg)
{
    set_attr_t *attr = (set_attr_t *)ptr;
    cJSON *item = cJSON_CreateString(attr->value.p);
    return item;
}
static void *OTASlientUpgrade_cb(void *ptr, void *arg)
{
    cJSON *item = (cJSON *)arg;
    if (item->valueint > 0)
    {
        ota_OTASlientUpgrade_set();
    }
    return NULL;
}
static set_attr_t g_ota_set_attr[] = {
    {
        cloud_key : "OTAState",
        fun_type : LINK_FUN_TYPE_ATTR_REPORT,
        cb : OTAState_cb
    },
    {
        cloud_key : "OTARquest",
        fun_type : LINK_FUN_TYPE_ATTR_CTRL,
        cb : OTARquest_cb

    },
    {
        cloud_key : "OTAProgress",
        fun_type : LINK_FUN_TYPE_ATTR_REPORT,
        cb : OTAProgress_cb
    },
    {
        cloud_key : "OTANewVersion",
        fun_type : LINK_FUN_TYPE_ATTR_REPORT,
        cb : OTANewVersion_cb,
        value : {p : NewVersion}
    },
    {
        cloud_key : "OTASlientUpgrade",
        fun_type : LINK_FUN_TYPE_ATTR_CTRL,
        cb : OTASlientUpgrade_cb
    },
};
static const int attr_len = sizeof(g_ota_set_attr) / sizeof(g_ota_set_attr[0]);
static set_attr_t *attr = g_ota_set_attr;

int ota_resp_get(cJSON *root, cJSON *resp)
{
    for (int i = 0; i < attr_len; ++i)
    {
        if (cJSON_HasObjectItem(root, attr[i].cloud_key))
        {
            set_attr_report_uds(resp, &attr[i]);
        }
    }
    return 0;
}

int ota_resp_getall(cJSON *root, cJSON *resp)
{
    for (int i = 0; i < attr_len; ++i)
    {
        set_attr_report_uds(resp, &attr[i]);
    }
    return 0;
}

int ota_resp_set(cJSON *root, cJSON *resp)
{
    for (int i = 0; i < attr_len; ++i)
    {
        if (cJSON_HasObjectItem(root, attr[i].cloud_key))
        {
            set_attr_ctrl_uds(resp, &attr[i], cJSON_GetObjectItem(root, attr[i].cloud_key));
        }
    }
    return 0;
}

static int ota_state_event(const int state, void *arg)
{
    dzlog_info("ota_state_event:%d", state);
    cJSON *root = cJSON_CreateObject();

    if (OTA_NO_FIRMWARE == state)
    {
    }
    else
    {
        if (get_OtaCmdPushType() == OTA_PUSH_TYPE_SILENT)
        {
            cJSON_Delete(root);
            return -1;
        }
        if (OTA_NEW_FIRMWARE == state)
        {
            strcpy(g_ota_set_attr[3].value.p, arg);
            cJSON_AddStringToObject(root, g_ota_set_attr[3].cloud_key, arg);
            set_OtaCmdPushType(0);
        }
        else if (OTA_DOWNLOAD_FAIL == state || OTA_INSTALL_FAIL == state)
        {
            set_OtaCmdPushType(0);
        }
    }
    set_attr_report_uds(root, &g_ota_set_attr[0]);

    return send_event_uds(root, NULL);
}

static void ota_progress_cb(const int precent)
{
    dzlog_info("ota_progress_cb:%d", precent);
    if (get_OtaCmdPushType() == OTA_PUSH_TYPE_SILENT)
        return;
    cJSON *root = cJSON_CreateObject();
    cJSON_AddNumberToObject(root, g_ota_set_attr[2].cloud_key, precent);

    send_event_uds(root, NULL);
}
static timer_t g_ota_timer = NULL;
static void POSIXTimer_cb(union sigval val)
{
    ota_query_timer_end();
}
static int ota_query_timer_start_cb(void)
{
    POSIXTimerSet(g_ota_timer, 0, 11);
    return 0;
}
static int ota_query_timer_stop_cb(void)
{
    POSIXTimerSet(g_ota_timer, 0, 0);
    return 0;
}
static int ota_install_cb(char *text)
{
    // return -1;
    int ret = -1;
    // system("sh " OTA_FILE);
    long size = getFileSize(text);
    dzlog_warn("ota_install_cb size:%ld", size);
    if (size <= 0)
        goto fail;
    char cmd[48] = {0};
    sprintf(cmd, "sh %s", text);
    ret = system(cmd);
    dzlog_warn("ota_install_cb ret:%d", ret);
fail:
    sprintf(cmd, "rm -rf %s", text);
    system(cmd);
    return ret;
}

static void ota_complete_cb(void)
{
    sync();
    if (get_OtaCmdPushType() == OTA_PUSH_TYPE_CONFIRM)
    {
        ota_OTASlientUpgrade_set();
        reboot(RB_AUTOBOOT);
    }
    else
    {
        cJSON *root = cJSON_CreateObject();
        cJSON_AddNumberToObject(root, "OTASlientUpgrade", 1);
        send_event_uds(root, NULL);
    }
    set_OtaCmdPushType(0);
}

int ota_task_init(void)
{
    register_ota_state_cb(ota_state_event);
    register_ota_progress_cb(ota_progress_cb);
    register_ota_install_cb(ota_install_cb);
    register_ota_complete_cb(ota_complete_cb);
    register_ota_query_timer_start_cb(ota_query_timer_start_cb);
    register_ota_query_timer_stop_cb(ota_query_timer_stop_cb);
    g_ota_timer = POSIXTimerCreate(0, POSIXTimer_cb);

    return 0;
}
void ota_task_deinit(void)
{
    if (g_ota_timer != NULL)
    {
        POSIXTimerDelete(g_ota_timer);
        g_ota_timer = NULL;
    }
}