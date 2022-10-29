#include "main.h"

#include "ecb_parse.h"
#include "ecb_uart.h"
#include "ecb_uart_parse_msg.h"
#include "ice_parse.h"

static timer_t left_work_timer;
static timer_t right_work_timer;
static unsigned short left_work_time_remaining = 0;
static unsigned short left_order_time_remaining = 0;
static unsigned short right_work_time_remaining = 0;
static unsigned short right_order_time_remaining = 0;
multistage_state_t left_multistage_state;
multistage_state_t right_multistage_state;

static unsigned char left_door_state = 0;
static unsigned char right_door_state = 0;
static unsigned char hood_min_speed = 0;

static unsigned char ecb_set_state[26];

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
        uart_cmd : 0x4d,
        uart_byte_len : 4,
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
    {
        uart_cmd : 0x5d,
        uart_byte_len : 4,
    },
    {
        uart_cmd : 0x5f,
        uart_byte_len : 1,
    },
    {
        uart_cmd : 0x60,
        uart_byte_len : 1,
    },
    {
        uart_cmd : 0x80,
        uart_byte_len : 1,
    },
    {
        uart_cmd : 0xf6,
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
unsigned short crc16_XMODEM(unsigned char *ptr, int len)
{
    unsigned int i;
    unsigned short crc = 0x0000;

    while (len--)
    {
        crc ^= (unsigned short)(*ptr++) << 8;
        for (i = 0; i < 8; ++i)
        {
            if (crc & 0x8000)
                crc = (crc << 1) ^ 0x1021;
            else
                crc <<= 1;
        }
    }
    return crc;
}

int right_ice_hood_speed(unsigned char speed)
{
    if (speed < hood_min_speed)
        return -1;

    ecb_set_state[16] = speed;
    ecb_attr_t *ecb_attr = get_event_state(EVENT_SET_HoodSpeed);
    ecb_attr->value[0] = speed;
    ecb_attr->change = 1;
    return 0;
}
int right_ice_hood_light(unsigned char light)
{
    if (light)
        ecb_set_state[8] |= 0x01;
    else
        ecb_set_state[8] &= ~0x01;

    ecb_attr_t *ecb_attr = get_event_state(EVENT_SET_HoodLight);
    ecb_attr->value[0] = light;
    ecb_attr->change = 1;
    return 0;
}
int right_ice_time_left(unsigned short time)
{
    ecb_attr_t *ecb_attr;
    ecb_attr = get_event_state(EVENT_RStOvSetTimerLeft);
    ecb_attr->value[0] = time >> 8;
    ecb_attr->value[1] = time;
    ecb_attr->change = 1;
    return 0;
}
static int right_ice_operation(unsigned char operation)
{
    if (operation == WORK_OPERATION_STOP)
    {
        if (g_ice_event_state.iceStOvState != REPORT_WORK_STATE_STOP)
            ice_uart_send_set_msg(operation, 0, 0, 0);
    }
    else if (operation == WORK_OPERATION_PAUSE || operation == WORK_OPERATION_FINISH)
    {
        if (operation == WORK_OPERATION_FINISH)
            operation = WORK_OPERATION_STOP;
        ice_uart_send_set_msg(operation, 0, 0, 0);
    }
    else
    {
        unsigned short mode = ecb_set_state[11];
        if (mode == RStOvMode_ICE)
        {
            g_ice_event_state.first_ack = 1;
            unsigned short temp = (ecb_set_state[13] << 8) + ecb_set_state[12];
            ecb_attr_t *ecb_attr = get_event_state(EVENT_SET_RStOvSetTimer);
            unsigned short time = (ecb_attr->value[0] << 8) + ecb_attr->value[1];
            ice_uart_send_set_msg(operation, mode, temp, time);
            return -1;
        }
        else
        {
            ice_uart_send_set_msg(WORK_OPERATION_STOP, 0, 0, 0);
        }
    }
    return 0;
}

void system_poweroff()
{
    if (left_work_time_remaining != 0 || left_order_time_remaining != 0)
    {
        POSIXTimerSet(left_work_timer, 0, 0);
        left_work_time_remaining = 0;
        left_order_time_remaining = 0;
    }
    if (right_work_time_remaining != 0 || right_order_time_remaining != 0)
    {
        POSIXTimerSet(right_work_timer, 0, 0);
        right_work_time_remaining = 0;
        right_order_time_remaining = 0;
    }
    if (hood_min_speed != 0)
        hood_min_speed = 0;

    if (ecb_set_state[16] != hood_min_speed)
    {
        ecb_set_state[16] = hood_min_speed;

        ecb_attr_t *ecb_attr = get_event_state(EVENT_SET_HoodSpeed);
        ecb_attr->value[0] = hood_min_speed;
        ecb_attr->change = 1;
    }
    right_ice_operation(WORK_OPERATION_STOP);
}
static void work_state_operation(unsigned char dir, unsigned char state)
{
    dzlog_error("%s,start state:%d,ecb_set_state[7]:%x", __func__, state, ecb_set_state[7]);
    if (dir == 0)
    {
        if (state == WORK_STATE_NOWORK)
        {
            ecb_set_state[10] = 0;
            ecb_set_state[17] &= 0xf0;
        }
        ecb_set_state[7] &= 0xf0;
        ecb_set_state[7] |= state;
    }
    else
    {
        if (state == WORK_STATE_NOWORK)
        {
            if (ecb_set_state[11] != RStOvMode_ICE)
                ecb_set_state[11] = 0;
            ecb_set_state[17] &= 0x0f;
            // ecb_attr_t *ecb_attr = get_event_state(EVEN_SET_RMultiMode);
            // ecb_attr->value[0] = 0;
            // ecb_attr->change = 1;
        }
        ecb_set_state[7] &= 0x0f;
        ecb_set_state[7] |= state << 4;
    }
    dzlog_error("%s,end state:%d,ecb_set_state[7]:%x", __func__, state, ecb_set_state[7]);
}
int ecb_parse_set_heart()
{
    ecb_set_state[0] = 0xfa;
    ecb_set_state[1] = 0x19;
    ecb_set_state[2] = 0x04;
    ecb_set_state[3] = 0x01;
    unsigned short crc16 = crc16_XMODEM(&ecb_set_state[1], 23);
    ecb_set_state[24] = crc16;
    ecb_set_state[25] = crc16 >> 8;
    return ecb_uart_send(ecb_set_state, sizeof(ecb_set_state));
}
int ecb_parse_event_uds(unsigned char cmd)
{
    unsigned char event_buf[255];
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
            ecb_attr->change = 0;
        }
    }
    if (index > 0)
        ecb_uart_send_event_msg(event_buf, index);
    return 0;
}

