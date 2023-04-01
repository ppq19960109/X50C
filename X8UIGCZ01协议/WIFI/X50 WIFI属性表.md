# X50 WIFI属性表

[TOC]

## WIFI 属性
| 属性名称   | 描述     | 属性类型 | 取值范围                                                       | 设置 |  上报 |
| ---------- | -------- | -------- | ------------------------------------------------------------ | ---- | ---- | 
| WifiState  | WIFI状态 | Number   | 0 - 空闲<br>1 - 连接中<br>2 - 连接失败<br>3 - 因为密码错误连接失败<br>4 - 已连接<br>5 - 断开连接 | 0    | 1    |
| WifiEnable | WIFI使能 | Bool     | true - 使能<br>false - 失能                                  | 1    | 0    |

## WIFI STA属性
| 属性名称           | 描述             | 属性类型 | 取值范围               | 设置 | 上报 |
| ------------------ | ---------------- | -------- | ---------------------- | ---- | ---- |
| WifiScan           | WIFI扫描         | null     |                        | 1    | 0    |
| WifiScanR          | WIFI扫描结果     | String   | 4096字节               | 0    | 1    |
| WifiConnect        | WIFI连接         | Object   | ConnectInfo属性        | 1    | 0    |
| WifiDisconnect     | WIFI断开连接     | null     |                        | 1    | 0    |
| WifiCurConnected | WIFI当前连接信息 | Object   | CurrentConnectInfo属性 | 0    | 1    |

> ConnectInfo属性

> | 属性名称 | 描述     | 属性类型 | 取值范围                                   | 设置 | 上报 |
> | -------- | -------- | -------- | ------------------------------------------ | ---- | ---- |
> | ssid     | 用户名   | String   | 64字节                                     | 1    | 0    |
> | psk      | 密码     | String   | 8 - 32字节                                     | 1    | 0    |
> | encryp   | 加密方式 | Number   | 0 - 无加密<br/>1 - WPA加密<br/>2 - WEP加密 | 1    | 0    |

> CurrentConnectInfo属性

> | 属性名称    | 描述    | 属性类型 | 取值范围 | 设置 | 上报 |
> | ----------- | ------- | -------- | -------- | ---- | ---- |
> | ssid        | 用户名  | String   | 64字节   | 0    | 1    |
> | bssid       | bssid   | String   | 20字节   | 0    | 1    |
> | ip_address  | ip地址  | String   | 20字节   | 0    | 1    |
> | mac_address | mac地址 | String   | 20字节   | 0    | 1    |

## WIFI AP属性
| 属性名称      | 描述        | 属性类型 | 取值范围   | 设置 | 上报 |
| ------------- | ----------- | -------- | ---------- | ---- | ---- |
| WifiEnableAP  | WIFI AP开启 | Object   | APInfo属性 | 1    | 0    |
| WifiDisableAP | WIFI AP关闭 | null     |            | 1    | 0    |
> APInfo属性

> | 属性名称 | 描述       | 属性类型 | 取值范围 | 设置 | 上报 |
> | -------- | ---------- | -------- | -------- | ---- | ---- |
> | ssid     | 用户名     | String   | 64字节   | 1    | 0    |
> | passwd   | 密码       | String   | 32字节   | 1    | 0    |
> | ip       | AP的ip地址 | String   | 16字节   | 1    | 0    |

