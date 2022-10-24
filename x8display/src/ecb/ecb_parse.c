#include "main.h"

#include "ecb_parse.h"
#include "ecb_uart.h"
#include "ecb_uart_parse_msg.h"

static timer_t left_work_timer;
static timer_t right_work_timer;
static unsigned short left_work_time_remaining = 0;
static unsigned short right_work_time_remaining = 0;

static unsigned char ecb_set_state[26];

void work_state_operation(char dir, char state)
{
    if (dir == 0)
    {
        ecb_set_state[7] &= 0xf0;
        ecb_set_state[7] |= state;
    }
    else
    {
        ecb_set_state[7] &= 0x0f;
        ecb_set_state[7] |= state << 4;
    }
}

static ecb_attr_t g_event_state[] = {
    {
        uart_cmd : 0x03,
        uart_byte_len : 1,
    },
    {
        uart_cmd : 0x04,
        uart_byte_len : 1,
    },
    {
        uart_cmd : 0x05,
        uart_byte_len : 1,
    },
    {
        uart_cmd : 0x0a,
        uart_byte_len : 4,
    },
    {
        uart_cmd : 0x0b,
        uart_byte_len : 1,
    },
    {
        uart_cmd : 0x11,
        uart_byte_len : 1,
    },
    {
        uart_cmd : 0x12,
        uart_byte_len : 1,
    },
    {
        uart_cmd : 0x13,
        uart_byte_len : 1,
    },
    {
        uart_cmd : 0x14,
        uart_byte_len : 1,
    },
    {
        uart_cmd : 0x31,
        uart_byte_len : 1,
    },
    {
        uart_cmd : 0x32,
        uart_byte_len : 1,
    },
    {
        uart_cmd : 0x34,
        uart_byte_len : 1,
    },
    {
        uart_cmd : 0x38,
        uart_byte_len : 1,
    },
    {
        uart_cmd : 0x42,
        uart_byte_len : 1,
    },
    {
        uart_cmd : 0x43,
        uart_byte_len : 1,
    },
    {
        uart_cmd : 0x44,
        uart_byte_len : 2,
    },
    {
        uart_cmd : 0x45,
        uart_byte_len : 2,
    },
    {
        uart_cmd : 0x46,
        uart_byte_len : 2,
    },
    {
        uart_cmd : 0x47,
        uart_byte_len : 2,
    },
    {
        uart_cmd : 0x48,
        uart_byte_len : 2,
    },
    {
        uart_cmd : 0x49,
        uart_byte_len : 2,
    },
    {
        uart_cmd : 0x4a,
        uart_byte_len : 1,
    },
    {
        uart_cmd : 0x4b,
        uart_byte_len : 1,
    },
    {
        uart_cmd : 0x4f,
        uart_byte_len : 1,
    },
    {
        uart_cmd : 0x50,
        uart_byte_len : 4,
    },
    {
        uart_cmd : 0x52,
        uart_byte_len : 1,
    },
    {
        uart_cmd : 0x53,
        uart_byte_len : 2,
    },
    {
        uart_cmd : 0x54,
        uart_byte_len : 2,
    },
    {
        uart_cmd : 0x55,
        uart_byte_len : 2,
    },
    {
        uart_cmd : 0x56,
        uart_byte_len : 2,
    },
    {
        uart_cmd : 0x57,
        uart_byte_len : 2,
    },
    {
        uart_cmd : 0x58,
        uart_byte_len : 2,
    },
    {
        uart_cmd : 0x59,
        uart_byte_len : 1,
    },
    {
        uart_cmd : 0x5a,
        uart_byte_len : 1,
    },
    {
        uart_cmd : 0x5b,
        uart_byte_len : 1,
    },
};

static int g_event_state_len = sizeof(g_event_state) / sizeof(g_event_state[0]);

