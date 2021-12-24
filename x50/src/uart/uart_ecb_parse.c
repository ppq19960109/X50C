#include "main.h"

#include "linkkit_func.h"
#include "uart_ecb_task.h"
#include "uart_ecb_parse.h"
#include "uart_resend.h"
#include "uart_cloud_task.h"
#include "uds_protocol.h"

static unsigned short ecb_seq_id = 0;
static int get_ack_count = 0;
typedef enum
{
    ECB_UART_READ_VALID = 0,
    ECB_UART_READ_NO_HEADER,
    ECB_UART_READ_LEN_SMALL,
    ECB_UART_READ_LEN_ERR,
    ECB_UART_READ_CHECK_ERR,
    ECB_UART_READ_TAILER_ERR
} ecb_uart_read_status_t;

int get_ecb_disconnect_count(void)
{
    return get_ack_count;
}
static void InvertUint16(unsigned short *dBuf, unsigned short *srcBuf)
{
    int i;
    unsigned short tmp[4] = {0};

    for (i = 0; i < 16; i++)
    {
        if (srcBuf[0] & (1 << i))
            tmp[0] |= 1 << (15 - i);
    }
    dBuf[0] = tmp[0];
}

unsigned short CRC16_MAXIM(const unsigned char *data, unsigned int datalen)
{
    unsigned short wCRCin = 0x0000;
    unsigned short wCPoly = 0x8005;

    InvertUint16(&wCPoly, &wCPoly);
    while (datalen--)
    {
        wCRCin ^= *(data++);
        for (int i = 0; i < 8; i++)
        {
            if (wCRCin & 0x01)
                wCRCin = (wCRCin >> 1) ^ wCPoly;
            else
                wCRCin = wCRCin >> 1;
        }
    }
    return (wCRCin ^ 0xFFFF);
}

int uart_ecb_send_msg(const unsigned char command, unsigned char *msg, const int msg_len, unsigned char resend)
{
    
    int index = 0;
    unsigned char *send_msg = (unsigned char *)malloc(ECB_MSG_MIN_LEN + msg_len);
    send_msg[index++] = 0xe6;
    send_msg[index++] = 0xe6;
    unsigned short seq_id = ecb_seq_id++;
    send_msg[index++] = seq_id >> 8;
    send_msg[index++] = seq_id & 0xff;
    send_msg[index++] = command;
    send_msg[index++] = msg_len >> 8;
    send_msg[index++] = msg_len & 0xff;
    if (msg_len > 0 && msg != NULL)
    {
        memcpy(&send_msg[index], msg, msg_len);
        index += msg_len;
    }
    unsigned short crc16 = CRC16_MAXIM((const unsigned char *)(send_msg + 2), index - 2);
    send_msg[index++] = crc16 >> 8;
    send_msg[index++] = crc16 & 0xff;
    send_msg[index++] = 0x6e;
    send_msg[index++] = 0x6e;
    return uart_send_ecb(send_msg, ECB_MSG_MIN_LEN + msg_len, resend, 0);
}

void send_error_to_cloud(int error_code)
{
    unsigned char payload[8] = {0};
    int index = 0;
    int code = 1 << (error_code - 1);
    payload[index++] = 0x0a;
    payload[index++] = code >> 24;
    payload[index++] = code >> 16;
    payload[index++] = code >> 6;
    payload[index++] = code;
    payload[index++] = 0x0b;
    payload[index++] = error_code;
    send_data_to_cloud(payload, index);
}
int uart_ecb_get_msg()
{
    if (get_ack_count <= ECB_DISCONNECT_COUNT)
        ++get_ack_count;
    if (get_ack_count == ECB_DISCONNECT_COUNT)
    {
        send_error_to_cloud(9);
    }
    return uart_ecb_send_msg(ECB_UART_COMMAND_GET, NULL, 0, 0);
}
int clound_to_uart_ecb_msg(unsigned char *msg, const int msg_len)
{
    return uart_ecb_send_msg(ECB_UART_COMMAND_SET, msg, msg_len, 1);
}
int ecb_uart_send_nak(unsigned char error_code)
{
    return uart_ecb_send_msg(ECB_UART_COMMAND_NAK, &error_code, 1, 0);
}

int ecb_uart_send_factory(ft_ret_t ret)
{
    unsigned char send[2] = {0};
    send[0] = UART_STORE_FT_RESULT;
    send[1] = ret;
    return uart_ecb_send_msg(ECB_UART_COMMAND_STORE, send, sizeof(send), 1);
}

