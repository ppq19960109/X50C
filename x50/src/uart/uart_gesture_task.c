#include "main.h"

#include "rkwifi.h"
#include "uart_ecb_task.h"
#include "uart_ecb_parse.h"
#include "uart_gesture_task.h"
#include "uart_cloud_task.h"
#include "wifi_task.h"

#define GESTURE_ERROR (20)
static unsigned char gesture_error_status = 0;
static int running = 0;
static timer_t g_gesture_timer;
static timer_t g_gesture_heart_timer;

static int gesture_fd = 0;
static pthread_mutex_t lock;
static unsigned char gesture_sync_time_start = 0;

unsigned short msg_verify(unsigned char *data, int len)
{
    unsigned short verify = 0;
    for (int i = 0; i < len; ++i)
    {
        verify += data[i];
    }
    // dzlog_debug("msg_verify:%d,%d", verify, ~verify);
    return ~verify;
}
void gesture_send_error_to_cloud(int error_code, int clear)
{
    unsigned char payload[8] = {0};
    int index = 0;
    unsigned int code = get_ErrorCode();

    payload[index++] = 0x0a;
    if (clear)
    {
        code &= ~(1 << (error_code - 1));
    }
    else
    {
        code |= 1 << (error_code - 1);
    }
    payload[index++] = code >> 24;
    payload[index++] = code >> 16;
    payload[index++] = code >> 6;
    payload[index++] = code;
    if (clear == 0 && get_ErrorCodeShow() == 0)
    {
        payload[index++] = 0x0b;
        payload[index++] = error_code;
    }
    send_data_to_cloud(payload, index);
}
static void sync_gesture_time(void)
{
    if (getWifiRunningState() == RK_WIFI_State_CONNECTED)
    {
        gesture_time_sync_task(1);
    }
    else
    {
        gesture_time_sync_task(0);
    }
}

static int uart_gesture_parse(unsigned char *in, const int in_len, int *end)
{
    static int gesture_recv_error = 0;
    int index, i;
    if (in_len < 1)
    {
        *end = 0;
        dzlog_error("input len too small");
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
        dzlog_error("input len too small");
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
        return ECB_UART_READ_CHECK_ERR;
    }
    if (msg_len > 0)
    {
        data1 = in[index + 4];
        data2 = in[index + 5];

        unsigned char msg[8];
        unsigned char msg_len = 0;
        if (data1 & (1 << 1))
        {
            if (++gesture_recv_error > 4)
            {
                gesture_recv_error = 0;
                sync_gesture_time();
            }
        }
        else
        {
            gesture_recv_error = 0;
        }

        if (data1 & (1 << 4))
        {
            msg[msg_len++] = 0xfa;
            msg[msg_len++] = 0x01;
        }
        else if (data1 & (1 << 5))
        {
            msg[msg_len++] = 0xfa;
            msg[msg_len++] = 0x02;
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
            gesture_send_error_to_cloud(GESTURE_ERROR, 0);
            gesture_error_status = 1;
        }

        if (gesture_error_status > 0 && data2 != 0x15)
        {
            gesture_error_status = 0;
            gesture_send_error_to_cloud(GESTURE_ERROR, 1);
        }
        if (msg_len > 0)
            uart_ecb_send_msg(ECB_UART_COMMAND_SET, msg, msg_len, 1);
        // hour = in[index + 6];
        // minute = in[index + 7];
    }
    *end = index + 10;

    return ECB_UART_READ_VALID;
}

static void uart_gesture_read_parse(unsigned char *in, int *in_len)
{
    int start = 0;
    ecb_uart_read_status_t status;
    for (;;)
    {
        dzlog_info("start:%d,in len:%d", start, *in_len);
        status = uart_gesture_parse(&in[start], *in_len, &start);
        *in_len -= start;

        if (status == ECB_UART_READ_VALID || status == ECB_UART_READ_CHECK_ERR || status == ECB_UART_READ_TAILER_ERR || status == ECB_UART_READ_LEN_ERR)
        {
            if (*in_len <= 0)
            {
                break;
            }
        }
        else if (status == ECB_UART_READ_LEN_SMALL)
        {
            break;
        }
        else if (status == ECB_UART_READ_NO_HEADER)
        {
            *in_len = 0;
            break;
        }
        else
        {
        }
    }
    dzlog_info("last start:%d,in len:%d", start, *in_len);
    if (*in_len <= 0)
    {
        *in_len = 0;
    }
    else if (start > 0)
    {
        memmove(in, &in[start], *in_len);
    }
}

