#include "main.h"

#include "ice_uart.h"
#include "ice_parse.h"
#include "ecb_parse.h"
#include "uds_parse_msg.h"
#include "uds_protocol.h"

ice_event_state_t g_ice_event_state;
static int msg_get_timeout_count = 0;
static unsigned short g_seq_id = 0;

int ice_uart_send_msg(const unsigned char command, unsigned char *msg, const int msg_len, unsigned char resend, int seq_id)
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
        seq_id = g_seq_id++;

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

    // if (ECB_UART_COMMAND_ACK != command)
    // {
    //     dzlog_warn("uart send to ice--------------------------");
    //     hdzlog_info(send_msg, ECB_MSG_MIN_LEN + msg_len);
    // }
    int res = ice_uart_send(send_msg, ECB_MSG_MIN_LEN + msg_len);

    // if (resend == 0 || res < 0)
    // {
    free(send_msg);
    // }
    return res;
}

int ice_uart_msg_get(bool increase)
{
    if (increase)
    {
        if (msg_get_timeout_count <= ECB_UART_DISCONNECT_COUNT)
            ++msg_get_timeout_count;
        if (msg_get_timeout_count == ECB_UART_DISCONNECT_COUNT)
        {
        }
        return ice_uart_send_msg(ECB_UART_COMMAND_GET, NULL, 0, 0, -1);
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

int ice_uart_send_event_msg(unsigned char *msg, const int msg_len)
{
    return ice_uart_send_msg(ECB_UART_COMMAND_EVENT, msg, msg_len, 0, -1);
}
static int ice_uart_send_ack(int seq_id)
{
    return ice_uart_send_msg(ECB_UART_COMMAND_ACK, NULL, 0, 0, seq_id);
}
static int ice_uart_send_nak(unsigned char error_code, int seq_id)
{
    return ice_uart_send_msg(ECB_UART_COMMAND_NAK, &error_code, 1, 0, seq_id);
}
int ice_uart_send_set_real_temp(unsigned short temp)
{
    unsigned char msg[16];
    int msg_len = 0;
    msg[msg_len++] = EVENT_SET_IceStOvRealTemp;
    msg[msg_len++] = temp >> 8;
    msg[msg_len++] = temp;
    return ice_uart_send_msg(ECB_UART_COMMAND_SET, msg, msg_len, 0, -1);
}
int ice_uart_send_set_msg(unsigned char operation, unsigned char mode, unsigned short temp, unsigned short time)
{
    dzlog_warn("%s,-----------------operation:%d mode:%d temp:%d time:%d", __func__, operation, mode, temp, time);
    unsigned char msg[16];
    int msg_len = 0;
    switch (operation)
    {
    case WORK_OPERATION_RUN:
    case WORK_OPERATION_RUN_NOW:
        if (g_ice_event_state.iceStOvState != REPORT_WORK_STATE_PAUSE)
        {
            msg[msg_len++] = EVENT_SET_IceStOvMode;
            msg[msg_len++] = mode;
            msg[msg_len++] = EVENT_SET_IceStOvSetTemp;
            msg[msg_len++] = temp >> 8;
            msg[msg_len++] = temp;
            msg[msg_len++] = EVENT_SET_IceStOvSetTimer;
            msg[msg_len++] = time >> 8;
            msg[msg_len++] = time;
        }
        msg[msg_len++] = SET_IceStOvOperation;
        msg[msg_len++] = operation;
        break;
    case WORK_OPERATION_PAUSE:
    case WORK_OPERATION_STOP:
    case WORK_OPERATION_FINISH:
        msg[msg_len++] = SET_IceStOvOperation;
        msg[msg_len++] = operation;
        break;
    }
    return ice_uart_send_msg(ECB_UART_COMMAND_SET, msg, msg_len, 0, -1);
}
static int ice_parse_event_cmd(const unsigned char cmd, const unsigned char *value)
{
    int ret = 1;
    switch (cmd)
    {
    case EVENT_SET_IceStOvMode:
        g_ice_event_state.iceStOvMode = value[0];
        break;
    case EVENT_IceStOvState:
        if (g_ice_event_state.iceStOvState != value[0] && value[0] == REPORT_WORK_STATE_FINISH && g_ice_event_state.iceStOvMode == RStOvMode_ICE)
        {
            right_ice_state_finish();
        }
        g_ice_event_state.iceStOvState = value[0];

        break;
    case EVENT_SET_IceStOvSetTemp:
        g_ice_event_state.iceStOvSetTemp = (value[0] << 8) + value[1];
        ret = 2;
        break;
    case EVENT_SET_IceStOvRealTemp:
        g_ice_event_state.iceStOvRealTemp = (value[0] << 8) + value[1];
        ret = 2;
        break;
    case EVENT_SET_IceStOvSetTimer:
        g_ice_event_state.iceStOvSetTimer = (value[0] << 8) + value[1];
        ret = 2;
        break;
    case EVENT_IceStOvSetTimerLeft:
        g_ice_event_state.iceStOvSetTimerLeft = (value[0] << 8) + value[1];
        if (g_ice_event_state.iceStOvState != REPORT_WORK_STATE_STOP && g_ice_event_state.iceStOvMode == RStOvMode_ICE)
            right_ice_time_left(g_ice_event_state.iceStOvSetTimerLeft);
        ret = 2;
        break;
    case EVENT_IceHoodSpeed:
        if (g_ice_event_state.iceStOvState != REPORT_WORK_STATE_FINISH && g_ice_event_state.iceStOvState != REPORT_WORK_STATE_STOP)
        {
            right_ice_hood_speed(value[0]);
        }
        break;
    case EVENT_IceHoodLight:
        if (g_ice_event_state.iceStOvState != REPORT_WORK_STATE_FINISH && g_ice_event_state.iceStOvState != REPORT_WORK_STATE_STOP)
        {
            right_ice_hood_light(value[0]);
        }
        break;
    }
    return ret;
}
static int ice_parse_event_msg(const unsigned char *data, unsigned int len)
{
    int ret = 0;
    for (int i = 0; i < len - 1; ++i)
    {
        ret = ice_parse_event_cmd(data[i], &data[i + 1]);
        i += ret;
    }
    dzlog_warn("%s,-----------------iceStOvState:%d", __func__, g_ice_event_state.iceStOvState);
    g_ice_event_state.first_ack = 0;
    return 0;
}
int ice_uart_parse_msg(const unsigned char *in, const int in_len, int *end)
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
        ice_uart_send_nak(ECB_NAK_TAILER, seq_id);
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
        // ice_uart_send_nak(ECB_NAK_CHECKSUM, seq_id);
        // return ECB_UART_READ_CHECK_ERR;
    }
    //----------------------

    dzlog_info("read from ice-------------------- command:%d", command);
    // if (data_len > 0)
    //     hdzlog_info((unsigned char *)payload, data_len);
    hdzlog_info(&in[index], msg_index);

    if (command == ECB_UART_COMMAND_EVENT || command == ECB_UART_COMMAND_KEYPRESS)
    {
        ice_uart_send_ack(seq_id);
        if (command == ECB_UART_COMMAND_EVENT)
        {
            ice_parse_event_msg(payload, data_len);
        }
        else if (command == ECB_UART_COMMAND_KEYPRESS)
        {
        }
        else
        {
        }
    }
    else if (command == ECB_UART_COMMAND_GETACK)
    {
        msg_get_timeout_count = 0;
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
        ice_uart_send_nak(ECB_NAK_COMMAND_UNKNOWN, seq_id);
    }

    return ECB_UART_READ_VALID;
}
