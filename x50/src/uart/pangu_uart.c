#include "main.h"

#include "uart_task.h"
#include "ecb_uart.h"
#include "ecb_uart_parse_msg.h"
#include "uds_protocol.h"
#include "pangu_uart.h"

static timer_t g_pangu_timer;
static int g_time = 0, g_total_time = 0, g_order_time = 0;

static int fd = -1;
static pthread_mutex_t lock;
static struct Select_Client_Event select_client_event;
static pangu_cook_t pangu_cook;

static pangu_attr_t g_pangu_attr[] = {
    {
        key : "Weight",
        value_len : 2,
    },
    {
        key : "WaterInletValve",
        value_len : 1,
    },
    {
        key : "Exhaust",
        value_len : 1,
    },
    {
        key : "IndicatorLight",
        value_len : 1,
    },
    {
        key : "PressurePot",
        value_len : 1,
    },
    {
        key : "PushRod",
        value_len : 1,
    },
    {
        key : "HeatDissipation",
        value_len : 1,
    },
    {
        key : "hallState",
        value_len : 1,
    },
    //--------------------------------
    {
        key : "HeatingMethod",
        value_len : 1,
    },
    {
        key : "HeatingTemp",
        value_len : 1,
    },
    {
        key : "MotorDir",
        value_len : 1,
    },
    {
        key : "MotorSpeed",
        value_len : 1,
    },
    {
        key : "PotLidLock",
        value_len : 1,
    },
    //----------------------
    {
        key : "RStOvState",
        value_len : 1,
    },
    {
        key : "RStOvMode",
        value_len : 1,
    },
    {
        key : "RMultiMode",
        value_len : 1,
    },
    {
        key : "ClearMode",
        value_len : 1,
    },
    {
        key : "RStOvSetTemp",
        value_len : 2,
    },
    {
        key : "RStOvRealTemp",
        value_len : 2,
    },
    {
        key : "RStOvSetTimer",
        value_len : 2,
    },
    {
        key : "RStOvSetTimerLeft",
        value_len : 2,
    },
    {
        key : "RStOvOrderTimer",
        value_len : 2,
    },
    {
        key : "RStOvOrderTimerLeft",
        value_len : 2,
    },
    {
        key : "RCookbookName",
        value_len : 64,
        value_type : LINK_VALUE_TYPE_STRING
    },
};
static int g_pangu_attr_len = sizeof(g_pangu_attr) / sizeof(g_pangu_attr[0]);
static pangu_attr_t *get_pangu_attr(const char *key)
{
    for (int i = 0; i < g_pangu_attr_len; ++i)
    {
        if (strcmp(key, g_pangu_attr[i].key) == 0)
        {
            return &g_pangu_attr[i];
        }
    }
    return NULL;
}
static int cal_value_int(const char *value, const char value_len)
{
    int num = 0;
    for (int i = 0; i < value_len; ++i)
    {
        num = (num << 8) + value[i];
    }
    return num;
}
int pangu_state_event(unsigned char cmd)
{
    cJSON *resp = cJSON_CreateObject();
    for (int i = 0; i < g_pangu_attr_len; ++i)
    {
        pangu_attr_t *attr = &g_pangu_attr[i];
        if (0 < attr->change || cmd > 0)
        {
            if (attr->value_type == LINK_VALUE_TYPE_NUM)
            {
                cJSON_AddNumberToObject(resp, attr->key, cal_value_int(attr->value, attr->value_len));
            }
            else if (attr->value_type == LINK_VALUE_TYPE_STRING)
            {
                cJSON_AddStringToObject(resp, attr->key, attr->value);
            }
            attr->change = 0;
        }
    }
    send_event_uds(resp, NULL);
    return 0;
}

static unsigned short crc16_modbus(const unsigned char *ptr, int len)
{
    unsigned int i;
    unsigned short crc = 0xFFFF;
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
    return crc;
}