ecb_attr_t *get_event_state(signed short uart_cmd)
{
    for (int i = 0; i < g_event_state_len; ++i)
    {
        if (uart_cmd == g_event_state[i].uart_cmd)
        {
            return &g_event_state[i];
        }
    }
    return NULL;
}
int ecb_parse_set_heart()
{
    ecb_set_state[0] = 0xfa;
    ecb_set_state[1] = 0x19;
    ecb_set_state[2] = 0x04;
    ecb_set_state[3] = 0x01;
    unsigned short crc16 = crc16_maxim_single(&ecb_set_state[0], 24);
    ecb_set_state[24] = crc16;
    ecb_set_state[25] = crc16 >> 8;
    return ecb_uart_send(ecb_set_state, sizeof(ecb_set_state));
}
int ecb_parse_event_uds(unsigned char cmd)
{
    static unsigned char event_buf[255];
    int index = 0;
    dzlog_warn("%s", __func__);
    for (int i = 0; i < g_event_state_len; ++i)
    {
        ecb_attr_t *ecb_attr = &g_event_state[i];
        if (0 < ecb_attr->change || cmd > 0)
        {
            event_buf[index++] = ecb_attr->uart_cmd;
            memcpy(&event_buf[index], ecb_attr->value, ecb_attr->uart_byte_len);
            index += ecb_attr->uart_byte_len;
        }
    }
    if (index > 0)
        ecb_uart_send_event_msg(event_buf, index);
    return 0;
}
static int ecb_parse_event_cmd(unsigned char *data)
{
    ecb_attr_t *ecb_attr;
    ecb_attr = get_event_state(EVENT_SET_SysPower);
    if (*ecb_attr->value != data[4])
    {
        *ecb_attr->value = data[4];
        ecb_attr->change = 1;
    }

    ecb_attr = get_event_state(EVENT_ErrorCode);
    if (ecb_attr->value[2] != data[6] || ecb_attr->value[3] != data[5])
    {
        ecb_attr->value[2] = data[6];
        ecb_attr->value[3] = data[5];
        ecb_attr->change = 1;
    }
    unsigned short error = (data[6] << 8) + data[5];
    unsigned char error_show = 0;
    unsigned char input_state = data[8];

    if (error & (1 << 9))
    {
        error_show = 7;
    }
    else if (error & (1 << 10))
    {
        error_show = 8;
    }
    else if (error & (1 << 11))
    {
        error_show = 9;
    }
    else if (error & (1 << 8))
    {
        error_show = 6;
    }
    else if (error & (1 << 0))
    {
        error_show = 5;
    }
    else if (error & (1 << 1))
    {
        error_show = 5;
    }
    else if (error & (1 << 2))
    {
        error_show = 4;
    }
    else if (error & (1 << 3))
    {
        error_show = 1;
    }
    else if (error & (1 << 4))
    {
        error_show = 14;
    }
    else if (error & (1 << 5))
    {
        error_show = 14;
    }
    else if (error & (1 << 6))
    {
        error_show = 13;
    }
    else if (error & (1 << 7))
    {
        error_show = 12;
    }
    else if (input_state & (1 << 0))
    {
        error_show = 2;
    }
    else if (input_state & (1 << 2))
    {
        error_show = 3;
    }

    ecb_attr = get_event_state(EVENT_ErrorCodeShow);
    if (ecb_attr->value[0] != error_show)
    {
        ecb_attr->value[0] = error_show;
        ecb_attr->change = 1;
    }

    unsigned short state;

    state = input_state & (1 << 3);
    ecb_attr = get_event_state(EVENT_LStOvDoorState);
    if (ecb_attr->value[0] != state)
    {
        ecb_attr->value[0] = state;
        ecb_attr->change = 1;
    }
    state = input_state & (1 << 4);
    ecb_attr = get_event_state(EVENT_RStOvDoorState);
    if (ecb_attr->value[0] != state)
    {
        ecb_attr->value[0] = state;
        ecb_attr->change = 1;
    }

    state = data[9];
    ecb_attr = get_event_state(EVENT_SET_LStOvMode);
    if (ecb_attr->value[0] != state)
    {
        ecb_attr->value[0] = state;
        ecb_attr->change = 1;
    }
    state = data[10];
    ecb_attr = get_event_state(EVENT_SET_RStOvMode);
    if (ecb_attr->value[0] != state)
    {
        ecb_attr->value[0] = state;
        ecb_attr->change = 1;
    }

    unsigned short set_temp = (data[11] << 8) + data[12];
    ecb_attr = get_event_state(EVENT_SET_LStOvSetTemp);
    if (ecb_attr->value[0] != data[11] || ecb_attr->value[1] != data[12])
    {
        ecb_attr->value[0] = data[11];
        ecb_attr->value[1] = data[12];
        ecb_attr->change = 1;
    }
    unsigned short real_temp = (data[15] << 8) + data[16];
    ecb_attr = get_event_state(EVENT_LStOvRealTemp);
    if (ecb_attr->value[0] != data[15] || ecb_attr->value[1] != data[16])
    {
        ecb_attr->value[0] = data[15];
        ecb_attr->value[1] = data[16];
        ecb_attr->change = 1;
    }
    state = data[7] & 0x0f;
    if (state == WORK_STATE_PREHEAT)
    {
        state = REPORT_WORK_STATE_PREHEAT;
        if (set_temp <= real_temp)
        {
            ecb_attr_t *ecb_attr = get_event_state(EVENT_SET_LStOvSetTimer);
            left_work_time_remaining = (ecb_attr->value[0] << 8) + ecb_attr->value[1];
            left_work_time_remaining |= 0x8000;
            work_state_operation(0, WORK_STATE_RUN);
        }
    }
    else if (state == WORK_STATE_PREHEAT_PAUSE || state == WORK_STATE_PAUSE || state == WORK_STATE_ERROR)
    {
        state = REPORT_WORK_STATE_PAUSE;
        if ((left_work_time_remaining & 0x8000) == 0)
        {
            POSIXTimerSet(left_work_timer, 0, 0);
            left_work_time_remaining |= 0x8000;
        }
    }
    else if (state == WORK_STATE_RUN)
    {
        if (left_work_time_remaining & 0x8000)
        {
            POSIXTimerSet(left_work_timer, 60, 60);
            left_work_time_remaining &= 0x7fff;
        }
        else if (left_work_time_remaining == 0)
        {
            work_state_operation(0, WORK_STATE_NOWORK);
        }
    }
    else if (state == WORK_STATE_FINISH || state == WORK_STATE_NOWORK)
    {
        if (state == WORK_STATE_FINISH)
            state = REPORT_WORK_STATE_FINISH;
        else
            state = REPORT_WORK_STATE_STOP;
        if (left_work_time_remaining > 0)
        {
            POSIXTimerSet(left_work_timer, 0, 0);
            left_work_time_remaining = 0;
        }
    }
    else if (state == WORK_STATE_RESERVE)
    {
        state = REPORT_WORK_STATE_RESERVE;
    }

    ecb_attr = get_event_state(EVENT_LStOvState);
    if (ecb_attr->value[0] != state)
    {
        ecb_attr->value[0] = state;
        ecb_attr->change = 1;
    }

    ecb_attr = get_event_state(EVENT_SET_RStOvSetTemp);
    if (ecb_attr->value[0] != data[13] || ecb_attr->value[1] != data[14])
    {
        ecb_attr->value[0] = data[13];
        ecb_attr->value[1] = data[14];
        ecb_attr->change = 1;
    }

    ecb_attr = get_event_state(EVENT_RStOvRealTemp);
    if (ecb_attr->value[0] != data[17] || ecb_attr->value[1] != data[18])
    {
        ecb_attr->value[0] = data[17];
        ecb_attr->value[1] = data[18];
        ecb_attr->change = 1;
    }
    state = (data[7] & 0xf0) >> 4;
    ecb_attr = get_event_state(EVENT_RStOvState);
    if (ecb_attr->value[0] != state)
    {
        ecb_attr->value[0] = state;
        ecb_attr->change = 1;
    }
    return 0;
}
int ecb_parse_event_msg(unsigned char *data, unsigned int len)
{
    if (len < 26)
        return 0;

    int index = 0;
    if (data[index + 0] != 0xf5 || data[index + 1] != 0x19)
    {
        return 0;
    }
    unsigned short crc16_src = (data[25] << 8) + data[24];
    unsigned short crc16 = crc16_maxim_single(&data[0], 24);
    dzlog_warn("crc16:%d,%d", crc16_src, crc16);
    ecb_parse_event_cmd(&data[index + 0]);
    ecb_parse_event_uds(0);
    return 0;
}
static int ecb_parse_set_cmd(const unsigned char cmd, const unsigned char *value)
{
    int ret = 1;
    switch (cmd)
    {
    case EVENT_SET_SysPower:
        ecb_set_state[4] = *value;
        break;
    case EVENT_SET_HoodLight:
        if (*value)
            ecb_set_state[8] |= 0x01;
        else
            ecb_set_state[8] &= ~0x01;
        break;
    case EVENT_SET_HoodSpeed:
        ecb_set_state[16] = *value;
        break;
    case SET_LStOvOperation:
    {
        char work_state = ecb_set_state[7] & 0x0f;
        if (*value == 0)
        {
            if (work_state == WORK_STATE_NOWORK || work_state == WORK_STATE_PREHEAT_PAUSE || work_state == WORK_STATE_PAUSE || work_state == WORK_STATE_FINISH)
                work_state = 1;
        }
        else if (*value == 1)
        {
            if (work_state == WORK_STATE_PREHEAT)
                work_state = WORK_STATE_PREHEAT_PAUSE;
            else if (work_state == WORK_STATE_RUN)
                work_state = WORK_STATE_PAUSE;
        }
        else if (*value == 2)
        {
            work_state = WORK_STATE_FINISH;
        }
        work_state_operation(0, work_state);
    }
    break;
    case EVENT_SET_LStOvMode:
        ecb_set_state[10] = *value;
        break;
    case EVENT_SET_RStOvMode:
        ecb_set_state[11] = *value;
        break;
    case EVENT_SET_LStOvSetTemp:
        ecb_set_state[12] = value[0];
        ecb_set_state[13] = value[1];
        ret = 2;
        break;
    case EVENT_SET_RStOvSetTemp:
        ecb_set_state[14] = value[0];
        ecb_set_state[15] = value[1];
        ret = 2;
        break;
    case EVENT_SET_LStOvSetTimer:
    {
        ecb_attr_t *ecb_attr = get_event_state(EVENT_SET_LStOvSetTimer);
        ecb_attr->value[0] = value[0];
        ecb_attr->value[1] = value[1];
        ecb_attr->change = 1;
        ret = 2;
    }
    break;
    case EVENT_SET_RStOvSetTimer:
    {
        ecb_attr_t *ecb_attr = get_event_state(EVENT_SET_RStOvSetTimer);
        ecb_attr->value[0] = value[0];
        ecb_attr->value[1] = value[1];
        ecb_attr->change = 1;
        ret = 2;
    }
    break;
    default:
        ret = 0;
        break;
    }
    return ret;
}
int ecb_parse_set_msg(const unsigned char *data, unsigned int len)
{
    int ret = 0;
    for (int i = 0; i < len - 1; ++i)
    {
        ret = ecb_parse_set_cmd(data[i], &data[i + 1]);
        i += ret;
    }
    ecb_parse_set_heart();
    return 0;
}

