#include "main.h"

// #include "link_reset_posix.h"
// #include "uart_resend.h"
#include "ecb_uart.h"
#include "ecb_uart_parse_msg.h"
#include "cloud_platform_task.h"
#include "uds_protocol.h"
#include "ota_power_task.h"
#include "curl_http_request.h"
#include "uart_resend.h"

static int msg_get_timeout_count = 0;
static int ecb_heart_count = 0;
static unsigned short ecb_seq_id = 0;

unsigned short crc16_maxim_single(const unsigned char *ptr, int len)
{
    unsigned int i;
    unsigned short crc = 0x0000;

    while (len--)
    {
        crc ^= *ptr++;
        for (i = 0; i < 8; ++i)
        {
            if (crc & 1)
                crc = (crc >> 1) ^ 0xA001;
            else
                crc = (crc >> 1);
        }
    }

    return ~crc;
}

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

int ecb_uart_send_msg(const unsigned char command, unsigned char *msg, const int msg_len, unsigned char resend, int seq_id)
{
    if (get_ecb_ota_power_state() != 0 && ECB_UART_COMMAND_OTA != command)
    {
        return -1;
    }
    int index = 0;
    unsigned char *send_msg = (unsigned char *)malloc(ECB_MSG_MIN_LEN + msg_len);
    if (send_msg == NULL)
    {
        LOGE("malloc error\n");
        return -1;
    }
    send_msg[index++] = 0xe6;
    send_msg[index++] = 0xe6;
    if (seq_id < 0)
        seq_id = ecb_seq_id++;

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
    unsigned short crc16 = crc16_maxim_single((const unsigned char *)(send_msg + 2), index - 2);
    send_msg[index++] = crc16 >> 8;
    send_msg[index++] = crc16 & 0xff;
    send_msg[index++] = 0x6e;
    send_msg[index++] = 0x6e;

    if (ECB_UART_COMMAND_ACK != command)
    {
        // LOGW("uart send to ecb--------------------------%ld", get_systime_ms());
        LOGW("uart send to ecb--------------------------");
        hdzlog_info(send_msg, ECB_MSG_MIN_LEN + msg_len);
    }
    if (ECB_UART_COMMAND_SET == command)
    {
        http_report_hex("SET:", send_msg, ECB_MSG_MIN_LEN + msg_len);
    }
#ifdef ICE_ENABLE
    int res = display_send(send_msg, ECB_MSG_MIN_LEN + msg_len);
    free(send_msg);
#else
    int res = ecb_uart_send(send_msg, ECB_MSG_MIN_LEN + msg_len, resend, 0);
    if (resend == 0 || res < 0)
    {
        free(send_msg);
    }
#endif

    return res;
}
int ecb_uart_send_cloud_msg(unsigned char *msg, const int msg_len)
{
    return ecb_uart_send_msg(ECB_UART_COMMAND_SET, msg, msg_len, 1, -1);
}
int ecb_uart_send_ota_msg(unsigned char *msg, const int msg_len, unsigned char resend)
{
    return ecb_uart_send_msg(ECB_UART_COMMAND_OTA, msg, msg_len, resend, -1);
}
static int ecb_uart_send_ack(int seq_id)
{
    return ecb_uart_send_msg(ECB_UART_COMMAND_ACK, NULL, 0, 0, seq_id);
}
static int ecb_uart_send_nak(unsigned char error_code, int seq_id)
{
    return ecb_uart_send_msg(ECB_UART_COMMAND_NAK, &error_code, 1, 0, seq_id);
}

