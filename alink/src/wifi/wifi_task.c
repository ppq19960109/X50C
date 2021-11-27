#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <limits.h>

#include "cJSON.h"
#include "commonFunc.h"
#include "networkFunc.h"
#include "logFunc.h"

#include "uds_recv_send.h"
#include "tcp_uds_server.h"
#include "rkwifi.h"
#include "wifi_task.h"

static void *WifiState_cb(void *ptr, void *arg)
{
    set_attr_t *attr = (set_attr_t *)ptr;

    cJSON *item = cJSON_CreateNumber(attr->value.n);
    return item;
}

static void *WifiEnable_cb(void *ptr, void *arg)
{
    set_attr_t *attr = (set_attr_t *)ptr;
    cJSON *item = (cJSON *)arg;

    if (wifiEnable(item->valueint) < 0)
    {
        return NULL;
    }
    attr->value.n = item->valueint;
    item = cJSON_CreateNumber(attr->value.n);
    return item;
}

static void *WifiScan_cb(void *ptr, void *arg)
{
    wifiScan();
    return NULL;
}

static void *WifiScanR_cb(void *ptr, void *arg)
{
    cJSON *item = cJSON_CreateString(getWifiScanR());
    return item;
}

static void *WifiConnect_cb(void *ptr, void *arg)
{
    // set_attr_t *attr = (set_attr_t *)ptr;
    cJSON *item = (cJSON *)arg;
    cJSON *ssid = cJSON_GetObjectItem(item, "ssid");
    if (ssid == NULL)
        return NULL;
    cJSON *psk = cJSON_GetObjectItem(item, "psk");
    if (psk == NULL)
        return NULL;
    cJSON *encryp = cJSON_GetObjectItem(item, "encryp");
    if (encryp == NULL)
        return NULL;
    if (wifiConnect(ssid->valuestring, psk->valuestring, encryp->valueint) < 0)
    {
        return NULL;
    }
    return NULL;
}

static void *WifiDisconnect_cb(void *ptr, void *arg)
{
    if (wifiDisconnect() < 0)
    {
        return NULL;
    }
    return NULL;
}

static void *wifiCurrentConnect_cb(void *ptr, void *arg)
{
    RK_WIFI_INFO_Connection_s wifiInfo;
    if (getWifiConnectionInfo(&wifiInfo) < 0)
    {
        return NULL;
    }
    cJSON *item = cJSON_CreateObject();
    cJSON_AddStringToObject(item, "ssid", wifiInfo.ssid);
    cJSON_AddStringToObject(item, "bssid", wifiInfo.bssid);
    cJSON_AddStringToObject(item, "ip_address", wifiInfo.ip_address);
    cJSON_AddStringToObject(item, "mac_address", wifiInfo.mac_address);
    return item;
}

static set_attr_t g_wifi_set_attr[] = {
    {
        cloud_key : "WifiState",
        fun_type : LINK_FUN_TYPE_ATTR_REPORT,
        cb : WifiState_cb
    },
    {
        cloud_key : "WifiEnable",
        fun_type : LINK_FUN_TYPE_ATTR_REPORT_CTRL,
        cb : WifiEnable_cb
    },
    {
        cloud_key : "WifiScan",
        fun_type : LINK_FUN_TYPE_ATTR_CTRL,
        cb : WifiScan_cb
    },
    {
        cloud_key : "WifiScanR",
        fun_type : LINK_FUN_TYPE_ATTR_REPORT,
        cb : WifiScanR_cb
    },
    {
        cloud_key : "WifiConnect",
        fun_type : LINK_FUN_TYPE_ATTR_CTRL,
        cb : WifiConnect_cb
    },
    {
        cloud_key : "WifiDisconnect",
        fun_type : LINK_FUN_TYPE_ATTR_CTRL,
        cb : WifiDisconnect_cb
    },
    {
        cloud_key : "wifiCurrentConnect",
        fun_type : LINK_FUN_TYPE_ATTR_REPORT,
        cb : wifiCurrentConnect_cb
    },
};

static int set_attr_report_uds(cJSON *root, set_attr_t *attr)
{
    if (root == NULL)
    {
        return -1;
    }

    if (attr != NULL)
    {
        if (attr->fun_type == LINK_FUN_TYPE_ATTR_REPORT_CTRL || attr->fun_type == LINK_FUN_TYPE_ATTR_REPORT)
        {
            cJSON *item = (cJSON *)attr->cb(attr, NULL);
            if (item != NULL)
                cJSON_AddItemToObject(root, attr->cloud_key, item);
        }
    }
    return 0;
}

static int WiFiCallback(int event)
{
    g_wifi_set_attr[0].value.n = event;
    cJSON *root = cJSON_CreateObject();
    set_attr_report_uds(root, &g_wifi_set_attr[0]);
    send_event_uds(root);
}

static int set_attr_ctrl_uds(cJSON *root, set_attr_t *attr, cJSON *item)
{
    if (root == NULL)
    {
        return -1;
    }
    if (attr->fun_type == LINK_FUN_TYPE_ATTR_REPORT_CTRL || attr->fun_type == LINK_FUN_TYPE_ATTR_CTRL)
    {
        item = (cJSON *)attr->cb(attr, item);
        if (item != NULL)
        {
            cJSON_AddItemToObject(root, attr->cloud_key, item);
        }
    }
    return 0;
}

int wifi_recv_from_uds(cJSON *root, const char *type)
{
    int i;
    static const int attr_len = sizeof(g_wifi_set_attr) / sizeof(g_wifi_set_attr[0]);
    set_attr_t *attr = g_wifi_set_attr;

    cJSON *root_send = cJSON_CreateObject();
    if (strcmp(TYPE_GET, type) == 0)
    {
        for (i = 0; i < attr_len; ++i)
        {
            if (cJSON_HasObjectItem(root, attr[i].cloud_key))
            {
                set_attr_report_uds(root_send, &attr[i]);
            }
        }
    }
    else if (strcmp(TYPE_SET, type) == 0)
    {
        for (i = 0; i < attr_len; ++i)
        {
            if (cJSON_HasObjectItem(root, attr[i].cloud_key))
            {
                set_attr_ctrl_uds(root_send, &attr[i], cJSON_GetObjectItem(root, attr[i].cloud_key));
            }
        }
    }
    else
    {
        for (i = 0; i < attr_len; ++i)
        {
            set_attr_report_uds(root_send, &attr[i]);
        }
    }

    send_event_uds(root_send);
    return 0;
}

int wifi_task_init(void)
{
    wifiRegsiterCallback(WiFiCallback);
    return 0;
}

void wifi_task_deinit(void)
{
}