static int hood_min_speed_control(char left_state, char left_mode, char right_state, char right_mode)
{
    unsigned char speed = 0;
    if (left_state == WORK_STATE_PREHEAT || left_state == WORK_STATE_RUN)
    {
        if ((left_mode >= 3 && left_mode <= 8) || (left_mode >= 15 && left_mode <= 16))
            speed = 2;
        else
            speed = 1;
        ecb_set_state[8] |= 1 << 2;
    }
    else
    {
        if (left_state == WORK_STATE_NOWORK || left_state == WORK_STATE_FINISH)
            ecb_set_state[8] &= ~(1 << 2);
    }
    if (right_state == WORK_STATE_PREHEAT || right_state == WORK_STATE_RUN)
    {
        if ((right_mode >= 3 && right_mode <= 8) || (right_mode >= 15 && right_mode <= 16))
            speed = 2;
        else
            speed = 1;
        ecb_set_state[8] |= 1 << 3;
    }
    else
    {
        if (right_state == WORK_STATE_NOWORK || right_state == WORK_STATE_FINISH)
            ecb_set_state[8] &= ~(1 << 3);
    }
    if (hood_min_speed != speed)
        hood_min_speed = speed;
    if (ecb_set_state[16] < hood_min_speed)
    {
        ecb_set_state[16] = hood_min_speed;

        ecb_attr_t *ecb_attr = get_event_state(EVENT_SET_HoodSpeed);
        ecb_attr->value[0] = hood_min_speed;
        ecb_attr->change = 1;
    }
    dzlog_warn("%s,hood_min_speed:%d,speed:%d", __func__, hood_min_speed, speed);

    return 0;
}
static int ecb_parse_event_cmd(unsigned char *data)
{
    unsigned char left_state, left_mode, right_state, right_mode;
    unsigned short state;
    ecb_attr_t *ecb_attr;

    switch (data[4])
    {
    case 0:
        ecb_set_state[4] = 1;
        break;
    default:
        break;
    }
    state = data[4];
    if (state <= 1)
    {
        state = 0;
        system_poweroff();
    }
    else
    {
        state = 1;
    }
    ecb_attr = get_event_state(EVENT_SET_SysPower);
    if (*ecb_attr->value != state)
    {
        *ecb_attr->value = state;
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
    else if ((input_state & (1 << 0)) == 0)
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

    left_mode = state = data[9];
    switch (data[9])
    {
    case 17:
        state = 0x04;
        break;
    case 5:
        state = 0x23;
        break;
    case 6:
        state = 0x24;
        break;
    case 7:
        state = 0x26;
        break;
    case 15:
        state = 0x28;
        break;
    case 16:
        state = 0x2a;
        break;
    default:
        break;
    }
    ecb_attr = get_event_state(EVENT_SET_LStOvMode);
    if (ecb_attr->value[0] != state)
    {
        ecb_attr->value[0] = state;
        ecb_attr->change = 1;
    }
    right_mode = state = data[10];
    switch (data[10])
    {
    case 1:
        state = 0x01;
        break;
    case 2:
        state = 0x03;
        break;
    case 17:
        state = 0x04;
        break;
    case 9:
        state = 0x41;
        break;
    case 12:
        state = 0x42;
        break;
    case 11:
        state = 0x44;
        break;
    default:
        break;
    }
    ecb_attr = get_event_state(EVENT_SET_RStOvMode);
    if (ecb_attr->value[0] != state)
    {
        ecb_attr->value[0] = state;
        ecb_attr->change = 1;
    }

    unsigned short set_temp = (data[12] << 8) + data[11];
    ecb_attr = get_event_state(EVENT_SET_LStOvSetTemp);
    if (ecb_attr->value[1] != data[11] || ecb_attr->value[0] != data[12])
    {
        ecb_attr->value[1] = data[11];
        ecb_attr->value[0] = data[12];
        ecb_attr->change = 1;
    }
    unsigned short real_temp = (data[16] << 8) + data[15];
    ecb_attr = get_event_state(EVENT_LStOvRealTemp);
    if (ecb_attr->value[1] != data[15] || ecb_attr->value[0] != data[16])
    {
        ecb_attr->value[1] = data[15];
        ecb_attr->value[0] = data[16];
        ecb_attr->change = 1;
    }

    left_state = state = data[7] & 0x0f;
    // dzlog_warn("%s,------------------start left_work_time_remaining:%x", __func__, left_work_time_remaining);
    if (state == WORK_STATE_PREHEAT)
    {
        state = REPORT_WORK_STATE_PREHEAT;
        if (set_temp <= real_temp)
        {
            ecb_attr_t *ecb_attr = get_event_state(EVENT_SET_LStOvSetTimer);
            left_work_time_remaining = (ecb_attr->value[0] << 8) + ecb_attr->value[1];
            left_work_time_remaining |= 0x8000;
            work_state_operation(WORK_DIR_LEFT, WORK_STATE_RUN);
        }
    }
    else if (state == WORK_STATE_PREHEAT_PAUSE || state == WORK_STATE_PAUSE || state == WORK_STATE_ERROR)
    {
        if (state == WORK_STATE_PAUSE || state == WORK_STATE_ERROR)
        {
            if (left_work_time_remaining > 0 && (left_work_time_remaining & 0x8000) == 0)
            {
                POSIXTimerSet(left_work_timer, 0, 0);
                left_work_time_remaining |= 0x8000;
            }
        }
        state = REPORT_WORK_STATE_PAUSE;
    }
    else if (state == WORK_STATE_RUN)
    {
        if (left_work_time_remaining & 0x8000)
        {
            POSIXTimerSet(left_work_timer, 60, 60);
            left_work_time_remaining &= 0x7fff;
        }
        if (left_work_time_remaining == 0 && left_multistage_state.valid == 0)
        {
            POSIXTimerSet(left_work_timer, 0, 0);
            work_state_operation(WORK_DIR_LEFT, WORK_STATE_NOWORK);
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
        left_multistage_state.valid = 0;
    }
    else if (state == WORK_STATE_RESERVE)
    {
        state = REPORT_WORK_STATE_RESERVE;
    }
    dzlog_warn("%s,------------end left_work_time_remaining:%x left_multistage_state.valid:%d current_step:%d,total_step:%d", __func__, left_work_time_remaining, left_multistage_state.valid, left_multistage_state.current_step, left_multistage_state.total_step);
    if (left_order_time_remaining == 0)
    {
        ecb_attr = get_event_state(EVENT_LStOvState);
        if (ecb_attr->value[0] != state)
        {
            ecb_attr->value[0] = state;
            ecb_attr->change = 1;
        }
    }
    else
        dzlog_warn("%s,------------------left_order_time_remaining:%x", __func__, left_order_time_remaining);

    set_temp = (data[14] << 8) + data[13];
    ecb_attr = get_event_state(EVENT_SET_RStOvSetTemp);
    if (ecb_attr->value[1] != data[13] || ecb_attr->value[0] != data[14])
    {
        ecb_attr->value[1] = data[13];
        ecb_attr->value[0] = data[14];
        ecb_attr->change = 1;
    }
    real_temp = (data[18] << 8) + data[17];
    ecb_attr = get_event_state(EVENT_RStOvRealTemp);
    if (ecb_attr->value[1] != data[17] || ecb_attr->value[0] != data[18])
    {
        ecb_attr->value[1] = data[17];
        ecb_attr->value[0] = data[18];
        ecb_attr->change = 1;

        ice_uart_send_set_real_temp(real_temp);
    }

    right_state = state = (data[7] & 0xf0) >> 4;
    // dzlog_warn("%s,------------------start right_work_time_remaining:%x", __func__, right_work_time_remaining);
    if (state == WORK_STATE_PREHEAT)
    {
        state = REPORT_WORK_STATE_PREHEAT;
        if (set_temp <= real_temp)
        {
            ecb_attr_t *ecb_attr = get_event_state(EVENT_SET_RStOvSetTimer);
            right_work_time_remaining = (ecb_attr->value[0] << 8) + ecb_attr->value[1];
            right_work_time_remaining |= 0x8000;
            work_state_operation(WORK_DIR_RIGHT, WORK_STATE_RUN);
        }
    }
    else if (state == WORK_STATE_PREHEAT_PAUSE || state == WORK_STATE_PAUSE || state == WORK_STATE_ERROR)
    {
        if (state == WORK_STATE_PAUSE || state == WORK_STATE_ERROR)
        {
            if (right_work_time_remaining > 0 && (right_work_time_remaining & 0x8000) == 0)
            {
                POSIXTimerSet(right_work_timer, 0, 0);
                right_work_time_remaining |= 0x8000;
            }
        }
        state = REPORT_WORK_STATE_PAUSE;
    }
    else if (state == WORK_STATE_RUN)
    {
        if (right_work_time_remaining & 0x8000)
        {
            POSIXTimerSet(right_work_timer, 60, 60);
            right_work_time_remaining &= 0x7fff;
        }
        if (right_work_time_remaining == 0 && right_multistage_state.valid == 0)
        {
            POSIXTimerSet(right_work_timer, 0, 0);
            work_state_operation(WORK_DIR_RIGHT, WORK_STATE_NOWORK);
        }
    }
    else if (state == WORK_STATE_FINISH || state == WORK_STATE_NOWORK)
    {
        if (state == WORK_STATE_FINISH)
            state = REPORT_WORK_STATE_FINISH;
        else
            state = REPORT_WORK_STATE_STOP;
        if (right_work_time_remaining > 0)
        {
            POSIXTimerSet(right_work_timer, 0, 0);
            right_work_time_remaining = 0;
        }
        if ((g_ice_event_state.iceStOvState == WORK_STATE_FINISH || g_ice_event_state.iceStOvState == WORK_STATE_NOWORK) && g_ice_event_state.first_ack == 0)
            right_multistage_state.valid = 0;
    }
    else if (state == WORK_STATE_RESERVE)
    {
        state = REPORT_WORK_STATE_RESERVE;
    }
    dzlog_warn("%s,---------------------end right_work_time_remaining:%x right_multistage_state.valid:%d current_step:%d total_step:%d", __func__, right_work_time_remaining, right_multistage_state.valid, right_multistage_state.current_step, right_multistage_state.total_step);

    if (right_order_time_remaining == 0)
    {
        ecb_attr = get_event_state(EVENT_RStOvState);
        if (state == REPORT_WORK_STATE_STOP)
        {
            if (ecb_attr->value[0] != g_ice_event_state.iceStOvState)
            {
                ecb_attr->value[0] = g_ice_event_state.iceStOvState;
                ecb_attr->change = 1;
            }
        }
        else
        {
            if (ecb_attr->value[0] != state)
            {
                {
                    ecb_attr->value[0] = state;
                    ecb_attr->change = 1;
                }
            }
        }
    }
    else
        dzlog_warn("%s,------------------right_order_time_remaining:%x", __func__, right_order_time_remaining);

    // unsigned char door_state = (data[8] >> 3) & 0x01;
    // if (left_door_state != door_state)
    // {
    //     left_door_state = door_state;
    //     if (left_door_state)
    //     {
    //         if (left_state == WORK_STATE_PREHEAT)
    //         {
    //             work_state_operation(WORK_DIR_LEFT, WORK_STATE_PREHEAT_PAUSE);
    //         }
    //         else if (left_state == WORK_STATE_RUN)
    //         {
    //             work_state_operation(WORK_DIR_LEFT, WORK_STATE_PAUSE);
    //         }
    //     }
    //     else
    //     {
    //         if (left_state == WORK_STATE_PREHEAT_PAUSE)
    //         {
    //             work_state_operation(WORK_DIR_LEFT, WORK_STATE_PREHEAT);
    //         }
    //         else if (left_state == WORK_STATE_PAUSE)
    //         {
    //             work_state_operation(WORK_DIR_LEFT, WORK_STATE_RUN);
    //         }
    //     }
    // }
    // right_door_state = (data[8] >> 4) & 0x01;
    hood_min_speed_control(left_state, left_mode, right_state, right_mode);
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
    // unsigned short crc16_src = (data[25] << 8) + data[24];
    // unsigned short crc16 = crc16_XMODEM(&data[1], 23);
    // dzlog_warn("crc16:%d,%d", crc16_src, crc16);
    ecb_parse_event_cmd(&data[index + 0]);
    ecb_parse_event_uds(0);
    return 0;
}

static int set_work_operation(unsigned char work_state, unsigned char operation)
{
    switch (operation)
    {
    case WORK_OPERATION_RUN:
        if (work_state == WORK_STATE_NOWORK || work_state == WORK_STATE_PREHEAT_PAUSE || work_state == WORK_STATE_FINISH || work_state == WORK_STATE_ERROR)
            work_state = WORK_STATE_PREHEAT;
        else if (work_state == WORK_STATE_PAUSE)
            work_state = WORK_STATE_RUN;
        break;
    case WORK_OPERATION_PAUSE:
        if (work_state == WORK_STATE_PREHEAT)
            work_state = WORK_STATE_PREHEAT_PAUSE;
        else if (work_state == WORK_STATE_RUN)
            work_state = WORK_STATE_PAUSE;
        break;
    case WORK_OPERATION_STOP:
        work_state = WORK_STATE_NOWORK;
        break;
    case WORK_OPERATION_FINISH:
        work_state = WORK_STATE_NOWORK;
        break;
    case WORK_OPERATION_RUN_NOW:
        work_state = WORK_STATE_PREHEAT;
        break;
    }
    return work_state;
}
static int set_work_mode(char dir, char mode)
{
    if (dir == 0)
    {
        switch (mode)
        {
        case 0x04:
            mode = 17;
            break;
        case 0x23:
            mode = 5;
            break;
        case 0x24:
            mode = 6;
            break;
        case 0x26:
            mode = 7;
            break;
        case 0x28:
            mode = 15;
            break;
        case 0x2a:
            mode = 16;
            break;
        default:
            break;
        }
        if (ecb_set_state[10] != mode)
        {
            ecb_set_state[7] &= 0xf0;
        }
        ecb_set_state[10] = mode;
    }
    else
    {
        switch (mode)
        {
        case 0x01:
            mode = 1;
            break;
        case 0x03:
            mode = 2;
            break;
        case 0x04:
            mode = 17;
            break;
        case 0x41:
            mode = 9;
            break;
        case 0x42:
            mode = 12;
            break;
        case 0x44:
            mode = 11;
            break;
        default:
            break;
        }
        if (ecb_set_state[11] != mode)
        {
            ecb_set_state[7] &= 0x0f;
        }
        ecb_set_state[11] = mode;
    }
    return 0;
}
static int set_work_time(char dir, unsigned short time)
{
    ecb_attr_t *ecb_attr;
    if (dir == 0)
    {
        ecb_attr = get_event_state(EVENT_SET_LStOvSetTimer);
        ecb_attr->value[0] = time >> 8;
        ecb_attr->value[1] = time;
        ecb_attr->change = 1;

        ecb_attr = get_event_state(EVENT_LStOvSetTimerLeft);
        ecb_attr->value[0] = time >> 8;
        ecb_attr->value[1] = time;
        ecb_attr->change = 1;

        if (time > 15)
            time = 15;
        ecb_set_state[17] &= 0xf0;
        ecb_set_state[17] |= time;
    }
    else
    {
        ecb_attr = get_event_state(EVENT_SET_RStOvSetTimer);
        ecb_attr->value[0] = time >> 8;
        ecb_attr->value[1] = time;
        ecb_attr->change = 1;

        ecb_attr = get_event_state(EVENT_RStOvSetTimerLeft);
        ecb_attr->value[0] = time >> 8;
        ecb_attr->value[1] = time;
        ecb_attr->change = 1;

        if (time > 15)
            time = 15;
        ecb_set_state[17] &= 0x0f;
        ecb_set_state[17] |= time << 4;
    }
    return 0;
}
static int set_work_temp(char dir, unsigned short temp)
{
    if (dir == 0)
    {
        ecb_set_state[13] = temp >> 8;
        ecb_set_state[12] = temp;
    }
    else
    {
        ecb_set_state[15] = temp >> 8;
        ecb_set_state[14] = temp;
    }
    return 0;
}
static int set_work_mode_time_temp(char dir, char mode, unsigned short temp, unsigned short time)
{
    set_work_mode(dir, mode);
    set_work_temp(dir, temp);
    set_work_time(dir, time);
    return 0;
}
static void set_multiStageState(char dir)
{
    ecb_attr_t *ecb_attr;
    if (dir == WORK_DIR_LEFT)
    {
        ecb_attr = get_event_state(EVENT_LMultiStageState);
        ecb_attr->value[0] = left_multistage_state.total_step;
        ecb_attr->value[1] = left_multistage_state.current_step;
        ecb_attr->change = 1;

        set_work_mode_time_temp(WORK_DIR_LEFT, left_multistage_state.step[left_multistage_state.current_step - 1].mode, left_multistage_state.step[left_multistage_state.current_step - 1].temp, left_multistage_state.step[left_multistage_state.current_step - 1].time);
    }
    else
    {
        ecb_attr = get_event_state(EVENT_RMultiStageState);
        ecb_attr->value[0] = right_multistage_state.total_step;
        ecb_attr->value[1] = right_multistage_state.current_step;
        ecb_attr->change = 1;

        set_work_mode_time_temp(WORK_DIR_RIGHT, right_multistage_state.step[right_multistage_state.current_step - 1].mode, right_multistage_state.step[right_multistage_state.current_step - 1].temp, right_multistage_state.step[right_multistage_state.current_step - 1].time);
    }
}
int right_ice_state_finish()
{
    if (right_multistage_state.valid && right_multistage_state.current_step < right_multistage_state.total_step)
    {
        right_multistage_state.current_step += 1;
        set_multiStageState(WORK_DIR_RIGHT);
        if (right_ice_operation(WORK_OPERATION_RUN) == 0)
        {
            work_state_operation(WORK_DIR_RIGHT, WORK_STATE_PREHEAT);
        }
        else
            work_state_operation(WORK_DIR_RIGHT, WORK_STATE_NOWORK);
    }
    else
    {
        right_multistage_state.valid = 0;
    }
    return 0;
}
static int ecb_parse_set_cmd(const unsigned char cmd, const unsigned char *value)
{
    ecb_attr_t *ecb_attr;
    int ret = 1;
    switch (cmd)
    {
    case EVENT_SET_SysPower:
        if (*value)
        {
            ecb_set_state[4] = 3;
        }
        else
        {
            ecb_set_state[4] = 1;
        }
        break;
    case EVENT_SET_HoodLight:
        if (*value)
            ecb_set_state[8] |= 0x01;
        else
            ecb_set_state[8] &= ~0x01;

        ecb_attr = get_event_state(EVENT_SET_HoodLight);
        ecb_attr->value[0] = *value;
        ecb_attr->change = 1;
        break;
    case EVENT_SET_HoodSpeed:
        if (*value < hood_min_speed)
            break;

        ecb_set_state[16] = *value;
        ecb_attr = get_event_state(EVENT_SET_HoodSpeed);
        ecb_attr->value[0] = *value;
        ecb_attr->change = 1;
        break;
    case EVENT_SET_LStOvMode:
        set_work_mode(WORK_DIR_LEFT, value[0]);
        break;
    case EVENT_SET_RStOvMode:
        set_work_mode(WORK_DIR_RIGHT, value[0]);
        break;
    case EVENT_SET_LStOvSetTemp:
        set_work_temp(WORK_DIR_LEFT, (value[0] << 8) + value[1]);
        ret = 2;
        break;
    case EVENT_SET_RStOvSetTemp:
        set_work_temp(WORK_DIR_RIGHT, (value[0] << 8) + value[1]);
        ret = 2;
        break;
    case EVENT_SET_LStOvSetTimer:
        set_work_time(WORK_DIR_LEFT, (value[0] << 8) + value[1]);
        ret = 2;
        break;
    case EVENT_SET_RStOvSetTimer:
        set_work_time(WORK_DIR_RIGHT, (value[0] << 8) + value[1]);
        ret = 2;
        break;
    case EVENT_SET_LStOvOrderTimer:
        ecb_attr = get_event_state(EVENT_SET_LStOvOrderTimer);
        ecb_attr->value[0] = value[0];
        ecb_attr->value[1] = value[1];
        ecb_attr->change = 1;

        ecb_attr = get_event_state(EVENT_LStOvOrderTimerLeft);
        ecb_attr->value[0] = value[0];
        ecb_attr->value[1] = value[1];
        ecb_attr->change = 1;

        left_order_time_remaining = (value[0] << 8) + value[1];
        ret = 2;
        break;
    case EVENT_SET_RStOvOrderTimer:
        ecb_attr = get_event_state(EVENT_SET_RStOvOrderTimer);
        ecb_attr->value[0] = value[0];
        ecb_attr->value[1] = value[1];
        ecb_attr->change = 1;

        ecb_attr = get_event_state(EVENT_RStOvOrderTimerLeft);
        ecb_attr->value[0] = value[0];
        ecb_attr->value[1] = value[1];
        ecb_attr->change = 1;

        right_order_time_remaining = (value[0] << 8) + value[1];
        ret = 2;
        break;
    case SET_LStOvOperation:
    {
        unsigned char work_state = ecb_set_state[7] & 0x0f;
        unsigned char operation = *value;
        if (left_order_time_remaining == 0)
        {
            work_state = set_work_operation(work_state, operation);
            work_state_operation(WORK_DIR_LEFT, work_state);
        }
        else
        {
            switch (operation)
            {
            case 0x00:
                left_order_time_remaining &= 0x7fff;
                POSIXTimerSet(left_work_timer, 60, 60);
                work_state = REPORT_WORK_STATE_RESERVE;
                break;
            case 0x01:
                left_order_time_remaining |= 0x8000;
                POSIXTimerSet(left_work_timer, 0, 0);
                work_state = REPORT_WORK_STATE_RESERVE_PAUSE;
                break;
            case 0x02:
                left_order_time_remaining = 0;
                POSIXTimerSet(left_work_timer, 0, 0);
                work_state = REPORT_WORK_STATE_STOP;
                break;
            case 0x03:

                break;
            case 0x04:
                left_order_time_remaining = 0;
                work_state = WORK_STATE_PREHEAT;
                work_state_operation(WORK_DIR_LEFT, work_state);
                break;
            }
            if (operation != 0x04)
            {
                ecb_attr_t *ecb_attr = get_event_state(EVENT_LStOvState);
                if (ecb_attr->value[0] != work_state)
                {
                    ecb_attr->value[0] = work_state;
                    ecb_attr->change = 1;
                    ecb_parse_event_uds(0);
                }
            }
        }
    }
    break;
    case SET_RStOvOperation:
    {
        unsigned char work_state = ecb_set_state[7] >> 4;
        unsigned char operation = *value;
        if (right_order_time_remaining == 0)
        {
            if (right_ice_operation(operation) == 0)
            {
                work_state = set_work_operation(work_state, operation);
                work_state_operation(WORK_DIR_RIGHT, work_state);
            }
        }
        else
        {
            switch (operation)
            {
            case 0x00:
                right_order_time_remaining &= 0x7fff;
                POSIXTimerSet(right_work_timer, 60, 60);
                work_state = REPORT_WORK_STATE_RESERVE;
                break;
            case 0x01:
                right_order_time_remaining |= 0x8000;
                POSIXTimerSet(right_work_timer, 0, 0);
                work_state = REPORT_WORK_STATE_RESERVE_PAUSE;
                break;
            case 0x02:
                right_order_time_remaining = 0;
                POSIXTimerSet(right_work_timer, 0, 0);
                work_state = REPORT_WORK_STATE_STOP;
                break;
            case 0x03:

                break;
            case 0x04:
                right_order_time_remaining = 0;
                work_state = WORK_STATE_PREHEAT;
                work_state_operation(WORK_DIR_RIGHT, work_state);
                break;
            }
            if (operation != 0x04)
            {
                ecb_attr_t *ecb_attr = get_event_state(EVENT_RStOvState);
                if (ecb_attr->value[0] != work_state)
                {
                    ecb_attr->value[0] = work_state;
                    ecb_attr->change = 1;
                    ecb_parse_event_uds(0);
                }
            }
        }
    }
    break;
    case EVENT_SET_LMultiMode:
        ecb_attr = get_event_state(EVENT_SET_LMultiMode);
        ecb_attr->value[0] = value[0];
        ecb_attr->change = 1;
        break;
    case SET_LMultiStageContent:
    {
        left_multistage_state.valid = 1;
        left_multistage_state.total_step = value[0];
        left_multistage_state.current_step = 1;
        unsigned char index = 1;
        for (int i = 0; i < left_multistage_state.total_step; ++i)
        {
            left_multistage_state.step[i].mode = value[index + 2];
            left_multistage_state.step[i].temp = (value[index + 3] << 8) + value[index + 4];
            left_multistage_state.step[i].time = (value[index + 5] << 8) + value[index + 6];
            index += 13;
        }
        set_multiStageState(WORK_DIR_LEFT);
        ret = index;
    }
    break;
    case SET_RMultiStageContent:
    {
        right_multistage_state.valid = 1;
        right_multistage_state.total_step = value[0];
        right_multistage_state.current_step = 1;
        unsigned char index = 1;
        for (int i = 0; i < right_multistage_state.total_step; ++i)
        {
            right_multistage_state.step[i].mode = value[index + 2];
            right_multistage_state.step[i].temp = (value[index + 3] << 8) + value[index + 4];
            right_multistage_state.step[i].time = (value[index + 5] << 8) + value[index + 6];
            index += 13;
        }
        set_multiStageState(WORK_DIR_RIGHT);
        ret = index;
    }
    break;
    case EVENT_SET_LCookbookID:
        ecb_attr = get_event_state(EVENT_SET_LCookbookID);
        memcpy(ecb_attr->value, value, ecb_attr->uart_byte_len);
        ecb_attr->change = 1;
        ret = 4;
        break;
    case EVENT_SET_RCookbookID:
        ecb_attr = get_event_state(EVENT_SET_RCookbookID);
        memcpy(ecb_attr->value, value, ecb_attr->uart_byte_len);
        ecb_attr->change = 1;
        ret = 4;
        break;
    case EVEN_SET_RMultiMode:
        ecb_attr = get_event_state(EVEN_SET_RMultiMode);
        memcpy(ecb_attr->value, value, ecb_attr->uart_byte_len);
        ecb_attr->change = 1;
        break;
    case EVENT_SET_LSteamGear:
        ecb_attr = get_event_state(EVENT_SET_LSteamGear);
        ecb_attr->value[0] = value[0];
        ecb_attr->change = 1;
        break;
    case EVENT_SET_DataReportReason:
        ecb_attr = get_event_state(EVENT_SET_DataReportReason);
        ecb_attr->value[0] = value[0];
        ecb_attr->change = 1;
        break;
    case SET_BuzControl:
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
    dzlog_warn("%s,val.sival_int:%d", __func__, val.sival_int);
    if (val.sival_int == 0)
    {
        if (left_order_time_remaining == 0)
        {
            left_work_time_remaining &= 0x7fff;
            if (left_work_time_remaining > 0)
            {
                --left_work_time_remaining;
                ecb_attr_t *ecb_attr = get_event_state(EVENT_LStOvSetTimerLeft);
                ecb_attr->value[0] = left_work_time_remaining >> 8;
                ecb_attr->value[1] = left_work_time_remaining;
                ecb_attr->change = 1;
            }

            if (left_work_time_remaining > 15)
                left_work_time_remaining = 15;
            ecb_set_state[17] &= 0xf0;
            ecb_set_state[17] |= left_work_time_remaining;

            if ((left_work_time_remaining & 0x7fff) <= 0)
            {
                POSIXTimerSet(left_work_timer, 0, 0);
                left_work_time_remaining = 0;
                if (left_multistage_state.valid && left_multistage_state.current_step < left_multistage_state.total_step)
                {
                    left_multistage_state.current_step += 1;
                    set_multiStageState(WORK_DIR_LEFT);
                    work_state_operation(WORK_DIR_LEFT, WORK_STATE_PREHEAT);
                }
                else
                {
                    left_multistage_state.valid = 0;
                    work_state_operation(WORK_DIR_LEFT, WORK_STATE_FINISH);
                }
            }
        }
        else
        {
            left_order_time_remaining &= 0x7fff;
            if (left_order_time_remaining > 0)
            {
                --left_order_time_remaining;
                ecb_attr_t *ecb_attr = get_event_state(EVENT_LStOvOrderTimerLeft);
                ecb_attr->value[0] = left_order_time_remaining >> 8;
                ecb_attr->value[1] = left_order_time_remaining;
                ecb_attr->change = 1;
                ecb_parse_event_uds(0);
            }

            if ((left_order_time_remaining & 0x7fff) <= 0)
            {
                POSIXTimerSet(left_work_timer, 0, 0);
                work_state_operation(WORK_DIR_LEFT, WORK_STATE_PREHEAT);
                left_order_time_remaining = 0;
            }
        }
    }
    else if (val.sival_int == 1)
    {
        if (right_order_time_remaining == 0)
        {
            right_work_time_remaining &= 0x7fff;
            if (right_work_time_remaining > 0)
            {
                --right_work_time_remaining;
                ecb_attr_t *ecb_attr = get_event_state(EVENT_RStOvSetTimerLeft);
                ecb_attr->value[0] = right_work_time_remaining >> 8;
                ecb_attr->value[1] = right_work_time_remaining;
                ecb_attr->change = 1;
            }
            if (right_work_time_remaining > 15)
                right_work_time_remaining = 15;
            ecb_set_state[17] &= 0x0f;
            ecb_set_state[17] |= right_work_time_remaining << 4;
            if ((right_work_time_remaining & 0x7fff) <= 0)
            {
                POSIXTimerSet(right_work_timer, 0, 0);
                right_work_time_remaining = 0;
                if (right_multistage_state.valid && right_multistage_state.current_step < right_multistage_state.total_step)
                {
                    right_multistage_state.current_step += 1;
                    set_multiStageState(WORK_DIR_RIGHT);
                    if (right_ice_operation(WORK_OPERATION_RUN) == 0)
                        work_state_operation(WORK_DIR_RIGHT, WORK_STATE_PREHEAT);
                    else
                        work_state_operation(WORK_DIR_RIGHT, WORK_STATE_NOWORK);
                }
                else
                {
                    right_multistage_state.valid = 0;
                    work_state_operation(WORK_DIR_RIGHT, WORK_STATE_FINISH);
                }
            }
        }
        else
        {
            right_order_time_remaining &= 0x7fff;
            if (right_order_time_remaining > 0)
            {
                --right_order_time_remaining;
                ecb_attr_t *ecb_attr = get_event_state(EVENT_RStOvOrderTimerLeft);
                ecb_attr->value[0] = right_order_time_remaining >> 8;
                ecb_attr->value[1] = right_order_time_remaining;
                ecb_attr->change = 1;
                ecb_parse_event_uds(0);
            }

            if ((right_order_time_remaining & 0x7fff) <= 0)
            {
                POSIXTimerSet(right_work_timer, 0, 0);
                right_order_time_remaining = 0;

                work_state_operation(WORK_DIR_RIGHT, WORK_STATE_PREHEAT);
            }
        }
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