#include "main.h"

#include "linkkit_func.h"
#include "ecb_uart.h"
#include "ecb_uart_parse_msg.h"
#include "uart_resend.h"
#include "cloud_platform_task.h"
#include "uds_protocol.h"

static int get_ack_count = 0;

int ecb_disconnect_count(void)
{
    return get_ack_count;
}

static void ecb_send_error_cloud(int error_code)
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

int ecb_uart_msg_get(void)
{
    if (get_ack_count <= ECB_DISCONNECT_COUNT)
        ++get_ack_count;
    if (get_ack_count == ECB_DISCONNECT_COUNT)
    {
        ecb_send_error_cloud(9);
    }
    return ecb_uart_send_msg(ECB_UART_COMMAND_GET, NULL, 0, 0);
}

int ecb_uart_send_cloud_msg(unsigned char *msg, const int msg_len)
{
    return ecb_uart_send_msg(ECB_UART_COMMAND_SET, msg, msg_len, 1);
}
int ecb_uart_send_nak(unsigned char error_code)
{
    return ecb_uart_send_msg(ECB_UART_COMMAND_NAK, &error_code, 1, 0);
}

int ecb_uart_send_factory(ft_ret_t ret)
{
    unsigned char send[2] = {0};
    send[0] = UART_STORE_FT_RESULT;
    send[1] = ret;
    return ecb_uart_send_msg(ECB_UART_COMMAND_STORE, send, sizeof(send), 1);
}

void keypress_local_pro(unsigned char value)
{
    switch (value)
    {
    case KEYPRESS_LOCAL_POWER_ON: /* 上电提示 */
        break;
    case KEYPRESS_LOCAL_FT_START: /* 厂测开始 */
    {
        dzlog_warn("Factory test began");
        cJSON *resp = cJSON_CreateObject();
        cJSON_AddNullToObject(resp, "ProductionTest");
        send_event_uds(resp, NULL);
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
        send_event_uds(resp, NULL);
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

int ecb_uart_parse_msg(const unsigned char *in, const int in_len, int *end)
{
    int index = 0, i;
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

    if (in_len - index < ECB_MSG_MIN_LEN)
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
    if (data_len > 512)
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
    if (crc16 != check_sum)
    {
        dzlog_error("data check error");
        // ecb_uart_send_nak(ECB_NAK_CHECKSUM);
        // return ECB_UART_READ_CHECK_ERR;
    }
    //----------------------
    dzlog_info("command:%d", command);
    hdzlog_info((unsigned char *)payload, data_len);
    if (command == ECB_UART_COMMAND_EVENT || command == ECB_UART_COMMAND_KEYPRESS)
    {
        ecb_uart_send_msg(ECB_UART_COMMAND_ACK, NULL, 0, 0);
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
            ecb_send_error_cloud(0);
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

void uart_parse_msg(unsigned char *in, int *in_len, int(func)(const unsigned char *, const int, int *))
{
    int index = 0;
    int msg_len = *in_len;
    ecb_uart_read_status_t status;
    for (;;)
    {
        dzlog_info("index:%d,msg_len:%d", index, msg_len);
        status = func(&in[index], msg_len, &index);
        msg_len -= index;

        if (status == ECB_UART_READ_VALID || status == ECB_UART_READ_CHECK_ERR || status == ECB_UART_READ_TAILER_ERR || status == ECB_UART_READ_LEN_ERR)
        {
        }
        else if (status == ECB_UART_READ_LEN_SMALL)
        {
            break;
        }
        else if (status == ECB_UART_READ_NO_HEADER)
        {
            msg_len = 0;
            break;
        }
        else
        {
            dzlog_error("error return value..");
        }
        if (msg_len <= 0)
        {
            break;
        }
    }
    index = *in_len - msg_len;

    dzlog_info("last move index:%d,msg_len:%d", index, msg_len);
    if (msg_len <= 0)
    {
        msg_len = 0;
    }
    else if (index > 0)
    {
        memmove(in, &in[index], msg_len);
    }
    *in_len = msg_len;
}