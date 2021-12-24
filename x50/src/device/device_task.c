#include "main.h"

#include "uds_protocol.h"
#include "tcp_uds_server.h"

#include "uart_cloud_task.h"
#include "device_task.h"
#include "linkkit_func.h"

static void *SoftVersion_cb(void *ptr, void *arg)
{
    cloud_dev_t *cloud_dev = get_cloud_dev();
    cJSON *item = cJSON_CreateString(cloud_dev->software_ver);
    return item;
}
static void *HardVersion_cb(void *ptr, void *arg)
{
    cloud_dev_t *cloud_dev = get_cloud_dev();
    cJSON *item = cJSON_CreateString(cloud_dev->software_ver);
    return item;
}
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

static void *DeviceName_cb(void *ptr, void *arg)
{
    cloud_dev_t *cloud_dev = get_cloud_dev();
    cJSON *item = cJSON_CreateString(cloud_dev->device_name);
    return item;
}

static void *QrCode_cb(void *ptr, void *arg)
{
    cloud_dev_t *cloud_dev = get_cloud_dev();
    char buf[256];
    sprintf(buf, "http://club.marssenger.com/hxr/download.html?pk=%s&dn=%s", cloud_dev->product_key, cloud_dev->device_name);
    cJSON *item = cJSON_CreateString(buf);
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
    char buf[256];
    sprintf(buf, "http://club.marssenger.com/hxr/download.html?pk=%s", cloud_dev->product_key);
    cJSON *item = cJSON_CreateString(buf);
    return item;
}
static set_attr_t g_device_set_attr[] = {
    {
        cloud_key : "SoftVersion",
        fun_type : LINK_FUN_TYPE_ATTR_REPORT,
        cb : SoftVersion_cb
    },
    {
        cloud_key : "HardVersion",
        fun_type : LINK_FUN_TYPE_ATTR_REPORT,
        cb : HardVersion_cb
    },
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
        cloud_key : "DeviceName",
        fun_type : LINK_FUN_TYPE_ATTR_REPORT,
        cb : DeviceName_cb
    },
    {
        cloud_key : "QrCode",
        fun_type : LINK_FUN_TYPE_ATTR_REPORT,
        cb : QrCode_cb
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
