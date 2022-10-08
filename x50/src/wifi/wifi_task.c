#include "main.h"

#include "uds_protocol.h"
#include "uds_tcp_server.h"

#include "cloud_platform_task.h"
#include "wifi_task.h"
#include "rkwifi.h"
#include "link_solo.h"

static int wiFiReport(int event);

static int g_wifi_state = 0, g_link_state = 0;
// static int back_online = 0;
int get_link_state(void)
{
    if (g_link_state == RK_WIFI_State_CONNECTED)
        return 1;
    return 0;
}
void wifi_reset(void)
{
    link_disconnect();
    g_wifi_state = 0;
    g_link_state = 0;
    wifiEnable(1);
    wifiDisconnect();
    systemRun("wpa_cli remove_network all && wpa_cli save_config && sync");
}
static void *WifiState_cb(void *ptr, void *arg)
{
    set_attr_t *attr = (set_attr_t *)ptr;

    cloud_dev_t *cloud_dev = get_cloud_dev();
    int link_connected_state = get_link_connected_state();
    int secret_len = strlen(cloud_dev->device_secret);
    int wifi_state = getWifiRunningState();

    g_wifi_state = wifi_state;
    if (wifi_state == RK_WIFI_State_CONNECTED && link_connected_state == 0 && secret_len > 0)
    {
        attr->value.n = RK_WIFI_State_DISCONNECTED;
    }
    else
        attr->value.n = wifi_state;
    dzlog_warn("WifiState_cb wifi_state:%d link_connected_state:%d", wifi_state, link_connected_state);
    // if (attr->value.n == RK_WIFI_State_CONNECTED && get_link_connected_state() == 0)
    // {
    //     attr->value.n = RK_WIFI_State_DISCONNECTED;
    // }
    g_link_state = attr->value.n;
    cJSON *item = cJSON_CreateNumber(attr->value.n);
    return item;
}

static void *WifiEnable_cb(void *ptr, void *arg)
{
    if (NULL == arg)
        return NULL;
    cJSON *item = (cJSON *)arg;

    if (item->valueint == 0)
    {
        link_disconnect();
    }
    if (wifiEnable(item->valueint) < 0)
    {
        item = cJSON_CreateNumber(0);
    }
    else
    {
        if (item->valueint == 0)
        {
            wiFiReport(RK_WIFI_State_DISCONNECTED);
        }
        item = cJSON_CreateNumber(item->valueint);
    }
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
    g_link_state = g_wifi_state = 0;
    link_disconnect();
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
        return NULL;
        // memset(&wifiInfo, 0, sizeof(RK_WIFI_INFO_Connection_s));
    }
    cJSON *item = cJSON_CreateObject();
    cJSON_AddStringToObject(item, "ssid", wifiInfo.ssid);
    cJSON_AddStringToObject(item, "bssid", wifiInfo.bssid);
    cJSON_AddStringToObject(item, "ip_address", wifiInfo.ip_address);
    cJSON_AddStringToObject(item, "mac_address", wifiInfo.mac_address);
    return item;
}

static void *BackOnline_cb(void *ptr, void *arg)
{
    // back_online = 1;
    link_disconnect();
    return NULL;
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
    {
        cloud_key : "BackOnline",
        fun_type : LINK_FUN_TYPE_ATTR_CTRL,
        cb : BackOnline_cb
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
    g_link_state = g_wifi_state = event;
    cJSON *root = cJSON_CreateObject();
    cJSON_AddNumberToObject(root, "WifiState", event);

    return send_event_uds(root, NULL);
}

static int wiFiCallback(int event)
{
    cloud_dev_t *cloud_dev = get_cloud_dev();
    int link_connected_state = get_link_connected_state();
    int secret_len = strlen(cloud_dev->device_secret);
    dzlog_warn("wiFiCallback:%d link_connected_state:%d secret_len:%d", event, link_connected_state, secret_len);
    g_wifi_state = event;
    // if (event > RK_WIFI_State_CONNECTING)
    // {
    //     link_disconnect();
    // }
    // if (event == RK_WIFI_State_CONNECTED && secret_len > 0)
    // {
    //     return -1;
    // }
    if (event == RK_WIFI_State_CONNECTED)
    {
        cloud_attr_t *attr = get_attr_ptr("ProductionTestStatus");
        if (attr == NULL)
        {
            dzlog_error("not attr ProductionTestStatus");
            if (link_connected_state == 0 && secret_len > 0)
                return -1;
        }
        else
        {
            if (*attr->value == 0)
            {
                dzlog_warn("wait ProductionTestStatus");
                if (link_connected_state == 0 && secret_len > 0)
                    return -1;
            }
        }
    }
    return wiFiReport(event);
}
static void linkkit_connected_cb(int connect)
{
    dzlog_warn("linkkit_connected_cb:%d", connect);
    if (connect)
    {
        send_all_to_cloud();
        wiFiReport(RK_WIFI_State_CONNECTED);
    }
    else
    {
        if (g_link_state != 0)
        {
            // if (RK_WIFI_State_CONNECTED == getWifiRunningState())
            //     systemRun("(wpa_cli list_networks | tail -n +3 | grep 'TEMP-DISABLED' | awk '{system(\"wpa_cli remove_network \" $1)}' && wpa_cli save_config) &");
            wiFiReport(RK_WIFI_State_DISCONNECTED);
        }
    }
}
static int link_wifi_state_cb()
{
    dzlog_warn("link_wifi_state_cb:%d", g_wifi_state);
    if (RK_WIFI_State_CONNECTED != g_wifi_state)
    {
        g_wifi_state = getWifiRunningState();
    }
    if (RK_WIFI_State_CONNECTED == g_wifi_state)
    {
        // if (back_online == 0)
        // {
        //     wifiScan();
        //     sleep(1);
        // }
        // else
        // {
        //     back_online = 0;
        // }
        return 1;
    }
    return 0;
}
int wifi_task_init(void)
{
    wifiInit();
    wifiRegsiterCallback(wiFiCallback);
    register_connected_cb(linkkit_connected_cb);
    register_link_wifi_state_cb(link_wifi_state_cb);
    return 0;
}