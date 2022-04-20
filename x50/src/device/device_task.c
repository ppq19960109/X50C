#include "main.h"

#include "uds_protocol.h"
#include "uds_tcp_server.h"

#include "cloud_platform_task.h"
#include "device_task.h"
#include "link_reset_posix.h"
#include "link_bind_posix.h"
#include "database_task.h"
#include "gesture_uart.h"

static void *ProductCategory_cb(void *ptr, void *arg)
{
    cloud_dev_t *cloud_dev = get_cloud_dev();
    cJSON *item = cJSON_CreateString(cloud_dev->device_category);
    return item;
}

static void *ProductModel_cb(void *ptr, void *arg)
{
    cloud_dev_t *cloud_dev = get_cloud_dev();
    cJSON *item = cJSON_CreateString(cloud_dev->device_model);
    return item;
}
static void *ProductKey_cb(void *ptr, void *arg)
{
    cloud_dev_t *cloud_dev = get_cloud_dev();
    cJSON *item = cJSON_CreateString(cloud_dev->product_key);
    return item;
}
static void *DeviceName_cb(void *ptr, void *arg)
{
    cloud_dev_t *cloud_dev = get_cloud_dev();
    cJSON *item = cJSON_CreateString(cloud_dev->device_name);
    return item;
}
static void *ProductSecret_cb(void *ptr, void *arg)
{
    cloud_dev_t *cloud_dev = get_cloud_dev();
    cJSON *item = cJSON_CreateString(cloud_dev->product_secret);
    return item;
}

static void *DeviceSecret_cb(void *ptr, void *arg)
{
    cloud_dev_t *cloud_dev = get_cloud_dev();
    cJSON *item = cJSON_CreateString(cloud_dev->device_secret);
    return item;
}
static void *QrCode_cb(void *ptr, void *arg)
{
    cloud_dev_t *cloud_dev = get_cloud_dev();
    char buf[180];
    sprintf(buf, "http://club.marssenger.com/hxr/download.html?pk=%s&dn=%s", cloud_dev->product_key, cloud_dev->device_name);
    cJSON *item = cJSON_CreateString(buf);
    return item;
}
static void *UpdateLog_cb(void *ptr, void *arg)
{
    cloud_dev_t *cloud_dev = get_cloud_dev();

    cJSON *item = cJSON_CreateString(cloud_dev->update_log);
    return item;
}

static void *AfterSalesPhone_cb(void *ptr, void *arg)
{
    cloud_dev_t *cloud_dev = get_cloud_dev();
    cJSON *item = cJSON_CreateString(cloud_dev->after_sales_phone);
    return item;
}
static void *AfterSalesQrCode_cb(void *ptr, void *arg)
{
    cloud_dev_t *cloud_dev = get_cloud_dev();
    char buf[180];
    sprintf(buf, "http://club.marssenger.com/hxr/download.html?pk=%s", cloud_dev->product_key);
    cJSON *item = cJSON_CreateString(buf);
    return item;
}
static void *BindTokenState_cb(void *ptr, void *arg)
{
    cJSON *item = cJSON_CreateNumber(get_token_state());
    return item;
}

static void *Reset_cb(void *ptr, void *arg)
{
    // wifiDisconnect();
    // systemRun("wpa_cli remove_network all && wpa_cli save_config");
    // systemRun("wpa_cli reconfigure");
    database_task_reinit();
    link_reset_report();
    uds_event_all();
    cJSON *item = cJSON_CreateNumber(1);
    return item;
}

static void *Alarm_cb(void *ptr, void *arg)
{
    gesture_auto_sync_time_alarm(1);
    return NULL;
}

static set_attr_t g_device_set_attr[] = {
    {
        cloud_key : "ProductCategory",
        fun_type : LINK_FUN_TYPE_ATTR_REPORT,
        cb : ProductCategory_cb
    },
    {
        cloud_key : "ProductModel",
        fun_type : LINK_FUN_TYPE_ATTR_REPORT,
        cb : ProductModel_cb
    },
    {
        cloud_key : "ProductKey",
        fun_type : LINK_FUN_TYPE_ATTR_REPORT,
        cb : ProductKey_cb
    },
    {
        cloud_key : "DeviceName",
        fun_type : LINK_FUN_TYPE_ATTR_REPORT,
        cb : DeviceName_cb
    },
    {
        cloud_key : "ProductSecret",
        fun_type : LINK_FUN_TYPE_ATTR_REPORT,
        cb : ProductSecret_cb
    },
    {
        cloud_key : "DeviceSecret",
        fun_type : LINK_FUN_TYPE_ATTR_REPORT,
        cb : DeviceSecret_cb
    },
    {
        cloud_key : "QrCode",
        fun_type : LINK_FUN_TYPE_ATTR_REPORT,
        cb : QrCode_cb
    },
    {
        cloud_key : "UpdateLog",
        fun_type : LINK_FUN_TYPE_ATTR_REPORT,
        cb : UpdateLog_cb
    },
    {
        cloud_key : "AfterSalesPhone",
        fun_type : LINK_FUN_TYPE_ATTR_REPORT,
        cb : AfterSalesPhone_cb
    },
    {
        cloud_key : "AfterSalesQrCode",
        fun_type : LINK_FUN_TYPE_ATTR_REPORT,
        cb : AfterSalesQrCode_cb
    },
    {
        cloud_key : "Reset",
        fun_type : LINK_FUN_TYPE_ATTR_CTRL,
        cb : Reset_cb
    },
    {
        cloud_key : "Alarm",
        fun_type : LINK_FUN_TYPE_ATTR_CTRL,
        cb : Alarm_cb
    },
    {
        cloud_key : "BindTokenState",
        fun_type : LINK_FUN_TYPE_ATTR_REPORT,
        cb : BindTokenState_cb
    },
};
static const int attr_len = sizeof(g_device_set_attr) / sizeof(g_device_set_attr[0]);
static set_attr_t *attr = g_device_set_attr;

int device_resp_get(cJSON *root, cJSON *resp)
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

int device_resp_getall(cJSON *root, cJSON *resp)
{
    for (int i = 0; i < attr_len; ++i)
    {
        set_attr_report_uds(resp, &attr[i]);
    }
    return 0;
}

int device_resp_set(cJSON *root, cJSON *resp)
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

static void device_token_state_cb(int state)
{
    cJSON *root = cJSON_CreateObject();
    cJSON_AddNumberToObject(root, "BindTokenState", state);

    send_event_uds(root, NULL);
}

int device_task_init(void)
{
    register_token_state_cb(device_token_state_cb);
    return 0;
}