static void POSIXTimer_cb(union sigval val)
{
    if (val.sival_int == 0)
    {
        if (left_work_time_remaining > 0)
            --left_work_time_remaining;
        if ((left_work_time_remaining & 0x7fff) <= 0)
        {
            POSIXTimerSet(left_work_timer, 0, 0);
            work_state_operation(0, WORK_STATE_FINISH);
            left_work_time_remaining = 0;
        }
    }
    else if (val.sival_int == 1)
    {
    }
}

void ecb_parse_init()
{
    left_work_timer = POSIXTimerCreate(0, POSIXTimer_cb);
    right_work_timer = POSIXTimerCreate(1, POSIXTimer_cb);
    for (int i = 0; i < g_event_state_len; ++i)
    {
        if (g_event_state[i].uart_byte_len > 0)
        {
            g_event_state[i].value = (char *)malloc(g_event_state[i].uart_byte_len);
            if (g_event_state[i].value == NULL)
            {
                dzlog_error("g_event_state[%d].value malloc error\n", i);
                return;
            }
            memset(g_event_state[i].value, 0, g_event_state[i].uart_byte_len);
        }
    }
}
void ecb_parse_deinit()
{
    for (int i = 0; i < g_event_state_len; ++i)
    {
        if (g_event_state[i].uart_byte_len > 0)
            free(g_event_state[i].value);
    }
    POSIXTimerDelete(left_work_timer);
    POSIXTimerDelete(right_work_timer);
}