int pangu_uart_send(unsigned char *in, int in_len)
{
    dzlog_warn("uart send to pangu--------------------------");
    hdzlog_info(in, in_len);
    if (fd < 0)
    {
        dzlog_error("pangu_uart_send fd error\n");
        return -1;
    }
    int res = 0;
    if (pthread_mutex_lock(&lock) == 0)
    {
        if (in == NULL || in_len <= 0)
        {
            res = 0;
            goto fail;
        }
        res = write(fd, in, in_len);
        usleep(200000);
        res = write(fd, in, in_len);
        usleep(200000);
        res = write(fd, in, in_len);
        usleep(200000);
    fail:
        pthread_mutex_unlock(&lock);
        return res;
    }
    else
    {
        dzlog_error("pthread_mutex_lock error\n");
    }
    return res;
}

int pangu_send_msg(const unsigned char command, unsigned char *msg, const int msg_len)
{
    int index = 0;
    unsigned char *send_msg = (unsigned char *)malloc(8 + msg_len);
    if (send_msg == NULL)
    {
        dzlog_error("malloc error\n");
        return -1;
    }
    send_msg[index++] = '*';
    unsigned short cmd_len = msg_len + 8;
    send_msg[index++] = cmd_len >> 8;
    send_msg[index++] = cmd_len & 0xff;
    send_msg[index++] = command;
    send_msg[index++] = 0;
    if (msg_len > 0 && msg != NULL)
    {
        memcpy(&send_msg[index], msg, msg_len);
        index += msg_len;
    }
    unsigned short crc16 = crc16_modbus((const unsigned char *)(send_msg + 3), index - 3);
    send_msg[index++] = crc16 >> 8;
    send_msg[index++] = crc16;
    send_msg[index++] = '#';
    int res = pangu_uart_send(send_msg, index);
    free(send_msg);
    return res;
}
static int pangu_payload_parse(const unsigned char cmd, const unsigned char *payload, const int len)
{
    // dzlog_warn("%s,cmd:%d",__func__,cmd);
    // hdzlog_info(payload, len);
    pangu_attr_t *attr;
    unsigned short state;
    if (cmd == 0x10)
    {
        state = (payload[0] << 8) + payload[1];
        state = (state >> 14) & 0x01;
        attr = get_pangu_attr("MotorDir");
        if (attr->value[0] != state)
        {
            attr->value[0] = state;
            attr->change = 1;
        }

        state = (payload[2] >> 7) & 0x01;
        attr = get_pangu_attr("PotLidLock");
        if (attr->value[0] != state)
        {
            attr->value[0] = state;
            attr->change = 1;
        }
        state = payload[2] & 0x7f;
        attr = get_pangu_attr("MotorSpeed");
        if (attr->value[0] != state)
        {
            attr->value[0] = state;
            attr->change = 1;
        }
        state = payload[7];
        attr = get_pangu_attr("RStOvRealTemp");
        if (attr->value[1] != state)
        {
            attr->value[0] = 0;
            attr->value[1] = payload[7];
            attr->change = 1;
        }
    }
    else if (cmd == 0x08)
    {
        if (payload[0] != 0)
            return -1;
        // unsigned short cmd_id = (payload[1] << 8) + payload[2];
    }
    else if (cmd == 0x0a)
    {
        attr = &g_pangu_attr[0];
        if (attr->value[0] != payload[0] || attr->value[1] != payload[1])
        {
            attr->value[0] = payload[0];
            attr->value[1] = payload[1];
            attr->change = 1;
        }

        state = payload[2];
        attr = &g_pangu_attr[1];
        if (attr->value[0] != state)
        {
            attr->value[0] = state;
            attr->change = 1;
        }

        state = payload[3];
        attr = &g_pangu_attr[2];
        if (attr->value[0] != state)
        {
            attr->value[0] = state;
            attr->change = 1;
        }

        state = payload[4];
        attr = &g_pangu_attr[3];
        if (attr->value[0] != state)
        {
            attr->value[0] = state;
            attr->change = 1;
        }

        state = payload[5];
        attr = &g_pangu_attr[4];
        if (attr->value[0] != state)
        {
            attr->value[0] = state;
            attr->change = 1;
        }

        state = payload[6];
        attr = &g_pangu_attr[5];
        if (attr->value[0] != state)
        {
            attr->value[0] = state;
            attr->change = 1;
        }

        state = payload[7];
        attr = &g_pangu_attr[6];
        if (attr->value[0] != state)
        {
            attr->value[0] = state;
            attr->change = 1;
        }

        state = payload[8];
        attr = &g_pangu_attr[7];
        if (attr->value[0] != state)
        {
            attr->value[0] = state;
            attr->change = 1;
        }
    }
    else
    {
        return -1;
    }
    return 0;
}
static int pangu_uart_parse_msg(const unsigned char *in, const int in_len, int *end)
{
    int index = 0, i;
    if (in_len < 1)
    {
        *end = 0;
        dzlog_warn("input len too small");
        return ECB_UART_READ_LEN_SMALL;
    }

    for (i = 0; i < in_len; ++i)
    {
        if (in[i] == '*')
            break;
    }
    if (i >= in_len)
    {
        dzlog_error("no header was detected");
        return ECB_UART_READ_NO_HEADER;
    }
    index = i;

    if (in_len - index < 8)
    {
        *end = index;
        dzlog_warn("input len too small");
        return ECB_UART_READ_LEN_SMALL;
    }
    unsigned short cmd_index = 1;
    unsigned short cmd_len;
    unsigned char cmd;
    // unsigned char device_type;
    const unsigned char *payload;
    unsigned short payload_len;
    unsigned short verify;

    cmd_len = (in[index + cmd_index] << 8) + in[index + cmd_index + 1];
    cmd_index += 2;
    cmd = in[index + cmd_index];
    cmd_index += 1;
    // device_type = in[index + cmd_index];
    cmd_index += 1;
    payload = &in[index + cmd_index];
    payload_len = cmd_len - 8;
    cmd_index += payload_len;
    if (index + cmd_index + 3 > in_len)
    {
        *end = index;
        dzlog_error("input data len too small");
        return ECB_UART_READ_LEN_SMALL;
    }
    else if (in[index + cmd_index + 2] != '#')
    {
        *end = index + 1;
        dzlog_error("no tailer was detected");
        return ECB_UART_READ_TAILER_ERR;
    }
    verify = (in[index + cmd_index] << 8) + in[index + cmd_index + 1];
    unsigned short verify_val = crc16_modbus(&in[index + 3], cmd_index - 3);
    cmd_index += 3;
    *end = index + cmd_index;
    if (verify_val != verify)
    {
        dzlog_error("verify error:%x,%x", verify_val, verify);
        // return ECB_UART_READ_CHECK_ERR;
    }
    //----------------------
    if (cmd != 0x10)
    {
        hdzlog_info(&in[index], cmd_index);
    }
    pangu_payload_parse(cmd, payload, payload_len);
    return ECB_UART_READ_VALID;
}

