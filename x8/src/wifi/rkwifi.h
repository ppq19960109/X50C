#ifndef RKWIFI_H
#define RKWIFI_H

#ifdef __cplusplus
extern "C"
{
#endif
#include <DeviceIo/Rk_wifi.h>
#include <stdbool.h>
    typedef int (*AppWiFiCallback)(int event);
    void wifiRegsiterCallback(AppWiFiCallback callback);

    void wifiInit(); //WiFi初始化
    int wifiEnable(const int enable);
    int wifiConnect(const char *ssid, const char *psk, const RK_WIFI_CONNECTION_Encryp_e encryp); //WiFi连接
    int wifiDisconnect();                                                                         //断开WiFi连接
    int getWifiRunningState();                                                                    //获取WiFi运行状态
    int getWifiConnectionInfo(RK_WIFI_INFO_Connection_s *wifiInfo);                               //获取当前WiFi信息
    int wifiScan();                                                                               //获取扫描结果
    char *getWifiScanR();                                                                         //获取扫描结果
    bool wifiPing(char *addr);                                                                              //检测网络是否接通

#ifdef __cplusplus
}
#endif
#endif // RKWIFI_H
