#include "main.h"

#include "uds_protocol.h"
#include "uds_tcp_server.h"

#include "cloud_platform_task.h"
#include "wifi_task.h"
#include "rkwifi.h"
#include "linkkit_solo.h"

static void *WifiState_cb(void *ptr, void *arg)
{
    dzlog_warn("WifiState_cb");
    set_attr_t *attr = (set_attr_t *)ptr;
    attr->value.n = getWifiRunningState();
    cJSON *item = cJSON_CreateNumber(attr->value.n);
    return item;
}

static void *WifiEnable_cb(void *ptr, void *arg)
{
    if (NULL == arg)
        return NULL;
    cJSON *item = (cJSON *)arg;

    if (wifiEnable(item->valueint) < 0)
    {
        return NULL;
    }
    item = cJSON_CreateNumber(item->valueint);
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
    if (NULL == arg)
        return NULL;
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
    dzlog_warn("WifiConnect_cb ssid:%s,psk:%s,encryp:%d", ssid->valuestring, psk->valuestring, encryp->valueint);
    if (wifiConnect(ssid->valuestring, psk->valuestring, encryp->valueint) < 0)
    {
        return NULL;
    }
    dzlog_warn("WifiConnect_cb succcess");
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

static void *WifiCurConnected_cb(void *ptr, void *arg)
{
    RK_WIFI_INFO_Connection_s wifiInfo = {0};
    if (getWifiConnectionInfo(&wifiInfo) < 0)
    {
        dzlog_error("getWifiConnectionInfo error");
        // return NULL;
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
        fun_type : LINK_FUN_TYPE_ATTR_CTRL,
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
        cloud_key : "WifiCurConnected",
        fun_type : LINK_FUN_TYPE_ATTR_REPORT,
        cb : WifiCurConnected_cb
    },
};

static const int attr_len = sizeof(g_wifi_set_attr) / sizeof(g_wifi_set_attr[0]);
static set_attr_t *attr = g_wifi_set_attr;

int wifi_resp_get(cJSON *root, cJSON *resp)
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

int wifi_resp_getall(cJSON *root, cJSON *resp)
{
    for (int i = 0; i < attr_len; ++i)
    {
        set_attr_report_uds(resp, &attr[i]);
    }
    return 0;
}

int wifi_resp_set(cJSON *root, cJSON *resp)
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

void (*wifi_connected_cb)(int);
void register_wifi_connected_cb(void (*cb)(int))
{
    wifi_connected_cb = cb;
}
static int wiFiReport(int event)
{
    dzlog_warn("%s,%d", __func__, event);
    if (wifi_connected_cb != NULL)
    {
        if (event == RK_WIFI_State_CONNECTED)
        {
            wifi_connected_cb(1);
        }
        else if (event == RK_WIFI_State_DISCONNECTED)
        {
            wifi_connected_cb(0);
        }
    }

    cJSON *root = cJSON_CreateObject();
    cJSON_AddNumberToObject(root, "WifiState", event);

    return send_event_uds(root,NULL);
}

static int wiFiCallback(int event)
{
    if (event == RK_WIFI_State_CONNECTED && get_linkkit_connected_state() == 0)
    {
        return -1;
    }

    return wiFiReport(event);
}
static void linkkit_connected_cb(int connect)
{
    if (connect)
    {
        wiFiReport(RK_WIFI_State_CONNECTED);
    }
    else
    {
        wiFiReport(RK_WIFI_State_DISCONNECTED);
    }
}
int wifi_task_init(void)
{
    wifiInit();
    wifiRegsiterCallback(wiFiCallback);
    register_connected_cb(linkkit_connected_cb);
    return 0;
}