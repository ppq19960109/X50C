#include "main.h"

#include "uart_task.h"
#include "ecb_uart.h"
#include "ecb_uart_parse_msg.h"
#include "cloud_platform_task.h"
#include "uds_protocol.h"
#include "pangu_uart.h"

static timer_t g_pangu_timer;

static int fd = -1;
static pthread_mutex_t lock;
static struct Select_Client_Event select_client_event;

static pangu_attr_t g_pangu_attr[] = {
    {
        key : "Weigh",
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
            cJSON_AddNumberToObject(resp, attr->key, cal_value_int(attr->value, attr->value_len));
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
    int res = pangu_uart_send(msg, index);
    free(send_msg);
    return res;
}
static int pangu_payload_parse(const unsigned char cmd, const unsigned char *payload, const int len)
{
    pangu_attr_t *attr;
    unsigned short state;
    if (cmd == 0x10)
    {
        state = (payload[0] << 8) + payload[1];
        attr = get_pangu_attr("MotorDirection");
        attr->value[0] = (state >> 14) & 0x01;
        attr->change = 1;

        state = payload[2];
        attr = get_pangu_attr("PotLidLock");
        attr->value[0] = (state >> 7) & 0x01;
        attr->change = 1;
        attr = get_pangu_attr("HeatingGear");
        attr->value[0] = state & 0x7f;
        attr->change = 1;

        state = (payload[3] << 8) + payload[4];
        attr = get_pangu_attr("MotorSpeed");
        attr->value[0] = state >> 8;
        attr->value[1] = state;
        attr->change = 1;

        state = payload[13];
        attr = get_pangu_attr("HeatingTemp");
        attr->value[0] = state;
        attr->change = 1;

        state = payload[14];
        attr = get_pangu_attr("HeatingMethod");
        attr->value[0] = state;
        attr->change = 1;
    }
    else if (cmd == 0x08)
    {
        if (payload[0] != 0)
            return -1;
        unsigned short cmd_id = (payload[1] << 8) + payload[2];
    }
    else if (cmd == 0x0a)
    {
        state = (payload[0] << 8) + payload[1];
        attr = &g_pangu_attr[0];
        attr->value[0] = state >> 8;
        attr->value[1] = state;
        attr->change = 1;

        state = payload[2];
        attr = &g_pangu_attr[1];
        attr->value[0] = state;
        attr->change = 1;

        state = payload[3];
        attr = &g_pangu_attr[2];
        attr->value[0] = state;
        attr->change = 1;

        state = payload[4];
        attr = &g_pangu_attr[3];
        attr->value[0] = state;
        attr->change = 1;

        state = payload[5];
        attr = &g_pangu_attr[4];
        attr->value[0] = state;
        attr->change = 1;

        state = payload[6];
        attr = &g_pangu_attr[5];
        attr->value[0] = state;
        attr->change = 1;

        state = payload[76];
        attr = &g_pangu_attr[6];
        attr->value[0] = state;
        attr->change = 1;
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
    unsigned char device_type;
    const unsigned char *payload;
    unsigned short payload_len;
    unsigned short verify;

    cmd_len = (in[index + cmd_index] << 8) + in[index + cmd_index + 1];
    cmd_index += 2;
    cmd = in[index + cmd_index];
    cmd_index += 1;
    device_type = in[index + cmd_index];
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
        return ECB_UART_READ_CHECK_ERR;
    }
    //----------------------
    pangu_payload_parse(cmd, payload, payload_len);
    return ECB_UART_READ_VALID;
}

static void POSIXTimer_cb(union sigval val)
{
    dzlog_warn("%s sival_int:%d", __func__, val.sival_int);
    if (val.sival_int == 0)
    {
    }
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
        dzlog_warn("recv from pangu-------------------------- uart_read_len:%d uart_read_buf_index:%d", uart_read_len, uart_read_buf_index);
        hdzlog_info(uart_read_buf, uart_read_buf_index);
        uart_parse_msg(uart_read_buf, &uart_read_buf_index, pangu_uart_parse_msg);
        dzlog_warn("pangu uart_read_buf_index:%d", uart_read_buf_index);
        // hdzlog_info(uart_read_buf, uart_read_buf_index);
        pangu_state_event(0);
    }
    return 0;
}

int pangu_recv_set(void *data)
{
    static unsigned char uart_buf[256];
    unsigned short uart_buf_len = 0;
    uart_buf[uart_buf_len++] = 0x00;
    uart_buf[uart_buf_len++] = 0x01;
    uart_buf[uart_buf_len++] = 0xb2;
    uart_buf[uart_buf_len++] = 0x07;
    cJSON *root = data;
    if (cJSON_HasObjectItem(root, "HeatingMethod") && cJSON_HasObjectItem(root, "HeatingTemp"))
    {
        uart_buf[uart_buf_len++] = 0x04;
        uart_buf[uart_buf_len++] = 0x01;
        cJSON *item = cJSON_GetObjectItem(root, "HeatingMethod");
        pangu_send_msg(0x73, uart_buf, uart_buf_len);
    }
    uart_buf_len = 4;
    if (cJSON_HasObjectItem(root, "PotLidLock"))
    {
        uart_buf[uart_buf_len++] = 0x04;
        uart_buf[uart_buf_len++] = 0x02;
        pangu_send_msg(0x73, uart_buf, uart_buf_len);
    }
    uart_buf_len = 4;
    if (cJSON_HasObjectItem(root, "HeatingGear") && cJSON_HasObjectItem(root, "HeatingTemp"))
    {
        uart_buf[uart_buf_len++] = 0x04;
        uart_buf[uart_buf_len++] = 0x03;
        pangu_send_msg(0x73, uart_buf, uart_buf_len);
    }
    if (cJSON_HasObjectItem(root, "HeatingTime"))
    {

    }
    uart_buf_len = 7;
    uart_buf[0] = 0;
    pangu_attr_t *attr;
    for (int i = 1; i < 7; ++i)
    {
        attr = &g_pangu_attr[i];
        if (cJSON_HasObjectItem(root, attr->key))
        {
            cJSON *item = cJSON_GetObjectItem(root, attr->key);
            uart_buf[i] = item->valueint;
            uart_buf[0] |= 1 << (6 - i);
        }
        else
        {
            uart_buf[i] = 0;
        }
    }
    if (uart_buf[0] > 0)
        pangu_send_msg(0x1a, uart_buf, uart_buf_len);
    return 0;
}
void pangu_uart_deinit(void)
{
    POSIXTimerDelete(g_pangu_timer);
    close(fd);
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

    select_client_event.fd = fd;
    select_client_event.read_cb = pangu_recv_cb;
    add_select_client_uart(&select_client_event);
}