static int pangu_recv_cb(void *arg)
{
    static unsigned char uart_read_buf[256];
    int uart_read_len;
    static int uart_read_buf_index = 0;

    uart_read_len = read(fd, &uart_read_buf[uart_read_buf_index], sizeof(uart_read_buf) - uart_read_buf_index);
    if (uart_read_len > 0)
    {
        uart_read_buf_index += uart_read_len;
        // dzlog_warn("recv from pangu-------------------------- uart_read_len:%d uart_read_buf_index:%d", uart_read_len, uart_read_buf_index);
        // hdzlog_info(uart_read_buf, uart_read_buf_index);
        uart_parse_msg(uart_read_buf, &uart_read_buf_index, pangu_uart_parse_msg);
        // dzlog_warn("pangu uart_read_buf_index:%d", uart_read_buf_index);
        //  hdzlog_info(uart_read_buf, uart_read_buf_index);
        pangu_state_event(0);
    }
    return 0;
}
static void pangu_transfer_set(signed char WaterInletValve, signed char Exhaust, signed char IndicatorLight, signed char PressurePot, signed char PushRod, signed char HeatDissipation)
{
    unsigned char uart_buf[8] = {0};

    if (WaterInletValve >= 0)
    {
        uart_buf[0] |= 1 << 5;
        uart_buf[1] = WaterInletValve;
    }
    if (Exhaust >= 0)
    {
        uart_buf[0] |= 1 << 4;
        uart_buf[2] = Exhaust;
    }
    if (IndicatorLight >= 0)
    {
        uart_buf[0] |= 1 << 3;
        uart_buf[3] = IndicatorLight;
    }
    if (PressurePot >= 0)
    {
        uart_buf[0] |= 1 << 2;
        uart_buf[4] = PressurePot;
    }
    if (PushRod >= 0)
    {
        uart_buf[0] |= 1 << 1;
        uart_buf[5] = PushRod;
    }
    if (HeatDissipation >= 0)
    {
        uart_buf[0] |= 1 << 0;
        uart_buf[6] = HeatDissipation;
    }
    pangu_send_msg(0x1a, uart_buf, 7);
}
static void hoodSpeed_set(unsigned char speed)
{
    // if (speed <= 0)
    //     return;
    char hoodSpeed = get_HoodSpeed();
    if (hoodSpeed >= 0 && speed <= hoodSpeed)
    {
        return;
    }

    unsigned char uart_buf[2];
    uart_buf[0] = 0x31;
    uart_buf[1] = speed;
    ecb_uart_send_cloud_msg(uart_buf, sizeof(uart_buf));
}
static void report_work_state(unsigned char state)
{
    pangu_attr_t *attr = get_pangu_attr("RStOvState");
    if (attr->value[0] != state)
    {
        attr->value[0] = state;
        attr->change = 1;
        pangu_state_event(0);
    }
}
static void pangu_cuyi_set(unsigned char *buf, int len)
{
    unsigned char uart_buf[16] = {0x00, 0x01, 0xb2, 0x07};
    memcpy(&uart_buf[4], buf, len);
    pangu_send_msg(0x73, uart_buf, 4 + len);
}
// static void pangu_cuyi_get(unsigned char *buf, int len)
// {
//     unsigned char uart_buf[16] = {0x00, 0x01, 0xb2, 0x07};
//     memcpy(&uart_buf[4], buf, len);
//     pangu_send_msg(0x72, uart_buf, 4 + len);
// }
static int pangu_single_set(pangu_cook_attr_t *attr)
{
    pangu_attr_t *pangu_attr;
    if (attr->waterTime)
    {
        pangu_transfer_set(1, -1, -1, -1, -1, -1);
        if (g_time == 0)
            g_time = attr->waterTime;
        report_work_state(REPORT_WORK_STATE_WATER);
    }
    else
    {
        pangu_attr = get_pangu_attr("RStOvMode");
        if (pangu_attr->value[0] > 0)
        {
            report_work_state(REPORT_WORK_STATE_CLEAN);
        }
        else
            report_work_state(REPORT_WORK_STATE_RUN);

        unsigned char uart_buf[16] = {0};
        unsigned short uart_buf_len = 0;

        uart_buf[uart_buf_len++] = 0x04;
        uart_buf[uart_buf_len++] = 0x01;
        uart_buf[uart_buf_len] = 0;
        uart_buf[uart_buf_len] |= attr->mode << 6;
        uart_buf[uart_buf_len] |= attr->fire;
        ++uart_buf_len;
        uart_buf[uart_buf_len++] = attr->temp;
        pangu_cuyi_set(uart_buf, uart_buf_len);

        uart_buf_len = 0;
        uart_buf[uart_buf_len++] = 0x04;
        uart_buf[uart_buf_len++] = 0x02;
        uart_buf[uart_buf_len++] = attr->motorDir;
        pangu_cuyi_set(uart_buf, uart_buf_len);

        uart_buf_len = 0;
        uart_buf[uart_buf_len++] = 0x04;
        uart_buf[uart_buf_len++] = 0x03;
        uart_buf[uart_buf_len] = 0;
        uart_buf[uart_buf_len] |= 1 << 5;
        uart_buf[uart_buf_len] |= attr->motorSpeed;
        ++uart_buf_len;
        pangu_cuyi_set(uart_buf, uart_buf_len);

        if (attr->fan)
            pangu_transfer_set(-1, -1, -1, -1, -1, 1);
        if (attr->hoodSpeed > 0)
            hoodSpeed_set(attr->hoodSpeed);

        if (g_time == 0)
            g_time = attr->time;
    }
    POSIXTimerSet(g_pangu_timer, 1, 1);

    pangu_attr = get_pangu_attr("RStOvSetTemp");
    pangu_attr->value[0] = attr->temp >> 8;
    pangu_attr->value[1] = attr->temp;
    pangu_attr->change = 1;

    pangu_attr = get_pangu_attr("RStOvSetTimerLeft");
    pangu_attr->value[0] = g_total_time >> 8;
    pangu_attr->value[1] = g_total_time;
    pangu_attr->change = 1;
    pangu_state_event(0);

    return 0;
}
int pangu_cook_stop_pause_finish(unsigned char state)
{
    if (state == 0)
    {
        g_time = 0;
        g_order_time = 0;
        pangu_cook.total_step = 0;
        report_work_state(REPORT_WORK_STATE_STOP);
    }
    else if (state == 2)
    {
        g_time = 0;
        g_order_time = 0;
        pangu_cook.total_step = 0;
    }
    else
    {
        if (g_order_time)
            report_work_state(REPORT_WORK_STATE_RESERVE_PAUSE);
        else
            report_work_state(REPORT_WORK_STATE_PAUSE);
    }

    POSIXTimerSet(g_pangu_timer, 0, 0);
    if (g_order_time == 0)
    {
        pangu_transfer_set(0, -1, -1, -1, -1, 0);
        unsigned char uart_buf[16] = {0};
        unsigned short uart_buf_len = 0;

        uart_buf[uart_buf_len++] = 0x04;
        uart_buf[uart_buf_len++] = 0x01;
        uart_buf[uart_buf_len] = 0;
        uart_buf[uart_buf_len] |= 1 << 6;
        uart_buf[uart_buf_len] |= 0;
        ++uart_buf_len;
        uart_buf[uart_buf_len++] = 0;
        pangu_cuyi_set(uart_buf, uart_buf_len);
        uart_buf_len = 0;
        uart_buf[uart_buf_len++] = 0x04;
        uart_buf[uart_buf_len++] = 0x03;
        uart_buf[uart_buf_len] = 0;
        uart_buf[uart_buf_len] |= 2 << 5;
        uart_buf[uart_buf_len] |= 0;
        ++uart_buf_len;
        pangu_cuyi_set(uart_buf, uart_buf_len);
    }
    return 0;
}
int pangu_cook_start()
{
    pangu_attr_t *pangu_attr;
    if (g_order_time)
    {
        POSIXTimerSet(g_pangu_timer, 60, 60);
        report_work_state(REPORT_WORK_STATE_RESERVE);
        pangu_attr = get_pangu_attr("RStOvOrderTimerLeft");
        pangu_attr->value[0] = g_order_time >> 8;
        pangu_attr->value[1] = g_order_time;
        pangu_attr->change = 1;
        pangu_state_event(0);
    }
    else
    {
        if (pangu_cook.total_step != 0)
        {
            if (pangu_cook.current_step < pangu_cook.total_step)
            {
                pangu_single_set(&pangu_cook.cook_attr[pangu_cook.current_step]);
                return 1;
            }
            else
            {
                pangu_attr = get_pangu_attr("RStOvMode");
                if (pangu_attr->value[0] > 0)
                    report_work_state(REPORT_WORK_STATE_CLEAN_FINISH);
                else
                    report_work_state(REPORT_WORK_STATE_FINISH);
                pangu_cook_stop_pause_finish(2);
                hoodSpeed_set(0);
            }
        }
    }
    return 0;
}