void keypress_local_pro(unsigned char value)
{
    switch (value)
    {
    case KEYPRESS_LOCAL_POWER_ON: /* 上电提示 */
        break;
    case KEYPRESS_LOCAL_FT_START: /* 厂测开始 */
    {
        LOGW("Factory test began");
    }
    break;
    case KEYPRESS_LOCAL_FT_END: /* 厂测结束 */
        LOGE("End of the factory test");
        break;
    case KEYPRESS_LOCAL_RUN_IN_DO: /* 蒸烤模式下门未关闭时，按运行键 */
        break;
    case KEYPRESS_LOCAL_FT_WIFI: /* 整机厂测，通讯及WIFI检测 */
        LOGI("factory test: wifi test ");
        break;
    case KEYPRESS_LOCAL_FT_BT: /* 整机厂测，蓝牙检测 */
        LOGI("factory test: bluetooth test ");
        break;
    case KEYPRESS_LOCAL_FT_SPEAKER: /* 整机厂测，喇叭检测 */
        LOGI("factory test: speaker test ");
        break;
    case KEYPRESS_LOCAL_FT_RESET: /* 整机厂测，恢复出厂设置 */
        LOGI("factory test: reset test ");
        break;
    case KEYPRESS_LOCAL_RESET: /* 通讯板重启（主要用于强制断电前进行通知和追溯） */
    case 0xFF:                 /*重启 */
        LOGE("now reboot......");
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
        LOGE("input len too small");
        return ECB_UART_READ_LEN_SMALL;
    }

    for (i = 0; i < in_len - 1; ++i)
    {
        if (in[i] == 0xe6 && in[i + 1] == 0xe6)
            break;
    }
    if (i >= in_len - 1)
    {
        LOGE("no header was detected");
        return ECB_UART_READ_NO_HEADER;
    }
    index = i;

    if (in_len - index < ECB_MSG_MIN_LEN)
    {
        *end = index;
        LOGE("input len too small");
        return ECB_UART_READ_LEN_SMALL;
    }
    int msg_index = 2;

    const unsigned short seq_id = in[index + msg_index] * 256 + in[index + msg_index + 1];
    msg_index += 2;
    unsigned char command = in[index + msg_index];
    msg_index += 1;
    int data_len = in[index + msg_index] * 256 + in[index + msg_index + 1];
    if (data_len > 256)
    {
        *end = index + 1;
        LOGE("input data len error");
        return ECB_UART_READ_LEN_ERR;
    }
    msg_index += 2;
    const unsigned char *payload = &in[index + msg_index];
    msg_index += data_len;
    if (index + msg_index + 2 + 2 > in_len)
    {
        *end = index;
        LOGE("input data len too small");
        return ECB_UART_READ_LEN_SMALL;
    }
    else if (in[index + msg_index + 2] != 0x6e || in[index + msg_index + 2 + 1] != 0x6e)
    {
        *end = index + msg_index + 2 + 2;
        LOGE("no tailer was detected");
        ecb_uart_send_nak(ECB_NAK_TAILER, seq_id);
        return ECB_UART_READ_TAILER_ERR;
    }
    unsigned short crc16 = crc16_maxim_single(&in[index + 2], msg_index - 2);
    unsigned short check_sum = in[index + msg_index] * 256 + in[index + msg_index + 1];
    // LOGI("crc16:%x,check_sum:%x", crc16, check_sum);
    msg_index += 2;
    msg_index += 2;

    *end = index + msg_index;
    if (crc16 != check_sum)
    {
        LOGE("data check error");
        ecb_uart_send_nak(ECB_NAK_CHECKSUM, seq_id);
        return ECB_UART_READ_CHECK_ERR;
    }
    //----------------------
    if (command != ECB_UART_COMMAND_HEART)
    {
        LOGI("read from ecb-------------------- command:%d", command);
        // if (data_len > 0)
        //     hdzlog_info((unsigned char *)payload, data_len);
        hdzlog_info(&in[index], msg_index);
    }
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
        http_report_hex("EVENT:", &in[index], msg_index);
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
        // http_report_hex("GETACK:", &in[index], msg_index);
    }
    else if (command == ECB_UART_COMMAND_ACK)
    {
        ecb_resend_list_del_by_id(seq_id);
    }
    else if (command == ECB_UART_COMMAND_NAK)
    {
        LOGW("uart nak:%d", payload[0]);
    }
    else if (command == ECB_UART_COMMAND_OTAACK)
    {
#ifdef OTA_RESEND
        ecb_resend_list_del_by_id(seq_id);
#endif
        if (payload[0] == 0xfa)
            ota_power_ack(&payload[1]);
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
        // LOGI("index:%d,end:%d,msg_len:%d", index, end, msg_len);
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
            LOGE("error return value..");
        }
        if (msg_len <= 0)
        {
            break;
        }
    }
    // index = *in_len - msg_len;

    // LOGI("last move index:%d,msg_len:%d", index, msg_len);
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