static int send_factory_test(void)
{
    cloud_dev_t *cloud_dev = get_cloud_dev();

    int index = 0;
    unsigned char send_msg[33] = {0};
    char mac[6] = {0};
    // 软件版本
    send_msg[index++] = UART_STORE_SW_VERSION;
    send_msg[index++] = cloud_dev->software_ver[0] - 0x30;
    send_msg[index++] = cloud_dev->software_ver[2] - 0x30;
    send_msg[index++] = cloud_dev->software_ver[4] - 0x30;
    // 硬件版本
    send_msg[index++] = UART_STORE_HW_VERSION;
    send_msg[index++] = (cloud_dev->hardware_ver / 100) % 10;
    send_msg[index++] = (cloud_dev->hardware_ver / 10) % 10;
    send_msg[index++] = cloud_dev->hardware_ver % 10;
    // MAC地址
    send_msg[index++] = UART_STORE_MAC;
    //wifi mac
    getNetworkMac(ETH_NAME, mac, sizeof(mac), NULL);
    send_msg[index++] = mac[0];
    send_msg[index++] = mac[1];
    send_msg[index++] = mac[2];
    send_msg[index++] = mac[3];
    send_msg[index++] = mac[4];
    send_msg[index++] = mac[5];
    //蓝牙mac
    send_msg[index++] = 0;
    send_msg[index++] = 0;
    send_msg[index++] = 0;
    send_msg[index++] = 0;
    send_msg[index++] = 0;
    send_msg[index++] = 0;
    return uart_ecb_send_msg(ECB_UART_COMMAND_STORE, send_msg, index, 1);
}

void keypress_local_pro(unsigned char value)
{
    switch (value)
    {
    case KEYPRESS_LOCAL_POWER_ON: /* 上电提示 */
        break;
    case KEYPRESS_LOCAL_FT_START: /* 厂测开始 */
    {
        dzlog_error("Factory test began");
        send_factory_test();
        cJSON *resp = cJSON_CreateObject();
        cJSON_AddNullToObject(resp, "ProductionTest");
        send_event_uds(resp);
    }
    break;
    case KEYPRESS_LOCAL_FT_END: /* 厂测结束 */
        dzlog_error("End of the factory test");
        break;
    case KEYPRESS_LOCAL_RUN_IN_DO: /* 蒸烤模式下门未关闭时，按运行键 */
        break;
    case KEYPRESS_LOCAL_FT_WIFI: /* 整机厂测，通讯及WIFI检测 */
        dzlog_info("factory test: wifi test ");
        break;
    case KEYPRESS_LOCAL_STEAM_START: /* 启动蒸烤箱 */
    {
        dzlog_info("SteamStart");
        cJSON *resp = cJSON_CreateObject();
        cJSON_AddNullToObject(resp, "SteamStart");
        send_event_uds(resp);
    }
    break;
    case KEYPRESS_LOCAL_FT_BT: /* 整机厂测，蓝牙检测 */
        dzlog_info("factory test: bluetooth test ");
        break;
    case KEYPRESS_LOCAL_FT_SPEAKER: /* 整机厂测，喇叭检测 */
        dzlog_info("factory test: speaker test ");
        break;
    case KEYPRESS_LOCAL_FT_RESET: /* 整机厂测，恢复出厂设置 */
        dzlog_info("factory test: reset test ");
        if (0 != linkkit_unbind())
        {
            dzlog_error("factory test: reset error ");
            ecb_uart_send_factory(FT_RET_ERR_RESET);
            break;
        }

        sleep(1);
        ecb_uart_send_factory(FT_RET_OK_RESET);
        break;
    case KEYPRESS_LOCAL_RESET: /* 通讯板重启（主要用于强制断电前进行通知和追溯） */
    case 0xFF:                 /*重启 */
        dzlog_error("now reboot......");
        sync();
        reboot(RB_AUTOBOOT);
        break;

    default:
        break;
    }
}

