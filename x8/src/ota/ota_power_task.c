#include "main.h"

#include "uds_protocol.h"
#include "uds_tcp_server.h"

#include "cloud_platform_task.h"
#include "ota_power_task.h"
#include "link_fota_power_posix.h"
#include "link_solo.h"
#include "ecb_uart_parse_msg.h"

#define POWER_OTA_FILE "/tmp/power_upgrade"

static void *OTAState_cb(void *ptr, void *arg)
{
    cJSON *item = cJSON_CreateNumber(get_ota_power_state());
    return item;
}

static void *OTARquest_cb(void *ptr, void *arg)
{
    if (NULL == arg)
        return NULL;
    cJSON *item = (cJSON *)arg;
    if (item->valueint == 0)
    {
        link_fota_power_query_firmware();
    }
    else
    {
        link_fota_power_download_firmware();
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

static set_attr_t g_ota_set_attr[] = {
    {
        cloud_key : "OTAPowerState",
        fun_type : LINK_FUN_TYPE_ATTR_REPORT,
        cb : OTAState_cb
    },
    {
        cloud_key : "OTAPowerRquest",
        fun_type : LINK_FUN_TYPE_ATTR_CTRL,
        cb : OTARquest_cb

    },
    {
        cloud_key : "OTAPowerProgress",
        fun_type : LINK_FUN_TYPE_ATTR_REPORT,
        cb : OTAProgress_cb
    },
    {
        cloud_key : "OTAPowerNewVersion",
        fun_type : LINK_FUN_TYPE_ATTR_REPORT,
        cb : OTANewVersion_cb,
        value : {p : NewVersion}
    },
};
static const int attr_len = sizeof(g_ota_set_attr) / sizeof(g_ota_set_attr[0]);
static set_attr_t *attr = g_ota_set_attr;

int ota_power_resp_get(cJSON *root, cJSON *resp)
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

int ota_power_resp_getall(cJSON *root, cJSON *resp)
{
    for (int i = 0; i < attr_len; ++i)
    {
        set_attr_report_uds(resp, &attr[i]);
    }
    return 0;
}

int ota_power_resp_set(cJSON *root, cJSON *resp)
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
    if (OTA_NEW_FIRMWARE == state)
    {
        strcpy(g_ota_set_attr[3].value.p, arg);
        cJSON_AddStringToObject(root, g_ota_set_attr[3].cloud_key, arg);
    }
    set_attr_report_uds(root, &g_ota_set_attr[0]);

    return send_event_uds(root, NULL);
}

static void ota_progress_cb(const int precent)
{
    dzlog_info("ota_progress_cb:%d", precent);
    cJSON *root = cJSON_CreateObject();
    cJSON_AddNumberToObject(root, g_ota_set_attr[2].cloud_key, precent);

    send_event_uds(root, NULL);
}
static timer_t g_ota_timer = NULL;
static void POSIXTimer_cb(union sigval val)
{
    ota_query_timer_cb();
}
static int ota_query_timer_start_cb(void)
{
    POSIXTimerSet(g_ota_timer, 0, 12);
    return 0;
}
static int ota_query_timer_stop_cb(void)
{
    POSIXTimerSet(g_ota_timer, 0, 0);
    return 0;
}
enum ota_cmd_status_t
{
    OTA_CMD_START = 0,
    OTA_CMD_DATA,
    OTA_CMD_STOP,
    OTA_CMD_END,
};

static char ota_power_steps = 0;
static int ota_power_send_data(const char cmd)
{
    static char buf[256 + 16];
    int len = 0;
    buf[len++] = 0xf9;
    buf[len++] = 0x01;
    buf[len++] = cmd;
    switch (cmd)
    {
    case 0:

        break;
    case 1:

        break;
    case 3:

        break;
    case 4:

        break;
    }
    if (len != 0)
        ecb_uart_send_ota_msg(buf, len);
    return 0;
}
void ota_power_ack(const unsigned char *data)
{
    if (data[0] = 0)
    {
        if (ota_power_steps == 0)
        {
        }
        else if (ota_power_steps == 1)
        {
        }
        else if (ota_power_steps == 2)
        {
        }
    }
    else
    {
        dzlog_error("ota_power_ack error:%d", data[1]);
    }
}
static int ota_install_cb(char *text)
{
    int ret = -1;
    long size = getFileSize(text);
    dzlog_warn("ota_install_cb size:%ld", size);
    if (size <= 0)
        goto fail;
    char cmd[48] = {0};
    sprintf(cmd, "sh %s", text);
    ret = system(cmd);
    if (ret != 0)
        goto fail;

    while (ota_power_steps > 0)
        ;
    ret = ota_power_steps;
fail:
    dzlog_warn("ota_install_cb ret:%d", ret);
    sprintf(cmd, "rm -rf %s", text);
    system(cmd);
    return ret;
}
int ota_power_task_init(void)
{
    register_ota_power_state_cb(ota_state_event);
    register_ota_power_progress_cb(ota_progress_cb);
    register_ota_power_install_cb(ota_install_cb);
    register_ota_power_query_timer_start_cb(ota_query_timer_start_cb);
    register_ota_power_query_timer_stop_cb(ota_query_timer_stop_cb);
    g_ota_timer = POSIXTimerCreate(0, POSIXTimer_cb);
    return 0;
}
void ota_power_task_deinit(void)
{
    if (g_ota_timer != NULL)
    {
        POSIXTimerDelete(g_ota_timer);
        g_ota_timer = NULL;
    }
}