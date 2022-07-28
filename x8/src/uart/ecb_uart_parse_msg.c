#include "main.h"

// #include "link_reset_posix.h"
// #include "uart_resend.h"
#include "ecb_uart.h"
#include "ecb_uart_parse_msg.h"
#include "cloud_platform_task.h"
#include "uds_protocol.h"

static int msg_get_timeout_count = 0;
static int ecb_heart_count = 0;

void send_error_to_cloud(int error_code)
{
    unsigned char payload[8] = {0};
    int index = 0, code = 0;
    if (error_code > 0)
        code = 1 << (error_code - 1);

    payload[index++] = 0x0a;
    payload[index++] = code >> 24;
    payload[index++] = code >> 16;
    payload[index++] = code >> 8;
    payload[index++] = code;
    payload[index++] = 0x0b;
    payload[index++] = error_code;
    send_data_to_cloud(payload, index, ECB_UART_COMMAND_EVENT);
}

int ecb_uart_msg_get(bool increase)
{
    if (increase)
    {
        if (msg_get_timeout_count <= ECB_UART_DISCONNECT_COUNT)
            ++msg_get_timeout_count;
        if (msg_get_timeout_count == ECB_UART_DISCONNECT_COUNT)
        {
            // send_error_to_cloud(POWER_BOARD_ERROR_CODE);
        }
        return ecb_uart_send_msg(ECB_UART_COMMAND_GET, NULL, 0, 0, -1);
    }
    else
    {
        if (msg_get_timeout_count >= ECB_UART_DISCONNECT_COUNT)
            return ECB_UART_DISCONNECT;
        else if (msg_get_timeout_count == 0)
        {
            return ECB_UART_CONNECTED;
        }
        return ECB_UART_CONNECTINT;
    }
}
int ecb_uart_heart_timeout(bool increase)
{
    if (increase)
        return ++ecb_heart_count;
    else
        return ecb_heart_count;
}

int ecb_uart_send_cloud_msg(unsigned char *msg, const int msg_len)
{
    return ecb_uart_send_msg(ECB_UART_COMMAND_SET, msg, msg_len, 1, -1);
}
int ecb_uart_send_ack(int seq_id)
{
    return ecb_uart_send_msg(ECB_UART_COMMAND_ACK, NULL, 0, 0, seq_id);
}
int ecb_uart_send_nak(unsigned char error_code, int seq_id)
{
    return ecb_uart_send_msg(ECB_UART_COMMAND_NAK, &error_code, 1, 0, seq_id);
}

// int ecb_uart_send_factory(ft_ret_t ret)
// {
//     unsigned char send[2] = {0};
//     send[0] = UART_STORE_FT_RESULT;
//     send[1] = ret;
//     return ecb_uart_send_msg(ECB_UART_COMMAND_STORE, send, sizeof(send), 1, -1);
// }

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
        cJSON *resp = cJSON_CreateObject();
        cJSON_AddNullToObject(resp, "SteamStart");
        send_event_uds(resp, NULL);
    }
    break;
    case KEYPRESS_LOCAL_DEMO_START: /* 启动demo */
    {
        cJSON *resp = cJSON_CreateObject();
        cJSON_AddNullToObject(resp, "DemoStart");
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
        // if (0 != link_reset_report())
        // {
        //     dzlog_error("factory test: reset error ");
        //     ecb_uart_send_factory(FT_RET_ERR_RESET);
        //     break;
        // }
        // sleep(1);
        // ecb_uart_send_factory(FT_RET_OK_RESET);
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
    if (data_len > 1024)
    {
        *end = index + 1;
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
            *end = index + 1;
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
        ecb_uart_send_nak(ECB_NAK_TAILER, seq_id);
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
        ecb_uart_send_nak(ECB_NAK_CHECKSUM, seq_id);
        return ECB_UART_READ_CHECK_ERR;
    }
    //----------------------
    dzlog_info("command:%d", command);
    if (data_len > 0)
        hdzlog_info((unsigned char *)payload, data_len);
    if (command == ECB_UART_COMMAND_EVENT || command == ECB_UART_COMMAND_KEYPRESS)
    {
        ecb_uart_send_ack(seq_id);
        if (command == ECB_UART_COMMAND_EVENT)
        {
            send_data_to_cloud(payload, data_len, ECB_UART_COMMAND_EVENT);
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
    else if (command == ECB_UART_COMMAND_HEART)
    {
        if (data_len == 0 || payload[0] == 0)
        {
            if (ecb_heart_count >= MSG_HEART_TIME)
            {
                uds_report_reset();
            }
            ecb_heart_count = 0;
        }
        else
        {
            if (ecb_heart_count < MSG_HEART_TIME)
            {
                ecb_heart_count = MSG_HEART_TIME;
                send_error_to_cloud(POWER_BOARD_ERROR_CODE);
            }
        }
        ecb_uart_send_ack(seq_id);
    }
    else if (command == ECB_UART_COMMAND_GETACK)
    {
        msg_get_timeout_count = 0;
        // ecb_resend_list_del_by_id(seq_id);
        send_data_to_cloud(payload, data_len, ECB_UART_COMMAND_GETACK);
    }
    else if (command == ECB_UART_COMMAND_ACK)
    {
        ecb_resend_list_del_by_id(seq_id);
    }
    else if (command == ECB_UART_COMMAND_NAK)
    {
        dzlog_warn("uart nak:%d", payload[0]);
    }
    else
    {
        ecb_uart_send_nak(ECB_NAK_COMMAND_UNKNOWN, seq_id);
    }

    return ECB_UART_READ_VALID;
}

void uart_parse_msg(unsigned char *in, int *in_len, int(func)(const unsigned char *, const int, int *))
{
    int index = 0, end;
    int msg_len = *in_len;
    ecb_uart_read_status_t status;
    for (;;)
    {
        dzlog_info("index:%d,end:%d,msg_len:%d", index, end, msg_len);
        status = func(&in[index], msg_len, &end);
        msg_len -= end;
        index += end;

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
    // index = *in_len - msg_len;

    // dzlog_info("last move index:%d,msg_len:%d", index, msg_len);
    if (msg_len <= 0)
    {
        msg_len = 0;
    }
    else
    {
        memmove(in, &in[index], msg_len);
    }
    *in_len = msg_len;
}