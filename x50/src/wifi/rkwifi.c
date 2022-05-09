#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "rkwifi.h"

AppWiFiCallback app_wifi_cb = NULL;

/**
 * @brief rk_wifi_callback WiFi回调函数
 * @param status
 * @return
 */
int rk_wifi_cb(RK_WIFI_RUNNING_State_e state)
{
    printf("rk_wifi_cb state:%d\n", state);

    if (app_wifi_cb != NULL)
    {
        app_wifi_cb(state);
    }
    return 0;
}

/**
 * @brief wifiRegsiterCallback 注册WiFi状态回调函数
 * @param callback 回调函数
 */
void wifiRegsiterCallback(AppWiFiCallback cb)
{
    app_wifi_cb = cb;
    RK_wifi_register_callback(rk_wifi_cb);
    // RK_wifi_ble_register_callback(rk_wifi_cb);
}

/**
 * @brief wifiInit WiFi初始化
 */
void wifiInit()
{
    // RK_wifi_disable_ap();
    //获取Mac地址
    char mac[16] = {0};
    RK_wifi_get_mac(mac);
    printf("RK_wifi Mac:%s\n", mac);

    char hostname_get[16] = {0};
    RK_wifi_get_hostname(hostname_get, sizeof(hostname_get));
    printf("RK_wifi hostname_get:%s\n", hostname_get);

    char hostname[16] = {0};
    sprintf(hostname, "X50BCZ_%c%c%c%c", mac[12], mac[13], mac[15], mac[16]);
    // char cmd[64] = {0};
    printf("RK_wifi hostname_set:%s\n", hostname);
    if (strcmp(hostname_get, hostname) != 0)
    {
        RK_wifi_set_hostname(hostname);

        //     sprintf(cmd, "echo %s > /etc/hostname", hostname);
        //     system(cmd);

        // sprintf(cmd, "export HOSTNAME=%s", hostname);
        // system(cmd);
        // setenv("HOSTNAME", hostname, 1);
    }
}

/**
 * @brief wifiEnable
 */
int wifiEnable(const int enable)
{
    int ret = 0;
    // ret = RK_wifi_enable(enable);
    // if (ret < 0)
    // {
    //     printf("RK_wifi_enable fail!\n");
    // }
    // else
    // {
    //     printf("RK_wifi_enable:%d\n", enable);
    // }
    if (enable)
        system("wpa_cli enable_network all && wpa_cli save_config && wpa_cli wpa_cli reconnect");
    else
        system("wpa_cli disable_network all && wpa_cli save_config");
    return ret;
}

/**
 * @brief wifiDisconnect WiFi连接
 */
int wifiConnect(const char *ssid, const char *psk, const RK_WIFI_CONNECTION_Encryp_e encryp)
{
    int result = -1;
    printf("ssid:%s psk:%s encryp:%d\n", ssid, psk, encryp);
    // RK_wifi_disconnect_network();
    result = RK_wifi_connect1(ssid, psk, encryp, 0);
    // result = RK_wifi_connect(ssid, psk);
    if (result < 0)
    {
        printf("RK_wifi_connect_network fail!\n");
        rk_wifi_cb(getWifiRunningState());
    }

    return result;
}

/**
 * @brief wifiDisconnect 断开WiFi连接
 */
int wifiDisconnect()
{
    int result = RK_wifi_disconnect_network();
    if (result < 0)
    {
        printf("RK_wifi_disconnect_network fail!\n");
    }
    return result;
}

/**
 * @brief getWifiRunningState 获取WiFi运行状态
 * @return
 */
int getWifiRunningState()
{
    RK_WIFI_RUNNING_State_e state;
    RK_wifi_running_getState(&state);
    printf("getWifiRunningState WiFi state :%d\n", state);
    return state;
}
/**
 * @brief getCurrentWifi 获取当前WiFi信息
 * @return
 */
int getWifiConnectionInfo(RK_WIFI_INFO_Connection_s *wifiInfo)
{
    if (RK_WIFI_State_CONNECTED != getWifiRunningState())
        return -1;
    return RK_wifi_running_getConnectionInfo(wifiInfo);
}
int wifiScan()
{
    int ret = RK_wifi_scan();
    printf("WiFi scan\n");
    if (ret < 0)
    {
        printf("WiFi scan error\n");
    }
    return ret;
}
/**
 * @brief getWifiScanR 获取WiFi扫描结果
 * @return Json格式的无线数据列表
 */
char *getWifiScanR()
{
    char *wifiList = RK_wifi_scan_r();
    //    char *wifiList =RK_wifi_scan_r_sec(10);
    printf("WiFi List:%s\n", wifiList);
    return wifiList;
}

/**
 * @brief checkNetWorkOnline 检测网络是否接通
 * @return true->OK false->NG
 */
bool WifiPing()
{
    bool status = false;
    int ret = RK_wifi_ping();
    if (ret == 0x01)
    {
        status = true;
        printf("Network connection OK!\n");
    }
    else
    {
        status = false;
        printf("Network connection Fail!\n");
    }
    return status;
}
