#include "main.h"

#include "uds_protocol.h"
#include "uds_tcp_server.h"

#include "cloud_platform_task.h"
#include "ota_task.h"
#include "linkkit_ota.h"
#include "linkkit_solo.h"

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
    if (item->valueint == 0)
    {
        request_fota_image();
    }
    else
    {
        download_fota_image();
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
    if (OTA_NEW_FIRMWARE == state)
    {
        strcpy(g_ota_set_attr[3].value.p, arg);
        cJSON_AddStringToObject(root, g_ota_set_attr[3].cloud_key, arg);
    }
    set_attr_report_uds(root, &g_ota_set_attr[0]);

    return send_event_uds(root,NULL);
}

static void ota_progress_cb(int precent)
{
    dzlog_info("ota_progress_cb:%d", precent);
    cJSON *root = cJSON_CreateObject();
    cJSON_AddNumberToObject(root, g_ota_set_attr[2].cloud_key, precent);

    send_event_uds(root,NULL);
}

int ota_task_init(void)
{
    register_ota_state_cb(ota_state_event);
    register_dm_fota_download_percent_cb(ota_progress_cb);
    return 0;
}