static int uart_data_parse(const unsigned char *in, const int in_len, int *end)
{
    int index, i;
    if (in_len < 2)
    {
        *end = 0;
        dzlog_error("input len too small");
        return ECB_UART_READ_LEN_SMALL;
    }

    for (i = 0; i < in_len - 1; ++i)
    {
        if (in[i] == 0xe6 && in[i + 1] == 0xe6)
            break;
    }
    if (i >= in_len - 1)
    {
        dzlog_error("no header was detected");
        return ECB_UART_READ_NO_HEADER;
    }
    index = i;

    if (index + in_len < ECB_MSG_MIN_LEN)
    {
        *end = index;
        dzlog_error("input len too small");
        return ECB_UART_READ_LEN_SMALL;
    }
    int msg_index = 2;

    const unsigned short seq_id = in[index + msg_index] * 256 + in[index + msg_index + 1];
    msg_index += 2;
    unsigned char command = in[index + msg_index];
    msg_index += 1;
    int data_len = in[index + msg_index] * 256 + in[index + msg_index + 1];
    if (data_len > 1024)
    {
        *end = index + 2;
        dzlog_error("input data len error");
        return ECB_UART_READ_LEN_ERR;
    }
    msg_index += 2;
    const unsigned char *payload = &in[index + msg_index];
    msg_index += data_len;
    if (index + msg_index + 2 + 2 > in_len)
    {
        if (in[in_len - 1] == 0x6e && in[in_len - 2] == 0x6e)
        {
            *end = index + 2;
            dzlog_error("input data len error2");
            return ECB_UART_READ_LEN_ERR;
        }
        else
        {
            *end = index;
            dzlog_error("input data len too small");
            return ECB_UART_READ_LEN_SMALL;
        }
    }
    else if (in[index + msg_index + 2] != 0x6e || in[index + msg_index + 2 + 1] != 0x6e)
    {
        *end = index + msg_index + 2 + 2;
        dzlog_error("no tailer was detected");
        ecb_uart_send_nak(ECB_NAK_TAILER);
        return ECB_UART_READ_TAILER_ERR;
    }
    unsigned short crc16 = CRC16_MAXIM(&in[index + 2], msg_index - 2);
    unsigned short check_sum = in[index + msg_index] * 256 + in[index + msg_index + 1];
    dzlog_info("crc16:%x,check_sum:%x", crc16, check_sum);
    msg_index += 2;
    msg_index += 2;

    *end = index + msg_index;
    // if (crc16 != check_sum)
    // {
    //     dzlog_error( "data check error");
    //     ecb_uart_send_nak(ECB_NAK_CHECKSUM);
    //     return ECB_UART_READ_CHECK_ERR;
    // }
    //----------------------
    dzlog_info("command:%d", command);
    hdzlog_info((unsigned char *)payload, data_len);
    if (command == ECB_UART_COMMAND_EVENT || command == ECB_UART_COMMAND_KEYPRESS)
    {
        uart_ecb_send_msg(ECB_UART_COMMAND_ACK, NULL, 0, 0);
        if (command == ECB_UART_COMMAND_EVENT)
        {
            send_data_to_cloud(payload, data_len);
        }
        else if (command == ECB_UART_COMMAND_KEYPRESS)
        {
            for (int i = 0; i < data_len; ++i)
            {
                if (payload[i] == 0xf1)
                {
                    keypress_local_pro(payload[i + 1]);
                    i += 1;
                }
                else if (payload[i] == 0xf9)
                {
                    cJSON *resp = cJSON_CreateObject();
                    cJSON_AddNullToObject(resp, "SteamStart");
                    send_event_uds(resp);
                    i += 1;
                }
            }
        }
        else
        {
        }
    }
    else if (command == ECB_UART_COMMAND_GETACK)
    {
        if (get_ack_count >= ECB_DISCONNECT_COUNT)
        {
            send_error_to_cloud(0);
        }
        get_ack_count = 0;
        // ecb_resend_list_del_by_id(seq_id);
        send_data_to_cloud(payload, data_len);
    }
    else if (command == ECB_UART_COMMAND_ACK || command == ECB_UART_COMMAND_NAK)
    {
        ecb_resend_list_del_by_id(seq_id);
    }
    else
    {
        ecb_uart_send_nak(ECB_NAK_COMMAND_UNKNOWN);
    }

    return ECB_UART_READ_VALID;
}

void uart_read_parse(unsigned char *in, int *in_len)
{
    int start = 0;
    ecb_uart_read_status_t status;
    for (;;)
    {
        dzlog_info("start:%d,in len:%d", start, *in_len);
        status = uart_data_parse(&in[start], *in_len, &start);
        *in_len -= start;

        if (status == ECB_UART_READ_VALID || status == ECB_UART_READ_CHECK_ERR || status == ECB_UART_READ_TAILER_ERR || status == ECB_UART_READ_LEN_ERR)
        {
            if (*in_len <= 0)
            {
                break;
            }
        }
        else if (status == ECB_UART_READ_LEN_SMALL)
        {
            break;
        }
        else if (status == ECB_UART_READ_NO_HEADER)
        {
            *in_len = 0;
            break;
        }
        else
        {
        }
    }
    dzlog_info("last start:%d,in len:%d", start, *in_len);
    if (*in_len <= 0)
    {
        *in_len = 0;
    }
    else if (start > 0)
    {
        memmove(in, &in[start], *in_len);
    }
}