int pangu_recv_set(void *data)
{
    cJSON *root = data;
    cJSON *item;
    pangu_attr_t *pangu_attr;
    if (cJSON_HasObjectItem(root, "RMultiStageContent"))
    {
        g_total_time = 0;
        item = cJSON_GetObjectItem(root, "RMultiStageContent");
        int arraySize = cJSON_GetArraySize(item);
        if (arraySize == 0)
        {
            dzlog_error("arraySize is 0\n");
            return 0;
        }
        pangu_cook.total_step = arraySize;
        pangu_cook.current_step = 0;
        pangu_cook_attr_t *pangu_cook_attr = pangu_cook.cook_attr;
        cJSON *arraySub;
        for (int i = 0; i < arraySize; i++)
        {
            arraySub = cJSON_GetArrayItem(item, i);
            if (arraySub == NULL)
                continue;
            cJSON *mode = cJSON_GetObjectItem(arraySub, "mode");
            cJSON *fire = cJSON_GetObjectItem(arraySub, "fire");
            cJSON *temp = cJSON_GetObjectItem(arraySub, "temp");
            cJSON *time = cJSON_GetObjectItem(arraySub, "time");
            cJSON *motorSpeed = cJSON_GetObjectItem(arraySub, "motorSpeed");
            cJSON *motorDir = cJSON_GetObjectItem(arraySub, "motorDir");
            cJSON *waterTime = cJSON_GetObjectItem(arraySub, "waterTime");
            cJSON *fan = cJSON_GetObjectItem(arraySub, "fan");
            cJSON *hoodSpeed = cJSON_GetObjectItem(arraySub, "hoodSpeed");
            cJSON *repeat = cJSON_GetObjectItem(arraySub, "repeat");
            cJSON *repeatStep = cJSON_GetObjectItem(arraySub, "repeatStep");
            pangu_cook_attr[i].mode = mode->valueint;
            pangu_cook_attr[i].fire = fire->valueint;
            pangu_cook_attr[i].temp = temp->valueint;
            pangu_cook_attr[i].time = time->valueint;
            pangu_cook_attr[i].motorSpeed = motorSpeed->valueint;
            pangu_cook_attr[i].motorDir = motorDir->valueint;
            pangu_cook_attr[i].waterTime = waterTime->valueint;
            pangu_cook_attr[i].fan = fan->valueint;
            pangu_cook_attr[i].hoodSpeed = hoodSpeed->valueint;
            pangu_cook_attr[i].repeat = repeat->valueint;
            pangu_cook_attr[i].repeatStep = repeatStep->valueint;
            if (pangu_cook_attr[i].repeat > 0 && pangu_cook_attr[i].repeatStep == 0)
                pangu_cook_attr[i].repeatStep = 1;
            g_total_time += pangu_cook_attr[i].waterTime;
            if (pangu_cook_attr[i].repeat && i > 0)
            {
                g_total_time += pangu_cook_attr[i].time * (pangu_cook_attr[i].repeat + 1);
                for (int j = pangu_cook_attr[i].repeatStep; j > 0; --j)
                {
                    g_total_time += pangu_cook_attr[i - j].time * (pangu_cook_attr[i].repeat);
                }
            }
            else
                g_total_time += pangu_cook_attr[i].time;
        }
        pangu_attr = get_pangu_attr("RStOvSetTimer");
        pangu_attr->value[0] = g_total_time >> 8;
        pangu_attr->value[1] = g_total_time;
        pangu_attr->change = 1;
    }
    if (cJSON_HasObjectItem(root, "RStOvMode"))
    {
        item = cJSON_GetObjectItem(root, "RStOvMode");
        pangu_attr = get_pangu_attr("RStOvMode");
        pangu_attr->value[0] = item->valueint;
        pangu_attr->change = 1;
    }
    if (cJSON_HasObjectItem(root, "RMultiMode"))
    {
        item = cJSON_GetObjectItem(root, "RMultiMode");
        pangu_attr = get_pangu_attr("RMultiMode");
        pangu_attr->value[0] = item->valueint;
        pangu_attr->change = 1;
    }
    if (cJSON_HasObjectItem(root, "ClearMode"))
    {
        item = cJSON_GetObjectItem(root, "ClearMode");
        pangu_attr = get_pangu_attr("ClearMode");
        pangu_attr->value[0] = item->valueint;
        pangu_attr->change = 1;
    }
    if (cJSON_HasObjectItem(root, "RStOvOrderTimer"))
    {
        item = cJSON_GetObjectItem(root, "RStOvOrderTimer");
        g_order_time = item->valueint;
        pangu_attr = get_pangu_attr("RStOvOrderTimer");
        pangu_attr->value[0] = g_order_time >> 8;
        pangu_attr->value[1] = g_order_time;
        pangu_attr->change = 1;
    }
    if (cJSON_HasObjectItem(root, "RCookbookName"))
    {
        item = cJSON_GetObjectItem(root, "RCookbookName");
        pangu_attr = get_pangu_attr("RCookbookName");
        strcpy(pangu_attr->value, item->valuestring);
        pangu_attr->change = 1;
    }
    if (cJSON_HasObjectItem(root, "RStOvOperation"))
    {
        item = cJSON_GetObjectItem(root, "RStOvOperation");
        switch (item->valueint)
        {
        case WORK_OPERATION_RUN:
            pangu_cook_start();
            break;
        case WORK_OPERATION_PAUSE:
            pangu_cook_stop_pause_finish(1);
            break;
        case WORK_OPERATION_STOP:
            pangu_cook_stop_pause_finish(0);
            break;
        case WORK_OPERATION_FINISH:
            report_work_state(REPORT_WORK_STATE_STOP);
            break;
        case WORK_OPERATION_RUN_NOW:
            g_order_time = 0;
            pangu_cook_start();
            break;
        }
    }
    if (cJSON_HasObjectItem(root, "PushRod"))
    {
        item = cJSON_GetObjectItem(root, "PushRod");
        pangu_transfer_set(-1, -1, -1, -1, item->valueint, -1);
    }
    return 0;
}
static void POSIXTimer_cb(union sigval val)
{
    // dzlog_warn("%s sival_int:%d", __func__, val.sival_int);
    if (val.sival_int == 0)
    {
        pangu_attr_t *pangu_attr;
        if (g_order_time)
        {
            --g_order_time;
            if (g_order_time == 0)
            {
                pangu_cook_start();
            }
            pangu_attr = get_pangu_attr("RStOvOrderTimerLeft");
            pangu_attr->value[0] = g_order_time >> 8;
            pangu_attr->value[1] = g_order_time;
            pangu_attr->change = 1;
            pangu_state_event(0);
        }
        else
        {
            --g_time;
            --g_total_time;
            if (g_time == 0)
            {
                POSIXTimerSet(g_pangu_timer, 0, 0);
                if (pangu_cook.cook_attr[pangu_cook.current_step].waterTime)
                {
                    pangu_cook.cook_attr[pangu_cook.current_step].waterTime = 0;
                    pangu_transfer_set(0, -1, -1, -1, -1, -1);
                    pangu_cook_start();
                }
                else
                {
                    if (pangu_cook.cook_attr[pangu_cook.current_step].repeat && pangu_cook.current_step >= pangu_cook.cook_attr[pangu_cook.current_step].repeatStep)
                    {
                        pangu_cook.cook_attr[pangu_cook.current_step].repeat -= 1;
                        pangu_cook.current_step -= pangu_cook.cook_attr[pangu_cook.current_step].repeatStep;
                    }
                    else
                    {
                        pangu_cook.current_step += 1;
                    }
                    pangu_cook_start();
                }
            }
            if (g_total_time % 60 == 0)
            {
                pangu_attr = get_pangu_attr("RStOvSetTimerLeft");
                pangu_attr->value[0] = g_total_time >> 8;
                pangu_attr->value[1] = g_total_time;
                pangu_attr->change = 1;
                pangu_state_event(0);
            }
        }
    }
}
void pangu_uart_deinit(void)
{
    POSIXTimerDelete(g_pangu_timer);
    close(fd);

    for (int i = 0; i < g_pangu_attr_len; ++i)
    {
        if (g_pangu_attr[i].value_len > 0)
            free(g_pangu_attr[i].value);
    }

    pthread_mutex_destroy(&lock);
}
// ntpdate pool.ntp.org
/*********************************************************************************
 *Function:  uart_ecb_task
 *Description： 手势任务函数，接收手势控制板的数据并处理
 *Input:
 *Return:
 **********************************************************************************/
void pangu_uart_init(void)
{
    fd = uart_init("/dev/ttyS3", BAUDRATE_9600, DATABIT_8, PARITY_NONE, STOPBIT_1, FLOWCTRL_NONE, BLOCKING_BLOCK);
    if (fd < 0)
    {
        dzlog_error("pangu_uart uart init error:%d,%s", errno, strerror(errno));
        return;
    }
    dzlog_info("pangu_uart,fd:%d", fd);

    pthread_mutex_init(&lock, NULL);

    g_pangu_timer = POSIXTimerCreate(0, POSIXTimer_cb);

    for (int i = 0; i < g_pangu_attr_len; ++i)
    {
        if (g_pangu_attr[i].value_len > 0)
        {
            g_pangu_attr[i].value = (char *)malloc(g_pangu_attr[i].value_len);
            if (g_pangu_attr[i].value == NULL)
            {
                dzlog_error("g_pangu_attr[%d].value malloc error\n", i);
                return;
            }
            memset(g_pangu_attr[i].value, 0, g_pangu_attr[i].value_len);
        }
    }

    select_client_event.fd = fd;
    select_client_event.read_cb = pangu_recv_cb;
    add_select_client_uart(&select_client_event);

    pangu_cook_stop_pause_finish(0);
}
