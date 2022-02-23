#ifndef _ECB_UART_PARSE_H_
#define _ECB_UART_PARSE_H_

#define ECB_MSG_MIN_LEN (11)

typedef enum
{
    ECB_UART_READ_VALID = 0,
    ECB_UART_READ_NO_HEADER,
    ECB_UART_READ_LEN_SMALL,
    ECB_UART_READ_LEN_ERR,
    ECB_UART_READ_CHECK_ERR,
    ECB_UART_READ_TAILER_ERR
} ecb_uart_read_status_t;

typedef enum
{
    ECB_UART_COMMAND_GET = 0x01, /* GET */
    ECB_UART_COMMAND_SET,        /* SET */
    ECB_UART_COMMAND_EVENT,
    ECB_UART_COMMAND_KEYPRESS,
    ECB_UART_COMMAND_STORE,
    ECB_UART_COMMAND_GETACK = 0X0d,
    ECB_UART_COMMAND_ACK,
    ECB_UART_COMMAND_NAK,
} ecb_uart_command_type_t;

typedef enum
{
    ECB_NAK_CHECKSUM = 0x01, /*  */
    ECB_NAK_TAILER,          /*  */
    ECB_NAK_COMMAND_VERSION,
    ECB_NAK_COMMAND_UNKNOWN,
    ECB_NAK_COMMAND_ATTR_MISMATCH,
    ECB_NAK_ATTR_UNKNOWN,
    ECB_NAK_ATTR_OVERRANGE,
    ECB_NAK_NO_ALLOW,
    ECB_NAK_RECV_ABNORMAL,
    ECB_NAK_UNDEFINED_ERROR,
} ecb_uart_nak_type_t;

typedef enum
{
    KEYPRESS_LOCAL_POWER_ON = 0X01,  /* 上电提示 */
    KEYPRESS_LOCAL_RESET = 0X02,     /* 通讯板重启（主要用于强制断电前进行通知和追溯） */
    KEYPRESS_LOCAL_FT_START = 0X03,  /* 厂测开?*/
    KEYPRESS_LOCAL_FT_END = 0X04,    /* 厂测结束 */
    KEYPRESS_LOCAL_RUN_IN_DO = 0X05, /* 蒸烤模式下门未关闭时，按运行?*/
    KEYPRESS_LOCAL_POWER_LONG = 0X06,
    KEYPRESS_LOCAL_POWER_SHORT = 0X07,
    KEYPRESS_LOCAL_HOOD_SPEED = 0X08,
    KEYPRESS_LOCAL_HOOD_LIGHT = 0X09,
    KEYPRESS_LOCAL_STEAM_START = 0X0A,
    KEYPRESS_LOCAL_FT_WIFI = 0XA1,    /* 整机厂测，通讯及WIFI检?*/
    KEYPRESS_LOCAL_FT_BT = 0XA2,      /* 整机厂测，蓝牙检?*/
    KEYPRESS_LOCAL_FT_SPEAKER = 0XA3, /* 整机厂测，喇叭检?*/
    KEYPRESS_LOCAL_FT_RESET = 0XA4,   /* 整机厂测，恢复出厂设?*/
} keypress_local_type_t;

typedef enum
{
    FT_RET_OK_WIFI = 0,       /* 厂测结果--WIFI成功 */
    FT_RET_ERR_WIFI = 1,      /* 厂测结果--WIFI异常 */
    FT_RET_ERR_BT = 2,        /* 厂测结果--蓝牙异常 */
    FT_RET_ERR_RESET = 3,     /* 厂测结果--恢复出厂设置失败 */
    FT_RET_OK_RESET = 4,      /* 厂测结果--恢复出厂设置成功 */
    FT_RET_OK_META = 5,       /* 厂测结果--四元组失败 */
    FT_RET_ERR_WIFI_WEAK = 6, /* 厂测结果--WIFI信号弱 */
} ft_ret_t;

typedef enum
{
    /**********************系统类*********************/
    UART_STORE_NET_STATE = 0x06, /* 联网状态 */
    UART_STORE_BT_STATE = 0x08,  /* 蓝牙模式状态 */
    /**********************服务类*********************/
    UART_STORE_TIME = 0x61, /* 时间 */
    /**********************特殊功能类*********************/
    UART_STORE_FT_RESULT = 0XF2, /* 厂测结果 */
    UART_STORE_SW_VERSION,       /* 通讯板软件版本 */
    UART_STORE_HW_VERSION,       /* 通讯板硬件版本 */
    UART_STORE_MAC,              /* 通讯板MAC地址 */
    UART_STORE_CMD_SRC,          /* 指令来源 */
} ecb_uart_store_type_t;

#define WIFI_TEST_SSID "moduletest" /* 厂测SSID */
#define WIFI_TEST_PASSWD "58185818" /* 厂测PSK */

#define ECB_DISCONNECT_COUNT (5)
int ecb_disconnect_count(void);

int ecb_uart_msg_get(void);
int ecb_uart_send_cloud_msg(unsigned char *msg, const int msg_len);
int ecb_uart_send_factory(ft_ret_t ret);

int ecb_uart_parse_msg(const unsigned char *in, const int in_len, int *end);
void uart_parse_msg(unsigned char *in, int *in_len, int(func)(const unsigned char *, const int, int *));

#endif