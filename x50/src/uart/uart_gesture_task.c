#include "main.h"

#include "rkwifi.h"
#include "uart_gesture_task.h"
#include "wifi_task.h"

static int running = 0;
static timer_t g_gesture_timer;
static timer_t g_gesture_heart_timer;
static int gesture_fd = 0;
static pthread_mutex_t lock;
int uart_gesture_parse(unsigned char *data, int len)
{
    // unsigned char type;
    unsigned char msg_len;
    // unsigned char cmd;
    int verify;
    unsigned char data1;
    unsigned char hour, minute;
    for (int i = 0; i < len; ++i)
    {
        if (data[i] == 0xaa)
        {
            if (i + 10 < len)
            {
                continue;
            }
            // type = data[i + 1];
            msg_len = data[i + 2];
            // cmd = data[i + 3];
            verify = data[i + 8] + (data[i + 9] << 8);
            if (msg_len > 0)
            {
                data1 = data[i + 4];
                if (data1 & (1 << 1))
                {
                }
                else if (data1 & (1 << 0))
                {
                    if (data1 & (1 << 4))
                    {
                    }
                    else if (data1 & (1 << 5))
                    {
                    }
                }
                hour = data[i + 6];
                minute = data[i + 7];
            }
            i += 10;
        }
    }
    return 0;
}
int uart_send_gesture(unsigned char *in, int in_len)
{
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
        hdzlog_info(in, in_len);

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

unsigned short msg_verify(unsigned char *data, int len)
{
    unsigned short verify = 0;
    for (int i = 0; i < len; ++i)
    {
        verify += data[i];
    }
    return ~verify;
}
// aa 01 08 01 09 01 02 3f ff
// aa 01 08 01 01 00 00 4a ff
// aa 01 08 01 09 ff ff fd 42
// aa 01 08 01 01 ff ff fd 4a
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
    dzlog_info("gesture_time_sync\n");
    systemRun("ntpdate pool.ntp.org && hwclock -w");
    time_t systemTime;
    time(&systemTime);
    struct tm *local_tm = localtime(&systemTime);
    send_gesture_msg(0, 1, local_tm->tm_hour, local_tm->tm_min);

    return NULL;
}

void gesture_time_sync_task(int state)
{
    if (state)
    {
        pthread_t tid;
        pthread_create(&tid, NULL, gesture_time_sync, NULL);
        pthread_detach(tid);
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
        if (getWifiRunningState() == RK_WIFI_State_CONNECTED)
        {
            gesture_time_sync_task(1);
        }
        else
        {
            gesture_time_sync_task(0);
        }
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
    int uart_read_len;

    gesture_fd = uart_init("/dev/ttyS1", BAUDRATE_9600, DATABIT_8, PARITY_NONE, STOPBIT_1, FLOWCTRL_NONE);
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
    POSIXTimerSet(g_gesture_timer, 1 * 60 * 60, 10);
    g_gesture_heart_timer = POSIXTimerCreate(2, POSIXTimer_gesture_cb);
    POSIXTimerSet(g_gesture_heart_timer, 60, 60);

    running = 1;
    while (running)
    {
        uart_read_len = read(gesture_fd, uart_read_buf, sizeof(uart_read_buf));
        if (uart_read_len > 0)
        {
            hdzlog_info(uart_read_buf, uart_read_len);
            uart_gesture_parse(uart_read_buf, uart_read_len);
        }
    }
    POSIXTimerDelete(g_gesture_timer);
    POSIXTimerDelete(g_gesture_heart_timer);
    close(gesture_fd);
    pthread_mutex_destroy(&lock);
    return NULL;
}
