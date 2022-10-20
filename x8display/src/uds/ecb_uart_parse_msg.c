#include "main.h"

#include "ecb_uart_parse_msg.h"
#include "uds_protocol.h"

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

int ecb_uart_send_msg(const unsigned char command, unsigned char *msg, const int msg_len, unsigned char resend, int seq_id)
{
    int index = 0;
    unsigned char *send_msg = (unsigned char *)malloc(ECB_MSG_MIN_LEN + msg_len);
    if (send_msg == NULL)
    {
        dzlog_error("malloc error\n");
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
        dzlog_warn("uart send to comm board--------------------------");
        hdzlog_info(send_msg, ECB_MSG_MIN_LEN + msg_len);
    }
    int res = send_to_uds(send_msg, ECB_MSG_MIN_LEN + msg_len);

    // if (resend == 0 || res < 0)
    // {
    free(send_msg);
    // }
    return res;
}

int ecb_uart_send_event_msg(unsigned char *msg, const int msg_len)
{
    return ecb_uart_send_msg(ECB_UART_COMMAND_EVENT, msg, msg_len, 0, -1);
}
int ecb_uart_send_ack(int seq_id)
{
    return ecb_uart_send_msg(ECB_UART_COMMAND_ACK, NULL, 0, 0, seq_id);
}
int ecb_uart_send_nak(unsigned char error_code, int seq_id)
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
        dzlog_warn("Factory test began");
        break;
    case KEYPRESS_LOCAL_FT_END: /* 厂测结束 */
        dzlog_error("End of the factory test");
        break;
    case KEYPRESS_LOCAL_RUN_IN_DO: /* 蒸烤模式下门未关闭时，按运行键 */
        break;
    case KEYPRESS_LOCAL_FT_WIFI: /* 整机厂测，通讯及WIFI检测 */
        dzlog_info("factory test: wifi test ");
        break;
    case KEYPRESS_LOCAL_FT_BT: /* 整机厂测，蓝牙检测 */
        dzlog_info("factory test: bluetooth test ");
        break;
    case KEYPRESS_LOCAL_FT_SPEAKER: /* 整机厂测，喇叭检测 */
        dzlog_info("factory test: speaker test ");
        break;
    case KEYPRESS_LOCAL_FT_RESET: /* 整机厂测，恢复出厂设置 */
        dzlog_info("factory test: reset test ");
        break;
    case KEYPRESS_LOCAL_RESET: /* 通讯板重启（主要用于强制断电前进行通知和追溯） */
    case 0xFF:                 /*重启 */
        dzlog_error("now reboot......");
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
    unsigned short crc16 = crc16_maxim_single(&in[index + 2], msg_index - 2);
    unsigned short check_sum = in[index + msg_index] * 256 + in[index + msg_index + 1];
    // dzlog_info("crc16:%x,check_sum:%x", crc16, check_sum);
    msg_index += 2;
    msg_index += 2;

    *end = index + msg_index;
    if (crc16 != check_sum)
    {
        dzlog_error("data check error");
        ecb_uart_send_nak(ECB_NAK_CHECKSUM, seq_id);
        // return ECB_UART_READ_CHECK_ERR;
    }
    //----------------------

    dzlog_info("read from comm board-------------------- command:%d", command);
    // if (data_len > 0)
    //     hdzlog_info((unsigned char *)payload, data_len);
    hdzlog_info(&in[index], msg_index);

    if (command == ECB_UART_COMMAND_SET || command == ECB_UART_COMMAND_KEYPRESS)
    {
        ecb_uart_send_ack(seq_id);
        if (command == ECB_UART_COMMAND_SET)
        {
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
    else if (command == ECB_UART_COMMAND_GET)
    {
    }
    else if (command == ECB_UART_COMMAND_ACK)
    {
        dzlog_warn("uart ack");
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
        // dzlog_info("index:%d,end:%d,msg_len:%d", index, end, msg_len);
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