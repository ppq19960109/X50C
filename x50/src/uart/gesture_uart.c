#include "main.h"

#include "rkwifi.h"
#include "wifi_task.h"

#include "uart_task.h"
#include "ecb_uart.h"
#include "ecb_uart_parse_msg.h"
#include "cloud_platform_task.h"
#include "uds_protocol.h"
#include "link_ntp_posix.h"
#define GESTURE_ERROR (15)

static timer_t g_gesture_timer;
static timer_t g_gesture_heart_timer;

static int fd = -1;
static pthread_mutex_t lock;
static unsigned char gesture_error_code = 0;
static unsigned char gesture_alarm_status = 0;
static struct Select_Client_Event select_client_event;
static unsigned char gesture_power = 0;
static int gesture_send_error = 0;

static unsigned short msg_verify(const unsigned char *data, int len)
{
    unsigned short verify = 0;
    for (int i = 0; i < len; ++i)
    {
        verify += data[i];
    }
    // dzlog_debug("msg_verify:%d,%d", verify, ~verify);
    return ~verify;
}

int gesture_uart_send(unsigned char *in, int in_len)
{
    dzlog_warn("uart send to gesture--------------------------");
    hdzlog_info(in, in_len);
    if (fd < 0)
    {
        dzlog_error("gesture_uart_send fd error\n");
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

// aa 01 08 01 0d 03 04 37 ff
// aa 01 08 01 09 01 02 3f ff
// aa 01 08 01 01 00 00 4a ff
// aa 01 08 01 09 ff ff 44 fd
// aa 01 08 01 01 ff ff 4c fd
int gesture_send_msg(unsigned char whole_show, unsigned char sync, unsigned char hour, unsigned char minute, int alarm)
{
    unsigned char data1 = 0;
    if (gesture_power)
        data1 |= (1 << 0);
    if (sync)
        data1 |= (1 << 3);
    if (alarm)
        data1 |= (1 << 4);
    if (whole_show)
        data1 |= (1 << 2);

    unsigned char msg[16] = {0};
    unsigned char index = 0;
    msg[index++] = 0xaa;
    msg[index++] = 0x01;
    msg[index++] = 8;
    msg[index++] = 0x01;
    msg[index++] = data1;
    msg[index++] = hour;
    msg[index++] = minute;
    unsigned short verify = msg_verify(msg, index);
    msg[index++] = verify & 0xff;
    msg[index++] = verify >> 8;
    return gesture_uart_send(msg, index);
}
static void gesture_sync_time_and_alarm(int state, int alarm)
{
    dzlog_info("gesture_sync_time_and_alarm:%d,%d\n", state, alarm);
    if (state)
    {
        time_t systemTime;
        time(&systemTime);

        dzlog_info("gesture sync time:%ld\n", systemTime);
        if (systemTime < 1640966400) // 2022-01-01 00:00:00
        {
            gesture_send_msg(0, 1, 0xff, 0xff, alarm);
        }
        else
        {
            struct tm *local_tm = localtime(&systemTime);
            gesture_send_msg(0, 1, local_tm->tm_hour, local_tm->tm_min, alarm);
        }
    }
    else
    {
        gesture_send_msg(0, 0, 0, 0, alarm);
    }
}

static void gesture_sync_time_cb(int state)
{
    gesture_sync_time_and_alarm(state, 0);
}
void gesture_error_code_func(int *error_code)
{
    if (gesture_error_code > 0)
    {
        *error_code |= 1 << (gesture_error_code - 1);
    }
}

void gesture_error_show_func(int *error_show)
{
    if (gesture_error_code > 0 && *error_show == 0)
    {
        *error_show = gesture_error_code;
    }
}
void gesture_send_error_cloud(int error_code, int clear)
{
    dzlog_warn("%s,error_code:%d clear:%d", __func__, error_code, clear);
    unsigned char payload[8] = {0};
    int index = 0;
    unsigned int code = get_ErrorCode();
    dzlog_warn("%s,code:%d", __func__, code);
    if (code < 0)
        return;
    payload[index++] = 0x0a;
    if (clear)
    {
        gesture_error_code = 0;
        code &= ~(1 << (error_code - 1));
    }
    else
    {
        gesture_error_code = error_code;
        code |= 1 << (error_code - 1);
    }
    payload[index++] = code >> 24;
    payload[index++] = code >> 16;
    payload[index++] = code >> 8;
    payload[index++] = code;

    unsigned char codeShow = get_ErrorCodeShow();
    dzlog_warn("%s,codeShow:%d", __func__, codeShow);
    if (codeShow < 0)
        return;
    if (clear)
    {
        if (codeShow == error_code || codeShow == 0)
        {
            payload[index++] = 0x0b;
            payload[index++] = 0;
        }
    }
    else
    {
        if (codeShow == 0)
        {
            payload[index++] = 0x0b;
            payload[index++] = error_code;
        }
    }
    hdzlog_info(payload, index);
    send_data_to_cloud(payload, index, ECB_UART_COMMAND_EVENT);
}

void gesture_auto_sync_time_alarm(int alarm)
{
    if (get_link_state() > 0)
    {
        gesture_sync_time_and_alarm(1, alarm);
    }
    else
    {
        gesture_sync_time_and_alarm(0, alarm);
    }
}
void set_gesture_power(const int power)
{
    if (gesture_power != power)
    {
        gesture_power = power;

        gesture_auto_sync_time_alarm(0);
    }
}
static int gesture_uart_parse_msg(const unsigned char *in, const int in_len, int *end)
{
    static int gesture_recv_error = 0;
    int index = 0, i;
    if (in_len < 1)
    {
        *end = 0;
        dzlog_warn("input len too small");
        return ECB_UART_READ_LEN_SMALL;
    }

    for (i = 0; i < in_len; ++i)
    {
        if (in[i] == 0xaa)
            break;
    }
    if (i >= in_len)
    {
        dzlog_error("no header was detected");
        return ECB_UART_READ_NO_HEADER;
    }
    index = i;

    if (in_len - index < 10)
    {
        *end = index;
        dzlog_warn("input len too small");
        return ECB_UART_READ_LEN_SMALL;
    }

    // unsigned char type;
    unsigned char msg_len;
    // unsigned char cmd;
    unsigned short verify;
    unsigned char data1, data2;
    // unsigned char hour, minute;

    // type = in[index + 1];
    msg_len = in[index + 2];
    // cmd = in[index + 3];
    verify = (in[index + 8] << 8) + in[index + 9];

    unsigned short verify_val = msg_verify(&in[index], 8);
    if (verify_val != verify)
    {
        dzlog_error("verify error:%x,%x", verify_val, verify);
        *end = index + 10;
        return ECB_UART_READ_CHECK_ERR;
    }
    if (msg_len > 0)
    {
        data1 = in[index + 4];
        data2 = in[index + 5];
        dzlog_warn("%s,data1:%d data2:%d", __func__, data1, data2);
        unsigned char msg[8];
        unsigned char msg_len = 0;
        if (data1 & (1 << 1))
        {
            if (++gesture_recv_error >= 4)
            {
                if (gesture_recv_error % 4 == 0)
                {
                    gesture_auto_sync_time_alarm(0);
                }
                if (gesture_recv_error == 4)
                    gesture_send_error_cloud(GESTURE_ERROR, 0);
            }
        }
        else
        {
            if (gesture_recv_error > 0 || gesture_send_error > 2)
            {
                gesture_send_error_cloud(GESTURE_ERROR, 1);
                gesture_auto_sync_time_alarm(0);
            }
            gesture_send_error = 0;
            gesture_recv_error = 0;
        }

        if (data1 & (1 << 5)) //右挥标志位
        {
            char speed = get_HoodSpeed();
            if (speed >= 0 && speed < 3)
            {
                msg[msg_len++] = 0x31;
                if (speed == 0)
                    msg[msg_len++] = 2;
                else
                    msg[msg_len++] = ++speed;
            }
        }
        else if (data1 & (1 << 4)) //左挥标志位
        {
            char speed = get_HoodSpeed();
            if (speed > 0)
            {
                // if (speed == 1 && get_StoveStatus() > 0)
                // {
                // }
                // else
                // {
                msg[msg_len++] = 0x31;
                msg[msg_len++] = --speed;
                // }
            }
        }
        if (data1 & (1 << 6)) //闹钟提示标志位
        {
            if (gesture_alarm_status == 0)
            {
                cJSON *resp = cJSON_CreateObject();
                cJSON_AddNumberToObject(resp, "Alarm", 1);
                send_event_uds(resp, NULL);
                gesture_alarm_status = 1;
            }
        }
        else
        {
            if (gesture_alarm_status == 1)
            {
                cJSON *resp = cJSON_CreateObject();
                cJSON_AddNumberToObject(resp, "Alarm", 0);
                send_event_uds(resp, NULL);
                gesture_alarm_status = 0;
            }
        }

        if (data2 == 0x13)
        {
            msg[msg_len++] = 0xf7;
            msg[msg_len++] = 0x01;
        }
        else if (data2 == 0x14)
        {
            msg[msg_len++] = 0xf7;
            msg[msg_len++] = 0x03;
        }
        else if (data2 == 0x15)
        {
            msg[msg_len++] = 0xf7;
            msg[msg_len++] = 0x02;
        }

        if (msg_len > 0)
        {
            msg[msg_len++] = 0xf6;
            msg[msg_len++] = 0x00;
            ecb_uart_send_msg(ECB_UART_COMMAND_SET, msg, msg_len, 1, -1);
        }
        // hour = in[index + 6];
        // minute = in[index + 7];
    }
    *end = index + 10;

    return ECB_UART_READ_VALID;
}

static void gesture_POSIXTimer_cb(union sigval val)
{
    dzlog_warn("gesture_POSIXTimer_cb timeout:%d gesture_send_error:%d", val.sival_int, gesture_send_error);
    if (val.sival_int == 1)
    {
        gesture_auto_sync_time_alarm(0);
    }
    else if (val.sival_int == 2)
    {
        if (get_link_state() > 0)
        {
            gesture_send_msg(0, 1, 0xff, 0xff, 0);
        }
        else
        {
            gesture_send_msg(0, 0, 0, 0, 0);
        }
        if (gesture_send_error < 6)
        {
            if (++gesture_send_error == 6)
                gesture_send_error_cloud(GESTURE_ERROR, 0);
        }
    }
}

static int gesture_recv_cb(void *arg)
{
    static unsigned char uart_read_buf[256];
    int uart_read_len;
    static int uart_read_buf_index = 0;

    uart_read_len = read(fd, &uart_read_buf[uart_read_buf_index], sizeof(uart_read_buf) - uart_read_buf_index);
    if (uart_read_len > 0)
    {
        uart_read_buf_index += uart_read_len;
        dzlog_warn("recv from gesture-------------------------- uart_read_len:%d uart_read_buf_index:%d", uart_read_len, uart_read_buf_index);
        hdzlog_info(uart_read_buf, uart_read_buf_index);
        uart_parse_msg(uart_read_buf, &uart_read_buf_index, gesture_uart_parse_msg);
        dzlog_warn("uart_read_buf_index:%d", uart_read_buf_index);
        // hdzlog_info(uart_read_buf, uart_read_buf_index);
    }
    return 0;
}

static void gesture_link_timestamp_cb(const unsigned int timestamp)
{
    time_t time = timestamp;
    dzlog_warn("gesture_link_timestamp_cb:%ld", time);

    struct tm *local_tm = localtime(&time);
    gesture_send_msg(0, 1, local_tm->tm_hour, local_tm->tm_min, 0);
}

void gesture_uart_deinit(void)
{
    POSIXTimerDelete(g_gesture_timer);
    POSIXTimerDelete(g_gesture_heart_timer);
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
void gesture_uart_init(void)
{
    fd = uart_init("/dev/ttyS3", BAUDRATE_9600, DATABIT_8, PARITY_NONE, STOPBIT_1, FLOWCTRL_NONE, BLOCKING_BLOCK);
    if (fd < 0)
    {
        dzlog_error("gesture_uart uart init error:%d,%s", errno, strerror(errno));
        return;
    }
    dzlog_info("gesture_uart,fd:%d", fd);

    pthread_mutex_init(&lock, NULL);
    register_wifi_connected_cb(gesture_sync_time_cb);
    register_link_timestamp_cb(gesture_link_timestamp_cb);
    gesture_send_msg(1, 0, 0, 0, 0);

    g_gesture_timer = POSIXTimerCreate(1, gesture_POSIXTimer_cb);
    POSIXTimerSet(g_gesture_timer, 1 * 60 * 60, 60);
    g_gesture_heart_timer = POSIXTimerCreate(2, gesture_POSIXTimer_cb);
    POSIXTimerSet(g_gesture_heart_timer, 25, 25);

    select_client_event.fd = fd;
    select_client_event.read_cb = gesture_recv_cb;
    add_select_client_uart(&select_client_event);
}