int uart_send_gesture(unsigned char *in, int in_len)
{
    hdzlog_info(in, in_len);
    if (gesture_fd <= 0)
    {
        dzlog_error("uart_send_gesture fd error\n");
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

        res = write(gesture_fd, in, in_len);

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
int send_gesture_msg(unsigned char whole_show, unsigned char sync, unsigned char hour, unsigned char minute)
{
    unsigned char data1 = 0;
    data1 |= (1 << 0);
    if (sync)
        data1 |= (1 << 3);
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
    return uart_send_gesture(msg, index);
}

static void *gesture_time_sync(void *arg)
{
    dzlog_info("gesture_time_sync....................\n");
    systemRun("ntpdate pool.ntp.org && hwclock -w");
    // sleep(1);
    time_t systemTime;
    time(&systemTime);
    struct tm *local_tm = localtime(&systemTime);
    send_gesture_msg(0, 1, local_tm->tm_hour, local_tm->tm_min);

    return NULL;
}

void gesture_time_sync_task(int state)
{
    dzlog_info("gesture_time_sync_task:%d\n", state);
    if (state)
    {
        if (gesture_sync_time_start)
        {
            pthread_t tid;
            pthread_create(&tid, NULL, gesture_time_sync, NULL);
            pthread_detach(tid);
        }
    }
    else
    {
        send_gesture_msg(0, 0, 0, 0);
    }
}

static void POSIXTimer_gesture_cb(union sigval val)
{
    dzlog_warn("POSIXTimer_gesture_cb timeout:%d", val.sival_int);
    if (val.sival_int == 1)
    {
        gesture_sync_time_start = 1;
        sync_gesture_time();
    }
    else if (val.sival_int == 2)
    {
        if (getWifiRunningState() == RK_WIFI_State_CONNECTED)
        {
            send_gesture_msg(0, 1, 0xff, 0xff);
        }
        else
        {
            send_gesture_msg(0, 0, 0, 0);
        }
    }
}
void uart_gesture_task_close(void)
{
    running = 0;
}
//ntpdate pool.ntp.org
/*********************************************************************************
  *Function:  uart_ecb_task
  *Description： 手势任务函数，接收手势控制板的数据并处理
  *Input:  
  *Return:
**********************************************************************************/
void *uart_gesture_task(void *arg)
{
    static unsigned char uart_read_buf[128];
    int uart_read_len, uart_read_buf_index = 0;

    gesture_fd = uart_init("/dev/ttyS0", BAUDRATE_115200, DATABIT_8, PARITY_NONE, STOPBIT_1, FLOWCTRL_NONE);
    if (gesture_fd <= 0)
    {
        dzlog_error("uart_gesture_task uart init error:%d,%s", errno, strerror(errno));
        return NULL;
    }
    dzlog_info("uart_gesture_task,fd:%d", gesture_fd);

    pthread_mutex_init(&lock, NULL);
    register_wifi_connected_cb(gesture_time_sync_task);
    send_gesture_msg(1, 0, 0, 0);

    g_gesture_timer = POSIXTimerCreate(1, POSIXTimer_gesture_cb);
    POSIXTimerSet(g_gesture_timer, 1 * 60 * 60, 30);
    g_gesture_heart_timer = POSIXTimerCreate(2, POSIXTimer_gesture_cb);
    POSIXTimerSet(g_gesture_heart_timer, 30, 30);

    running = 1;
    while (running)
    {
        uart_read_len = read(gesture_fd, &uart_read_buf[uart_read_buf_index], sizeof(uart_read_buf) - uart_read_buf_index);
        if (uart_read_len > 0)
        {
            uart_read_buf_index += uart_read_len;
            hdzlog_info(uart_read_buf, uart_read_buf_index);
            uart_gesture_read_parse(uart_read_buf, &uart_read_buf_index);
            hdzlog_info(uart_read_buf, uart_read_buf_index);
        }
    }
    POSIXTimerDelete(g_gesture_timer);
    POSIXTimerDelete(g_gesture_heart_timer);
    close(gesture_fd);
    pthread_mutex_destroy(&lock);
    return NULL;
}
